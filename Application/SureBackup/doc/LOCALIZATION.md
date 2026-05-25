# Localization System Documentation

## Overview

SureBackup implements a **simple, efficient localization system** that automatically detects the Windows UI language and displays the interface in the appropriate language. Currently supports **5 languages**:
- **English** (Default)
- **Japanese** (日本語)
- **Spanish** (Español)
- **French** (Français)
- **German** (Deutsch)

---

## Architecture

### Design Philosophy
- **Zero external dependencies**: No resource files, DLLs, or XML configs
- **Compile-time safety**: All strings defined in C++ code with type-safe enum access
- **Automatic detection**: Uses Windows API to detect user's system language
- **Single header implementation**: All localization code contained in `Localization.h`

### Key Components

#### 1. String Identifier Enum (`StrId`)
```cpp
enum class StrId {
  Menu_File,      // Menu items
  Ctx_Run,        // Context menu items
  Dlg_UnitName,   // Dialog labels
  Main_Stop,      // Main window controls
  Eng_Std         // Engine names
};
```

**Categories:**
- **Menu_*** - Main menu items (File, Help, etc.)
- **Ctx_*** - Context menu actions (Run, Edit, Delete, etc.)
- **Dlg_*** - Dialog labels, buttons, and prompts
- **Main_*** - Main window UI elements and status messages
- **Eng_*** - Backup engine selection names
- **Err_*** - User-facing error messages
- **Day_*** - Days of the week for scheduling

#### 2. Localization Class
```cpp
class Localization {
public:
  static void Init();                      // Initialize based on system language
  static const wchar_t *Get(StrId id);    // Retrieve localized string
private:
  static std::map<StrId, std::wstring> s_strings;  // String storage
};
```

---

## Language Detection

### Automatic Detection Process
```cpp
LANGID langId = GetUserDefaultUILanguage();
WORD primaryLang = langId & 0xFF;

bool isJapanese = (primaryLang == LANG_JAPANESE);
bool isSpanish = (primaryLang == LANG_SPANISH);
bool isFrench = (primaryLang == LANG_FRENCH);
bool isGerman = (primaryLang == LANG_GERMAN);
```

**Detection Logic:**
1. Calls Windows API `GetUserDefaultUILanguage()`
2. Extracts primary language ID using bitwise AND with 0xFF
3. Compares against Windows language constants (`LANG_JAPANESE`, `LANG_SPANISH`, etc.)
4. Loads the appropriate language map into `s_strings`
5. Falls back to English if the language is not specifically supported

---

## Supported Languages

### English (Default)
- **Windows Language**: All languages except specifically supported ones.
- Clean, professional English labels.
- UPPERCASE used for primary action buttons in the main dashboard.
- Examples: "File", "Backup Preview (Dry Run)", "Incremental Copy (Append)".

### Japanese (日本語)
- **Windows Language**: Japanese (日本語).
- Native Japanese translations following Windows UI conventions.
- Examples: "ファイル", "プレビュー (Dry Run)", "コピーモード (追記)".

### Spanish (Español)
- **Windows Language**: Spanish (all variants).
- Professional translations using standard Spanish computer terminology.
- Examples: "Archivo", "Vista Previa", "Sincronización (Espejo)".

### French (Français)
- **Windows Language**: French (all variants).
- Professional translations with proper grammar and accents.
- Examples: "Fichier", "Aperçu de Sauvegarde", "Synchronisation (Miroir)".

### German (Deutsch)
- **Windows Language**: German (all variants).
- Professional German translations following technical compound noun conventions.
- Examples: "Datei", "Backup-Vorschau", "Synchronisation (Spiegel)".

---

## String Categories Reference

### Menu Items (Menu_*)
| StrId | English | Japanese | Spanish |
|-------|---------|----------|---------|
| Menu_File | File | ファイル | Archivo |
| Menu_NewSet | New Backup Set | 新規バックアップセット | Nuevo Conjunto |
| Menu_Exit | Exit | 終了 | Salir |
| Menu_Run | Run | 実行 | Ejecutar |
| Menu_RunSet | Run Backup Set | バックアップ実行 | Ejecutar Conjunto |
| Menu_Preview | Backup Preview (Dry Run) | プレビュー (Dry Run) | Vista Previa |
| Menu_Help | Help | ヘルプ | Ayuda |
| Menu_About | About SureBackup | SureBackupについて | Acerca de SureBackup |

### Context Menu (Ctx_*)
| StrId | English | Japanese | Spanish |
|-------|---------|----------|---------|
| Ctx_Run | Run | 実行 | Ejecutar |
| Ctx_Preview | Preview | プレビュー | Vista Previa |
| Ctx_EditUnit | Edit Backup Unit | ユニット編集 | Editar unidad |
| Ctx_EditSet | Edit Backup Set | セット編集 | Editar conjunto |
| Ctx_AddUnit | Add Backup Unit | ユニット追加 | Añadir unidad |
| Ctx_Duplicate | Duplicate | 複製 | Duplicar |
| Ctx_Delete | Delete | 削除 | Eliminar |
| Ctx_Refresh | Refresh | 更新 | Refrescar |
| Ctx_SetAsSource | Set as Source | ソースとして設定 | Establecer como Origen |
| Ctx_SetAsTarget | Set as Target | ターゲットとして設定 | Establecer como Destino |

### Engines & Modes (Eng_* / Dlg_Mode_*)
| StrId | English | Japanese | German |
|-------|---------|----------|--------|
| Eng_Std | Standard Engine | 標準エンジン | Standard-Engine |
| Eng_Par | Multithreaded | マルチスレッド | Multithread |
| Eng_Blk | Block Clone | ブロッククローン | Block-Klon |
| Eng_Vss | VSS (Snapshot) | VSS (スナップショット) | VSS (Snapshot) |
| Dlg_Mode_Copy | Incremental Copy | 追記コピー | Inkrementelle Kopie |
| Dlg_Mode_Sync | Sync (Mirror) | 同期 (ミラー) | Synchronisation |

---

## Usage Examples

### Initialization
```cpp
// In WinMain:
Localization::Init();
```

### Retrieving Strings
```cpp
// Menu bar example
AppendMenuW(hMenu, MF_STRING, ID_FILE, Localization::Get(StrId::Menu_File));

// Button text example
SetWindowTextW(hButton, Localization::Get(StrId::Main_Stop));

// Dialog label example
SetDlgItemTextW(hDlg, IDC_LABEL, Localization::Get(StrId::Dlg_Source));
```

---

## Best Practices

### ✅ Do:
- Always use `Localization::Get()` for any user-facing UI string
- Add translations for **all 5 languages** when adding a new string identifier
- Use the `StrId` enum to ensure compile-time validation of string IDs
- Keep the `Localization.h` file organized by category

### ❌ Don't:
- Don't hardcode strings in UI logic (e.g., `SetWindowTextW(h, L"Stop")`)
- Don't modify the `s_strings` map after the `Init()` phase
- Don't forget that `Localization::Get()` returns a `const wchar_t*`

---

## Technical Details

- **Storage**: `std::map<StrId, std::wstring>`
- **Detection**: `GetUserDefaultUILanguage()` (Win32 API)
- **Encoding**: UTF-16 (Unicode/Wide Strings)
- **Performance**: O(log n) lookup, negligible overhead for UI operations
- **Memory**: Static allocation persists for the lifetime of the application

---

## Summary

The SureBackup localization system provides a robust, zero-dependency way to support multiple languages natively on Windows. By strictly following the `StrId` pattern, developers ensure that the application remains professional and accessible to international users.
