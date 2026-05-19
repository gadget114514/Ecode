# FastFD — Multi-Pane Multi-Column Directory Manipulator Design

## 1. Overview

FastFD is a native Win32 / Direct2D file manager.  
Two headline features drive the design:

- **Multiple independent panes** — 1 to 4 directory panels, each browsing a different path.
- **Multiple configurable columns per pane** — each pane shows a Detail-view style list with N
  user-chosen, resizable, reorderable columns (Name, Size, Type, Date, Attr, …).

Reference inspiration: MS-DOS **FD** (Idei Atsushi) and **WinFD** — see `designdoc/dirfd.md`.

---

## 2. Screen Layout

```
┌──────────────────────────────────────────────────────────────────┐
│ Pane 0 header (path)  │ Pane 1 header (path)  │ Pane 2 header   │  ← pathH per pane
├────────────┬──────────┼────────────┬──────────┼─────────────────┤
│ Name  Size │Type Date │ Name  Size │Type Date │ Name  Date  Attr │  ← column headers
├────────────┼──────────┼────────────┼──────────┼─────────────────┤
│ file rows  │          │ file rows  │          │ file rows        │
│   …        │          │   …        │          │   …              │  ← scrollable list
│            │          │            │          │                  │
├──────────────────────────────────────────────────────────────────┤
│ STATUS LINE  [active path]  [N marked, X bytes]  [Scanning…]    │  ← statusH = 20px
├──────────────────────────────────────────────────────────────────┤
│ F1:Help F2:Ren F5:Copy F6:Move F7:MkDir F8:Del F9:Refresh F0:Quit│  ← fkeyH = 20px
└──────────────────────────────────────────────────────────────────┘
```

Panes share the horizontal space (or vertical, depending on layout mode).  
The status line and function-key bar span the full window width.

---

## 3. Pane Model

### 3.1 Data Structures

```cpp
// ── Column definition ─────────────────────────────────────────
enum class ColType { Name, Ext, Size, SizeOnDisk, Date, Time, Attr, Type };

struct Column {
    ColType type;
    float   width;      // current pixel width (user-resizable)
    bool    visible;    // can be hidden
};

// ── Per-pane state ────────────────────────────────────────────
struct Pane {
    std::wstring           currentPath;
    std::vector<FileEntry> entries;
    std::vector<bool>      marked;       // Space-key per entry
    std::vector<Column>    columns;      // ordered, N columns
    int                    selectedIndex = 0;
    int                    scrollOffset  = 0;
    int                    visibleRows   = 0;
    // column resize drag state
    int                    draggingCol   = -1;
    float                  dragStartX    = 0.f;
    // sort
    ColType                sortCol       = ColType::Name;
    bool                   sortAsc       = true;
    // scan
    HANDLE                 scanThread    = nullptr;
    CRITICAL_SECTION       cs;
    bool                   scanning      = false;
    void InitCS() { InitializeCriticalSection(&cs); }
    void DoneCS() { DeleteCriticalSection(&cs); }
};
```

### 3.2 Global State

```cpp
// replace fixed g_panes[2] and g_dividerPos
static std::vector<Pane>  g_panes;
static std::vector<float> g_dividerFractions; // proportional (0..1), len = panes-1
static int                g_activePane = 0;

enum class LayoutMode { Horizontal, Vertical, Grid2x2 };
static LayoutMode         g_layoutMode = LayoutMode::Horizontal;

// drag
static int  g_draggingPaneDivider = -1;  // index into g_dividerFractions, -1 = none
static bool g_draggingColHeader   = false;
```

### 3.3 Pane Count & Layout Modes

| Mode | Max panes | Dividers |
|------|-----------|----------|
| Horizontal | 1–4 | vertical lines |
| Vertical   | 1–4 | horizontal lines |
| Grid 2×2   | 4   | one H + one V |

`Ctrl+P` adds a pane (cycles 1→2→3→4→1).  
`Ctrl+W` closes the active pane.  
`Alt+H` / `Alt+V` / `Alt+G` switch layout mode.

---

## 4. Column System

### 4.1 Default Column Set (per new pane)

| # | Type | Default width | Align |
|---|------|--------------|-------|
| 0 | Name | 200 px | left |
| 1 | Ext  |  48 px | left |
| 2 | Size |  80 px | right |
| 3 | Date | 100 px | left |
| 4 | Time |  60 px | left |
| 5 | Attr |  48 px | left |

### 4.2 Column Interactions

| Action | Trigger |
|--------|---------|
| Resize column | Drag column separator in header |
| Sort by column | Click column header |
| Reverse sort | Click same header again |
| Show/hide columns | Right-click column header → context menu |
| Reorder columns | (future: drag column header) |

### 4.3 Rendering Column Headers

```
│ Name ▲           │ Ext  │     Size │ Date       │ Time  │ Attr │
```

- Sort indicator (`▲`/`▼`) on the active sort column.
- Column separators are draggable (hit area ±3 px).
- Header background distinct from file rows.

---

## 5. File List Rendering

Each pane's file list area:

```
listY  = paneY + pathH + headerH
listH  = paneH - pathH - headerH
visibleRows = (int)(listH / rowH)
```

Per row:
- Selection bar: full-width fill behind the row.
- Mark indicator: `*` in a 12px prefix column before Name.
- Each column clipped to its `width` with ellipsis.

### 5.1 Color Coding

| Entry type | Foreground |
|------------|-----------|
| Directory | `#00BFFF` (cyan) |
| Executable (.exe .com .bat .cmd) | `#00C800` (green) |
| System / hidden | `#FF40FF` (magenta) |
| Marked (`*`) | `#FFD700` (yellow) |
| Normal file | `#F0F0F0` (near-white) |
| Selected row bg | `#000080` (navy) |
| Background | `#1A1A1A` (near-black) |

---

## 6. Mark Feature

- `Space` — toggle `*` mark on current entry, advance cursor.
- `+` — mark by glob pattern (status-bar mini-prompt).
- `-` — unmark by glob pattern.
- `*` — invert all marks.
- Status bar shows: `3 marked (1.23 MB)` when marks exist.
- File operations use the marked set when non-empty, else cursor entry.

---

## 7. Keyboard Shortcuts

| Key | Action |
|-----|--------|
| ↑ ↓ | Move cursor |
| PgUp / PgDn | Page scroll |
| Home / End | First / last entry |
| Enter | Open file (ShellExecute) / enter directory |
| Backspace | Go to parent |
| Tab | Cycle active pane forward |
| Shift+Tab | Cycle active pane backward |
| Space | Toggle mark |
| F1 | Help |
| F2 | Rename (inline in status bar) |
| F4 | Directory tree view overlay |
| F5 | Copy marked/cursor → other pane path |
| F6 | Move marked/cursor → other pane path |
| F7 | New directory (prompt in status bar) |
| F8 | Delete with confirmation |
| F9 | Refresh (re-scan current pane) |
| F10 | Quit |
| Ctrl+P | Add pane |
| Ctrl+W | Close active pane |
| Ctrl+H | Toggle show hidden files |
| Ctrl+S | Sort menu |
| Alt+H | Horizontal layout |
| Alt+V | Vertical layout |
| Alt+G | Grid 2×2 layout |

---

## 8. File Operations

### Copy (F5) / Move (F6)
- Destination defaults to the path of pane `(g_activePane + 1) % g_panes.size()`.
- If only 1 pane exists, prompt for destination path.
- Confirmation: `Copy 3 files to D:\Backup? [Y/N]`
- Progress shown in status bar during operation.
- Uses `CopyFileW` / `MoveFileW`; for Recycle Bin use `SHFileOperationW`.

### Delete (F8)
- Single: `Delete "file.txt"? [Y/N]`
- Multi:  `Delete 3 files? [Y/N]`
- Sends to Recycle Bin via `SHFileOperationW` with `FOF_ALLOWUNDO`.

### Rename (F2)
- Status bar becomes edit prompt: `Rename: [old name___________]`
- Uses a hidden child `EDIT` control positioned over status bar.
- Commits on Enter, cancels on Escape.
- Calls `MoveFileW(oldPath, newPath)`.

### New Directory (F7)
- Status bar prompt: `New folder: [_________________]`
- `CreateDirectoryW`.

---

## 9. Status Bar

Single 20px strip above the function-key bar:

```
 C:\Users\you\Documents    |  3 marked (1.23 MB)  |  47 files  |  Scanning…
```

Sections: active-pane path | mark summary | file count | scan status.

---

## 10. Function Key Bar

Fixed 20px strip at the bottom, always visible:

```
 1Help  2Ren  3View  4Tree  5Copy  6Move  7MkDir  8Del  9Refresh  0Quit
```

Each segment: number rendered in highlight color, label in normal color.

---

## 11. Structural Changes to `Application/FastFD/src/main.cpp`

### Globals to replace

| Remove | Replace with |
|--------|-------------|
| `static Pane g_panes[2]` | `static std::vector<Pane> g_panes` |
| `static int g_dividerPos` | `static std::vector<float> g_dividerFractions` |
| `static bool g_draggingDivider` | `static int g_draggingPaneDivider = -1` |
| `static const float g_colW[]` | `Pane::columns` (per-pane) |
| `static const int g_colCount` | `pane.columns.size()` |

### New brushes (add to `EnsureRT` / `DiscardRT`)

```cpp
ID2D1SolidColorBrush* g_brushDir;      // #00BFFF
ID2D1SolidColorBrush* g_brushExe;      // #00C800
ID2D1SolidColorBrush* g_brushSys;      // #FF40FF
ID2D1SolidColorBrush* g_brushMarked;   // #FFD700
ID2D1SolidColorBrush* g_brushColHdr;   // column header bg
ID2D1SolidColorBrush* g_brushStatus;   // status bar bg
ID2D1SolidColorBrush* g_brushFKey;     // function key bar bg
ID2D1SolidColorBrush* g_brushFKeyNum;  // function key number highlight
```

### New/changed functions

```cpp
// Layout
std::vector<D2D1_RECT_F> CalculatePaneRects();   // replaces inline divider math in Render()
void InitPane(Pane& p, const std::wstring& path); // sets default columns, navigates

// Render
void DrawPane(Pane& pane, D2D1_RECT_F rect);      // existing, updated
void DrawColumnHeaders(Pane& pane, float x, float y, float w);
void DrawStatusBar(float y, float w);
void DrawFunctionKeyBar(float y, float w);

// Input
void OnPaneDividerDrag(int x);
void OnColumnHeaderDrag(Pane& pane, int x, int y);
void OnColumnHeaderClick(Pane& pane, int colIndex);

// File ops
void DoCopy(bool move = false);
void DoDelete();
void DoRename();
void DoMkDir();

// Helpers
D2D1_COLOR_F EntryColor(const FileEntry& e, bool marked);
std::wstring FormatSize(int64_t bytes);
std::wstring FormatDate(FILETIME ft);
std::wstring FormatAttr(DWORD attr);
```

---

## 12. Verification Checklist

1. Build: `cmake --build build --config Release --target FastFD`
2. Launch: `FastFD.exe C:\Windows C:\Temp`
3. **Multi-pane**
   - Default: 2 panes side by side, each browsing its own path.
   - `Ctrl+P` adds a third pane; layout recalculates evenly.
   - `Tab` moves focus; active pane path header is highlighted.
4. **Multi-column**
   - Each pane shows Name, Ext, Size, Date, Time, Attr columns.
   - Dragging a column separator resizes that column live.
   - Clicking a column header sorts; clicking again reverses.
   - Right-click header → context menu to hide/show columns.
5. **Colors**: directories cyan, .exe green, hidden/system magenta.
6. **Mark**: `Space` marks a file (`*`), status bar shows count+size.
7. **F5 Copy**: copies marked files to next pane's path.
8. **F7 / F8**: new folder / delete with confirmation prompt.
9. **F-key bar**: always visible at bottom with correct labels.
10. **Resize window**: all panes and columns reflow correctly.
