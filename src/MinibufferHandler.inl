// =============================================================================
// MinibufferHandler.inl
// Minibuffer subclassing, history, and input handling
// Included by main.cpp
// =============================================================================

// Helper: hide minibuffer and return focus
void HideMinibuffer() {
  DebugLog("Minibuffer: Hiding", LOG_DEBUG);
  g_minibufferVisible = false;
  SendMessage(g_mainHwnd, WM_SIZE, 0, 0);
  SetFocus(g_mainHwnd);
}

LRESULT CALLBACK MinibufferSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                        LPARAM lParam) {
  if (uMsg == WM_KEYDOWN) {
    DebugLog("Minibuffer: WM_KEYDOWN wParam=" + std::to_string(wParam),
             LOG_DEBUG);
    // Ctrl+G or Escape: quit (Emacs keyboard-quit)
    if (wParam == VK_ESCAPE ||
        (wParam == 'G' && (GetKeyState(VK_CONTROL) & 0x8000))) {
      DebugLog("Minibuffer: Escape/Ctrl+G - Quitting", LOG_INFO);
      HideMinibuffer();
      SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)L"Quit");
      return 0;
    }

    if (wParam == VK_UP || wParam == VK_DOWN) {
      if (g_minibufferHistory.empty())
        return 0;
      if (wParam == VK_UP) {
        if (g_historyIndex == -1)
          g_historyIndex = (int)g_minibufferHistory.size() - 1;
        else if (g_historyIndex > 0)
          g_historyIndex--;
      } else {
        if (g_historyIndex != -1 &&
            g_historyIndex < (int)g_minibufferHistory.size() - 1)
          g_historyIndex++;
        else if (g_historyIndex == (int)g_minibufferHistory.size() - 1)
          g_historyIndex = -1;
      }

      if (g_historyIndex != -1) {
        SetWindowTextA(hwnd, g_minibufferHistory[g_historyIndex].c_str());
        SendMessage(hwnd, EM_SETSEL, 0, -1);
        SendMessage(hwnd, EM_SETSEL, -1, -1); // move caret to end
      } else {
        SetWindowTextA(hwnd, "");
      }
      return 0;
    }

    if (wParam == VK_TAB) {
      int len = GetWindowTextLengthA(hwnd);
      std::string input(len, '\0');
      GetWindowTextA(hwnd, &input[0], len + 1);

      if (g_minibufferMode == MB_MX_COMMAND && !input.empty()) {
        // Simple command completion
        std::string js =
            "(function(){ var keys = Object.keys(emacs_mx_commands); "
            "var matches = keys.filter(function(k){ return k.startsWith('" +
            input +
            "'); });"
            "if (matches.length == 1) return matches[0];"
            "if (matches.length > 1) { "
            "  var common = matches[0]; "
            "  for(var i=1; i<matches.length; i++) { "
            "    while(matches[i].indexOf(common) !== 0) common = "
            "common.substring(0, common.length-1); "
            "  } "
            "  return common; "
            "} "
            "return ''; })();";
        std::string result = g_scriptEngine->Evaluate(js);
        if (!result.empty() && result != "''" && result != "null" &&
            result != "undefined") {
          if (result.size() >= 2 && result.front() == '\'' &&
              result.back() == '\'')
            result = result.substr(1, result.size() - 2);
          SetWindowTextA(hwnd, result.c_str());
          SendMessage(hwnd, EM_SETSEL, -1, -1);
        }
      } else if (g_minibufferMode == MB_CALLBACK &&
                 !g_minibufferJsCallback.empty() && !input.empty()) {
        std::string completeFunc = g_minibufferJsCallback + "_complete";
        std::string js = "if (typeof " + completeFunc + " === 'function') { " +
                         completeFunc + "('" + input + "'); } else { ''; }";
        std::string result = g_scriptEngine->Evaluate(js);
        if (!result.empty() && result != "''" && result != "null" &&
            result != "undefined") {
          if (result.size() >= 2 && result.front() == '\'' &&
              result.back() == '\'')
            result = result.substr(1, result.size() - 2);
          SetWindowTextA(hwnd, result.c_str());
          SendMessage(hwnd, EM_SETSEL, -1, -1);
        }
      }
      return 0;
    }

    if (wParam == VK_RETURN) {
      int len = GetWindowTextLengthA(hwnd);
      std::string input(len, '\0');
      GetWindowTextA(hwnd, &input[0], len + 1);
      DebugLog("Minibuffer: Return pressed. Input='" + input +
                   "' Mode=" + std::to_string(g_minibufferMode),
               LOG_INFO);

      if (!input.empty()) {
        if (g_minibufferHistory.empty() ||
            g_minibufferHistory.back() != input) {
          g_minibufferHistory.push_back(input);
        }
        g_historyIndex = -1;
      }

      if (g_minibufferMode == MB_EVAL) {
        std::string result = g_scriptEngine->Evaluate(input);
        std::wstring wresult = StringToWString(result);
        SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)wresult.c_str());
      } else if (g_minibufferMode == MB_MX_COMMAND) {
        std::string jsCall = "if (typeof emacs_mx_commands !== 'undefined' && "
                             "emacs_mx_commands['" +
                             input + "']) { emacs_mx_commands['" + input +
                             "'](); 'ok'; } else { 'Unknown command: " + input +
                             "'; }";
        std::string result = g_scriptEngine->Evaluate(jsCall);
        std::wstring wresult = StringToWString(result);
        SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)wresult.c_str());
      } else if (g_minibufferMode == MB_CALLBACK) {
        if (!g_minibufferJsCallback.empty()) {
          std::string jsCall = g_minibufferJsCallback + "('" + input + "');";
          g_scriptEngine->Evaluate(jsCall);
        }
      }

      HideMinibuffer();
      InvalidateRect(g_mainHwnd, NULL, FALSE);
      return 0;
    }
  }
  return CallWindowProc(g_oldMinibufferProc, hwnd, uMsg, wParam, lParam);
}
