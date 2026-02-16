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

static duk_ret_t js_editor_is_shell_buffer(duk_context *ctx) {
  if (!g_editor)
    return 0;
  Buffer *buf = g_editor->GetActiveBuffer();
  duk_push_boolean(ctx, buf ? buf->IsShell() : false);
  return 1;
}
