#include "JSONEditorLocalization.h"
#include <windows.h>
#include <vector>

JSONEditorLocalization::JSONEditorLocalization() : m_currentLang("en") {}

void JSONEditorLocalization::SetLanguage(const std::string &lang) {
  m_currentLang = lang;
}

std::wstring JSONEditorLocalization::GetLocalizedString(
    const std::string &key) const {
  const auto &translations = GetTranslations();
  auto itLang = translations.find(m_currentLang);
  if (itLang != translations.end()) {
    auto itStr = itLang->second.find(key);
    if (itStr != itLang->second.end()) {
      return itStr->second;
    }
  }
  return L"[" + std::wstring(key.begin(), key.end()) + L"]";
}

const std::map<std::string, std::map<std::string, std::wstring>> &
JSONEditorLocalization::GetTranslations() {
  static std::map<std::string, std::map<std::string, std::wstring>>
      translations = {
          {"en",
           {{"File", L"&File"},
            {"New", L"&New"},
            {"Open", L"&Open"},
            {"OpenMsgPack", L"Open Msg&Pack..."},
            {"OpenMsgPackLZ4", L"Open MsgPack &LZ4..."},
            {"Save", L"&Save"},
            {"SaveAs", L"Save &As..."},
            {"CloseTab", L"&Close Tab"},
            {"Exit", L"E&xit"},
            {"Format", L"F&ormat"},
            {"FormatJSON", L"Format &JSON"},
            {"FormatYAML", L"Format &YAML"},
            {"View", L"&View"},
            {"RefreshTree", L"Refresh &Tree"},
            {"LineEndings", L"&Line Endings"},
            {"Language", L"&Language"},
            {"English", L"&English"},
            {"Japanese", L"&Japanese"},
            {"Untitled", L"Untitled"}}},
          {"jp",
           {{"File", L"ファイル(&F)"},
            {"New", L"新規作成(&N)"},
            {"Open", L"開く(&O)..."},
            {"OpenMsgPack", L"MsgPackを開く(&P)..."},
            {"OpenMsgPackLZ4", L"LZ4圧縮MsgPackを開く(&Z)..."},
            {"Save", L"保存(&S)"},
            {"SaveAs", L"名前を付けて保存(&A)..."},
            {"CloseTab", L"タブを閉じる(&C)"},
            {"Exit", L"終了(&X)"},
            {"Format", L"整形(&F)"},
            {"FormatJSON", L"JSON整形(&J)"},
            {"FormatYAML", L"YAML整形(&Y)"},
            {"View", L"表示(&V)"},
            {"RefreshTree", L"ツリー更新(&R)"},
            {"LineEndings", L"改行コード(&L)"},
            {"Language", L"言語(&L)"},
            {"English", L"英語(&E)"},
            {"Japanese", L"日本語(&J)"},
            {"Untitled", L"無題"}}}};
  return translations;
}
