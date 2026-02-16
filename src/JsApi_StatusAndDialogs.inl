// =============================================================================
// JsApi_StatusAndDialogs.inl
// JS API: Console, Status Bar, Progress, Dialogs, Save/SaveAs
// Included by ScriptEngine.cpp
// =============================================================================

static duk_ret_t js_console_log(duk_context *ctx) {
  int n = duk_get_top(ctx);
  std::string out;
  for (int i = 0; i < n; i++) {
    out += duk_safe_to_string(ctx, i);
    if (i < n - 1)
      out += " ";
  }
  std::string outWithPrefix = "JS: " + out;
  DebugLog(outWithPrefix);
  OutputDebugStringA((outWithPrefix + "\n").c_str());
  std::cout << outWithPrefix << std::endl; // Ensure stdout output for tests
  std::cerr << outWithPrefix << std::endl;

  // Show in status bar
  std::wstring ws = StringToWString(out);
  if (g_statusHwnd)
    SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)ws.c_str());

  if (g_editor) {
    g_editor->LogMessage("JS: " + out);
  }

  return 0;
}

// JS-to-C++ Bridge Functions for Status/Progress
static duk_ret_t js_editor_set_status_text(duk_context *ctx) {
  const char *text = duk_get_string(ctx, 0);
  if (!text)
    text = "";
  std::wstring ws = StringToWString(text);
  SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)ws.c_str());
  duk_push_boolean(ctx, true);
  return 1;
}

static duk_ret_t js_editor_set_progress(duk_context *ctx) {
  int value = (int)duk_get_number(ctx, 0);
  SendMessage(g_progressHwnd, PBM_SETPOS, value, 0);
  duk_push_boolean(ctx, true);
  return 1;
}

// JS-to-C++ Bridge Functions for Dialogs
static duk_ret_t js_editor_open_dialog(duk_context *ctx) {
  std::wstring path = Dialogs::OpenFileDialog(g_mainHwnd);
  std::string pathA = WStringToString(path);
  duk_push_string(ctx, pathA.c_str());
  return 1;
}

static duk_ret_t js_editor_save_dialog(duk_context *ctx) {
  std::wstring path = Dialogs::SaveFileDialog(g_mainHwnd);
  std::string pathA = WStringToString(path);
  duk_push_string(ctx, pathA.c_str());
  return 1;
}

static duk_ret_t js_editor_about_dialog(duk_context *ctx) {
  Dialogs::ShowAboutDialog(g_mainHwnd);
  return 0;
}

static duk_ret_t js_editor_save_as(duk_context *ctx) {
  const char *path = duk_get_string(ctx, 0);
  if (!path) {
    duk_push_boolean(ctx, false);
    return 1;
  }
  std::wstring wpath = StringToWString(path);
  Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
  if (buf) {
    bool result = buf->SaveFile(wpath);
    duk_push_boolean(ctx, result);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_save(duk_context *ctx) {
  Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
  if (buf) {
    std::wstring path = buf->GetPath();
    if (path.empty()) {
      path = Dialogs::SaveFileDialog(g_mainHwnd);
      if (path.empty()) {
        duk_push_boolean(ctx, false);
        return 1;
      }
    }
    bool result = buf->SaveFile(path);
    duk_push_boolean(ctx, result);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}
