#pragma once

#include "duktape.h"
#include <map>
#include <string>

class ScriptEngine {
public:
  ScriptEngine();
  ~ScriptEngine();

  bool Initialize();
  std::string Evaluate(const std::string &code);
  bool RunFile(const std::wstring &path);

  // Bindings
  void RegisterBinding(const std::string &chord, const std::string &jsFuncName);
  bool HandleBinding(const std::string &chord);

  // Keyboard Capture
  void SetCaptureKeyboard(bool capture) { m_captureKeyboard = capture; }
  bool IsKeyboardCaptured() const { return m_captureKeyboard; }
  void SetKeyHandler(const std::string &jsFuncName) { m_keyHandler = jsFuncName; }
  bool HandleKeyEvent(const std::string &key, bool isChar);

private:
  void LoadDefaultBindings();
  duk_context *m_ctx;
  std::map<std::string, std::string> m_keyBindings;
  bool m_captureKeyboard = false;
  std::string m_keyHandler;
};
