#include "Localization.h"
#include <map>

CsvLanguage CsvLocalization::s_currentLanguage = CsvLanguage::English;

// Using \u escapes to avoid code page issues
static std::map<CsvStringId, std::wstring> s_stringsJP = {
    { CsvStringId::Menu_File, L"\u30D5\u30A1\u30A4\u30EB(&F)" }, // File
    { CsvStringId::Menu_File_Open, L"\u958B\u304F(&O)...\tCtrl+O" }, // Open
    { CsvStringId::Menu_File_Reopen, L"\u518D\u8AAD\u307F\u8FBC\u307F(&R)" }, // Reopen
    { CsvStringId::Menu_File_RecentFiles, L"\u6700\u8FD1\u4F7F\u3063\u305F\u30D5\u30A1\u30A4\u30EB" }, // Recent Files
    { CsvStringId::Menu_File_RecentFiles_Clear, L"\u30EA\u30B9\u30C8\u3092\u30AF\u30EA\u30A2" }, // Clear List
    { CsvStringId::Menu_File_Import, L"\u30A4\u30F3\u30DD\u30FC\u30C8(\u8FFD\u52A0)(&I)..." }, // Import (Append)...
    { CsvStringId::Menu_File_Save, L"\u4FDD\u5B58(&S)\tCtrl+S" }, // Save
    { CsvStringId::Menu_File_SaveAs, L"\u540D\u524D\u3092\u4ED8\u3051\u3066\u4FDD\u5B58(&A)..." }, // Save As
    { CsvStringId::Menu_File_Export_HTML, L"HTML \u306B\u30A8\u30AF\u30B9\u30DD\u30FC\u30C8" }, // Export to HTML
    { CsvStringId::Menu_File_Export_MD, L"Markdown \u306B\u30A8\u30AF\u30B9\u30DD\u30FC\u30C8" }, // Export to Markdown
    { CsvStringId::Menu_File_Exit, L"\u7D42\u4E86(&X)\tAlt+F4" }, // Exit

    { CsvStringId::Menu_Edit, L"\u7DE8\u96C6(&E)" }, // Edit
    { CsvStringId::Menu_Edit_Undo, L"\u5143\u306B\u623B\u3059(&U)\tCtrl+Z" }, // Undo
    { CsvStringId::Menu_Edit_Redo, L"\u3084\u308A\u76F4\u3057(&R)\tCtrl+Y" }, // Redo
    { CsvStringId::Menu_Edit_Cut, L"\u5207\u308A\u53D6\u308A(&T)\tCtrl+X" }, // Cut
    { CsvStringId::Menu_Edit_Copy, L"\u30B3\u30D4\u30FC(&C)\tCtrl+C" }, // Copy
    { CsvStringId::Menu_Edit_Paste, L"\u88BC\u308A\u4ED8\u3051(&P)\tCtrl+V" }, // Paste
    { CsvStringId::Menu_Edit_Delete, L"\u524A\u9664(&D)\tDel" }, // Delete
    { CsvStringId::Menu_Edit_InsertRow, L"\u884C\u3092\u633F\u5165(&R)" }, // Insert Row
    { CsvStringId::Menu_Edit_InsertCol, L"\u5217\u3092\u633F\u5165(&C)" }, // Insert Column
    { CsvStringId::Menu_Edit_InsertRowUp, L"\u4E0A\u306B\u884C\u3092\u633F\u5165" }, // Insert Row Above
    { CsvStringId::Menu_Edit_InsertRowDown, L"\u4E0B\u306B\u884C\u3092\u633F\u5165" }, // Insert Row Below
    { CsvStringId::Menu_Edit_InsertColLeft, L"\u5DE6\u306B\u5217\u3092\u633F\u5165" }, // Insert Column Left
    { CsvStringId::Menu_Edit_InsertColRight, L"\u53F3\u306B\u5217\u3092\u633F\u5165" }, // Insert Column Right
    { CsvStringId::Menu_Edit_Clear, L"\u5185\u5BB9\u3092\u30AF\u30EA\u30A2" }, // Clear

    { CsvStringId::Menu_Edit_Sort, L"\u30BD\u30FC\u30C8" }, // Sort
    { CsvStringId::Menu_Edit_SortRowsAsc, L"\u884C\u3092\u6607\u9806\u30BD\u30FC\u30C8 (A\u2192Z)" }, // Sort Rows Ascending
    { CsvStringId::Menu_Edit_SortRowsDesc, L"\u884C\u3092\u964D\u9806\u30BD\u30FC\u30C8 (Z\u2192A)" }, // Sort Rows Descending
    { CsvStringId::Menu_Edit_SortColsAsc, L"\u5217\u3092\u6607\u9806\u30BD\u30FC\u30C8 (A\u2192Z)" }, // Sort Cols Ascending
    { CsvStringId::Menu_Edit_SortColsDesc, L"\u5217\u3092\u964D\u9806\u30BD\u30FC\u30C8 (Z\u2192A)" }, // Sort Cols Descending

    { CsvStringId::Menu_Search, L"\u691C\u7D22(&S)" }, // Search
    { CsvStringId::Menu_Search_Find, L"\u691C\u7D22(&F)...\tCtrl+F" }, // Find
    { CsvStringId::Menu_Search_FindNext, L"\u6B21\u3092\u691C\u7D22(&N)\tF3" }, // Find Next
    { CsvStringId::Menu_Search_FindPrev, L"\u524D\u3092\u691C\u7D22(&P)\tShift+F3" }, // Find Prev
    { CsvStringId::Menu_Search_Replace, L"\u7F6E\u63DB(&R)...\tCtrl+H" }, // Replace

    { CsvStringId::Menu_Config, L"\u8A2D\u5B9A(&C)" }, // Config
    { CsvStringId::Menu_Config_Delimiter, L"\u533A\u5207\u308A\u6587\u5B57(&D)" }, // Delimiter
    { CsvStringId::Menu_Config_Delim_Comma, L"\u30AB\u30F3\u30DE (,)" }, // Comma
    { CsvStringId::Menu_Config_Delim_Tab, L"\u30BF\u30D6 (\\t)" }, // Tab
    { CsvStringId::Menu_Config_Delim_Semi, L"\u30BB\u30DF\u30B3\u30ED\u30F3 (;)" }, // Semi
    { CsvStringId::Menu_Config_Delim_Pipe, L"\u30D1\u30A4\u30D7 (|)" }, // Pipe
    { CsvStringId::Menu_Config_Encoding, L"\u6587\u5B57\u30B3\u30FC\u30C9(&E)" }, // Encoding
    { CsvStringId::Menu_Config_Enc_UTF8, L"UTF-8" },
    { CsvStringId::Menu_Config_Enc_ANSI, L"ANSI" },
    { CsvStringId::Menu_Config_Newline, L"\u6539\u884C\u30B3\u30FC\u30C9(&N)" }, // Newline
    { CsvStringId::Menu_Config_NL_CRLF, L"CRLF (Windows)" },
    { CsvStringId::Menu_Config_NL_LF, L"LF (Unix)" },
    { CsvStringId::Menu_Config_Font, L"\u30D5\u30A9\u30F3\u30C8(&F)..." }, // Font
    { CsvStringId::Menu_Config_Language, L"\u8A00\u8A9E(&L)" }, // Language
    { CsvStringId::Menu_Config_Lang_English, L"English" },
    { CsvStringId::Menu_Config_Lang_Japanese, L"\u65E5\u672C\u8A9E" }, // Japanese

    { CsvStringId::Menu_Tools, L"\u30C4\u30FC\u30EB(&T)" }, // Tools

    { CsvStringId::Menu_Help, L"\u30D8\u30EB\u30D7(&H)" }, // Help
    { CsvStringId::Menu_Help_View, L"\u30D8\u30EB\u30D7\u306E\u8868\u793A(&V)" }, // View Help
    { CsvStringId::Menu_Help_About, L"\u30D0\u30FC\u30B8\u30E7\u30F3\u60C5\u5831(&A)" }, // About

    { CsvStringId::Msg_Error, L"\u30A8\u30E9\u30FC" }, // Error
    { CsvStringId::Msg_Info, L"\u60C5\u5831" }, // Info
    { CsvStringId::Msg_About, L"\u30D0\u30FC\u30B8\u30E7\u30F3\u60C5\u5831" }, // About
    { CsvStringId::Msg_AppTitle, L"CSV Editor v0.1" },
    { CsvStringId::Msg_OpenFailed, L"\u30D5\u30A1\u30A4\u30EB\u3092\u958B\u3051\u307E\u305B\u3093\u3067\u3057\u305F\u3002" }, // Failed to open
    { CsvStringId::Msg_SaveSuccess, L"\u4FDD\u5B58\u3057\u307E\u3057\u305F\u3002" }, // Saved
    { CsvStringId::Msg_SaveFailed, L"\u4FDD\u5B58\u306B\u5931\u6557\u3057\u307E\u3057\u305F\u3002" }, // Save failed

    { CsvStringId::File_Filter_CSV, L"CSV \u30D5\u30A1\u30A4\u30EB" }, // CSV Files
    { CsvStringId::File_Filter_TSV, L"TSV \u30D5\u30A1\u30A4\u30EB" }, // TSV Files
    { CsvStringId::File_Filter_All, L"\u3059\u3079\u3066\u306E\u30D5\u30A1\u30A4\u30EB" } // All Files
};

static std::map<CsvStringId, std::wstring> s_stringsEN = {
    { CsvStringId::Menu_File, L"&File" },
    { CsvStringId::Menu_File_Open, L"&Open...\tCtrl+O" },
    { CsvStringId::Menu_File_Reopen, L"Re&open" },
    { CsvStringId::Menu_File_RecentFiles, L"Recent Files" },
    { CsvStringId::Menu_File_RecentFiles_Clear, L"Clear Recent List" },
    { CsvStringId::Menu_File_Import, L"&Import (Append)..." },
    { CsvStringId::Menu_File_Save, L"&Save\tCtrl+S" },
    { CsvStringId::Menu_File_SaveAs, L"Save &As..." },
    { CsvStringId::Menu_File_Export_HTML, L"Export to HTML..." },
    { CsvStringId::Menu_File_Export_MD, L"Export to Markdown..." },
    { CsvStringId::Menu_File_Exit, L"E&xit\tAlt+F4" },

    { CsvStringId::Menu_Edit, L"&Edit" },
    { CsvStringId::Menu_Edit_Undo, L"&Undo\tCtrl+Z" },
    { CsvStringId::Menu_Edit_Redo, L"&Redo\tCtrl+Y" },
    { CsvStringId::Menu_Edit_Cut, L"Cu&t\tCtrl+X" },
    { CsvStringId::Menu_Edit_Copy, L"&Copy\tCtrl+C" },
    { CsvStringId::Menu_Edit_Paste, L"&Paste\tCtrl+V" },
    { CsvStringId::Menu_Edit_Delete, L"&Delete\tDel" },
    { CsvStringId::Menu_Edit_InsertRow, L"Insert &Row" },
    { CsvStringId::Menu_Edit_InsertCol, L"Insert &Column" },
    { CsvStringId::Menu_Edit_InsertRowUp, L"Insert Row Above" },
    { CsvStringId::Menu_Edit_InsertRowDown, L"Insert Row Below" },
    { CsvStringId::Menu_Edit_InsertColLeft, L"Insert Column Left" },
    { CsvStringId::Menu_Edit_InsertColRight, L"Insert Column Right" },
    { CsvStringId::Menu_Config_Folding, L"Toggle Text Wrapping" },
    { CsvStringId::Menu_Edit_Clear, L"Cle&ar" },

    { CsvStringId::Menu_Edit_Sort, L"Sort" },
    { CsvStringId::Menu_Edit_SortRowsAsc, L"Sort Rows Ascending (A\u2192Z)" },
    { CsvStringId::Menu_Edit_SortRowsDesc, L"Sort Rows Descending (Z\u2192A)" },
    { CsvStringId::Menu_Edit_SortColsAsc, L"Sort Columns Ascending (A\u2192Z)" },
    { CsvStringId::Menu_Edit_SortColsDesc, L"Sort Columns Descending (Z\u2192A)" },

    { CsvStringId::Menu_Search, L"&Search" },
    { CsvStringId::Menu_Search_Find, L"&Find...\tCtrl+F" },
    { CsvStringId::Menu_Search_FindNext, L"Find &Next\tF3" },
    { CsvStringId::Menu_Search_FindPrev, L"Find &Previous\tShift+F3" },
    { CsvStringId::Menu_Search_Replace, L"&Replace...\tCtrl+H" },

    { CsvStringId::Menu_Config, L"&Config" },
    { CsvStringId::Menu_Config_Delimiter, L"&Delimiter" },
    { CsvStringId::Menu_Config_Delim_Comma, L"Comma (,)" },
    { CsvStringId::Menu_Config_Delim_Tab, L"Tab (\\t)" },
    { CsvStringId::Menu_Config_Delim_Semi, L"Semicolon (;)" },
    { CsvStringId::Menu_Config_Delim_Pipe, L"Pipe (|)" },
    { CsvStringId::Menu_Config_Encoding, L"&Encoding" },
    { CsvStringId::Menu_Config_Enc_UTF8, L"UTF-8" },
    { CsvStringId::Menu_Config_Enc_ANSI, L"ANSI" },
    { CsvStringId::Menu_Config_Newline, L"&Newline" },
    { CsvStringId::Menu_Config_NL_CRLF, L"CRLF (Windows)" },
    { CsvStringId::Menu_Config_NL_LF, L"LF (Unix)" },
    { CsvStringId::Menu_Config_Font, L"&Font..." },
    { CsvStringId::Menu_Config_Language, L"&Language" },
    { CsvStringId::Menu_Config_Lang_English, L"English" },
    { CsvStringId::Menu_Config_Lang_Japanese, L"\u65E5\u672C\u8A9E" }, // Japanese

    { CsvStringId::Menu_Tools, L"&Tools" },

    { CsvStringId::Menu_Help, L"&Help" },
    { CsvStringId::Menu_Help_View, L"&View Help" },
    { CsvStringId::Menu_Help_About, L"&About" },

    { CsvStringId::Msg_Error, L"Error" },
    { CsvStringId::Msg_Info, L"Info" },
    { CsvStringId::Msg_About, L"About" },
    { CsvStringId::Msg_AppTitle, L"CSV Editor v0.1" },
    { CsvStringId::Msg_OpenFailed, L"Failed to open file." },
    { CsvStringId::Msg_SaveSuccess, L"File saved." },
    { CsvStringId::Msg_SaveFailed, L"Failed to save file." },

    { CsvStringId::File_Filter_CSV, L"CSV Files" },
    { CsvStringId::File_Filter_TSV, L"TSV Files" },
    { CsvStringId::File_Filter_All, L"All Files" }
};

void CsvLocalization::SetLanguage(CsvLanguage lang)
{
    s_currentLanguage = lang;
}

CsvLanguage CsvLocalization::GetLanguage()
{
    return s_currentLanguage;
}

const TCHAR* CsvLocalization::GetString(CsvStringId id)
{
    if (s_currentLanguage == CsvLanguage::Japanese) {
        if (s_stringsJP.count(id)) return s_stringsJP[id].c_str();
    }
    // Default to English
    if (s_stringsEN.count(id)) return s_stringsEN[id].c_str();
    return _T("?");
}
