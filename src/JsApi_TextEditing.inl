// JS API: Text insert/delete, caret movement, selection, clipboard, undo/redo
// Included by ScriptEngine.cpp
// =============================================================================

void EnsureCaretVisible(HWND hwnd);
void UpdateScrollbars(HWND hwnd);
extern HWND g_mainHwnd;

// JS-to-C++ Bridge Functions
static duk_ret_t js_editor_set_key_handler(duk_context *ctx) {
  if (duk_is_function(ctx, 0)) {
    // Store the function in the global stash
    duk_push_global_stash(ctx);
    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "__key_handler_func");
    duk_pop(ctx);

    if (g_scriptEngine) {
      g_scriptEngine->SetKeyHandler("__JS_FUNCTION__");
    }
    duk_push_boolean(ctx, true);
    return 1;
  }

  const char *funcName = duk_get_string(ctx, 0);
  if (funcName && g_scriptEngine) {
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
  if (!text)
    text = "";
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

static duk_ret_t js_editor_move_caret_left(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretByChar(-1);
    if (!duk_get_boolean(ctx, 0))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_move_caret_right(duk_context *ctx) {
  DebugLog("js_editor_move_caret_right called", LOG_DEBUG);
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    buf->MoveCaretByChar(1);
    if (!duk_get_boolean(ctx, 0))
      buf->SetSelectionAnchor(buf->GetCaretPos());
    EnsureCaretVisible(g_mainHwnd);
    InvalidateRect(g_mainHwnd, NULL, FALSE);
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_delete_char(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    if (buf->HasSelection()) {
      buf->DeleteSelection();
    } else {
      size_t pos = buf->GetCaretPos();
      size_t total = buf->GetTotalLength();
      if (pos < total) {
        size_t nextPos = pos;
        std::string text = buf->GetText(pos, 4);
        if (!text.empty()) {
          unsigned char c = (unsigned char)text[0];
          if (c < 0x80)
            nextPos = pos + 1;
          else if ((c & 0xE0) == 0xC0)
            nextPos = pos + 2;
          else if ((c & 0xF0) == 0xE0)
            nextPos = pos + 3;
          else if ((c & 0xF8) == 0xF0)
            nextPos = pos + 4;
          else
            nextPos = pos + 1;
        }
        buf->Delete(pos, (std::min)(nextPos - pos, total - pos));
      }
    }
    duk_push_boolean(ctx, true);
    return 1;
  }
  duk_push_boolean(ctx, false);
  return 1;
}

static duk_ret_t js_editor_backspace(duk_context *ctx) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (buf) {
    if (buf->HasSelection()) {
      buf->DeleteSelection();
    } else {
      size_t pos = buf->GetCaretPos();
      if (pos > 0) {
        buf->MoveCaretByChar(-1);
        size_t newPos = buf->GetCaretPos();
        buf->Delete(newPos, pos - newPos);
      }
    }
    duk_push_boolean(ctx, true);
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
