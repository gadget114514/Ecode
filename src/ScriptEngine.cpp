#define _CRT_SECURE_NO_WARNINGS
#include "../include/ScriptEngine.h"
#include "../include/Dialogs.h"
#include "../include/Editor.h"
#include "../include/EditorBufferRenderer.h"
#include <commctrl.h>
#include <iostream>

// Forward declarations
extern std::unique_ptr<Editor> g_editor;
extern HWND g_mainHwnd;
extern HWND g_statusHwnd;
extern HWND g_progressHwnd;
extern HWND g_tabHwnd;
extern std::unique_ptr<class EditorBufferRenderer> g_renderer;
extern std::unique_ptr<class ScriptEngine> g_scriptEngine;

void UpdateMenu(HWND hwnd);

static std::string WStringToString(const std::wstring &ws) {
  if (ws.empty())
    return "";
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(),
                                        NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

static std::wstring StringToWString(const std::string &s) {
  if (s.empty())
    return L"";
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0],
                      size_needed);
  return wstrTo;
}

// JS-to-C++ Bridge Functions for Status/Progress
static duk_ret_t js_editor_set_status_text(duk_context *ctx) {
  const char *text = duk_get_string(ctx, 0);
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

// JS-to-C++ Bridge Functions
static duk_ret_t js_editor_set_key_handler(duk_context *ctx) {
  const char *funcName = duk_get_string(ctx, 0);
  if (g_scriptEngine) {
    g_scriptEngine->SetKeyHandler(funcName);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_insert(duk_context *ctx) {
  size_t pos = (size_t)duk_get_number(ctx, 0);
  const char *text = duk_get_string(ctx, 1);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->Insert(pos, text);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_delete(duk_context *ctx) {
  size_t pos = (size_t)duk_get_number(ctx, 0);
  size_t len = (size_t)duk_get_number(ctx, 1);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->Delete(pos, len);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_get_text(duk_context *ctx) {
  size_t pos = (size_t)duk_get_number(ctx, 0);
  size_t len = (size_t)duk_get_number(ctx, 1);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    std::string text = buf->GetText(pos, len);
    duk_push_string(ctx, text.c_str());
    return 1;
  }
  return 0;
}

static duk_ret_t js_editor_get_length(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    duk_push_number(ctx, (double)buf->GetTotalLength());
    return 1;
  }
  return 0;
}

static duk_ret_t js_editor_get_caret_pos(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    duk_push_number(ctx, (double)buf->GetCaretPos());
    return 1;
  }
  return 0;
}

static duk_ret_t js_editor_set_caret_pos(duk_context *ctx) {
  size_t pos = (size_t)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->SetCaretPos(pos);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret(duk_context *ctx) {
  int delta = (int)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaret(delta);
    if (!duk_get_boolean(ctx, 1))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret_by_char(duk_context *ctx) {
  int delta = (int)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretByChar(delta);
    if (!duk_get_boolean(ctx, 1))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret_up(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretUp();
    if (!duk_get_boolean(ctx, 0))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret_down(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretDown();
    if (!duk_get_boolean(ctx, 0))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret_home(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretHome();
    if (!duk_get_boolean(ctx, 0))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret_end(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretEnd();
    if (!duk_get_boolean(ctx, 0))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_font(duk_context *ctx) {
  const char *family = duk_get_string(ctx, 0);
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

static duk_ret_t js_editor_set_selection_anchor(duk_context *ctx) {
  size_t pos = static_cast<size_t>(duk_get_number(ctx, 0));
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->SetSelectionAnchor(pos);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_set_selection_mode(duk_context *ctx) {
  int mode = duk_get_int(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->SetSelectionMode(static_cast<SelectionMode>(mode));
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_copy(duk_context *ctx) {
  if (g_editor) {
    g_editor->Copy(g_mainHwnd);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_cut(duk_context *ctx) {
  if (g_editor) {
    g_editor->Cut(g_mainHwnd);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_paste(duk_context *ctx) {
  if (g_editor) {
    g_editor->Paste(g_mainHwnd);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_can_undo(duk_context *ctx) {
  Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
  duk_push_boolean(ctx, buf ? buf->CanUndo() : false);
  return 1;
}

static duk_ret_t js_editor_can_redo(duk_context *ctx) {
  Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
  duk_push_boolean(ctx, buf ? buf->CanRedo() : false);
  return 1;
}

static duk_ret_t js_editor_undo(duk_context *ctx) {
  if (g_editor) {
    g_editor->Undo();
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_redo(duk_context *ctx) {
  if (g_editor) {
    g_editor->Redo();
    duk_push_boolean(ctx, true);
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

static duk_ret_t js_editor_open(duk_context *ctx) {
  const char *path = duk_get_string(ctx, 0);
  std::string s(path);
  std::wstring ws(s.begin(), s.size() == 0 ? s.end() : s.begin() + s.size());
  // Note: the above is a bit messy, let's just use StringToWString
  std::wstring wpath = StringToWString(path);
  if (g_editor) {
    size_t index = g_editor->OpenFile(wpath);
    if (index != static_cast<size_t>(-1)) {
      duk_push_boolean(ctx, true);
      return 1;
    }
  }
  duk_push_boolean(ctx, false);
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

static duk_ret_t js_editor_find(duk_context *ctx) {
  const char *query = duk_get_string(ctx, 0);
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
// We already have extern g_scriptEngine at the top

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
  if (g_scriptEngine) {
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
  duk_push_c_function(m_ctx, js_editor_set_selection_mode, 1);
  duk_put_prop_string(m_ctx, -2, "setSelectionMode");

  duk_push_c_function(m_ctx, js_editor_open_dialog, 0);
  duk_put_prop_string(m_ctx, -2, "openDialog");
  duk_push_c_function(m_ctx, js_editor_save_dialog, 0);
  duk_put_prop_string(m_ctx, -2, "saveDialog");
  duk_push_c_function(m_ctx, js_editor_about_dialog, 0);
  duk_put_prop_string(m_ctx, -2, "showAbout");

  duk_push_c_function(m_ctx, js_editor_set_status_text, 1);
  duk_put_prop_string(m_ctx, -2, "setStatusText");
  duk_push_c_function(m_ctx, js_editor_set_progress, 1);
  duk_put_prop_string(m_ctx, -2, "setProgress");

  duk_push_c_function(m_ctx, js_editor_get_caret_pos, 0);
  duk_put_prop_string(m_ctx, -2, "getCaretPos");
  duk_push_c_function(m_ctx, js_editor_set_caret_pos, 1);
  duk_put_prop_string(m_ctx, -2, "setCaretPos");
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

  duk_push_c_function(m_ctx, js_editor_set_font, 3);
  duk_put_prop_string(m_ctx, -2, "setFont");
  duk_push_c_function(m_ctx, js_editor_set_ligatures, 1);
  duk_put_prop_string(m_ctx, -2, "setLigatures");
  duk_push_c_function(m_ctx, js_editor_set_selection_anchor, 1);
  duk_put_prop_string(m_ctx, -2, "setSelectionAnchor");
  duk_push_c_function(m_ctx, js_editor_copy, 0);
  duk_put_prop_string(m_ctx, -2, "copy");
  duk_push_c_function(m_ctx, js_editor_cut, 0);
  duk_put_prop_string(m_ctx, -2, "cut");
  duk_push_c_function(m_ctx, js_editor_paste, 0);
  duk_put_prop_string(m_ctx, -2, "paste");

  duk_push_c_function(m_ctx, js_editor_undo, 0);
  duk_put_prop_string(m_ctx, -2, "undo");
  duk_push_c_function(m_ctx, js_editor_redo, 0);
  duk_put_prop_string(m_ctx, -2, "redo");
  duk_push_c_function(m_ctx, js_editor_can_undo, 0);
  duk_put_prop_string(m_ctx, -2, "canUndo");
  duk_push_c_function(m_ctx, js_editor_can_redo, 0);
  duk_put_prop_string(m_ctx, -2, "canRedo");
  duk_push_c_function(m_ctx, js_editor_show_line_numbers, 1);
  duk_put_prop_string(m_ctx, -2, "showLineNumbers");
  duk_push_c_function(m_ctx, js_editor_show_physical_line_numbers, 1);
  duk_put_prop_string(m_ctx, -2, "showPhysicalLineNumbers");

  duk_push_c_function(m_ctx, js_editor_open, 1);
  duk_put_prop_string(m_ctx, -2, "open");
  duk_push_c_function(m_ctx, js_editor_close, 0);
  duk_put_prop_string(m_ctx, -2, "close");
  duk_push_c_function(m_ctx, js_editor_switch_buffer, 1);
  duk_put_prop_string(m_ctx, -2, "switchBuffer");
  duk_push_c_function(m_ctx, js_editor_get_buffers, 0);
  duk_put_prop_string(m_ctx, -2, "getBuffers");

  duk_push_c_function(m_ctx, js_editor_new_file, 0);
  duk_put_prop_string(m_ctx, -2, "newFile");
  duk_push_c_function(m_ctx, js_editor_set_theme, 1);
  duk_put_prop_string(m_ctx, -2, "setTheme");
  duk_push_c_function(m_ctx, js_editor_toggle_fullscreen, 0);
  duk_put_prop_string(m_ctx, -2, "toggleFullScreen");
  duk_push_c_function(m_ctx, js_editor_show_tabs, 1);
  duk_put_prop_string(m_ctx, -2, "showTabs");
  duk_push_c_function(m_ctx, js_editor_show_status_bar, 1);
  duk_put_prop_string(m_ctx, -2, "showStatusBar");
  duk_push_c_function(m_ctx, js_editor_show_menu_bar, 1);
  duk_put_prop_string(m_ctx, -2, "showMenuBar");
  duk_push_c_function(m_ctx, js_editor_set_opacity, 1);
  duk_put_prop_string(m_ctx, -2, "setOpacity");

  duk_push_c_function(m_ctx, js_editor_get_total_lines, 0);
  duk_put_prop_string(m_ctx, -2, "getTotalLines");
  duk_push_c_function(m_ctx, js_editor_get_line_at_offset, 1);
  duk_put_prop_string(m_ctx, -2, "getLineAtOffset");
  duk_push_c_function(m_ctx, js_editor_get_line_offset, 1);
  duk_put_prop_string(m_ctx, -2, "getLineOffset");
  duk_push_c_function(m_ctx, js_editor_jump_to_line, 1);
  duk_put_prop_string(m_ctx, -2, "jumpToLine");
  duk_push_c_function(m_ctx, js_editor_show_jump_to_line, 0);
  duk_put_prop_string(m_ctx, -2, "showJumpToLine");
  duk_push_c_function(m_ctx, js_editor_set_highlights, 1);
  duk_put_prop_string(m_ctx, -2, "setHighlights");

  duk_put_global_string(m_ctx, "Editor");

  LoadDefaultBindings();

  return true;
}

void ScriptEngine::LoadDefaultBindings() {
  const char *defaultBindings = R"(
    // Emacs-style Key Bindings
    Editor.setKeyBinding("Ctrl+N", "emacs_next_line");
    Editor.setKeyBinding("Ctrl+P", "emacs_prev_line");
    Editor.setKeyBinding("Ctrl+F", "emacs_forward_char");
    Editor.setKeyBinding("Ctrl+B", "emacs_backward_char");
    Editor.setKeyBinding("Ctrl+A", "emacs_line_start");
    Editor.setKeyBinding("Ctrl+E", "emacs_line_end");
    Editor.setKeyBinding("Ctrl+D", "emacs_delete_char");
    Editor.setKeyBinding("Ctrl+H", "emacs_backspace");
    Editor.setKeyBinding("Ctrl+K", "emacs_kill_line");
    Editor.setKeyBinding("Ctrl+Y", "emacs_yank");
    Editor.setKeyBinding("Ctrl+Space", "emacs_set_mark");
    Editor.setKeyBinding("Ctrl+V", "emacs_scroll_down");
    Editor.setKeyBinding("Alt+V", "emacs_scroll_up");
    Editor.setKeyBinding("Ctrl+T", "emacs_transpose_chars");
    Editor.setKeyBinding("Ctrl+S", "emacs_isearch_forward");
    Editor.setKeyBinding("Ctrl+R", "emacs_isearch_backward");
    Editor.setKeyBinding("Ctrl+G", "emacs_jump_to_line");
    Editor.setKeyBinding("F12", "tag_jump");

    var isearchQuery = "";
    var isearchStartPos = 0;
    var isearchForward = true;

    function emacs_isearch_forward() {
        isearchQuery = "";
        isearchStartPos = Editor.getCaretPos();
        isearchForward = true;
        Editor.setCaptureKeyboard(true);
        Editor.setKeyHandler("emacs_isearch_handler");
        Editor.setStatusText("I-search: ");
    }

    function emacs_isearch_backward() {
        isearchQuery = "";
        isearchStartPos = Editor.getCaretPos();
        isearchForward = false;
        Editor.setCaptureKeyboard(true);
        Editor.setKeyHandler("emacs_isearch_handler");
        Editor.setStatusText("I-search backward: ");
    }

    function emacs_isearch_handler(key, isChar) {
        if (!isChar) {
            if (key == "Enter") {
                Editor.setCaptureKeyboard(false);
                Editor.setStatusText("Ready");
                return true;
            }
            if (key == "Esc") {
                Editor.setCaretPos(isearchStartPos);
                Editor.setSelectionAnchor(isearchStartPos);
                Editor.setCaptureKeyboard(false);
                Editor.setStatusText("Canceled");
                return true;
            }
            if (key == "Backspace") {
                if (isearchQuery.length > 0) {
                    isearchQuery = isearchQuery.substring(0, isearchQuery.length - 1);
                    emacs_isearch_update();
                }
                return true;
            }
            if (key == "Ctrl+S") {
                isearchForward = true;
                emacs_isearch_next();
                return true;
            }
            if (key == "Ctrl+R") {
                isearchForward = false;
                emacs_isearch_next();
                return true;
            }
            return false;
        }

        isearchQuery += key;
        emacs_isearch_update();
        return true;
    }

    function emacs_isearch_update() {
        var prefix = isearchForward ? "I-search: " : "I-search backward: ";
        Editor.setStatusText(prefix + isearchQuery);
        
        var pos = Editor.find(isearchQuery, isearchStartPos, isearchForward, false, false);
        if (pos != -1) {
            Editor.setSelectionAnchor(pos);
            Editor.setCaretPos(pos + isearchQuery.length);
        }
    }

    function emacs_isearch_next() {
        var currentPos = Editor.getCaretPos();
        var searchStart = isearchForward ? currentPos : (currentPos - isearchQuery.length - 1);
        if (searchStart < 0) searchStart = 0;
        
        var pos = Editor.find(isearchQuery, searchStart, isearchForward, false, false);
        if (pos != -1) {
            Editor.setSelectionAnchor(pos);
            Editor.setCaretPos(pos + isearchQuery.length);
        } else {
            Editor.setStatusText("Failing " + (isearchForward ? "I-search: " : "I-search backward: ") + isearchQuery);
        }
    }

    function emacs_next_line() { Editor.moveCaretDown(); }
    function emacs_prev_line() { Editor.moveCaretUp(); }
    function emacs_forward_char() { Editor.moveCaretByChar(1); }
    function emacs_backward_char() { Editor.moveCaretByChar(-1); }
    function emacs_line_start() { Editor.moveCaretHome(); }
    function emacs_line_end() { Editor.moveCaretEnd(); }
    function emacs_delete_char() { Editor.delete(Editor.getCaretPos(), 1); }
    function emacs_backspace() { 
        var pos = Editor.getCaretPos();
        if (pos > 0) {
            Editor.setCaretPos(pos - 1);
            Editor.delete(pos - 1, 1); 
        }
    }
    function emacs_kill_line() {
        var pos = Editor.getCaretPos();
        var lineIdx = Editor.getLineAtOffset(pos);
        var lineEnd = Editor.getLineOffset(lineIdx + 1);
        if (lineEnd == 0) lineEnd = Editor.getLength();
        
        var len = lineEnd - pos;
        if (len > 0) {
            var text = Editor.getText(pos, len);
            var newlinePos = text.indexOf('\n');
            if (newlinePos == 0) {
                Editor.delete(pos, 1);
            } else if (newlinePos > 0) {
                Editor.setSelectionAnchor(pos);
                Editor.setCaretPos(pos + newlinePos);
                Editor.cut();
            } else {
                Editor.setSelectionAnchor(pos);
                Editor.setCaretPos(pos + len);
                Editor.cut();
            }
        }
    }
    function emacs_yank() { Editor.paste(); }
    function emacs_set_mark() { Editor.setSelectionAnchor(Editor.getCaretPos()); }

    function emacs_scroll_up() { 
        for(var i=0; i<20; i++) Editor.moveCaretUp();
    }
    function emacs_scroll_down() {
        for(var i=0; i<20; i++) Editor.moveCaretDown();
    }
    function emacs_transpose_chars() {
        var pos = Editor.getCaretPos();
        if (pos > 0 && pos < Editor.getLength()) {
            var c1 = Editor.getText(pos - 1, 1);
            var c2 = Editor.getText(pos, 1);
            Editor.delete(pos - 1, 2);
            Editor.insert(pos - 1, c2 + c1);
            Editor.setCaretPos(pos + 1);
        }
    }

    function tag_jump() {
        var pos = Editor.getCaretPos();
        var lineIdx = Editor.getLineAtOffset(pos);
        var start = Editor.getLineOffset(lineIdx);
        var end = Editor.getLineOffset(lineIdx + 1);
        if (end == 0) end = Editor.getLength(); 
        
        var text = Editor.getText(start, end - start);
        var match = text.match(/([a-zA-Z0-9_\-\.\/\\]+)[:\(](\d+)[:\)]?/);
        if (match) {
            var file = match[1];
            var line = parseInt(match[2]);
            Editor.open(file);
            var targetOffset = Editor.getLineOffset(line - 1);
            Editor.setCaretPos(targetOffset);
            Editor.setSelectionAnchor(targetOffset);
        }
    }

    function emacs_jump_to_line() { Editor.showJumpToLine(); }
  )";
  duk_peval_string(m_ctx, defaultBindings);
  duk_pop(m_ctx);
}

std::string ScriptEngine::Evaluate(const std::string &code) {
  if (!m_ctx)
    return "Engine not initialized";

  if (duk_peval_string(m_ctx, code.c_str()) != 0) {
    std::string err = duk_safe_to_string(m_ctx, -1);
    duk_pop(m_ctx);
    return "Error: " + err;
  }

  std::string result = duk_safe_to_string(m_ctx, -1);
  duk_pop(m_ctx);
  return result;
}

#include <fstream>
#include <sstream>

void DebugLog(const std::string &msg); // Forward declaration: Assume it's
                                       // linked if not static.
// Actually, DebugLog in main.cpp is not static?
// In main.cpp: void DebugLog(...) { ... }
// So I can just declare it.

bool ScriptEngine::RunFile(const std::wstring &path) {
  DebugLog("ScriptEngine::RunFile: " + WStringToString(path));
  std::ifstream f(path);
  if (!f.is_open()) {
    DebugLog("ScriptEngine::RunFile: Failed to open file");
    return false;
  }
  DebugLog("ScriptEngine::RunFile: File opened");
  std::stringstream ss;
  ss << f.rdbuf();
  std::string code = ss.str();
  DebugLog("ScriptEngine::RunFile: File read, size: " +
           std::to_string(code.size()));
  if (duk_peval_string(m_ctx, code.c_str()) != 0) {
    std::cerr << "Error running script file " << duk_safe_to_string(m_ctx, -1)
              << std::endl;
    DebugLog("ScriptEngine::RunFile: Error parsing/running");
    duk_pop(m_ctx);
    return false;
  }
  DebugLog("ScriptEngine::RunFile: Success");
  duk_pop(m_ctx);
  return true;
}

bool ScriptEngine::HandleKeyEvent(const std::string &key, bool isChar) {
  if (m_keyHandler.empty())
    return false;

  duk_get_global_string(m_ctx, m_keyHandler.c_str());
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
    return handled;
  }
  duk_pop(m_ctx);
  return false;
}

void ScriptEngine::RegisterBinding(const std::string &chord,
                                   const std::string &jsFuncName) {
  m_keyBindings[chord] = jsFuncName;
}

bool ScriptEngine::HandleBinding(const std::string &chord) {
  auto it = m_keyBindings.find(chord);
  if (it != m_keyBindings.end()) {
    duk_get_global_string(m_ctx, it->second.c_str());
    if (duk_is_function(m_ctx, -1)) {
      if (duk_pcall(m_ctx, 0) != 0) {
        std::cerr << "Script error in " << it->second << ": "
                  << duk_safe_to_string(m_ctx, -1) << std::endl;
      }
      duk_pop(m_ctx);
      return true;
    }
    duk_pop(m_ctx);
  }
  return false;
}
