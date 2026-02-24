// =============================================================================
// JsApi_EditorSettings.inl
// JS API: Font, ligatures, line numbers, theme, word wrap, highlights,
//         tabs, status bar, menu bar, opacity, fullscreen
// Included by ScriptEngine.cpp
// =============================================================================

static duk_ret_t js_editor_set_font(duk_context *ctx) {
  const char *family = duk_get_string(ctx, 0);
  if (!family)
    family = "Consolas";
  float size = static_cast<float>(duk_get_number(ctx, 1));
  int weight = DWRITE_FONT_WEIGHT_NORMAL;
  if (duk_is_number(ctx, 2)) {
    weight = (int)duk_get_number(ctx, 2);
  }

  std::wstring ws = StringToWString(family);
  if (g_renderer) {
    g_renderer->SetFont(ws, size, static_cast<DWRITE_FONT_WEIGHT>(weight));
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_ligatures(duk_context *ctx) {
  bool enable = duk_get_boolean(ctx, 0);
  if (g_renderer) {
    bool old = g_renderer->SetLigatures(enable);
    duk_push_boolean(ctx, old);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_show_line_numbers(duk_context *ctx) {
  bool show = duk_get_boolean(ctx, 0);
  if (g_renderer) {
    bool old = g_renderer->SetShowLineNumbers(show);
    duk_push_boolean(ctx, old);
    return 1;
  }
  duk_push_boolean(ctx, false); // Or whatever error return
  return 1;
}

static duk_ret_t js_editor_show_physical_line_numbers(duk_context *ctx) {
  // Deprecated/Removed functionality
  duk_push_boolean(ctx, false);
  return 1;
}

static D2D1_COLOR_F ParseColor(const char *hex) {
  if (!hex || hex[0] == '\0')
    return {0, 0, 0, 1};
  if (hex[0] == '#')
    hex++;
  unsigned int r, g, b, a = 255;
  if (strlen(hex) == 6) {
    sscanf(hex, "%02x%02x%02x", &r, &g, &b);
  } else if (strlen(hex) == 8) {
    sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
  } else {
    return {0, 0, 0, 1};
  }
  return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

static duk_ret_t js_editor_set_theme(duk_context *ctx) {
  if (!duk_is_object(ctx, 0)) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  Theme theme;

  auto GetColor = [&](const char *prop, D2D1_COLOR_F &out) {
    if (duk_get_prop_string(ctx, 0, prop)) {
      if (duk_is_string(ctx, -1)) {
        out = ParseColor(duk_get_string(ctx, -1));
      }
    }
    duk_pop(ctx);
  };

  GetColor("background", theme.background);
  GetColor("foreground", theme.foreground);
  GetColor("caret", theme.caret);
  GetColor("selection", theme.selection);
  GetColor("lineNumbers", theme.lineNumbers);
  GetColor("keyword", theme.keyword);
  GetColor("string", theme.string);
  GetColor("number", theme.number);
  GetColor("comment", theme.comment);
  GetColor("function", theme.function);

  if (g_renderer) {
    g_renderer->SetTheme(theme);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_highlights(duk_context *ctx) {
  if (!duk_is_array(ctx, 0)) {
    duk_push_boolean(ctx, false);
    return 1;
  }

  std::vector<Buffer::HighlightRange> highlights;
  duk_size_t n = duk_get_length(ctx, 0);
  for (duk_size_t i = 0; i < n; i++) {
    duk_get_prop_index(ctx, 0, i);
    if (duk_is_object(ctx, -1)) {
      Buffer::HighlightRange h;
      duk_get_prop_string(ctx, -1, "start");
      h.start = (size_t)duk_get_number(ctx, -1);
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "length");
      h.length = (size_t)duk_get_number(ctx, -1);
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "type");
      h.type = (int)duk_get_number(ctx, -1);
      duk_pop(ctx);
      highlights.push_back(h);
    }
    duk_pop(ctx);
  }

  Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
  if (buf) {
    buf->SetHighlights(highlights);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_show_tabs(duk_context *ctx) {
  bool show = duk_get_boolean(ctx, 0);
  bool old = IsWindowVisible(g_tabHwnd) != 0;
  ShowWindow(g_tabHwnd, show ? SW_SHOW : SW_HIDE);
  SendMessage(g_mainHwnd, WM_SIZE, 0, 0); // Trigger resize
  duk_push_boolean(ctx, old);
  return 1;
}

static duk_ret_t js_editor_show_status_bar(duk_context *ctx) {
  bool show = duk_get_boolean(ctx, 0);
  bool old = IsWindowVisible(g_statusHwnd) != 0;
  ShowWindow(g_statusHwnd, show ? SW_SHOW : SW_HIDE);
  SendMessage(g_mainHwnd, WM_SIZE, 0, 0);
  duk_push_boolean(ctx, old);
  return 1;
}

static duk_ret_t js_editor_show_menu_bar(duk_context *ctx) {
  bool show = duk_get_boolean(ctx, 0);
  bool old = GetMenu(g_mainHwnd) != NULL;
  if (show) {
    UpdateMenu(g_mainHwnd);
  } else {
    SetMenu(g_mainHwnd, NULL);
  }
  duk_push_boolean(ctx, old);
  return 1;
}

static duk_ret_t js_editor_set_opacity(duk_context *ctx) {
  float opacity = (float)duk_get_number(ctx, 0);
  LONG exStyle = GetWindowLong(g_mainHwnd, GWL_EXSTYLE);
  SetWindowLong(g_mainHwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
  if (SetLayeredWindowAttributes(g_mainHwnd, 0, (BYTE)(opacity * 255),
                                 LWA_ALPHA)) {
    duk_push_boolean(ctx, true);
  } else {
    duk_push_boolean(ctx, false);
  }
  return 1;
}

static duk_ret_t js_editor_toggle_fullscreen(duk_context *ctx) {
  static bool isFullscreen = false;
  static RECT oldPos;
  static LONG oldStyle;

  bool prev = isFullscreen;
  if (!isFullscreen) {
    GetWindowRect(g_mainHwnd, &oldPos);
    oldStyle = GetWindowLong(g_mainHwnd, GWL_STYLE);

    SetWindowLong(g_mainHwnd, GWL_STYLE,
                  oldStyle & ~(WS_CAPTION | WS_THICKFRAME));

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(g_mainHwnd, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED);
    isFullscreen = true;
  } else {
    SetWindowLong(g_mainHwnd, GWL_STYLE, oldStyle);
    SetWindowPos(g_mainHwnd, NULL, oldPos.left, oldPos.top,
                 oldPos.right - oldPos.left, oldPos.bottom - oldPos.top,
                 SWP_FRAMECHANGED);
    isFullscreen = false;
  }
  duk_push_boolean(ctx, prev);
  return 1;
}

static duk_ret_t js_editor_set_word_wrap(duk_context *ctx) {
  bool wrap = duk_get_boolean(ctx, 0);
  if (g_renderer) {
    bool old = g_renderer->SetWordWrap(wrap);
    duk_push_boolean(ctx, old);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_wrap_width(duk_context *ctx) {
  float width = (float)duk_get_number(ctx, 0);
  if (g_renderer) {
    g_renderer->SetWrapWidth(width);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_get_app_data_path(duk_context *ctx) {
  std::wstring path = SettingsManager::Instance().GetAppDataPath();
  std::string utf8_path = WStringToString(path);
  // Replace backslashes with forward slashes for JS friendliness
  std::replace(utf8_path.begin(), utf8_path.end(), '\\', '/');
  duk_push_string(ctx, utf8_path.c_str());
  return 1;
}
