// ConPtySession.cpp
// Follows the Microsoft PTYCON (win32 sample) pattern exactly:
//   https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session

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

    if (FAILED(hr)) {
        Close();
        return false;
    }

    running_ = true;
    StartReader();
    return true;
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------
bool ConPtySession::Write(const void* data, size_t len) {
    if (!running_ || inputWriteSide_ == INVALID_HANDLE_VALUE || len == 0)
        return false;
    DWORD written = 0;
    BOOL  ok = WriteFile(inputWriteSide_, data, (DWORD)len, &written, nullptr);
    return ok && written == (DWORD)len;
}

bool ConPtySession::Write(const std::wstring& text) {
    int need = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(),
                                   nullptr, 0, nullptr, nullptr);
    if (need <= 0) return false;
    std::string utf8(need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(),
                        utf8.data(), need, nullptr, nullptr);
    return Write(utf8.data(), utf8.size());
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
// Close — follows PTYCON teardown order:
//   1. politely ask shell to exit
//   2. close inputWriteSide_ so PTY stdin gets EOF
//   3. wait / kill process
//   4. ClosePseudoConsole (unblocks final ReadFile on outputReadSide_)
//   5. drain / close outputReadSide_
//   6. join reader thread
//   7. free attribute list and remaining handles
// ---------------------------------------------------------------------------
void ConPtySession::Close() {
    if (closing_.exchange(true)) return;

    // Politely ask shell to exit
    if (running_ && inputWriteSide_ != INVALID_HANDLE_VALUE) {
        const char exitCmd[] = "exit\r\n";
        DWORD w = 0;
        WriteFile(inputWriteSide_, exitCmd, sizeof(exitCmd) - 1, &w, nullptr);
    }

    // Close our write end so the PTY sees EOF on stdin
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

    // ClosePseudoConsole — this signals the PTY output pipe so ReadFile unblocks
    if (hPC_ && fnClose_) {
        fnClose_(hPC_);
        hPC_ = nullptr;
    }

    running_ = false;

    // Close read side — reader loop will exit because ReadFile returns 0/error
    if (outputReadSide_ != INVALID_HANDLE_VALUE) {
        CloseHandle(outputReadSide_);
        outputReadSide_ = INVALID_HANDLE_VALUE;
    }

    if (readerThread_) {
        WaitForSingleObject(readerThread_, 2000);
        CloseHandle(readerThread_);
        readerThread_ = nullptr;
    }

    // Free attribute list (HeapAlloc'd per PTYCON sample)
    if (attrList_) {
        DeleteProcThreadAttributeList(attrList_);
        HeapFree(GetProcessHeap(), 0, attrList_);
        attrList_ = nullptr;
    }

    CloseHandle_(hThread_);
    CloseHandle_(hProcess_);
    processId_ = 0;
    closing_   = false;
}

// ---------------------------------------------------------------------------
// LoadApi — dynamically resolve ConPTY entry points from kernel32.dll
// ---------------------------------------------------------------------------
HRESULT ConPtySession::LoadApi() {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) {
        lastError_ = L"GetModuleHandle(kernel32.dll) failed";
        return HRESULT_FROM_WIN32(GetLastError());
    }

    fnCreate_ = (FnCreate)GetProcAddress(k32, "CreatePseudoConsole");
    fnResize_ = (FnResize)GetProcAddress(k32, "ResizePseudoConsole");
    fnClose_  = (FnClose) GetProcAddress(k32, "ClosePseudoConsole");

    if (!fnCreate_ || !fnResize_ || !fnClose_) {
        lastError_ = L"ConPTY not available — Windows 10 1809 or newer required.";
        return E_NOTIMPL;
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// SetUpPseudoConsole (PTYCON: SetUpPseudoConsole)
//
// Creates two anonymous pipes then calls CreatePseudoConsole.
// The PTY-side handles (inputReadSide_, outputWriteSide_) are closed here
// immediately after CreatePseudoConsole returns, exactly as the PTYCON sample
// does (the PTY now owns them internally).
// ---------------------------------------------------------------------------
HRESULT ConPtySession::SetUpPseudoConsole(int cols, int rows) {
    // PTYCON sample: pipes must be inheritable for CreatePseudoConsole to duplicate them
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

    // Input pipe: we write to inputWriteSide_; PTY reads from inputReadSide_
    if (!CreatePipe(&inputReadSide_, &inputWriteSide_, &sa, 0)) {
        lastError_ = L"CreatePipe(input) failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Output pipe: PTY writes to outputWriteSide_; we read from outputReadSide_
    if (!CreatePipe(&outputReadSide_, &outputWriteSide_, &sa, 0)) {
        lastError_ = L"CreatePipe(output) failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const COORD consoleSize{
        (SHORT)std::max(20, cols),
        (SHORT)std::max(5,  rows)
    };

    HRESULT hr = fnCreate_(consoleSize, inputReadSide_, outputWriteSide_,
                           0 /* no PSEUDOCONSOLE_INHERIT_CURSOR to avoid deadlock */,
                           &hPC_);
    if (FAILED(hr)) {
        wchar_t buf[80];
        swprintf_s(buf, L"CreatePseudoConsole failed: 0x%08lX", (unsigned long)hr);
        lastError_ = buf;
        return hr;
    }

    // Close PTY-side pipe handles — ConPTY now owns them.
    // (PTYCON sample closes these immediately after CreatePseudoConsole.)
    CloseHandle(inputReadSide_);   inputReadSide_   = INVALID_HANDLE_VALUE;
    CloseHandle(outputWriteSide_); outputWriteSide_ = INVALID_HANDLE_VALUE;

    // Prevent our pipe ends from leaking to the child process
    SetHandleInformation(inputWriteSide_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outputReadSide_, HANDLE_FLAG_INHERIT, 0);

    // Make the HPCON handle inheritable so CreateProcessW can attach it to the child
    SetHandleInformation(hPC_, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    return S_OK;
}

// ---------------------------------------------------------------------------
// PrepareStartupInformation (PTYCON: PrepareStartupInformation)
//
// Allocates the proc-thread attribute list via HeapAlloc (matching the PTYCON
// sample), then attaches the HPCON to PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE.
// The caller (Close) must call DeleteProcThreadAttributeList + HeapFree.
// ---------------------------------------------------------------------------
HRESULT ConPtySession::PrepareStartupInformation() {
    siEx_ = {};
    siEx_.StartupInfo.cb = sizeof(siEx_);

    // First call: determine required buffer size
    SIZE_T attrBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrBytes);
    // attrBytes is now set to the required size

    // PTYCON sample uses HeapAlloc, not std::vector
    attrList_ = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrBytes);
    if (!attrList_) {
        lastError_ = L"HeapAlloc for attribute list failed";
        return E_OUTOFMEMORY;
    }

    if (!InitializeProcThreadAttributeList(attrList_, 1, 0, &attrBytes)) {
        lastError_ = L"InitializeProcThreadAttributeList failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (!UpdateProcThreadAttribute(attrList_, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
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
    std::wstring cmdLine  = shell; // CreateProcessW may mutate; keep a copy

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr, nullptr,
        TRUE,                                               // bInheritHandles (needed for PTY attachment)
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock.data(),
        nullptr,                                            // current directory
        reinterpret_cast<LPSTARTUPINFOW>(&siEx_),
        &pi);

    if (!ok) {
        lastError_ = L"CreateProcessW failed: " + Win32Error();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    hProcess_  = pi.hProcess;
    hThread_   = pi.hThread;
    processId_ = pi.dwProcessId;
    return S_OK;
}

// ---------------------------------------------------------------------------
// BuildEnvBlock — snapshot + override terminal identity + caller extras
// ---------------------------------------------------------------------------
std::wstring ConPtySession::BuildEnvBlock(
        const std::vector<std::pair<std::wstring,std::wstring>>& extra) {

    // Mandatory terminal identity vars (same set as termdock + terminalpp)
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
                    entry.substr(0, kv.first.size() + 1) == kv.first + L'=') {
                    skip = true; break;
                }
            if (!skip)
                for (auto& kv : extra)
                    if (entry.size() > kv.first.size() &&
                        entry.substr(0, kv.first.size() + 1) == kv.first + L'=') {
                        skip = true; break;
                    }
            if (!skip) { block += entry; block += L'\0'; }
        }
        FreeEnvironmentStringsW(env);
    }

    for (auto& kv : termVars) { block += kv.first + L'=' + kv.second; block += L'\0'; }
    for (auto& kv : extra)    { block += kv.first + L'=' + kv.second; block += L'\0'; }
    block += L'\0'; // double-null terminator
    return block;
}

// ---------------------------------------------------------------------------
// Reader thread — uses Win32 CreateThread to avoid MSVC <thread>/_beginthreadex issues
// ---------------------------------------------------------------------------
static DWORD WINAPI ReaderThreadProc(LPVOID param) {
    reinterpret_cast<ConPtySession*>(param)->ReaderLoop();
    return 0;
}

void ConPtySession::StartReader() {
    DWORD threadId = 0;
    readerThread_ = CreateThread(nullptr, 0, ReaderThreadProc, this, 0, &threadId);
}

void ConPtySession::ReaderLoop() {
    char buf[4096];
    while (true) {
        DWORD bytesRead = 0;
        // ReadFile returns FALSE when ClosePseudoConsole unblocks the output pipe
        BOOL ok = ReadFile(outputReadSide_, buf, (DWORD)sizeof(buf), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) break;
        if (onOutput_) onOutput_(buf, bytesRead);
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
