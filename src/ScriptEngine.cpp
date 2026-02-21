#define _CRT_SECURE_NO_WARNINGS
#include "Globals.inl"

// Redundant externs removed, they are in Globals.inl
void EnsureCaretVisible(HWND hwnd);
void UpdateScrollbars(HWND hwnd);
void UpdateMenu(HWND hwnd);

// =============================================================================
// JS-to-C++ Bridge Functions (organized into .inl modules)
// =============================================================================

#include "JsApi_EditorSettings.inl"
#include "JsApi_FileAndBuffer.inl"
#include "JsApi_Lsp.inl"
#include "JsApi_ShellAndMinibuffer.inl"
#include "JsApi_StatusAndDialogs.inl"
#include "JsApi_TextEditing.inl"

// =============================================================================
// ScriptEngine class implementation
// =============================================================================

ScriptEngine::ScriptEngine() : m_ctx(nullptr) {}

ScriptEngine::~ScriptEngine() {
  if (m_ctx) {
    duk_destroy_heap(m_ctx);
  }
}

bool ScriptEngine::Initialize() {
  m_ctx = duk_create_heap_default();
  if (!m_ctx)
    return false;

  // Create global 'Editor' object
  duk_push_object(m_ctx);
  duk_push_c_function(m_ctx, js_editor_insert, 2);
  duk_put_prop_string(m_ctx, -2, "insert");
  duk_push_c_function(m_ctx, js_editor_delete, 2);
  duk_put_prop_string(m_ctx, -2, "delete");
  duk_push_c_function(m_ctx, js_editor_get_text, 2);
  duk_put_prop_string(m_ctx, -2, "getText");
  duk_push_c_function(m_ctx, js_editor_get_length, 0);
  duk_put_prop_string(m_ctx, -2, "getLength");
  duk_push_c_function(m_ctx, js_editor_find, 5);
  duk_put_prop_string(m_ctx, -2, "find");
  duk_push_c_function(m_ctx, js_editor_set_key_binding, 2);
  duk_put_prop_string(m_ctx, -2, "setKeyBinding");
  duk_push_c_function(m_ctx, js_editor_set_capture_keyboard, 1);
  duk_put_prop_string(m_ctx, -2, "setCaptureKeyboard");
  duk_push_c_function(m_ctx, js_editor_set_key_handler, 1);
  duk_put_prop_string(m_ctx, -2, "setKeyHandler");
  duk_push_c_function(m_ctx, js_editor_get_caret_pos, 0);
  duk_put_prop_string(m_ctx, -2, "getCaretPos");
  duk_push_c_function(m_ctx, js_editor_set_caret_pos, 1);
  duk_put_prop_string(m_ctx, -2, "setCaretPos");
  duk_push_c_function(m_ctx, js_editor_get_selection_anchor, 0);
  duk_put_prop_string(m_ctx, -2, "getSelectionAnchor");
  duk_push_c_function(m_ctx, js_editor_move_caret, 2);
  duk_put_prop_string(m_ctx, -2, "moveCaret");
  duk_push_c_function(m_ctx, js_editor_move_caret_by_char, 2);
  duk_put_prop_string(m_ctx, -2, "moveCaretByChar");
  duk_push_c_function(m_ctx, js_editor_move_caret_up, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaretUp");
  duk_push_c_function(m_ctx, js_editor_move_caret_down, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaretDown");
  duk_push_c_function(m_ctx, js_editor_move_caret_home, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaretHome");
  duk_push_c_function(m_ctx, js_editor_move_caret_end, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaretEnd");
  duk_push_c_function(m_ctx, js_editor_move_caret_left, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaretLeft");
  duk_push_c_function(m_ctx, js_editor_move_caret_right, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaretRight");
  duk_push_c_function(m_ctx, js_editor_set_selection_anchor, 1);
  duk_put_prop_string(m_ctx, -2, "setSelectionAnchor");
  duk_push_c_function(m_ctx, js_editor_set_selection_mode, 1);
  duk_put_prop_string(m_ctx, -2, "setSelectionMode");
  duk_push_c_function(m_ctx, js_editor_set_font, 3);
  duk_put_prop_string(m_ctx, -2, "setFont");
  duk_push_c_function(m_ctx, js_editor_set_ligatures, 1);
  duk_put_prop_string(m_ctx, -2, "setLigatures");
  duk_push_c_function(m_ctx, js_editor_set_status_text, 1);
  duk_put_prop_string(m_ctx, -2, "setStatusText");
  duk_push_c_function(m_ctx, js_editor_set_progress, 1);
  duk_put_prop_string(m_ctx, -2, "setProgress");
  duk_push_c_function(m_ctx, js_editor_copy, 0);
  duk_put_prop_string(m_ctx, -2, "copy");
  duk_push_c_function(m_ctx, js_editor_cut, 0);
  duk_put_prop_string(m_ctx, -2, "cut");
  duk_push_c_function(m_ctx, js_editor_paste, 0);
  duk_put_prop_string(m_ctx, -2, "paste");
  duk_push_c_function(m_ctx, js_editor_can_undo, 0);
  duk_put_prop_string(m_ctx, -2, "canUndo");
  duk_push_c_function(m_ctx, js_editor_can_redo, 0);
  duk_put_prop_string(m_ctx, -2, "canRedo");
  duk_push_c_function(m_ctx, js_editor_undo, 0);
  duk_put_prop_string(m_ctx, -2, "undo");
  duk_push_c_function(m_ctx, js_editor_redo, 0);
  duk_put_prop_string(m_ctx, -2, "redo");
  duk_push_c_function(m_ctx, js_editor_show_line_numbers, 1);
  duk_put_prop_string(m_ctx, -2, "showLineNumbers");
  duk_push_c_function(m_ctx, js_editor_show_physical_line_numbers, 1);
  duk_put_prop_string(m_ctx, -2, "showPhysicalLineNumbers");
  duk_push_c_function(m_ctx, js_editor_set_theme, 1);
  duk_put_prop_string(m_ctx, -2, "setTheme");
  duk_push_c_function(m_ctx, js_editor_switch_buffer, 1);
  duk_put_prop_string(m_ctx, -2, "switchBuffer");
  duk_push_c_function(m_ctx, js_editor_close, 0);
  duk_put_prop_string(m_ctx, -2, "close");
  duk_push_c_function(m_ctx, js_editor_new_file, 1);
  duk_put_prop_string(m_ctx, -2, "newFile");
  duk_push_c_function(m_ctx, js_editor_set_scratch, 1);
  duk_put_prop_string(m_ctx, -2, "setScratch");
  duk_push_c_function(m_ctx, js_editor_toggle_fullscreen, 0);
  duk_put_prop_string(m_ctx, -2, "toggleFullscreen");
  duk_push_c_function(m_ctx, js_editor_set_highlights, 1);
  duk_put_prop_string(m_ctx, -2, "setHighlights");
  duk_push_c_function(m_ctx, js_editor_show_tabs, 1);
  duk_put_prop_string(m_ctx, -2, "showTabs");
  duk_push_c_function(m_ctx, js_editor_show_status_bar, 1);
  duk_put_prop_string(m_ctx, -2, "showStatusBar");
  duk_push_c_function(m_ctx, js_editor_show_menu_bar, 1);
  duk_put_prop_string(m_ctx, -2, "showMenuBar");
  duk_push_c_function(m_ctx, js_editor_set_opacity, 1);
  duk_put_prop_string(m_ctx, -2, "setOpacity");
  duk_push_c_function(m_ctx, js_editor_open, 1);
  duk_put_prop_string(m_ctx, -2, "open");
  duk_push_c_function(m_ctx, js_editor_get_total_lines, 0);
  duk_put_prop_string(m_ctx, -2, "getTotalLines");
  duk_push_c_function(m_ctx, js_editor_get_line_at_offset, 1);
  duk_put_prop_string(m_ctx, -2, "getLineAtOffset");
  duk_push_c_function(m_ctx, js_editor_get_line_offset, 1);
  duk_put_prop_string(m_ctx, -2, "getLineOffset");
  duk_push_c_function(m_ctx, js_editor_delete_char, 0);
  duk_put_prop_string(m_ctx, -2, "deleteChar");
  duk_push_c_function(m_ctx, js_editor_backspace, 0);
  duk_put_prop_string(m_ctx, -2, "backspace");
  duk_push_c_function(m_ctx, js_editor_open_dialog, 0);
  duk_put_prop_string(m_ctx, -2, "openDialog");
  duk_push_c_function(m_ctx, js_editor_save_dialog, 0);
  duk_put_prop_string(m_ctx, -2, "saveDialog");
  duk_push_c_function(m_ctx, js_editor_about_dialog, 0);
  duk_put_prop_string(m_ctx, -2, "aboutDialog");
  duk_push_c_function(m_ctx, js_editor_set_word_wrap, 1);
  duk_put_prop_string(m_ctx, -2, "setWordWrap");
  duk_push_c_function(m_ctx, js_editor_set_wrap_width, 1);
  duk_put_prop_string(m_ctx, -2, "setWrapWidth");
  duk_push_c_function(m_ctx, js_editor_get_buffers, 0);
  duk_put_prop_string(m_ctx, -2, "getBuffers");
  duk_push_c_function(m_ctx, js_editor_get_buffer_count, 0);
  duk_put_prop_string(m_ctx, -2, "getBufferCount");
  duk_push_c_function(m_ctx, js_editor_jump_to_line, 1);
  duk_put_prop_string(m_ctx, -2, "jumpToLine");
  duk_push_c_function(m_ctx, js_editor_show_jump_to_line, 0);
  duk_put_prop_string(m_ctx, -2, "showJumpToLine");
  duk_push_c_function(m_ctx, js_editor_save, 0);
  duk_put_prop_string(m_ctx, -2, "save");
  duk_push_c_function(m_ctx, js_editor_save_as, 1);
  duk_put_prop_string(m_ctx, -2, "saveAs");
  duk_push_c_function(m_ctx, js_editor_load_script, 1);
  duk_put_prop_string(m_ctx, -2, "loadScript");
  duk_push_c_function(m_ctx, js_editor_log_message, 1);
  duk_put_prop_string(m_ctx, -2, "logMessage");
  duk_push_c_function(m_ctx, js_editor_open_shell, 1);
  duk_put_prop_string(m_ctx, -2, "openShell");
  duk_push_c_function(m_ctx, js_editor_send_to_shell, 1);
  duk_put_prop_string(m_ctx, -2, "sendToShell");
  duk_push_c_function(m_ctx, js_editor_is_shell_buffer, 0);
  duk_put_prop_string(m_ctx, -2, "isShellBuffer");
  duk_push_c_function(m_ctx, js_editor_run_command, 1);
  duk_put_prop_string(m_ctx, -2, "runCommand");
  duk_push_c_function(m_ctx, js_editor_run_async, 2);
  duk_put_prop_string(m_ctx, -2, "runAsync");
  duk_push_c_function(m_ctx, js_editor_show_minibuffer, DUK_VARARGS);
  duk_put_prop_string(m_ctx, -2, "showMinibuffer");

  // LSP APIs
  duk_push_c_function(m_ctx, js_editor_lsp_start, 2);
  duk_put_prop_string(m_ctx, -2, "lspStart");
  duk_push_c_function(m_ctx, js_editor_lsp_stop, 0);
  duk_put_prop_string(m_ctx, -2, "lspStop");
  duk_push_c_function(m_ctx, js_editor_lsp_request, 2);
  duk_put_prop_string(m_ctx, -2, "lspRequest");
  duk_push_c_function(m_ctx, js_editor_lsp_notify, 2);
  duk_put_prop_string(m_ctx, -2, "lspNotify");
  duk_push_c_function(m_ctx, js_editor_lsp_get_response, 1);
  duk_put_prop_string(m_ctx, -2, "lspGetResponse");
  duk_push_c_function(m_ctx, js_editor_lsp_get_diagnostics, 0);
  duk_put_prop_string(m_ctx, -2, "lspGetDiagnostics");

  // Settings APIs
  duk_push_c_function(m_ctx, js_editor_get_settings, 0);
  duk_put_prop_string(m_ctx, -2, "getSettings");
  duk_push_c_function(m_ctx, js_editor_save_settings, 0);
  duk_put_prop_string(m_ctx, -2, "saveSettings");
  duk_push_c_function(m_ctx, js_editor_set_language, 1);
  duk_put_prop_string(m_ctx, -2, "setLanguage");

  duk_put_global_string(m_ctx, "Editor");

  // Create global 'console' object
  duk_push_object(m_ctx);
  duk_push_c_function(m_ctx, js_console_log, DUK_VARARGS);
  duk_put_prop_string(m_ctx, -2, "log");
  duk_put_global_string(m_ctx, "console");

  // Create aliases and global functions for Editor methods
  const char *setupScript =
      "(function() {"
      "  for (var key in Editor) {"
      "    if (typeof Editor[key] === 'function') {"
      "      this[key] = (function(k) { return function() { return "
      "Editor[k].apply(Editor, arguments); }; })(key);"
      "    }"
      "  }"
      "}).call(this);";
  if (duk_peval_string(m_ctx, setupScript) != 0) {
    std::cerr << "Setup script error: " << duk_safe_to_string(m_ctx, -1)
              << std::endl;
  }
  duk_pop(m_ctx);

  // Create additional aliases
  const char *aliases[] = {
      "var print = function() { console.log.apply(console, arguments); };",
      "var alert = function(msg) { Editor.setStatusText(String(msg)); };",
      NULL};
  for (int i = 0; aliases[i]; i++) {
    if (duk_peval_string(m_ctx, aliases[i]) != 0) {
      std::cerr << "Alias error: " << duk_safe_to_string(m_ctx, -1)
                << std::endl;
    }
    duk_pop(m_ctx);
  }

  LoadDefaultBindings();
  return true;
}

extern std::wstring g_scriptsDir;

void ScriptEngine::LoadDefaultBindings() {
  wchar_t exePath[MAX_PATH];
  if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
      exeDir = exeDir.substr(0, lastSlash + 1);
      g_scriptsDir = exeDir + L"scripts\\";

      // Load app-level init scripts
      std::wstring initScript = g_scriptsDir + L"ecodeinit.js";
      if (GetFileAttributesW(initScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DebugLog("Loading system ecodeinit.js from: " +
                 WStringToString(initScript));
        RunFile(initScript);
      }
    }
  }

  // Load user's Roaming ecodeinit.js
  wchar_t appData[MAX_PATH];
  if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
    std::wstring userScript = std::wstring(appData) + L"\\Ecode\\ecodeinit.js";
    if (GetFileAttributesW(userScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
      DebugLog("Loading user ecodeinit.js from: " +
               WStringToString(userScript));
      RunFile(userScript);
    }
  }
}

std::string ScriptEngine::Evaluate(const std::string &code) {
  if (!m_ctx)
    return "Error: no script context";

  // Push error handler
  duk_push_c_function(
      m_ctx,
      [](duk_context *ctx) -> duk_ret_t {
        duk_get_prop_string(ctx, -1, "stack");
        return 1;
      },
      1);
  int errIdx = duk_get_top_index(m_ctx);

  duk_push_string(m_ctx, code.c_str());
  if (duk_pcompile_string(m_ctx, 0, code.c_str()) != 0) {
    std::string err = duk_safe_to_string(m_ctx, -1);
    duk_pop_2(m_ctx); // pop error + handler
    return "Compile error: " + err;
  }

  if (duk_pcall(m_ctx, 0) != 0) {
    std::string err = duk_safe_to_string(m_ctx, -1);
    duk_pop_2(m_ctx);
    return "Error: " + err;
  }

  std::string result = duk_safe_to_string(m_ctx, -1);
  duk_pop_2(m_ctx);
  return result;
}

bool ScriptEngine::RunFile(const std::wstring &path) {
  if (!m_ctx)
    return false;

  std::wstring bytecodePath = path + L"b";
  bool useBytecode = false;

  if (!m_bypassCache) {
    try {
      if (fs::exists(fs::path(path))) {
        auto srcTime = fs::last_write_time(fs::path(path));
        if (fs::exists(fs::path(bytecodePath))) {
          auto binTime = fs::last_write_time(fs::path(bytecodePath));
          if (binTime >= srcTime) {
            useBytecode = true;
          }
        }
      } else if (fs::exists(fs::path(bytecodePath))) {
        // Only bytecode exists
        useBytecode = true;
      } else {
        DebugLog("ScriptEngine::RunFile: File not found: " +
                     WStringToString(path),
                 LOG_ERROR);
        return false;
      }
    } catch (...) {
      useBytecode = false;
    }
  }

  if (useBytecode) {
    DebugLog("ScriptEngine::RunFile: Loading bytecode: " +
                 WStringToString(bytecodePath),
             LOG_DEBUG);
    std::ifstream ifs(bytecodePath, std::ios::binary | std::ios::ate);
    if (ifs) {
      std::streamsize size = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      std::vector<char> buffer(size);
      if (ifs.read(buffer.data(), size)) {
        duk_push_lstring(m_ctx, buffer.data(), size);
        duk_load_function(m_ctx);
        if (duk_pcall(m_ctx, 0) != 0) {
          const char *err = duk_safe_to_string(m_ctx, -1);
          DebugLog("ScriptEngine::RunFile: Bytecode execution error: " +
                       std::string(err),
                   LOG_ERROR);
          duk_pop(m_ctx);
          return false;
        }
        duk_pop(m_ctx);
        return true;
      }
    }
    DebugLog("ScriptEngine::RunFile: Failed to load bytecode, falling back to "
             "source",
             LOG_WARN);
  }

  // Read source file
  DebugLog("ScriptEngine::RunFile: Loading source: " + WStringToString(path),
           LOG_DEBUG);
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs) {
    DebugLog("ScriptEngine::RunFile: Cannot open source file", LOG_ERROR);
    return false;
  }

  std::streamsize fileSize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);
  std::string code(fileSize, '\0');
  ifs.read(&code[0], fileSize);

  // Handle BOM
  if (code.size() >= 3 && (unsigned char)code[0] == 0xEF &&
      (unsigned char)code[1] == 0xBB && (unsigned char)code[2] == 0xBF) {
    code = code.substr(3);
  }

  // Compile
  duk_push_string(m_ctx,
                  WStringToString(path).c_str()); // filename for error messages
  if (duk_pcompile_string_filename(m_ctx, 0, code.c_str()) != 0) {
    const char *err = duk_safe_to_string(m_ctx, -1);
    DebugLog("ScriptEngine::RunFile: Compile error: " + std::string(err),
             LOG_ERROR);
    std::cerr << "Script error in " << WStringToString(path) << ": " << err
              << std::endl;
    duk_pop(m_ctx);
    return false;
  }

  // Dump bytecode
  duk_dup(m_ctx, -1);
  duk_dump_function(m_ctx);
  duk_size_t sz;
  const void *ptr = duk_get_lstring(m_ctx, -1, &sz);
  if (ptr && sz > 0) {
    std::ofstream ofs(bytecodePath, std::ios::binary);
    if (ofs) {
      ofs.write((const char *)ptr, sz);
      DebugLog("ScriptEngine::RunFile: Saved bytecode to " +
                   WStringToString(bytecodePath),
               LOG_DEBUG);
    }
  }
  duk_pop(m_ctx); // pop bytecode buffer

  // Execute
  if (duk_pcall(m_ctx, 0) != 0) {
    const char *err = duk_safe_to_string(m_ctx, -1);
    DebugLog("ScriptEngine::RunFile: Execution error: " + std::string(err),
             LOG_ERROR);
    std::cerr << "Script error in " << WStringToString(path) << ": " << err
              << std::endl;
    duk_pop(m_ctx);
    return false;
  }

  DebugLog("ScriptEngine::RunFile: Success");
  duk_pop(m_ctx); // pop return value
  return true;
}

bool ScriptEngine::HandleKeyEvent(const std::string &key, bool isChar) {
  if (m_keyHandler.empty())
    return false;

  if (m_keyHandler == "__JS_FUNCTION__") {
    duk_push_global_stash(m_ctx);
    duk_get_prop_string(m_ctx, -1, "__key_handler_func");
  } else {
    duk_get_global_string(m_ctx, m_keyHandler.c_str());
  }

  if (duk_is_function(m_ctx, -1)) {
    duk_push_string(m_ctx, key.c_str());
    duk_push_boolean(m_ctx, isChar);
    if (duk_pcall(m_ctx, 2) != 0) {
      std::cerr << "Script error in key handler: "
                << duk_safe_to_string(m_ctx, -1) << std::endl;
      SetCaptureKeyboard(false); // Failsafe
    }
    bool handled = duk_get_boolean(m_ctx, -1);
    duk_pop(m_ctx);
    if (m_keyHandler == "__JS_FUNCTION__")
      duk_pop(m_ctx); // pop stash
    return handled;
  }
  duk_pop(m_ctx);
  if (m_keyHandler == "__JS_FUNCTION__")
    duk_pop(m_ctx); // pop stash
  return false;
}

void ScriptEngine::RegisterBinding(const std::string &chord,
                                   const std::string &jsFuncName) {
  DebugLog("ScriptEngine::RegisterBinding: " + chord + " -> " + jsFuncName,
           LOG_DEBUG);
  m_keyBindings[chord] = jsFuncName;
}

bool ScriptEngine::HandleBinding(const std::string &chord) {
  DebugLog("ScriptEngine::HandleBinding: " + chord, LOG_INFO);
  auto it = m_keyBindings.find(chord);
  if (it == m_keyBindings.end()) {
    DebugLog("  No binding found for: " + chord, LOG_INFO);
    return false;
  }

  DebugLog("  Found binding -> " + it->second, LOG_INFO);
  duk_get_global_string(m_ctx, it->second.c_str());
  if (duk_is_function(m_ctx, -1)) {
    if (duk_pcall(m_ctx, 0) != 0) {
      std::cerr << "Script error in binding " << chord << ": "
                << duk_safe_to_string(m_ctx, -1) << std::endl;
      DebugLog("  Script error: " + std::string(duk_safe_to_string(m_ctx, -1)),
               LOG_ERROR);
    }
    duk_pop(m_ctx);
    return true;
  } else {
    DebugLog("  Function not found: " + it->second, LOG_WARN);
  }
  duk_pop(m_ctx);
  return false;
}

void ScriptEngine::CompileAllScripts() {
  DebugLog("ScriptEngine::CompileAllScripts: Starting", LOG_INFO);
  std::vector<std::wstring> paths;
  paths.push_back(g_scriptsDir);

  wchar_t appData[MAX_PATH];
  if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
    paths.push_back(std::wstring(appData) + L"\\Ecode\\");
  }

  for (const auto &basePath : paths) {
    if (basePath.empty() || !fs::exists(basePath))
      continue;

    try {
      for (const auto &entry : fs::recursive_directory_iterator(basePath)) {
        if (entry.path().extension() == L".js") {
          DebugLog("Compiling: " + WStringToString(entry.path().wstring()),
                   LOG_INFO);
          bool oldBypass = m_bypassCache;
          m_bypassCache = true;
          RunFile(entry.path().wstring());
          m_bypassCache = oldBypass;
        }
      }
    } catch (...) {
      DebugLog("Error during CompileAllScripts traversal of " +
                   WStringToString(basePath),
               LOG_ERROR);
    }
  }
  DebugLog("ScriptEngine::CompileAllScripts: Finished", LOG_INFO);
}

void ScriptEngine::CallGlobalFunction(const std::string &name,
                                      const std::string &arg) {
  if (!m_ctx)
    return;
  duk_push_global_object(m_ctx);
  if (duk_get_prop_string(m_ctx, -1, name.c_str())) {
    duk_push_string(m_ctx, arg.c_str());
    if (duk_pcall(m_ctx, 1) != 0) {
      DebugLog("Error calling JS function '" + name +
                   "': " + std::string(duk_safe_to_string(m_ctx, -1)),
               LOG_ERROR);
    }
  }
  duk_pop_2(m_ctx); // pop result/error and global object
}
