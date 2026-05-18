#include "Localization.h"

LanguageID Localization::s_currentLang = APP_LANG_ENGLISH;

// String Table: [Language][StringID]
const wchar_t *g_Strings[APP_LANG_COUNT][IDS_STRING_COUNT] = {
    // APP_LANG_ENGLISH
    {
        L"Search",             // IDS_SEARCH
        L"Search Filename:",   // IDS_FILENAME_LABEL
        L"Ready",              // IDS_STATUS_READY
        L"Searching...",       // IDS_STATUS_BUSY
        L"Found %d items",     // IDS_STATUS_FOUND_FMT
        L"Name",               // IDS_COL_NAME
        L"Path",               // IDS_COL_PATH
        L"Modified Date",      // IDS_COL_DATE
        L"Size",               // IDS_COL_SIZE
        L"Save Results",       // IDS_BTN_SAVE
        L"Search Candidates:", // IDS_LBL_TARGETS
        L"Add Folder",         // IDS_BTN_ADD
        L"Not candidate",      // IDS_BTN_REMOVE
        L"Not",                // IDS_CHK_NOT
        L"Code Page:",         // IDS_LBL_CODEPAGE

        // Search Options
        L"Search Options",       // IDS_CONFIG_TITLE
        L"Match Mode",           // IDS_GRP_MATCHMODE
        L"Substring",            // IDS_RAD_SUBSTRING
        L"Exact Match",          // IDS_RAD_EXACT
        L"Space Divided (All)",  // IDS_RAD_SPACED
        L"Regular Expression",   // IDS_RAD_REGEX
        L"Ignore Case (Global)", // IDS_CHK_IGNORECASE
        L"Size Filter",          // IDS_GRP_SIZE
        L"Enable Size Filter",   // IDS_CHK_SIZE
        L"Min:",                 // IDS_LBL_MIN
        L"Max:",                 // IDS_LBL_MAX
        L"Date Filter",          // IDS_GRP_DATE
        L"Enable Date Filter",   // IDS_CHK_DATE
        L"From:",                // IDS_LBL_FROM
        L"To:",                  // IDS_LBL_TO
        L"Include",              // IDS_GRP_INCLUDE
        L"Files",                // IDS_CHK_FILES
        L"Folders",              // IDS_CHK_FOLDERS
        L"Type Filter",          // IDS_GRP_TYPE
        L"Ext:",                 // IDS_LBL_EXT
        L"Advanced",             // IDS_GRP_ADVANCED
        L"Match Full Path",      // IDS_CHK_FULLPATH
        L"Exclude:",             // IDS_LBL_EXCLUDE
        L"OK",                   // IDS_BTN_OK
        L"Cancel",               // IDS_BTN_CANCEL

        // Menu
        L"File",              // IDS_MENU_FILE
        L"Config",            // IDS_MENU_CONFIG
        L"Language",          // IDS_MENU_LANGUAGE
        L"Help",              // IDS_MENU_HELP
        L"Search Options...", // IDS_MENU_SEARCH_OPTIONS
        L"About...",          // IDS_MENU_ABOUT
        L"Context",           // IDS_MENU_CONTEXT
        L"Copy Path"          // IDS_MENU_COPY_PATH
    },
    // APP_LANG_JAPANESE
    {
        L"検索",                // IDS_SEARCH
        L"検索ファイル名:",     // IDS_FILENAME_LABEL
        L"準備完了",            // IDS_STATUS_READY
        L"検索中...",           // IDS_STATUS_BUSY
        L"%d 件見つかりました", // IDS_STATUS_FOUND_FMT
        L"名前",                // IDS_COL_NAME
        L"パス",                // IDS_COL_PATH
        L"更新日時",            // IDS_COL_DATE
        L"サイズ",              // IDS_COL_SIZE
        L"結果を保存",          // IDS_BTN_SAVE
        L"検索対象:",           // IDS_LBL_TARGETS
        L"フォルダ追加",        // IDS_BTN_ADD
        L"検索対象外",          // IDS_BTN_REMOVE
        L"否定",                // IDS_CHK_NOT
        L"コードページ:",       // IDS_LBL_CODEPAGE

        // Search Options
        L"検索オプション",             // IDS_CONFIG_TITLE
        L"一致モード",                 // IDS_GRP_MATCHMODE
        L"部分一致",                   // IDS_RAD_SUBSTRING
        L"完全一致",                   // IDS_RAD_EXACT
        L"スペース区切り (すべて)",    // IDS_RAD_SPACED
        L"正規表現",                   // IDS_RAD_REGEX
        L"大文字/小文字を無視 (全体)", // IDS_CHK_IGNORECASE
        L"サイズフィルター",           // IDS_GRP_SIZE
        L"サイズフィルターを有効化",   // IDS_CHK_SIZE
        L"最小:",                      // IDS_LBL_MIN
        L"最大:",                      // IDS_LBL_MAX
        L"日付フィルター",             // IDS_GRP_DATE
        L"日付フィルターを有効化",     // IDS_CHK_DATE
        L"開始:",                      // IDS_LBL_FROM
        L"終了:",                      // IDS_LBL_TO
        L"検索対象",                   // IDS_GRP_INCLUDE
        L"ファイル",                   // IDS_CHK_FILES
        L"フォルダ",                   // IDS_CHK_FOLDERS
        L"種類フィルター",             // IDS_GRP_TYPE
        L"拡張子",                     // IDS_LBL_EXT
        L"詳細設定",                   // IDS_GRP_ADVANCED
        L"フルパスで一致",             // IDS_CHK_FULLPATH
        L"除外:",                      // IDS_LBL_EXCLUDE
        L"OK",                         // IDS_BTN_OK
        L"キャンセル",                 // IDS_BTN_CANCEL

        // Menu
        L"ファイル",          // IDS_MENU_FILE
        L"設定",              // IDS_MENU_CONFIG
        L"言語",              // IDS_MENU_LANGUAGE
        L"ヘルプ",            // IDS_MENU_HELP
        L"検索オプション...", // IDS_MENU_SEARCH_OPTIONS
        L"バージョン情報...", // IDS_MENU_ABOUT
        L"コンテキスト",      // IDS_MENU_CONTEXT
        L"パスをコピー"       // IDS_MENU_COPY_PATH
    },
    // APP_LANG_CHINESE_SIMP
    {L"搜索", L"文件名:", L"就绪", L"正在搜索...", L"找到 %d 个项目", L"名称",
     L"路径", L"修改日期", L"大小", L"保存结果", L"搜索目标:", L"添加文件夹",
     L"移除文件夹", L"非", L"代码页:",
     // Config
     L"Search Options", L"Match Mode", L"Substring", L"Exact Match",
     L"Space Divided (All)", L"Regular Expression", L"Ignore Case (Global)",
     L"Size Filter", L"Enable Size Filter", L"Min:", L"Max:", L"Date Filter",
     L"Enable Date Filter", L"From:", L"To:", L"Include", L"Files", L"Folders",
     L"Type Filter", L"Ext:", L"Advanced", L"Match Full Path", L"Exclude:",
     L"OK", L"Cancel",
     // Menu
     L"File", L"Config", L"Language", L"Help", L"Search Options...",
     L"About...", L"Context", L"Copy Path"},
    // APP_LANG_CHINESE_TRAD
    {L"搜尋", L"檔案名稱:", L"就緒", L"搜尋中...", L"找到 %d 個項目", L"名稱",
     L"路徑", L"修改日期", L"大小", L"儲存結果", L"搜尋目標:", L"新增資料夾",
     L"移除資料夾", L"非", L"代碼頁:",
     // Config
     L"Search Options", L"Match Mode", L"Substring", L"Exact Match",
     L"Space Divided (All)", L"Regular Expression", L"Ignore Case (Global)",
     L"Size Filter", L"Enable Size Filter", L"Min:", L"Max:", L"Date Filter",
     L"Enable Date Filter", L"From:", L"To:", L"Include", L"Files", L"Folders",
     L"Type Filter", L"Ext:", L"Advanced", L"Match Full Path", L"Exclude:",
     L"OK", L"Cancel",
     // Menu
     L"File", L"Config", L"Language", L"Help", L"Search Options...",
     L"About...", L"Context", L"Copy Path"},
    // APP_LANG_SPANISH
    {L"Buscar", L"Nombre de archivo:", L"Listo", L"Buscando...",
     L"Encontrado %d elementos", L"Nombre", L"Ruta", L"Fecha de modificación",
     L"Tamaño", L"Guardar resultados", L"Candidatos de búsqueda:",
     L"Añadir carpeta", L"Eliminar", L"No", L"Página de códigos:",
     // Config
     L"Search Options", L"Match Mode", L"Substring", L"Exact Match",
     L"Space Divided (All)", L"Regular Expression", L"Ignore Case (Global)",
     L"Size Filter", L"Enable Size Filter", L"Min:", L"Max:", L"Date Filter",
     L"Enable Date Filter", L"From:", L"To:", L"Include", L"Files", L"Folders",
     L"Type Filter", L"Ext:", L"Advanced", L"Match Full Path", L"Exclude:",
     L"OK", L"Cancel",
     // Menu
     L"File", L"Config", L"Language", L"Help", L"Search Options...",
     L"About...", L"Context", L"Copy Path"},
    // APP_LANG_FRENCH
    {L"Rechercher", L"Nom de fichier:", L"Prêt", L"Recherche en cours...",
     L"Trouvé %d éléments", L"Nom", L"Chemin", L"Date de modification",
     L"Taille", L"Enregistrer les résultats", L"Candidats de recherche:",
     L"Ajouter un dossier", L"Supprimer", L"Non", L"Page de codes:",
     // Config
     L"Search Options", L"Match Mode", L"Substring", L"Exact Match",
     L"Space Divided (All)", L"Regular Expression", L"Ignore Case (Global)",
     L"Size Filter", L"Enable Size Filter", L"Min:", L"Max:", L"Date Filter",
     L"Enable Date Filter", L"From:", L"To:", L"Include", L"Files", L"Folders",
     L"Type Filter", L"Ext:", L"Advanced", L"Match Full Path", L"Exclude:",
     L"OK", L"Cancel",
     // Menu
     L"File", L"Config", L"Language", L"Help", L"Search Options...",
     L"About...", L"Context", L"Copy Path"},
    // APP_LANG_GERMAN
    {L"Suchen", L"Dateiname:", L"Bereit", L"Suche...", L"%d Elemente gefunden",
     L"Name", L"Pfad", L"Änderungsdatum", L"Größe", L"Ergebnisse speichern",
     L"Suchkandidaten:", L"Ordner hinzufügen", L"Entfernen", L"Nicht",
     L"Codepage:",
     // Config
     L"Search Options", L"Match Mode", L"Substring", L"Exact Match",
     L"Space Divided (All)", L"Regular Expression", L"Ignore Case (Global)",
     L"Size Filter", L"Enable Size Filter", L"Min:", L"Max:", L"Date Filter",
     L"Enable Date Filter", L"From:", L"To:", L"Include", L"Files", L"Folders",
     L"Type Filter", L"Ext:", L"Advanced", L"Match Full Path", L"Exclude:",
     L"OK", L"Cancel",
     // Menu
     L"File", L"Config", L"Language", L"Help", L"Search Options...",
     L"About...", L"Context", L"Copy Path"},
    // APP_LANG_PORTUGUESE
    {L"Pesquisar", L"Nome do arquivo:", L"Pronto", L"Pesquisando...",
     L"Encontrado %d itens", L"Nome", L"Caminho", L"Data de modificação",
     L"Tamanho", L"Salvar resultados", L"Candidatos de pesquisa:",
     L"Adicionar pasta", L"Remover", L"Não", L"Página de códigos:",
     // Config
     L"Search Options", L"Match Mode", L"Substring", L"Exact Match",
     L"Space Divided (All)", L"Regular Expression", L"Ignore Case (Global)",
     L"Size Filter", L"Enable Size Filter", L"Min:", L"Max:", L"Date Filter",
     L"Enable Date Filter", L"From:", L"To:", L"Include", L"Files", L"Folders",
     L"Type Filter", L"Ext:", L"Advanced", L"Match Full Path", L"Exclude:",
     L"OK", L"Cancel",
     // Menu
     L"File", L"Config", L"Language", L"Help", L"Search Options...",
     L"About...", L"Context", L"Copy Path"}};

const wchar_t *g_LangNames[APP_LANG_COUNT] = {L"English",
                                              L"Japanese",
                                              L"Chinese (Simplified)",
                                              L"Chinese (Traditional)",
                                              L"Spanish",
                                              L"French",
                                              L"German",
                                              L"Portuguese"};

void Localization::SetLanguage(LanguageID lang) {
  if (lang < 0 || lang >= APP_LANG_COUNT)
    s_currentLang = APP_LANG_ENGLISH;
  else
    s_currentLang = lang;
}

LanguageID Localization::GetLanguage() { return s_currentLang; }

const wchar_t *Localization::GetString(StringID id) {
  if (id < 0 || id >= IDS_STRING_COUNT)
    return L"";
  return g_Strings[s_currentLang][id];
}

const wchar_t *Localization::GetLanguageName(LanguageID lang) {
  if (lang < 0 || lang >= APP_LANG_COUNT)
    return L"";
  return g_LangNames[lang];
}

int Localization::GetLanguageCount() { return APP_LANG_COUNT; }
