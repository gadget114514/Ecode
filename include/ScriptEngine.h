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

private:
  void LoadDefaultBindings();
  duk_context *m_ctx;
  std::map<std::string, std::string> m_keyBindings;
};
