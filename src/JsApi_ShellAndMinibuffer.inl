// =============================================================================
// JsApi_ShellAndMinibuffer.inl
// JS API: Minibuffer, shell process management
// Included by ScriptEngine.cpp
// =============================================================================

// Extern declarations for minibuffer in main.cpp
extern HWND g_minibufferHwnd;
extern bool g_minibufferVisible;
extern std::string g_minibufferPrompt;
extern int g_minibufferMode; // 0=EVAL, 1=MX_COMMAND, 2=CALLBACK
extern std::string g_minibufferJsCallback;

static duk_ret_t js_editor_get_buffer_count(duk_context *ctx) {
  if (g_editor) {
    duk_push_number(ctx, (double)g_editor->GetBuffers().size());
    return 1;
  }
  duk_push_number(ctx, 0);
  return 1;
}

static duk_ret_t js_editor_show_minibuffer(duk_context *ctx) {
  DebugLog("--> Entering js_editor_show_minibuffer", LOG_INFO);
  const char *prompt = duk_get_string(ctx, 0);
  const char *mode = duk_get_string(ctx, 1);

  if (prompt) {
    g_minibufferPrompt = prompt;
  }

  if (mode) {
    std::string modeStr(mode);
    if (modeStr == "mx")
      g_minibufferMode = 1; // MB_MX_COMMAND
    else if (modeStr == "eval")
      g_minibufferMode = 0; // MB_EVAL
    else if (modeStr == "callback") {
      g_minibufferMode = 2; // MB_CALLBACK
      const char *cb = duk_get_string(ctx, 2);
      g_minibufferJsCallback = cb ? cb : "";
    } else
      g_minibufferMode = 0; // MB_EVAL
  }

  DebugLog("js_editor_show_minibuffer: prompt=" + g_minibufferPrompt +
               " mode=" + std::to_string(g_minibufferMode),
           LOG_INFO);
  DebugLog(
      "  g_mainHwnd=" + std::to_string((long long)g_mainHwnd) +
          " g_minibufferHwnd=" + std::to_string((long long)g_minibufferHwnd),
      LOG_INFO);

  g_minibufferVisible = true;
  LRESULT sizeRes = SendMessage(g_mainHwnd, WM_SIZE, 0, 0);
  DebugLog("  WM_SIZE sent, result=" + std::to_string(sizeRes), LOG_INFO);

  SetWindowTextA(g_minibufferHwnd, "");
  BOOL show1 = ShowWindow(g_minibufferHwnd, SW_SHOW);
  BOOL show2 = ShowWindow(g_minibufferPromptHwnd, SW_SHOW);
  DebugLog("  ShowWindow mb=" + std::to_string(show1) +
               " prompt=" + std::to_string(show2),
           LOG_INFO);

  SetFocus(g_minibufferHwnd);
  InvalidateRect(g_mainHwnd, NULL, FALSE);
  DebugLog("<-- Exiting js_editor_show_minibuffer", LOG_INFO);
  return 0;
}

static duk_ret_t js_editor_open_shell(duk_context *ctx) {
  if (!g_editor)
    return 0;

  // Check if *shell* buffer already exists
  Buffer *existing = g_editor->GetBufferByName(L"*shell*");
  if (existing) {
    // Switch to existing shell buffer
    const auto &buffers = g_editor->GetBuffers();
    for (size_t i = 0; i < buffers.size(); ++i) {
      if (buffers[i].get() == existing) {
        g_editor->SwitchToBuffer(i);
        UpdateMenu(g_mainHwnd);
        InvalidateRect(g_mainHwnd, NULL, FALSE);
        duk_push_boolean(ctx, true);
        return 1;
      }
    }
  }

  const char *cmd = duk_get_string(ctx, 0);
  if (!cmd)
    cmd = "cmd.exe";

  std::wstring wcmd = StringToWString(cmd);
  size_t idx = g_editor->OpenShell(wcmd);
  if (idx != static_cast<size_t>(-1)) {
    UpdateMenu(g_mainHwnd);
    InvalidateRect(g_mainHwnd, NULL, FALSE);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_send_to_shell(duk_context *ctx) {
  if (!g_editor)
    return 0;

  const char *text = duk_get_string(ctx, 0);
  if (!text) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf && buf->IsShell()) {
    buf->SendToShell(text);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_run_async(duk_context *ctx) {
  const char *cmd = duk_get_string(ctx, 0);
  const char *cbName = duk_get_string(ctx, 1);
  if (!cmd || !g_editor) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  Buffer *active = g_editor->GetActiveBuffer();
  if (!active) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  std::wstring wcmd = StringToWString(cmd);
  std::string sCbName = cbName ? cbName : "";

  auto process = std::make_unique<Process>();
  Process *pRaw = process.get();

  if (process->Start(wcmd, [active, sCbName](const std::string &out) {
        if (g_scriptEngine) {
          ShellOutput *output = new ShellOutput();
          output->buffer = active;
          output->text = out;
          output->callback = sCbName;
          PostMessage(g_mainHwnd, WM_SHELL_OUTPUT, (WPARAM)output, (LPARAM)1);
        }
      })) {
    active->AddProcess(std::move(process));
    duk_push_boolean(ctx, true);
    return 1;
  }

  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_run_command(duk_context *ctx) {
  const char *cmd = duk_get_string(ctx, 0);
  if (!cmd) {
    duk_push_string(ctx, "");
    return 1;
  }

  std::wstring wcmd = StringToWString(cmd);
  std::string output;
  Process proc;
  bool success =
      proc.Start(wcmd, [&](const std::string &out) { output += out; });

  if (success) {
    // Wait for the process to finish
    while (proc.IsRunning()) {
      Sleep(10);
    }
    duk_push_string(ctx, output.c_str());
  } else {
    duk_push_string(ctx, "Error: Failed to start process");
  }
  return 1;
}

static duk_ret_t js_editor_is_shell_buffer(duk_context *ctx) {
  if (!g_editor)
    return 0;
  Buffer *buf = g_editor->GetActiveBuffer();
  duk_push_boolean(ctx, buf ? buf->IsShell() : false);
  return 1;
}

static duk_ret_t js_editor_exec_sync(duk_context *ctx) {
  const char *cmd = duk_get_string(ctx, 0);
  if (!cmd) {
    duk_push_string(ctx, "");
    return 1;
  }

  std::wstring wcmd = L"cmd.exe /c " + StringToWString(cmd);

  HANDLE hChildStd_OUT_Rd = NULL;
  HANDLE hChildStd_OUT_Wr = NULL;
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
    duk_push_string(ctx, "Error: CreatePipe failed");
    return 1;
  }
  if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
    duk_push_string(ctx, "Error: SetHandleInformation failed");
    return 1;
  }

  PROCESS_INFORMATION piProcInfo;
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
  STARTUPINFOW siStartInfo;
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
  siStartInfo.cb = sizeof(STARTUPINFOW);
  siStartInfo.hStdError = hChildStd_OUT_Wr;
  siStartInfo.hStdOutput = hChildStd_OUT_Wr;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  siStartInfo.wShowWindow = SW_HIDE;

  BOOL bSuccess = CreateProcessW(NULL, (LPWSTR)wcmd.c_str(), NULL, NULL, TRUE,
                                 CREATE_NO_WINDOW, NULL, NULL, &siStartInfo,
                                 &piProcInfo);
  
  if (!bSuccess) {
    CloseHandle(hChildStd_OUT_Wr);
    CloseHandle(hChildStd_OUT_Rd);
    duk_push_string(ctx, "Error: CreateProcess failed");
    return 1;
  }

  // Close our copy of the write handle so ReadFile doesn't block forever
  CloseHandle(hChildStd_OUT_Wr);

  DWORD dwRead;
  CHAR chBuf[4096];
  std::string output;
  bSuccess = FALSE;
  for (;;) {
    bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, 4096, &dwRead, NULL);
    if (!bSuccess || dwRead == 0)
      break;
    output.append(chBuf, dwRead);
  }

  WaitForSingleObject(piProcInfo.hProcess, INFINITE);
  CloseHandle(piProcInfo.hProcess);
  CloseHandle(piProcInfo.hThread);
  CloseHandle(hChildStd_OUT_Rd);

  duk_push_string(ctx, output.c_str());
  return 1;
}
