#include "../include/Localization.h"

Localization &Localization::Instance() {
  static Localization instance;
  return instance;
}

Localization::Localization() : m_currentLanguage(Language::English) {
  LoadTranslations();
}

void Localization::SetLanguage(Language lang) { m_currentLanguage = lang; }

std::wstring Localization::GetLocaleName() const {
  switch (m_currentLanguage) {
  case Language::Japanese:
    return L"ja-JP";
  case Language::Spanish:
    return L"es-ES";
  case Language::French:
    return L"fr-FR";
  case Language::German:
    return L"de-DE";
  case Language::English:
  default:
    return L"en-US";
  }
}

std::wstring Localization::GetString(const std::string &key) const {
  auto it = m_translations.find(m_currentLanguage);
  if (it != m_translations.end()) {
    auto strIt = it->second.find(key);
    if (strIt != it->second.end()) {
      return strIt->second;
    }
  }

  // Fallback to English
  if (m_currentLanguage != Language::English) {
    auto engIt = m_translations.find(Language::English);
    if (engIt != m_translations.end()) {
      auto strIt = engIt->second.find(key);
      if (strIt != engIt->second.end()) {
        return strIt->second;
      }
    }
  }

  return L"MISSING_STRING";
}

void Localization::LoadTranslations() {
  // English (Default)
  auto &en = m_translations[Language::English];
  en["title"] = L"Ecode - Native Win32 Text Editor";
  en["menu_file"] = L"&File";
  en["menu_file_new"] = L"&New";
  en["menu_file_open"] = L"&Open...";
  en["menu_file_save"] = L"&Save";
  en["menu_file_save_as"] = L"Save &As...";
  en["menu_file_close"] = L"&Close";
  en["menu_file_scratch"] = L"New &Scratch Buffer";
  en["menu_file_exit"] = L"E&xit";

  en["menu_edit"] = L"&Edit";
  en["menu_edit_undo"] = L"&Undo";
  en["menu_edit_redo"] = L"&Redo";
  en["menu_edit_cut"] = L"Cu&t";
  en["menu_edit_copy"] = L"&Copy";
  en["menu_edit_paste"] = L"&Paste";
  en["menu_edit_select_all"] = L"Select &All";
  en["menu_edit_find"] = L"&Find...";
  en["menu_edit_replace"] = L"&Replace...";
  en["menu_edit_goto"] = L"&Go to Line...";

  en["menu_view"] = L"&View";
  en["menu_view_toggle_ui"] = L"&Toggle UI Elements";
  en["menu_view_zoom_in"] = L"Zoom &In";
  en["menu_view_zoom_out"] = L"Zoom &Out";
  en["menu_view_zoom_reset"] = L"Zoom &Reset";

  en["menu_config"] = L"&Config";
  en["menu_config_settings"] = L"&Settings...";
  en["menu_config_theme"] = L"&Theme Selection...";
  en["menu_config_edit_init"] = L"Edit &ecodeinit.js";

  en["menu_tools"] = L"&Tools";
  en["menu_tools_run_macro"] = L"&Run Macro...";
  en["menu_tools_console"] = L"Script &Console";
  en["menu_tools_macro_gallery"] = L"Macro &Gallery";

  en["menu_language"] = L"&Language";
  en["menu_language_en"] = L"English";
  en["menu_language_jp"] = L"譌･譛ｬ隱・(Japanese)";
  en["menu_language_es"] = L"Espaﾃｱol (Spanish)";
  en["menu_language_fr"] = L"Franﾃｧais (French)";
  en["menu_language_de"] = L"Deutsch (German)";

  en["menu_buffers"] = L"&Buffers";
  en["menu_help"] = L"&Help";
  en["menu_help_doc"] = L"&Documentation";
  en["menu_help_about"] = L"&About";

  // Japanese
  auto &jp = m_translations[Language::Japanese];
  jp["title"] = L"Ecode - 繝阪う繝・ぅ繝・Win32 繝・く繧ｹ繝医お繝・ぅ繧ｿ";
  jp["menu_file"] = L"繝輔ぃ繧､繝ｫ(&F)";
  jp["menu_file_new"] = L"譁ｰ隕丈ｽ懈・(&N)";
  jp["menu_file_open"] = L"髢九￥(&O)...";
  jp["menu_file_save"] = L"菫晏ｭ・&S)";
  jp["menu_file_save_as"] = L"蜷榊燕繧剃ｻ倥￠縺ｦ菫晏ｭ・&A)...";
  jp["menu_file_close"] = L"髢峨§繧・&C)";
  jp["menu_file_scratch"] = L"譁ｰ縺励＞繧ｹ繧ｯ繝ｩ繝・メ繝舌ャ繝輔ぃ(&S)";
  jp["menu_file_exit"] = L"邨ゆｺ・&X)";

  jp["menu_edit"] = L"邱ｨ髮・&E)";
  jp["menu_edit_undo"] = L"蜈・↓謌ｻ縺・&U)";
  jp["menu_edit_redo"] = L"繧・ｊ逶ｴ縺・&R)";
  jp["menu_edit_cut"] = L"蛻・ｊ蜿悶ｊ(&T)";
  jp["menu_edit_copy"] = L"繧ｳ繝斐・(&C)";
  jp["menu_edit_paste"] = L"雋ｼ繧贋ｻ倥￠(&P)";
  jp["menu_edit_select_all"] = L"縺吶∋縺ｦ驕ｸ謚・&A)";
  jp["menu_edit_find"] = L"讀懃ｴ｢(&F)...";
  jp["menu_edit_replace"] = L"鄂ｮ謠・&R)...";
  jp["menu_edit_goto"] = L"謖・ｮ夊｡後∈遘ｻ蜍・&G)...";

  jp["menu_view"] = L"陦ｨ遉ｺ(&V)";
  jp["menu_view_toggle_ui"] = L"UI隕∫ｴ縺ｮ蛻・ｊ譖ｿ縺・&T)";
  jp["menu_view_zoom_in"] = L"諡｡螟ｧ(&I)";
  jp["menu_view_zoom_out"] = L"邵ｮ蟆・&O)";
  jp["menu_view_zoom_reset"] = L"繧ｺ繝ｼ繝繝ｪ繧ｻ繝・ヨ(&R)";

  jp["menu_config"] = L"險ｭ螳・&C)";
  jp["menu_config_settings"] = L"險ｭ螳・&S)...";
  jp["menu_config_theme"] = L"繝・・繝樣∈謚・&T)...";
  jp["menu_config_edit_init"] = L"ecodeinit.js繧堤ｷｨ髮・&E)";

  jp["menu_tools"] = L"繝・・繝ｫ(&T)";
  jp["menu_tools_run_macro"] = L"繝槭け繝ｭ螳溯｡・&R)...";
  jp["menu_tools_console"] = L"繧ｹ繧ｯ繝ｪ繝励ヨ繧ｳ繝ｳ繧ｽ繝ｼ繝ｫ(&C)";
  jp["menu_tools_macro_gallery"] = L"繝槭け繝ｭ繧ｮ繝｣繝ｩ繝ｪ繝ｼ(&G)";

  jp["menu_language"] = L"險隱・&L)";
  jp["menu_buffers"] = L"繝舌ャ繝輔ぃ(&B)";
  jp["menu_help"] = L"繝倥Ν繝・&H)";
  jp["menu_help_doc"] = L"繝峨く繝･繝｡繝ｳ繝・&D)";
  jp["menu_help_about"] = L"繝舌・繧ｸ繝ｧ繝ｳ諠・ｱ(&A)";

  // Spanish
  auto &es = m_translations[Language::Spanish];
  es["title"] = L"Ecode - Editor de texto nativo Win32";
  es["menu_file"] = L"&Archivo";
  es["menu_file_new"] = L"&Nuevo";
  es["menu_file_open"] = L"&Abrir...";
  es["menu_file_save"] = L"&Guardar";
  es["menu_file_save_as"] = L"Guardar &como...";
  es["menu_file_close"] = L"&Cerrar";
  es["menu_file_scratch"] = L"Nuevo &bﾃｺfer de notas";
  es["menu_file_exit"] = L"&Salir";

  es["menu_edit"] = L"&Editar";
  es["menu_edit_undo"] = L"&Deshacer";
  es["menu_edit_redo"] = L"&Rehacer";
  es["menu_edit_cut"] = L"Cor&tar";
  es["menu_edit_copy"] = L"&Copiar";
  es["menu_edit_paste"] = L"&Pegar";
  es["menu_edit_select_all"] = L"Seleccionar &todo";
  es["menu_edit_find"] = L"&Buscar...";
  es["menu_edit_replace"] = L"&Reemplazar...";
  es["menu_edit_goto"] = L"&Ir a la lﾃｭnea...";

  es["menu_view"] = L"&Vista";
  es["menu_config"] = L"&Configuraciﾃｳn";
  es["menu_tools"] = L"&Herramientas";
  es["menu_language"] = L"&Idioma";
  es["menu_buffers"] = L"&Bﾃｺferes";
  es["menu_help"] = L"&Ayuda";

  // French
  auto &fr = m_translations[Language::French];
  fr["title"] = L"Ecode - ﾃ嬰iteur de texte natif Win32";
  fr["menu_file"] = L"&Fichier";
  fr["menu_file_new"] = L"&Nouveau";
  fr["menu_file_open"] = L"&Ouvrir...";
  fr["menu_file_save"] = L"&Enregistrer";
  fr["menu_file_save_as"] = L"Enregistrer &sous...";
  fr["menu_file_close"] = L"&Fermer";
  fr["menu_file_scratch"] = L"Nouveau &brouillon";
  fr["menu_file_exit"] = L"&Quitter";

  fr["menu_edit"] = L"&Modifier";
  fr["menu_edit_undo"] = L"&Annuler";
  fr["menu_edit_redo"] = L"&Rﾃｩtablir";
  fr["menu_edit_cut"] = L"Cou&per";
  fr["menu_edit_copy"] = L"&Copier";
  fr["menu_edit_paste"] = L"&Coller";
  fr["menu_edit_select_all"] = L"&Tout sﾃｩlectionner";
  fr["menu_edit_find"] = L"&Rechercher...";
  fr["menu_edit_replace"] = L"&Remplacer...";
  fr["menu_edit_goto"] = L"&Aller ﾃ la ligne...";

  fr["menu_view"] = L"&Affichage";
  fr["menu_config"] = L"&Configuration";
  fr["menu_tools"] = L"&Outils";
  fr["menu_language"] = L"&Langue";
  fr["menu_buffers"] = L"&Tampons";
  fr["menu_help"] = L"&Aide";

  // German
  auto &de = m_translations[Language::German];
  de["title"] = L"Ecode - Nativer Win32-Texteditor";
  de["menu_file"] = L"&Datei";
  de["menu_file_new"] = L"&Neu";
  de["menu_file_open"] = L"&ﾃ貿fnen...";
  de["menu_file_save"] = L"&Speichern";
  de["menu_file_save_as"] = L"Speichern &unter...";
  de["menu_file_close"] = L"&Schlieﾃ歹n";
  de["menu_file_scratch"] = L"Neuer &Notizblock";
  de["menu_file_exit"] = L"&Beenden";

  de["menu_edit"] = L"&Bearbeiten";
  de["menu_edit_undo"] = L"&Rﾃｼckgﾃ､ngig";
  de["menu_edit_redo"] = L"&Wiederherstellen";
  de["menu_edit_cut"] = L"Aus&schneiden";
  de["menu_edit_copy"] = L"&Kopieren";
  de["menu_edit_paste"] = L"&Einfﾃｼgen";
  de["menu_edit_select_all"] = L"&Alles auswﾃ､hlen";
  de["menu_edit_find"] = L"&Suchen...";
  de["menu_edit_replace"] = L"&Ersetzen...";
  de["menu_edit_goto"] = L"&Gehe zu Zeile...";

  de["menu_view"] = L"&Ansicht";
  de["menu_config"] = L"&Konfiguration";
  de["menu_tools"] = L"&Extras";
  de["menu_language"] = L"&Sprache";
  de["menu_buffers"] = L"&Puffer";
  de["menu_help"] = L"&Hilfe";
}
