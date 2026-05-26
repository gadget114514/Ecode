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
  void SetKeyHandler(const std::string &jsFuncName) {
    m_keyHandler = jsFuncName;
  }
  bool HandleKeyEvent(const std::string &key, bool isChar);
  void SetBypassCache(bool bypass) { m_bypassCache = bypass; }
  void CompileAllScripts();
  void CallGlobalFunction(const std::string &name, const std::string &arg);

  // Error recovery
  bool IsFatalError() const { return m_fatalError; }
  void Reset();

private:
  static void FatalHandler(void *udata, const char *msg);
  void LoadDefaultBindings();
  duk_context *m_ctx;
  std::map<std::string, std::string> m_keyBindings;
  bool m_captureKeyboard = false;
  std::string m_keyHandler;
  bool m_bypassCache = false;
  bool m_fatalError = false;
};
