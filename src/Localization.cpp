#include <windows.h>
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

  // Return the key itself so we can see what's missing
  std::string keyUtf8 = key;
  int len = MultiByteToWideChar(CP_UTF8, 0, keyUtf8.c_str(), -1, NULL, 0);
  if (len > 0) {
      std::vector<wchar_t> buf(len);
      MultiByteToWideChar(CP_UTF8, 0, keyUtf8.c_str(), -1, buf.data(), len);
      return std::wstring(buf.data());
  }
  return L"???";
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
  en["menu_file_recent"] = L"Recent &Files";
  en["menu_file_recent_empty"] = L"(Empty)";
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
  en["menu_config_theme"] = L"&Manage Themes...";
  en["menu_config_edit_init"] = L"Edit &ecodeinit.js";

  en["menu_tools"] = L"&Tools";
  en["menu_tools_run_macro"] = L"&Run Macro...";
  en["menu_tools_console"] = L"Script &Console";
  en["menu_tools_macro_gallery"] = L"Macro &Gallery";

  en["menu_language"] = L"&Language";
  en["menu_language_en"] = L"English";
  en["menu_language_jp"] = L"Japanese";
  en["menu_language_es"] = L"Español (Spanish)";
  en["menu_language_fr"] = L"Français (French)";
  en["menu_language_de"] = L"Deutsch (German)";

  en["menu_buffers"] = L"&Buffers";
  en["menu_help"] = L"&Help";
  en["menu_help_doc"] = L"&Documentation";
  en["menu_help_keybindings"] = L"&Key Bindings";
  en["menu_help_about"] = L"&About";
  en["menu_help_copyright"] = L"&Copyright";
  en["menu_help_messages"] = L"Show &Messages";
  en["menu_help_clear_messages"] = L"&Clear Messages";

  en["settings_title"] = L"Ecode Settings";
  en["settings_cancel"] = L"Cancel";
  en["settings_tab_general"] = L"General";
  en["settings_tab_ai"] = L"AI Settings";
  en["settings_font_family"] = L"Font Family:";
  en["settings_font_browse"] = L"Font...";
  en["settings_font_size"] = L"Font Size:";
  en["settings_font_weight"] = L"Font Weight:";
  en["settings_show_line_numbers"] = L"Show Line Numbers";
  en["settings_enable_ligatures"] = L"Enable Ligatures";
  en["settings_word_wrap"] = L"Enable Word Wrap";
  en["settings_wrap_width"] = L"Wrap Width (0 for auto):";
  en["settings_language"] = L"Language:";
  en["settings_caret_style"] = L"Caret Style:";
  en["settings_caret_blinking"] = L"Caret Blinking";
  en["settings_log_level"] = L"Log Level:";
  en["settings_vt_debug"] = L"VT Debug";
  en["settings_bash_path"] = L"Bash Path:";
  en["settings_default_ext"] = L"Text Editor Default Extension:";
  en["settings_no_title_bar"] = L"Hide Title Bar (requires restart)";
  en["settings_reverse_scroll"] = L"Reverse Scroll Direction";
  en["settings_hide_messages"] = L"Hide *Messages* Buffer";
  en["settings_shared_localmsg"] = L"Shared LocalMsg (single instance)";
  en["settings_tab_font_style"] = L"Active Tab Font Style:";
  en["settings_plugins_dir"] = L"Plugins Directory:";

  en["settings_ai_server"] = L"Active Server:";
  en["settings_ai_config"] = L"Server Configuration";
  en["settings_ai_model"] = L"Model Name:";
  en["settings_ai_base"] = L"API Base URL:";
  en["settings_ai_key"] = L"API Key:";
  en["settings_ai_dir"] = L"Allowed Project Directory:";
  en["settings_ai_note"] = L"(Note: AI can only edit files within this folder if specified)";

  en["settings_caret_line"] = L"Line";
  en["settings_caret_block"] = L"Block";
  en["settings_caret_underline"] = L"Underline";

  en["settings_font_regular"] = L"Regular";
  en["settings_font_bold"] = L"Bold";
  en["settings_font_italic"] = L"Italic";
  en["settings_font_bold_italic"] = L"Bold Italic";

  // Japanese
  auto &jp = m_translations[Language::Japanese];
  jp["title"] = L"Ecode - ネイティブWin32 テキストエディタ";
  jp["menu_file"] = L"ファイル(&F)";
  jp["menu_file_new"] = L"新規作成(&N)";
  jp["menu_file_open"] = L"開く(&O)...";
  jp["menu_file_save"] = L"保存(&S)";
  jp["menu_file_save_as"] = L"名前を付けて保存(&A)...";
  jp["menu_file_close"] = L"閉じる(&C)";
  jp["menu_file_scratch"] = L"新しいスクラッチバッファ(&S)";
  jp["menu_file_recent"] = L"最近使ったファイル(&R)";
  jp["menu_file_recent_empty"] = L"(空)";
  jp["menu_file_exit"] = L"終了(&X)";

  jp["menu_edit"] = L"編集(&E)";
  jp["menu_edit_undo"] = L"元に戻す(&U)";
  jp["menu_edit_redo"] = L"やり直す(&R)";
  jp["menu_edit_cut"] = L"切り取り(&T)";
  jp["menu_edit_copy"] = L"コピー(&C)";
  jp["menu_edit_paste"] = L"貼り付け(&P)";
  jp["menu_edit_select_all"] = L"すべて選択(&A)";
  jp["menu_edit_find"] = L"検索(&F)...";
  jp["menu_edit_replace"] = L"置換(&R)...";
  jp["menu_edit_goto"] = L"指定行へ移動(&G)...";

  jp["menu_view"] = L"表示(&V)";
  jp["menu_view_toggle_ui"] = L"UI要素の切り替え(&T)";
  jp["menu_view_zoom_in"] = L"拡大(&I)";
  jp["menu_view_zoom_out"] = L"縮小(&O)";
  jp["menu_view_zoom_reset"] = L"ズームリセット(&R)";

  jp["menu_config"] = L"設定(&C)";
  jp["menu_config_settings"] = L"設定(&S)...";
  jp["menu_config_theme"] = L"テーマ管理(&T)...";
  jp["menu_config_edit_init"] = L"ecodeinit.jsを編集(&E)";

  jp["menu_tools"] = L"ツール(&T)";
  jp["menu_tools_run_macro"] = L"マクロ実行(&R)...";
  jp["menu_tools_console"] = L"スクリプトコンソール(&C)";
  jp["menu_tools_macro_gallery"] = L"マクロギャラリー(&G)";

  jp["menu_language"] = L"言語(&L)";
  jp["menu_buffers"] = L"バッファ(&B)";
  jp["menu_help"] = L"ヘルプ(&H)";
  jp["menu_help_doc"] = L"ドキュメント(&D)";
  jp["menu_help_keybindings"] = L"キーバインド(&K)";
  jp["menu_help_about"] = L"バージョン情報(&A)";
  jp["menu_help_copyright"] = L"著作権(&C)";
  jp["menu_help_messages"] = L"メッセージを表示(&M)";
  jp["menu_help_clear_messages"] = L"メッセージをクリア(&C)";

  jp["settings_title"] = L"Ecode 設定";
  jp["settings_cancel"] = L"キャンセル";
  jp["settings_tab_general"] = L"全般";
  jp["settings_tab_ai"] = L"AI 設定";
  jp["settings_font_family"] = L"フォントファミリー:";
  jp["settings_font_browse"] = L"フォント...";
  jp["settings_font_size"] = L"フォントサイズ:";
  jp["settings_font_weight"] = L"フォントの太さ:";
  jp["settings_show_line_numbers"] = L"行番号を表示";
  jp["settings_enable_ligatures"] = L"リガチャーを有効にする";
  jp["settings_word_wrap"] = L"右端で折り返す";
  jp["settings_wrap_width"] = L"折り返し幅 (0で自動):";
  jp["settings_language"] = L"言語:";
  jp["settings_caret_style"] = L"カーソルのスタイル:";
  jp["settings_caret_blinking"] = L"カーソルを点滅させる";
  jp["settings_log_level"] = L"ログレベル:";
  jp["settings_vt_debug"] = L"VTデバッグ";
  jp["settings_bash_path"] = L"Bashのパス:";
  jp["settings_default_ext"] = L"テキストエディタの既定の拡張子:";
  jp["settings_no_title_bar"] = L"タイトルバーを非表示にする (再起動が必要)";
  jp["settings_reverse_scroll"] = L"スクロール方向を反転する";
  jp["settings_hide_messages"] = L"*Messages* バッファを非表示にする";
  jp["settings_shared_localmsg"] = L"LocalMsgを共有する (シングルインスタンス)";
  jp["settings_tab_font_style"] = L"アクティブタブのフォントスタイル:";
  jp["settings_plugins_dir"] = L"プラグインのディレクトリ:";

  jp["settings_ai_server"] = L"アクティブなサーバー:";
  jp["settings_ai_config"] = L"サーバー構成";
  jp["settings_ai_model"] = L"モデル名:";
  jp["settings_ai_base"] = L"APIベースURL:";
  jp["settings_ai_key"] = L"APIキー:";
  jp["settings_ai_dir"] = L"許可されたプロジェクトディレクトリ:";
  jp["settings_ai_note"] = L"(注: 指定されている場合、AIはこのフォルダー内のファイルのみ編集できます)";

  jp["settings_caret_line"] = L"ライン";
  jp["settings_caret_block"] = L"ブロック";
  jp["settings_caret_underline"] = L"下線";

  jp["settings_font_regular"] = L"標準";
  jp["settings_font_bold"] = L"太字";
  jp["settings_font_italic"] = L"斜体";
  jp["settings_font_bold_italic"] = L"太字斜体";

  // Spanish
  auto &es = m_translations[Language::Spanish];
  es["title"] = L"Ecode - Editor de texto nativo Win32";
  es["menu_file"] = L"&Archivo";
  es["menu_file_new"] = L"&Nuevo";
  es["menu_file_open"] = L"&Abrir...";
  es["menu_file_save"] = L"&Guardar";
  es["menu_file_save_as"] = L"Guardar &como...";
  es["menu_file_close"] = L"&Cerrar";
  es["menu_file_scratch"] = L"Nuevo &búfer de notas";
  es["menu_file_recent"] = L"Archivos &recientes";
  es["menu_file_recent_empty"] = L"(Vacío)";
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
  es["menu_edit_goto"] = L"&Ir a la línea...";

  es["menu_view"] = L"&Vista";
  es["menu_config"] = L"&Configuración";
  es["menu_tools"] = L"&Herramientas";
  es["menu_language"] = L"&Idioma";
  es["menu_buffers"] = L"&Búferes";
  es["menu_help"] = L"&Ayuda";
  es["menu_help_doc"] = L"&Documentación";
  es["menu_help_keybindings"] = L"&Atajos de teclado";
  es["menu_help_about"] = L"&Acerca de";
  es["menu_help_copyright"] = L"&Copyright";
  es["menu_help_messages"] = L"&Mostrar mensajes";
  es["menu_help_clear_messages"] = L"&Limpiar mensajes";

  es["settings_title"] = L"Configuración de Ecode";
  es["settings_cancel"] = L"Cancelar";
  es["settings_tab_general"] = L"General";
  es["settings_tab_ai"] = L"Configuración de IA";
  es["settings_font_family"] = L"Familia de fuentes:";
  es["settings_font_browse"] = L"Fuente...";
  es["settings_font_size"] = L"Tamaño de fuente:";
  es["settings_font_weight"] = L"Grosor de fuente:";
  es["settings_show_line_numbers"] = L"Mostrar números de línea";
  es["settings_enable_ligatures"] = L"Habilitar ligaduras";
  es["settings_word_wrap"] = L"Habilitar ajuste de línea";
  es["settings_wrap_width"] = L"Ancho de ajuste (0 para auto):";
  es["settings_language"] = L"Idioma:";
  es["settings_caret_style"] = L"Estilo de cursor:";
  es["settings_caret_blinking"] = L"Parpadeo del cursor";
  es["settings_log_level"] = L"Nivel de registro:";
  es["settings_vt_debug"] = L"Depuración VT";
  es["settings_bash_path"] = L"Ruta de Bash:";
  es["settings_default_ext"] = L"Extensión predeterminada del editor:";
  es["settings_no_title_bar"] = L"Ocultar barra de título (requiere reiniciar)";
  es["settings_reverse_scroll"] = L"Invertir dirección de desplazamiento";
  es["settings_hide_messages"] = L"Ocultar búfer *Messages*";
  es["settings_shared_localmsg"] = L"LocalMsg compartido (instancia única)";
  es["settings_tab_font_style"] = L"Estilo de fuente de pestaña activa:";
  es["settings_plugins_dir"] = L"Directorio de complementos:";

  es["settings_ai_server"] = L"Servidor activo:";
  es["settings_ai_config"] = L"Configuración del servidor";
  es["settings_ai_model"] = L"Nombre del modelo:";
  es["settings_ai_base"] = L"URL base de la API:";
  es["settings_ai_key"] = L"Clave de API:";
  es["settings_ai_dir"] = L"Directorio de proyecto permitido:";
  es["settings_ai_note"] = L"(Nota: la IA solo puede editar archivos dentro de esta carpeta si se especifica)";

  es["settings_caret_line"] = L"Línea";
  es["settings_caret_block"] = L"Bloque";
  es["settings_caret_underline"] = L"Subrayado";

  es["settings_font_regular"] = L"Regular";
  es["settings_font_bold"] = L"Negrita";
  es["settings_font_italic"] = L"Cursiva";
  es["settings_font_bold_italic"] = L"Negrita Cursiva";

  // French
  auto &fr = m_translations[Language::French];
  fr["title"] = L"Ecode - Éditeur de texte natif Win32";
  fr["menu_file"] = L"&Fichier";
  fr["menu_file_new"] = L"&Nouveau";
  fr["menu_file_open"] = L"&Ouvrir...";
  fr["menu_file_save"] = L"&Enregistrer";
  fr["menu_file_save_as"] = L"Enregistrer &sous...";
  fr["menu_file_close"] = L"&Fermer";
  fr["menu_file_scratch"] = L"Nouveau &brouillon";
  fr["menu_file_recent"] = L"Fichiers &récents";
  fr["menu_file_recent_empty"] = L"(Vide)";
  fr["menu_file_exit"] = L"&Quitter";

  fr["menu_edit"] = L"&Modifier";
  fr["menu_edit_undo"] = L"&Annuler";
  fr["menu_edit_redo"] = L"&Rétablir";
  fr["menu_edit_cut"] = L"Cou&per";
  fr["menu_edit_copy"] = L"&Copier";
  fr["menu_edit_paste"] = L"&Coller";
  fr["menu_edit_select_all"] = L"&Tout sélectionner";
  fr["menu_edit_find"] = L"&Rechercher...";
  fr["menu_edit_replace"] = L"&Remplacer...";
  fr["menu_edit_goto"] = L"&Aller à la ligne...";

  fr["menu_view"] = L"&Affichage";
  fr["menu_config"] = L"&Configuration";
  fr["menu_tools"] = L"&Outils";
  fr["menu_language"] = L"&Langue";
  fr["menu_buffers"] = L"&Tampons";
  fr["menu_help"] = L"&Aide";
  fr["menu_help_doc"] = L"&Documentation";
  fr["menu_help_keybindings"] = L"&Raccourcis clavier";
  fr["menu_help_about"] = L"&À propos";
  fr["menu_help_copyright"] = L"&Copyright";
  fr["menu_help_messages"] = L"&Afficher les messages";
  fr["menu_help_clear_messages"] = L"&Effacer les messages";

  fr["settings_title"] = L"Paramètres d'Ecode";
  fr["settings_cancel"] = L"Annuler";
  fr["settings_tab_general"] = L"Général";
  fr["settings_tab_ai"] = L"Paramètres IA";
  fr["settings_font_family"] = L"Famille de polices :";
  fr["settings_font_browse"] = L"Police...";
  fr["settings_font_size"] = L"Taille de police :";
  fr["settings_font_weight"] = L"Graisse de police :";
  fr["settings_show_line_numbers"] = L"Afficher les numéros de ligne";
  fr["settings_enable_ligatures"] = L"Activer les ligatures";
  fr["settings_word_wrap"] = L"Activer le retour à la ligne";
  fr["settings_wrap_width"] = L"Largeur de retour (0 pour auto) :";
  fr["settings_language"] = L"Langue :";
  fr["settings_caret_style"] = L"Style du curseur :";
  fr["settings_caret_blinking"] = L"Clignotement du curseur";
  fr["settings_log_level"] = L"Niveau de journalisation :";
  fr["settings_vt_debug"] = L"Débogage VT";
  fr["settings_bash_path"] = L"Chemin Bash :";
  fr["settings_default_ext"] = L"Extension par défaut de l'éditeur :";
  fr["settings_no_title_bar"] = L"Masquer la barre de titre (redémarrage requis)";
  fr["settings_reverse_scroll"] = L"Inverser le sens du défilement";
  fr["settings_hide_messages"] = L"Masquer le tampon *Messages*";
  fr["settings_shared_localmsg"] = L"LocalMsg partagé (instance unique)";
  fr["settings_tab_font_style"] = L"Style de police de l'onglet actif :";
  fr["settings_plugins_dir"] = L"Dossier des plug-ins :";

  fr["settings_ai_server"] = L"Serveur actif :";
  fr["settings_ai_config"] = L"Configuration du serveur";
  fr["settings_ai_model"] = L"Nom du modèle :";
  fr["settings_ai_base"] = L"URL de base de l'API :";
  fr["settings_ai_key"] = L"API Key :";
  fr["settings_ai_dir"] = L"Dossier de projet autorisé :";
  fr["settings_ai_note"] = L"(Note : l'IA ne peut modifier que les fichiers de ce dossier s'il est spécifié)";

  fr["settings_caret_line"] = L"Ligne";
  fr["settings_caret_block"] = L"Bloc";
  fr["settings_caret_underline"] = L"Souligné";

  fr["settings_font_regular"] = L"Normal";
  fr["settings_font_bold"] = L"Gras";
  fr["settings_font_italic"] = L"Italique";
  fr["settings_font_bold_italic"] = L"Gras Italique";

  // German
  auto &de = m_translations[Language::German];
  de["title"] = L"Ecode - Nativer Win32-Texteditor";
  de["menu_file"] = L"&Datei";
  de["menu_file_new"] = L"&Neu";
  de["menu_file_open"] = L"&Öffnen...";
  de["menu_file_save"] = L"&Speichern";
  de["menu_file_save_as"] = L"Speichern &unter...";
  de["menu_file_close"] = L"&Schließen";
  de["menu_file_scratch"] = L"Neuer &Notizblock";
  de["menu_file_recent"] = L"&Zuletzt geöffnete Dateien";
  de["menu_file_recent_empty"] = L"(Leer)";
  de["menu_file_exit"] = L"&Beenden";

  de["menu_edit"] = L"&Bearbeiten";
  de["menu_edit_undo"] = L"&Rückgängig";
  de["menu_edit_redo"] = L"&Wiederherstellen";
  de["menu_edit_cut"] = L"Aus&schneiden";
  de["menu_edit_copy"] = L"&Kopieren";
  de["menu_edit_paste"] = L"&Einfügen";
  de["menu_edit_select_all"] = L"&Alles auswählen";
  de["menu_edit_find"] = L"&Suchen...";
  de["menu_edit_replace"] = L"&Ersetzen...";
  de["menu_edit_goto"] = L"&Gehe zu Zeile...";

  de["menu_view"] = L"&Ansicht";
  de["menu_config"] = L"&Konfiguration";
  de["menu_tools"] = L"&Extras";
  de["menu_language"] = L"&Sprache";
  de["menu_buffers"] = L"&Puffer";
  de["menu_help"] = L"&Hilfe";
  de["menu_help_doc"] = L"&Dokumentation";
  de["menu_help_keybindings"] = L"&Tastenkombinationen";
  de["menu_help_about"] = L"&Über";
  de["menu_help_copyright"] = L"&Urheberrecht";
  de["menu_help_messages"] = L"&Nachrichten anzeigen";
  de["menu_help_clear_messages"] = L"Nachrichten &löschen";

  de["settings_title"] = L"Ecode-Einstellungen";
  de["settings_cancel"] = L"Abbrechen";
  de["settings_tab_general"] = L"Allgemein";
  de["settings_tab_ai"] = L"KI-Einstellungen";
  de["settings_font_family"] = L"Schriftfamilie:";
  de["settings_font_browse"] = L"Schriftart...";
  de["settings_font_size"] = L"Schriftgröße:";
  de["settings_font_weight"] = L"Schriftgewicht:";
  de["settings_show_line_numbers"] = L"Zeilennummern anzeigen";
  de["settings_enable_ligatures"] = L"Ligaturen aktivieren";
  de["settings_word_wrap"] = L"Zeilenumbruch aktivieren";
  de["settings_wrap_width"] = L"Umbruchbreite (0 für Auto):";
  de["settings_language"] = L"Sprache:";
  de["settings_caret_style"] = L"Cursor-Stil:";
  de["settings_caret_blinking"] = L"Cursor-Blinken";
  de["settings_log_level"] = L"Protokollebene:";
  de["settings_vt_debug"] = L"VT-Debugging";
  de["settings_bash_path"] = L"Bash-Pfad:";
  de["settings_default_ext"] = L"Standarderweiterung des Texteditors:";
  de["settings_no_title_bar"] = L"Titelleiste ausblenden (Neustart erforderlich)";
  de["settings_reverse_scroll"] = L"Bildlaufrichtung umkehren";
  de["settings_hide_messages"] = L"Puffer *Messages* ausblenden";
  de["settings_shared_localmsg"] = L"Gemeinsames LocalMsg (Einzelinstanz)";
  de["settings_tab_font_style"] = L"Schriftstil der aktiven Registerkarte:";
  de["settings_plugins_dir"] = L"Plugin-Verzeichnis:";

  de["settings_ai_server"] = L"Aktiver Server:";
  de["settings_ai_config"] = L"Serverkonfiguration";
  de["settings_ai_model"] = L"Modellname:";
  de["settings_ai_base"] = L"API-Basis-URL:";
  de["settings_ai_key"] = L"API-Schlüssel:";
  de["settings_ai_dir"] = L"Zulässiges Projektverzeichnis:";
  de["settings_ai_note"] = L"(Hinweis: Die KI kann Dateien nur in diesem Ordner bearbeiten, wenn angegeben)";

  de["settings_caret_line"] = L"Linie";
  de["settings_caret_block"] = L"Block";
  de["settings_caret_underline"] = L"Unterstrichen";

  de["settings_font_regular"] = L"Standard";
  de["settings_font_bold"] = L"Fett";
  de["settings_font_italic"] = L"Kursiv";
  de["settings_font_bold_italic"] = L"Fett Kursiv";
}
