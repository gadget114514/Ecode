#pragma once

#include <string>
#include <tchar.h>

enum class CsvLanguage {
    English,
    Japanese
};

enum class CsvStringId {
    // Menu: File
    Menu_File,
    Menu_File_Open,
    Menu_File_Reopen,
    Menu_File_RecentFiles,
    Menu_File_RecentFiles_Clear,
    Menu_File_Import,
    Menu_File_Save,
    Menu_File_SaveAs,
    Menu_File_Export_HTML,
    Menu_File_Export_MD,
    Menu_File_Exit,

    // Menu: Edit
    Menu_Edit,
    Menu_Edit_Undo,
    Menu_Edit_Redo,
    Menu_Edit_Cut,
    Menu_Edit_Copy,
    Menu_Edit_Paste,
    Menu_Edit_Delete,
    Menu_Edit_InsertRow,
    Menu_Edit_InsertCol,
    Menu_Edit_InsertRowUp,
    Menu_Edit_InsertRowDown,
    Menu_Edit_InsertColLeft,
    Menu_Edit_InsertColRight,
    Menu_Edit_Clear,

    // Sort
    Menu_Edit_Sort,
    Menu_Edit_SortRowsAsc,
    Menu_Edit_SortRowsDesc,
    Menu_Edit_SortColsAsc,
    Menu_Edit_SortColsDesc,

    // Menu: Search
    Menu_Search,
    Menu_Search_Find,
    Menu_Search_FindNext,
    Menu_Search_FindPrev,
    Menu_Search_Replace,

    // Menu: Config
    Menu_Config,
    Menu_Config_Delimiter,
    Menu_Config_Delim_Comma,
    Menu_Config_Delim_Tab,
    Menu_Config_Delim_Semi,
    Menu_Config_Delim_Pipe,
    Menu_Config_Encoding,
    Menu_Config_Enc_UTF8,
    Menu_Config_Enc_ANSI,
    Menu_Config_Newline,
    Menu_Config_NL_CRLF,
    Menu_Config_NL_LF,
    Menu_Config_Font,
    Menu_Config_Folding,
    Menu_Config_Language, // New
    Menu_Config_Lang_English,
    Menu_Config_Lang_Japanese,

    // Menu: Tools
    Menu_Tools,

    // Menu: Help
    Menu_Help,
    Menu_Help_View,
    Menu_Help_About,

    // Messages
    Msg_Error,
    Msg_Info,
    Msg_About,
    Msg_AppTitle,
    Msg_OpenFailed,
    Msg_SaveSuccess,
    Msg_SaveFailed,

    // File Dialog
    File_Filter_CSV,
    File_Filter_TSV,
    File_Filter_All
};

class CsvLocalization {
public:
    static void SetLanguage(CsvLanguage lang);
    static CsvLanguage GetLanguage();
    static const TCHAR* GetString(CsvStringId id);

private:
    static CsvLanguage s_currentLanguage;
};
