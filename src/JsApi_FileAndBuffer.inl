// =============================================================================
// JsApi_FileAndBuffer.inl
// JS API: File open/close/new, buffer switching, find, getBuffers,
//         line info, key bindings, jump-to-line, script loading, logging
// Included by ScriptEngine.cpp
// =============================================================================

static duk_ret_t js_editor_switch_buffer(duk_context *ctx) {
  size_t index = (size_t)duk_get_number(ctx, 0);
  if (g_editor) {
    g_editor->SwitchToBuffer(index);
    UpdateMenu(g_mainHwnd);
    InvalidateRect(g_mainHwnd, NULL, FALSE);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_close(duk_context *ctx) {
  if (g_editor) {
    g_editor->CloseBuffer(g_editor->GetActiveBufferIndex());
    UpdateMenu(g_mainHwnd);
    InvalidateRect(g_mainHwnd, NULL, FALSE);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_new_file(duk_context *ctx) {
  if (g_editor) {
    g_editor->NewFile();
    UpdateMenu(g_mainHwnd);
    InvalidateRect(g_mainHwnd, NULL, FALSE);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_buffer_name(duk_context *ctx) {
  const char *name = duk_get_string(ctx, 0);
  if (name && g_editor) {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      buf->SetPath(StringToWString(name));
      UpdateMenu(g_mainHwnd);
      InvalidateRect(g_mainHwnd, NULL, FALSE);
      duk_push_boolean(ctx, true);
      return 1;
    }
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_get_buffer_name(duk_context *ctx) {
  if (g_editor) {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      std::string u8Name = StringHelpers::Utf16ToUtf8(buf->GetPath());
      duk_push_string(ctx, u8Name.c_str());
      return 1;
    }
  }
  duk_push_undefined(ctx);
  return 1;
}

static duk_ret_t js_editor_open(duk_context *ctx) {
  const char *path = duk_get_string(ctx, 0);
  if (!path) {
    duk_push_boolean(ctx, false);
    return 1;
  }
  std::wstring wpath = StringToWString(path);
  if (g_editor) {
    Buffer *existing = g_editor->GetBufferByName(wpath);
    if (existing) {
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

    size_t index = g_editor->OpenFile(wpath);
    if (index != static_cast<size_t>(-1)) {
      UpdateMenu(g_mainHwnd);
      InvalidateRect(g_mainHwnd, NULL, FALSE);
      duk_push_boolean(ctx, true);
      return 1;
    }
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_find(duk_context *ctx) {
  const char *query = duk_get_string(ctx, 0);
  if (!query)
    query = "";
  size_t startPos = (size_t)duk_get_number(ctx, 1);
  bool forward = duk_get_boolean(ctx, 2);
  bool useRegex = duk_get_boolean(ctx, 3);
  bool matchCase = duk_get_boolean(ctx, 4);

  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    size_t pos = buf->Find(query, startPos, forward, useRegex, matchCase);
    if (pos != std::string::npos) {
      duk_push_number(ctx, (double)pos);
      return 1;
    }
  }
  duk_push_number(ctx, -1);
  return 1;
}

static duk_ret_t js_editor_get_buffers(duk_context *ctx) {
  if (!g_editor)
    return 0;
  const auto &buffers = g_editor->GetBuffers();
  duk_push_array(ctx);
  for (size_t i = 0; i < buffers.size(); ++i) {
    duk_push_object(ctx);
    duk_push_number(ctx, (double)i);
    duk_put_prop_string(ctx, -2, "index");

    std::string path = WStringToString(buffers[i]->GetPath());
    duk_push_string(ctx, path.c_str());
    duk_put_prop_string(ctx, -2, "path");

    duk_push_boolean(ctx, buffers[i]->IsDirty());
    duk_put_prop_string(ctx, -2, "isDirty");

    duk_push_boolean(ctx, buffers[i]->IsScratch());
    duk_put_prop_string(ctx, -2, "isScratch");

    duk_put_prop_index(ctx, -2, (duk_uarridx_t)i);
  }
  return 1;
}

static duk_ret_t js_editor_get_total_lines(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    duk_push_number(ctx, (double)buf->GetTotalLines());
    return 1;
  }
  return 0;
}

static duk_ret_t js_editor_get_line_at_offset(duk_context *ctx) {
  size_t offset = (size_t)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    duk_push_number(ctx, (double)buf->GetLineAtOffset(offset));
    return 1;
  }
  return 0;
}

static duk_ret_t js_editor_get_line_offset(duk_context *ctx) {
  size_t lineIndex = (size_t)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    duk_push_number(ctx, (double)buf->GetLineOffset(lineIndex));
    return 1;
  }
  return 0;
}

static duk_ret_t js_editor_set_capture_keyboard(duk_context *ctx) {
  bool capture = duk_get_boolean(ctx, 0);
  if (g_scriptEngine) {
    g_scriptEngine->SetCaptureKeyboard(capture);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_key_binding(duk_context *ctx) {
  const char *chord = duk_get_string(ctx, 0);
  const char *funcName = duk_get_string(ctx, 1);
  if (chord && funcName) {
    DebugLog(std::string("js_editor_set_key_binding: ") + chord + " -> " + funcName, LOG_INFO);
  } else {
    DebugLog("js_editor_set_key_binding: missing chord or funcName", LOG_WARN);
  }

  if (chord && funcName && g_editor) {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      buf->SetKeyBinding(chord, funcName);
      std::string pathInfo = buf->GetPath().empty() ? "Untitled" : StringHelpers::Utf16ToUtf8(buf->GetPath());
      DebugLog("  Applied to buffer: " + pathInfo, LOG_INFO);
      duk_push_boolean(ctx, true);
      return 1;
    } else {
      DebugLog("  Failed: No active buffer!", LOG_WARN);
    }
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_global_key_binding(duk_context *ctx) {
  const char *chord = duk_get_string(ctx, 0);
  const char *funcName = duk_get_string(ctx, 1);
  if (chord && funcName && g_scriptEngine) {
    g_scriptEngine->RegisterBinding(chord, funcName);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_jump_to_line(duk_context *ctx) {
  size_t lineNum = (size_t)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf && lineNum > 0 && lineNum <= buf->GetTotalLines()) {
    size_t offset = buf->GetLineOffset(lineNum - 1);
    buf->SetCaretPos(offset);
    buf->SetSelectionAnchor(offset);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_show_jump_to_line(duk_context *ctx) {
  if (g_mainHwnd) {
    Dialogs::ShowJumpToLineDialog(g_mainHwnd);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_log_message(duk_context *ctx) {
  const char *msg = duk_get_string(ctx, 0);
  if (msg && g_editor) {
    g_editor->LogMessage(msg);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_load_script(duk_context *ctx) {
  const char *path = duk_get_string(ctx, 0);
  if (!path) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  extern std::wstring g_scriptsDir;
  std::wstring wpath;
  std::string spath(path);
  if (spath.compare(0, 8, "scripts/") == 0 ||
      spath.compare(0, 8, "scripts\\") == 0) {
    wpath = g_scriptsDir + StringToWString(spath.substr(8));
  } else {
    wpath = StringToWString(spath);
  }

  DebugLog("Editor.loadScript: " + spath + " -> " + WStringToString(wpath),
           LOG_INFO);
  if (g_scriptEngine) {
    bool success = g_scriptEngine->RunFile(wpath);
    DebugLog("  loadScript success: " + std::to_string(success), LOG_INFO);
    duk_push_boolean(ctx, success);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_write_file(duk_context *ctx) {
  const char *path = duk_get_string(ctx, 0);
  const char *content = duk_get_string(ctx, 1);
  if (path && content) {
    std::wstring wpath = StringToWString(path);
    std::ofstream out(wpath, std::ios::binary);
    if (out.is_open()) {
      out.write(content, strlen(content));
      out.close();
      duk_push_boolean(ctx, true);
      return 1;
    }
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_get_ai_vendor(duk_context *ctx) {
  std::wstring vendor = SettingsManager::Instance().GetAIVendor();
  if (vendor.empty()) vendor = L"Gemini"; // Default
  duk_push_string(ctx, WStringToString(vendor).c_str());
  return 1;
}

static duk_ret_t js_editor_get_ai_model(duk_context *ctx) {
  std::wstring model = SettingsManager::Instance().GetAIModel();
  if (model.empty()) model = L"gemini-1.5-pro"; // Default
  duk_push_string(ctx, WStringToString(model).c_str());
  return 1;
}

static duk_ret_t js_editor_get_ai_api_key(duk_context *ctx) {
  const char *vendor = duk_get_string(ctx, 0);
  if (vendor) {
    std::wstring apiKey = SettingsManager::Instance().GetAIApiKey(StringToWString(vendor));
    duk_push_string(ctx, WStringToString(apiKey).c_str());
    return 1;
  }
  duk_push_string(ctx, "");
  return 1;
}
