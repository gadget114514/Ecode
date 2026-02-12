#include "../include/ScriptEngine.h"
#include "../include/Dialogs.h"
#include "../include/Editor.h"
#include "../include/Renderer.h"
#include <commctrl.h>
#include <iostream>

// Forward declarations
extern std::unique_ptr<Editor> g_editor;
extern HWND g_mainHwnd;
extern HWND g_statusHwnd;
extern HWND g_progressHwnd;
extern std::unique_ptr<class Renderer> g_renderer;

// JS-to-C++ Bridge Functions for Status/Progress
static duk_ret_t js_editor_set_status_text(duk_context *ctx) {
  const char *text = duk_get_string(ctx, 0);
  std::string s(text);
  std::wstring ws(s.begin(), s.end());
  SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)ws.c_str());
  return 0;
}

static duk_ret_t js_editor_set_progress(duk_context *ctx) {
  int value = (int)duk_get_number(ctx, 0);
  SendMessage(g_progressHwnd, PBM_SETPOS, value, 0);
  return 0;
}

// JS-to-C++ Bridge Functions for Dialogs
static duk_ret_t js_editor_open_dialog(duk_context *ctx) {
  std::wstring path = Dialogs::OpenFileDialog(g_mainHwnd);
  std::string pathA(path.begin(), path.end());
  duk_push_string(ctx, pathA.c_str());
  return 1;
}

static duk_ret_t js_editor_save_dialog(duk_context *ctx) {
  std::wstring path = Dialogs::SaveFileDialog(g_mainHwnd);
  std::string pathA(path.begin(), path.end());
  duk_push_string(ctx, pathA.c_str());
  return 1;
}

static duk_ret_t js_editor_about_dialog(duk_context *ctx) {
  Dialogs::ShowAboutDialog(g_mainHwnd);
  return 0;
}

// JS-to-C++ Bridge Functions
static duk_ret_t js_editor_insert(duk_context *ctx) {
  size_t pos = (size_t)duk_get_number(ctx, 0);
  const char *text = duk_get_string(ctx, 1);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->Insert(pos, text);
  }
  return 0;
}

static duk_ret_t js_editor_delete(duk_context *ctx) {
  size_t pos = (size_t)duk_get_number(ctx, 0);
  size_t len = (size_t)duk_get_number(ctx, 1);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->Delete(pos, len);
  }
  return 0;
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
  }
  return 0;
}

static duk_ret_t js_editor_move_caret(duk_context *ctx) {
  int delta = (int)duk_get_number(ctx, 0);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaret(delta);
  }
  return 0;
}

static duk_ret_t js_editor_move_caret_up(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretUp();
  }
  return 0;
}

static duk_ret_t js_editor_move_caret_down(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretDown();
  }
  return 0;
}

static duk_ret_t js_editor_move_caret_home(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretHome();
  }
  return 0;
}

static duk_ret_t js_editor_move_caret_end(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretEnd();
  }
  return 0;
}

static duk_ret_t js_editor_set_font(duk_context *ctx) {
  const char *family = duk_get_string(ctx, 0);
  float size = static_cast<float>(duk_get_number(ctx, 1));
  std::string s(family);
  std::wstring ws(s.begin(), s.end());
  if (g_renderer) {
    g_renderer->SetFont(ws, size);
  }
  return 0;
}

static duk_ret_t js_editor_set_selection_anchor(duk_context *ctx) {
  size_t pos = static_cast<size_t>(duk_get_number(ctx, 0));
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->SetSelectionAnchor(pos);
  }
  return 0;
}

static duk_ret_t js_editor_copy(duk_context *ctx) {
  if (g_editor)
    g_editor->Copy(g_mainHwnd);
  return 0;
}

static duk_ret_t js_editor_cut(duk_context *ctx) {
  if (g_editor)
    g_editor->Cut(g_mainHwnd);
  return 0;
}

static duk_ret_t js_editor_paste(duk_context *ctx) {
  if (g_editor)
    g_editor->Paste(g_mainHwnd);
  return 0;
}

static duk_ret_t js_editor_undo(duk_context *ctx) {
  if (g_editor)
    g_editor->Undo();
  return 0;
}

static duk_ret_t js_editor_redo(duk_context *ctx) {
  if (g_editor)
    g_editor->Redo();
  return 0;
}

static duk_ret_t js_editor_show_line_numbers(duk_context *ctx) {
  bool show = duk_get_boolean(ctx, 0);
  if (g_renderer) {
    g_renderer->SetShowLineNumbers(show);
  }
  return 0;
}

static duk_ret_t js_editor_show_physical_line_numbers(duk_context *ctx) {
  bool show = duk_get_boolean(ctx, 0);
  if (g_renderer) {
    g_renderer->SetShowPhysicalLineNumbers(show);
  }
  return 0;
}

static duk_ret_t js_editor_open(duk_context *ctx) {
  const char *path = duk_get_string(ctx, 0);
  std::string s(path);
  std::wstring ws(s.begin(), s.end());
  if (g_editor) {
    g_editor->OpenFile(ws);
  }
  return 0;
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
    g_renderer->SetWordWrap(wrap);
  }
  return 0;
}

static duk_ret_t js_editor_set_wrap_width(duk_context *ctx) {
  float width = (float)duk_get_number(ctx, 0);
  if (g_renderer) {
    g_renderer->SetWrapWidth(width);
  }
  return 0;
}

// Bridging the ScriptEngine itself for SetKeyBinding
// We need a way to get the ScriptEngine instance, but for now we'll use a
// global pointer
extern std::unique_ptr<class ScriptEngine> g_scriptEngine;

static duk_ret_t js_editor_set_key_binding(duk_context *ctx) {
  const char *chord = duk_get_string(ctx, 0);
  const char *funcName = duk_get_string(ctx, 1);
  if (g_scriptEngine) {
    g_scriptEngine->RegisterBinding(chord, funcName);
  }
  return 0;
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
  duk_push_c_function(m_ctx, js_editor_set_key_binding, 2);
  duk_put_prop_string(m_ctx, -2, "setKeyBinding");

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
  duk_push_c_function(m_ctx, js_editor_move_caret, 1);
  duk_put_prop_string(m_ctx, -2, "moveCaret");
  duk_push_c_function(m_ctx, js_editor_move_caret_up, 0);
  duk_put_prop_string(m_ctx, -2, "moveCaretUp");
  duk_push_c_function(m_ctx, js_editor_move_caret_down, 0);
  duk_put_prop_string(m_ctx, -2, "moveCaretDown");
  duk_push_c_function(m_ctx, js_editor_move_caret_home, 0);
  duk_put_prop_string(m_ctx, -2, "moveCaretHome");
  duk_push_c_function(m_ctx, js_editor_move_caret_end, 0);
  duk_put_prop_string(m_ctx, -2, "moveCaretEnd");

  duk_push_c_function(m_ctx, js_editor_set_font, 2);
  duk_put_prop_string(m_ctx, -2, "setFont");
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
  duk_push_c_function(m_ctx, js_editor_show_line_numbers, 1);
  duk_put_prop_string(m_ctx, -2, "showLineNumbers");
  duk_push_c_function(m_ctx, js_editor_show_physical_line_numbers, 1);
  duk_put_prop_string(m_ctx, -2, "showPhysicalLineNumbers");

  duk_push_c_function(m_ctx, js_editor_open, 1);
  duk_put_prop_string(m_ctx, -2, "open");
  duk_push_c_function(m_ctx, js_editor_get_total_lines, 0);
  duk_put_prop_string(m_ctx, -2, "getTotalLines");
  duk_push_c_function(m_ctx, js_editor_get_line_at_offset, 1);
  duk_put_prop_string(m_ctx, -2, "getLineAtOffset");
  duk_push_c_function(m_ctx, js_editor_get_line_offset, 1);
  duk_put_prop_string(m_ctx, -2, "getLineOffset");

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
    Editor.setKeyBinding("Ctrl+T", "tag_jump");

    function emacs_next_line() { Editor.moveCaretDown(); }
    function emacs_prev_line() { Editor.moveCaretUp(); }
    function emacs_forward_char() { Editor.moveCaret(1); }
    function emacs_backward_char() { Editor.moveCaret(-1); }
    function emacs_line_start() { Editor.moveCaretHome(); }
    function emacs_line_end() { Editor.moveCaretEnd(); }
    function emacs_delete_char() { Editor.delete(1); }
    function emacs_backspace() { 
        var pos = Editor.getCaretPos();
        if (pos > 0) {
            Editor.setCaretPos(pos - 1);
            Editor.delete(1); 
        }
    }
    function emacs_kill_line() {
        Editor.cut(); 
    }
    function emacs_yank() { Editor.paste(); }
    function emacs_set_mark() { Editor.setSelectionAnchor(Editor.getCaretPos()); }

    function tag_jump() {
        var pos = Editor.getCaretPos();
        var lineIdx = Editor.getLineAtOffset(pos);
        var start = Editor.getLineOffset(lineIdx);
        var end = Editor.getLineOffset(lineIdx + 1);
        if (end == 0) end = Editor.getLength(); 
        
        var text = Editor.getText(start, end - start);
        
        // Match patterns like "filename.cpp:123" or "filename.cpp(123)"
        var match = text.match(/([a-zA-Z0-9_\-\.\/\\]+)[:\(](\d+)[:\)]?/);
  if (match) {
    var file = match[1];
    var line = parseInt(match[2]);
    Editor.open(file);

    // Move to line
    var targetOffset = Editor.getLineOffset(line - 1);
    Editor.setCaretPos(targetOffset);
    Editor.setSelectionAnchor(targetOffset);
  }
}
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

  bool ScriptEngine::RunFile(const std::wstring &path) {
    std::ifstream f(path);
    if (!f.is_open())
      return false;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string code = ss.str();
    if (duk_peval_string(m_ctx, code.c_str()) != 0) {
      std::cerr << "Error running script file " << duk_safe_to_string(m_ctx, -1)
                << std::endl;
      duk_pop(m_ctx);
      return false;
    }
    duk_pop(m_ctx);
    return true;
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
