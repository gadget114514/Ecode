// ConPtySession.cpp
// Single IoThread with OVERLAPPED I/O for select()-style inter-stream serialization.
// Named pipes are required — anonymous CreatePipe does not support FILE_FLAG_OVERLAPPED.

#include "../include/ConPtySession.h"
#include <algorithm>
#include <array>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------
ConPtySession::ConPtySession() = default;
ConPtySession::~ConPtySession() { Close(); }

void ConPtySession::SetOutputCallback(std::function<void(const char*, size_t)> cb) {
    onOutput_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
bool ConPtySession::Start(const std::wstring& shell, int cols, int rows,
                          const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv) {
    if (running_) return true;
    closing_ = false;

    HRESULT hr = LoadApi();
    if (SUCCEEDED(hr)) hr = SetUpPseudoConsole(cols, rows);
    if (SUCCEEDED(hr)) hr = PrepareStartupInformation();
    if (SUCCEEDED(hr)) hr = LaunchProcess(shell, extraEnv);

    if (FAILED(hr)) { Close(); return false; }

    running_ = true;
    StartIoThread();
    return true;
}

// ---------------------------------------------------------------------------
// Write — queue data; IoThread drains it with minimum scheduling latency.
// ---------------------------------------------------------------------------
bool ConPtySession::Write(const void* data, size_t len) {
    if (!running_ || inputWriteSide_ == INVALID_HANDLE_VALUE || len == 0)
        return false;
    std::vector<char> copy(static_cast<const char*>(data),
                           static_cast<const char*>(data) + len);
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        writeQueue_.push_back(std::move(copy));
    }
    SetEvent(ioWakeEvent_);
    return true;
}

// ---------------------------------------------------------------------------
// WriteFront — insert at front of write queue so emulator responses (DSR, DA,
// window reports) reach the shell before any pending user keystrokes.
// ---------------------------------------------------------------------------
bool ConPtySession::WriteFront(const void* data, size_t len) {
    if (!running_ || inputWriteSide_ == INVALID_HANDLE_VALUE || len == 0)
        return false;
    std::vector<char> copy(static_cast<const char*>(data),
                           static_cast<const char*>(data) + len);
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        writeQueue_.push_front(std::move(copy));
    }
    SetEvent(ioWakeEvent_);
    return true;
}

bool ConPtySession::WriteFront(const std::wstring& text) {
    int need = WideCharToMultiByte(codePage_, 0, text.c_str(), (int)text.size(),
                                   nullptr, 0, nullptr, nullptr);
    if (need <= 0) return false;
    std::string encoded(need, '\0');
    WideCharToMultiByte(codePage_, 0, text.c_str(), (int)text.size(),
                        encoded.data(), need, nullptr, nullptr);
    return WriteFront(encoded.data(), encoded.size());
}

bool ConPtySession::Write(const std::wstring& text) {
    int need = WideCharToMultiByte(codePage_, 0, text.c_str(), (int)text.size(),
                                   nullptr, 0, nullptr, nullptr);
    if (need <= 0) return false;
    std::string encoded(need, '\0');
    WideCharToMultiByte(codePage_, 0, text.c_str(), (int)text.size(),
                        encoded.data(), need, nullptr, nullptr);
    return Write(encoded.data(), encoded.size());
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------
void ConPtySession::Resize(int cols, int rows) {
    if (!hPC_ || !fnResize_ || cols <= 0 || rows <= 0) return;
    const COORD sz{ (SHORT)std::max(2, cols), (SHORT)std::max(1, rows) };
    fnResize_(hPC_, sz);
}

// ---------------------------------------------------------------------------
// Close — PTYCON teardown order:
//   1. best-effort "exit\r\n" through IoThread
//   2. signal closeEvent_ → IoThread cancels pending I/O and exits
//   3. join IoThread, close event handles
//   4. close inputWriteSide_ (PTY stdin gets EOF)
//   5. wait / kill process
//   6. ClosePseudoConsole (unblocks any residual ReadFile)
//   7. close outputReadSide_, free attribute list, close process handles
// ---------------------------------------------------------------------------
void ConPtySession::Close() {
    if (closing_.exchange(true)) return;

    // Best-effort: queue exit command and let IoThread flush it
    if (running_ && inputWriteSide_ != INVALID_HANDLE_VALUE && ioWakeEvent_) {
        const char exitCmd[] = "exit\r\n";
        std::vector<char> cmd(exitCmd, exitCmd + sizeof(exitCmd) - 1);
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            writeQueue_.push_back(std::move(cmd));
        }
        SetEvent(ioWakeEvent_);
        Sleep(50);
    }

    // Signal IoThread to cancel pending I/O and exit
    if (closeEvent_) SetEvent(closeEvent_);

    if (ioThread_) {
        WaitForSingleObject(ioThread_, 2000);
        CloseHandle(ioThread_);
        ioThread_ = nullptr;
    }

    if (readEvent_)   { CloseHandle(readEvent_);   readEvent_   = nullptr; }
    if (writeEvent_)  { CloseHandle(writeEvent_);  writeEvent_  = nullptr; }
    if (ioWakeEvent_) { CloseHandle(ioWakeEvent_); ioWakeEvent_ = nullptr; }
    if (closeEvent_)  { CloseHandle(closeEvent_);  closeEvent_  = nullptr; }

    // Close our write end — PTY stdin gets EOF
    if (inputWriteSide_ != INVALID_HANDLE_VALUE) {
        CloseHandle(inputWriteSide_);
        inputWriteSide_ = INVALID_HANDLE_VALUE;
    }

    // Wait briefly for process to exit, then kill
    if (hProcess_) {
        if (WaitForSingleObject(hProcess_, 700) == WAIT_TIMEOUT)
            TerminateProcess(hProcess_, 1);
        WaitForSingleObject(hProcess_, 500);
    }

    // ClosePseudoConsole — unblocks any residual ReadFile on outputReadSide_
    if (hPC_ && fnClose_) { fnClose_(hPC_); hPC_ = nullptr; }

    running_ = false;

    if (outputReadSide_ != INVALID_HANDLE_VALUE) {
        CloseHandle(outputReadSide_);
        outputReadSide_ = INVALID_HANDLE_VALUE;
    }

    if (attrList_) {
        DeleteProcThreadAttributeList(attrList_);
        HeapFree(GetProcessHeap(), 0, attrList_);
        attrList_ = nullptr;
    }

    CloseHandle_(hThread_);
    CloseHandle_(hProcess_);
    processId_ = 0;

    if (hConptyDll_) { FreeLibrary(hConptyDll_); hConptyDll_ = nullptr; }
    fnCreate_ = nullptr; fnResize_ = nullptr; fnClose_ = nullptr;

    closing_ = false;
}

// ---------------------------------------------------------------------------
// LoadApi — load ConPTY entry points from conpty.dll
// ---------------------------------------------------------------------------
HRESULT ConPtySession::LoadApi() {
    hConptyDll_ = LoadLibraryExW(L"conpty.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
    if (!hConptyDll_) {
        lastError_ = L"Failed to load conpty.dll from application directory";
        return HRESULT_FROM_WIN32(GetLastError());
    }
    fnCreate_ = (FnCreate)GetProcAddress(hConptyDll_, "ConptyCreatePseudoConsole");
    fnResize_ = (FnResize)GetProcAddress(hConptyDll_, "ConptyResizePseudoConsole");
    fnClose_  = (FnClose) GetProcAddress(hConptyDll_, "ConptyClosePseudoConsole");
    if (!fnCreate_ || !fnResize_ || !fnClose_) {
        lastError_ = L"ConptyCreatePseudoConsole not found in conpty.dll";
        FreeLibrary(hConptyDll_); hConptyDll_ = nullptr;
        return E_NOTIMPL;
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// SetUpPseudoConsole — named pipes for OVERLAPPED I/O
//
// Anonymous CreatePipe cannot be opened with FILE_FLAG_OVERLAPPED; named pipes
// can.  The server ends (our side) carry FILE_FLAG_OVERLAPPED; the client ends
// (PTY side) are inheritable handles passed to CreatePseudoConsole.
// ---------------------------------------------------------------------------
HRESULT ConPtySession::SetUpPseudoConsole(int cols, int rows) {
    DWORD pid = GetCurrentProcessId();
    static LONG pipeSeq = 0;
    LONG seq = InterlockedIncrement(&pipeSeq);

    wchar_t inName[64], outName[64];
    swprintf_s(inName,  L"\\\\.\\pipe\\conpty-in-%lu-%ld",  (unsigned long)pid, (long)seq);
    swprintf_s(outName, L"\\\\.\\pipe\\conpty-out-%lu-%ld", (unsigned long)pid, (long)seq);

    // Input pipe: server end = inputWriteSide_ (we write, OVERLAPPED)
    //             client end = inputReadSide_  (PTY reads, inheritable)
    inputWriteSide_ = CreateNamedPipeW(inName,
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 4096, 4096, 0, nullptr);
    if (inputWriteSide_ == INVALID_HANDLE_VALUE) {
        lastError_ = L"CreateNamedPipe(in) failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Output pipe: server end = outputReadSide_ (we read, OVERLAPPED)
    //              client end = outputWriteSide_ (PTY writes, inheritable)
    outputReadSide_ = CreateNamedPipeW(outName,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 4096, 4096, 0, nullptr);
    if (outputReadSide_ == INVALID_HANDLE_VALUE) {
        lastError_ = L"CreateNamedPipe(out) failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Client ends: inheritable so CreatePseudoConsole can pass them to the child
    SECURITY_ATTRIBUTES saInh = { sizeof(saInh), nullptr, TRUE };

    inputReadSide_ = CreateFileW(inName, GENERIC_READ, 0, &saInh, OPEN_EXISTING, 0, nullptr);
    if (inputReadSide_ == INVALID_HANDLE_VALUE) {
        lastError_ = L"CreateFile(in client) failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    outputWriteSide_ = CreateFileW(outName, GENERIC_WRITE, 0, &saInh, OPEN_EXISTING, 0, nullptr);
    if (outputWriteSide_ == INVALID_HANDLE_VALUE) {
        lastError_ = L"CreateFile(out client) failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const COORD consoleSize{ (SHORT)std::max(20, cols), (SHORT)std::max(5, rows) };
    HRESULT hr = fnCreate_(consoleSize, inputReadSide_, outputWriteSide_, 0, &hPC_);
    if (FAILED(hr)) {
        wchar_t buf[80];
        swprintf_s(buf, L"CreatePseudoConsole failed: 0x%08lX", (unsigned long)hr);
        lastError_ = buf;
        return hr;
    }

    // Close PTY-side handles — ConPTY now owns them internally
    CloseHandle(inputReadSide_);   inputReadSide_   = INVALID_HANDLE_VALUE;
    CloseHandle(outputWriteSide_); outputWriteSide_ = INVALID_HANDLE_VALUE;

    SetHandleInformation(inputWriteSide_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outputReadSide_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hPC_, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    return S_OK;
}

// ---------------------------------------------------------------------------
// PrepareStartupInformation (PTYCON: PrepareStartupInformation)
// ---------------------------------------------------------------------------
HRESULT ConPtySession::PrepareStartupInformation() {
    siEx_ = {};
    siEx_.StartupInfo.cb = sizeof(siEx_);
    SIZE_T attrBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrBytes);
    attrList_ = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrBytes);
    if (!attrList_) {
        lastError_ = L"HeapAlloc for attribute list failed";
        return E_OUTOFMEMORY;
    }
    if (!InitializeProcThreadAttributeList(attrList_, 1, 0, &attrBytes)) {
        lastError_ = L"InitializeProcThreadAttributeList failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if (!UpdateProcThreadAttribute(attrList_, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   hPC_, sizeof(hPC_), nullptr, nullptr)) {
        lastError_ = L"UpdateProcThreadAttribute failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }
    siEx_.lpAttributeList = attrList_;
    return S_OK;
}

// ---------------------------------------------------------------------------
// LaunchProcess
// ---------------------------------------------------------------------------
HRESULT ConPtySession::LaunchProcess(
        const std::wstring& shell,
        const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv) {
    std::wstring envBlock = BuildEnvBlock(extraEnv);
    std::wstring cmdLine  = shell;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr,
        TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock.data(), nullptr,
        reinterpret_cast<LPSTARTUPINFOW>(&siEx_), &pi);
    if (!ok) {
        lastError_ = L"CreateProcessW failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }
    hProcess_ = pi.hProcess; hThread_ = pi.hThread; processId_ = pi.dwProcessId;
    return S_OK;
}

// ---------------------------------------------------------------------------
// BuildEnvBlock
// ---------------------------------------------------------------------------
std::wstring ConPtySession::BuildEnvBlock(
        const std::vector<std::pair<std::wstring,std::wstring>>& extra) {
    const std::vector<std::pair<std::wstring,std::wstring>> termVars = {
        { L"TERM",           L"xterm-256color" },
        { L"COLORTERM",      L"truecolor"      },
        { L"CLICOLOR_FORCE", L"1"              },
        { L"FORCE_COLOR",    L"3"              },
    };
    wchar_t* env = GetEnvironmentStringsW();
    std::wstring block;
    if (env) {
        for (const wchar_t* p = env; *p; ) {
            std::wstring entry(p);
            p += entry.size() + 1;
            bool skip = false;
            for (auto& kv : termVars)
                if (entry.size() > kv.first.size() &&
                    entry.substr(0, kv.first.size() + 1) == kv.first + L'=')
                    { skip = true; break; }
            if (!skip)
                for (auto& kv : extra)
                    if (entry.size() > kv.first.size() &&
                        entry.substr(0, kv.first.size() + 1) == kv.first + L'=')
                        { skip = true; break; }
            if (!skip) { block += entry; block += L'\0'; }
        }
        FreeEnvironmentStringsW(env);
    }
    for (auto& kv : termVars) { block += kv.first + L'=' + kv.second; block += L'\0'; }
    for (auto& kv : extra)    { block += kv.first + L'=' + kv.second; block += L'\0'; }
    block += L'\0';
    return block;
}

// ---------------------------------------------------------------------------
// IoThread — single thread serializes both read and write via OVERLAPPED I/O
// and WaitForMultipleObjects (Windows equivalent of select(2)).
//
// Event array (priority order = index order in WaitForMultipleObjects):
//   [0] closeEvent_   — manual-reset, highest priority: cancel I/O and exit
//   [1] readEvent_    — manual-reset, OVERLAPPED ReadFile completion
//   [2] ioWakeEvent_  — auto-reset,   new data added to write queue
//   [3] writeEvent_   — manual-reset, OVERLAPPED WriteFile completion
//
// Write-first discipline: before waiting, always check the write queue and
// issue a write if one is pending.  This ensures user input reaches ConPTY
// with minimum latency relative to the output stream, replicating the causal
// ordering that Unix select() provides on a single PTY fd.
// ---------------------------------------------------------------------------
DWORD WINAPI IoThreadProc(LPVOID param) {
    reinterpret_cast<ConPtySession*>(param)->IoLoop();
    return 0;
}

void ConPtySession::StartIoThread() {
    closeEvent_  = CreateEventW(nullptr, TRUE,  FALSE, nullptr); // manual-reset
    readEvent_   = CreateEventW(nullptr, TRUE,  FALSE, nullptr); // manual-reset
    ioWakeEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr); // auto-reset
    writeEvent_  = CreateEventW(nullptr, TRUE,  FALSE, nullptr); // manual-reset

    DWORD threadId = 0;
    ioThread_ = CreateThread(nullptr, 0, IoThreadProc, this, 0, &threadId);
}

void ConPtySession::IoLoop() {
    HANDLE events[4] = { closeEvent_, readEvent_, ioWakeEvent_, writeEvent_ };

    // Issue the initial overlapped read
    readOv_ = {};
    readOv_.hEvent = readEvent_;
    ResetEvent(readEvent_);
    DWORD bytesRead = 0;
    if (!ReadFile(outputReadSide_, readBuf_, sizeof(readBuf_), &bytesRead, &readOv_)) {
        if (GetLastError() != ERROR_IO_PENDING) { running_ = false; return; }
    }

    bool writePending = false;

    for (;;) {
        // Write-first: issue one write before waiting so user input reaches
        // ConPTY before we process the next read event.
        if (!writePending) {
            std::vector<char> data;
            {
                std::lock_guard<std::mutex> lock(writeMutex_);
                if (!writeQueue_.empty()) {
                    data = std::move(writeQueue_.front());
                    writeQueue_.pop_front();
                }
            }
            if (!data.empty()) {
                writeBuf_ = std::move(data);
                writeOv_ = {};
                writeOv_.hEvent = writeEvent_;
                ResetEvent(writeEvent_);
                DWORD written = 0;
                if (!WriteFile(inputWriteSide_, writeBuf_.data(), (DWORD)writeBuf_.size(),
                               &written, &writeOv_)) {
                    if (GetLastError() != ERROR_IO_PENDING) break;
                }
                writePending = true;
            }
        }

        DWORD r = WaitForMultipleObjects(4, events, FALSE, INFINITE);

        if (r == WAIT_OBJECT_0) {
            // closeEvent_: cancel any pending I/O and exit
            CancelIoEx(outputReadSide_, &readOv_);
            if (writePending) CancelIoEx(inputWriteSide_, &writeOv_);
            DWORD bytes = 0;
            GetOverlappedResult(outputReadSide_, &readOv_, &bytes, TRUE);
            if (writePending) GetOverlappedResult(inputWriteSide_, &writeOv_, &bytes, TRUE);
            break;

        } else if (r == WAIT_OBJECT_0 + 1) {
            // readEvent_: read completed
            DWORD bytes = 0;
            BOOL ok = GetOverlappedResult(outputReadSide_, &readOv_, &bytes, FALSE);
            if (ok && bytes > 0 && onOutput_) onOutput_(readBuf_, bytes);
            if (!ok) break;

            // Rearm: issue next overlapped read immediately
            readOv_ = {};
            readOv_.hEvent = readEvent_;
            ResetEvent(readEvent_);
            if (!ReadFile(outputReadSide_, readBuf_, sizeof(readBuf_), &bytes, &readOv_)) {
                if (GetLastError() != ERROR_IO_PENDING) break;
            }

        } else if (r == WAIT_OBJECT_0 + 2) {
            // ioWakeEvent_: new write data queued — write-first check at top handles it
            (void)0;

        } else if (r == WAIT_OBJECT_0 + 3) {
            // writeEvent_: write completed
            DWORD bytes = 0;
            GetOverlappedResult(inputWriteSide_, &writeOv_, &bytes, FALSE);
            writePending = false;
            ResetEvent(writeEvent_);

        } else {
            break;
        }
    }

    running_ = false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void ConPtySession::CloseHandle_(HANDLE& h) {
    if (h && h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = nullptr; }
}

std::wstring ConPtySession::Win32Error(DWORD code) {
    if (code == 0) code = GetLastError();
    LPWSTR buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&buf, 0, nullptr);
    std::wstring msg = buf ? buf : L"Unknown error";
    if (buf) LocalFree(buf);
    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n'))
        msg.pop_back();
    return msg;
}
