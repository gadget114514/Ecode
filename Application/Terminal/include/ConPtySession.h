#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// ConPtySession
//
// Pure Win32 ConPTY wrapper following the Microsoft PTYCON sample pattern.
// https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
//
// Pipe naming matches the PTYCON sample exactly:
//   inputReadSide_ / outputWriteSide_  — PTY-side (closed after CreatePseudoConsole)
//   outputReadSide_ / inputWriteSide_  — terminal-side (retained by this class)
//
// Requires Windows 10 1809 (Build 17763) or later.
// ConPTY functions are loaded dynamically; Start() returns false on older systems.
// ---------------------------------------------------------------------------
class ConPtySession {
public:
    ConPtySession();
    ~ConPtySession();

    ConPtySession(const ConPtySession&)            = delete;
    ConPtySession& operator=(const ConPtySession&) = delete;

    // Register callback BEFORE calling Start().
    void SetOutputCallback(std::function<void(const char*, size_t)> cb);

    // Start the PTY + shell process.
    //   shell    — command line, e.g. L"powershell.exe"
    //   cols/rows — initial terminal dimensions
    //   extraEnv — additional KEY=VALUE pairs appended to the inherited env block
    bool Start(const std::wstring& shell,
               int cols, int rows,
               const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv = {});

    // Write raw bytes to the PTY input (inputWriteSide_).
    bool Write(const void* data, size_t len);
    bool Write(const std::string& text) { return Write(text.data(), text.size()); }
    bool Write(const std::wstring& text); // UTF-16 → UTF-8 then Write(void*)

    // Resize the pseudo-console.
    void Resize(int cols, int rows);

    // Graceful close: sends "exit\r\n", waits briefly, then kills.
    void Close();

    bool  IsRunning()  const { return running_; }
    DWORD ProcessId()  const { return processId_; }
    HANDLE GetProcessHandle() const { return hProcess_; }

    // Filled on Start() failure.
    std::wstring LastError() const { return lastError_; }

private:
    // --- PTYCON-pattern helpers ---
    HRESULT LoadApi();
    HRESULT SetUpPseudoConsole(int cols, int rows);   // CreatePipe x2 + CreatePseudoConsole
    HRESULT PrepareStartupInformation();              // HeapAlloc attr list + Update PCA
    HRESULT LaunchProcess(const std::wstring& shell,
                          const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv);

    void StartReader();
    void ReaderLoop();

    void StartWriter();
    void WriterLoop();

    friend DWORD WINAPI ReaderThreadProc(LPVOID param);
    friend DWORD WINAPI WriterThreadProc(LPVOID param);

    std::wstring BuildEnvBlock(const std::vector<std::pair<std::wstring,std::wstring>>& extra);
    void CloseHandle_(HANDLE& h);
    std::wstring Win32Error(DWORD code = 0);

    // --- ConPTY function pointer types ---
    using FnCreate = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
    using FnResize = HRESULT (WINAPI*)(HPCON, COORD);
    using FnClose  = void    (WINAPI*)(HPCON);

    FnCreate fnCreate_ = nullptr;
    FnResize fnResize_ = nullptr;
    FnClose  fnClose_  = nullptr;
    HMODULE  hConptyDll_ = nullptr;

    HPCON  hPC_ = nullptr;

    // PTYCON pipe naming:
    HANDLE inputReadSide_   = INVALID_HANDLE_VALUE; // PTY-side  (closed after CreatePseudoConsole)
    HANDLE outputWriteSide_ = INVALID_HANDLE_VALUE; // PTY-side  (closed after CreatePseudoConsole)
    HANDLE inputWriteSide_  = INVALID_HANDLE_VALUE; // our side  (write shell input here)
    HANDLE outputReadSide_  = INVALID_HANDLE_VALUE; // our side  (read shell output here)

    // Startup attribute list — HeapAlloc'd per PTYCON sample, freed in Close()
    PPROC_THREAD_ATTRIBUTE_LIST attrList_ = nullptr;
    STARTUPINFOEXW              siEx_{};

    HANDLE hProcess_      = nullptr;
    HANDLE hThread_       = nullptr;  // process main thread
    DWORD  processId_     = 0;

    HANDLE readerThread_  = nullptr;  // Win32 reader thread (avoids std::thread issues)
    HANDLE writerThread_  = nullptr;  // Win32 writer thread
    HANDLE writeWakeEvent_ = nullptr; // auto-reset event to wake writer

    std::mutex writeMutex_;
    std::queue<std::vector<char>> writeQueue_;

    std::atomic<bool> running_{false};
    std::atomic<bool> closing_{false};

    std::function<void(const char*, size_t)> onOutput_;
    std::wstring lastError_;
};
