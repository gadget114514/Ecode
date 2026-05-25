# Folder Icon Fix - Visual Distinction

## Issue Fixed
Folder icons were not showing a clear visual difference between open and closed states, making it difficult for users to distinguish expanded from collapsed folders in the tree view.

## Solution
Replaced the generic shell folder icons with **Shell32.dll's classic folder icons** which have very distinct visual differences:

### Before (Generic Shell Icons)
- Used `SHGetFileInfoW` with `SHGFI_OPENICON` flag
- Icons were too similar in appearance
- Hard to distinguish open from closed state

### After (Shell32 Classic Icons)
- **Closed Folder**: Shell32 icon resource #4 (classic yellow closed folder)
- **Open Folder**: Shell32 icon resource #5 (classic yellow open folder with visible contents)
- Very distinct visual difference
- Instantly recognizable folder states

## Implementation

### Code Changes (InitImageList function)

#### Closed Folder Icon (Index 1)
```cpp
// Load Shell32 icon #4 (closed folder)
HICON hClosedFolder = (HICON)LoadImageW(
    GetModuleHandleW(L"shell32.dll"), 
    MAKEINTRESOURCEW(4), 
    IMAGE_ICON, 16, 16, 0);

if (hClosedFolder) {
  ImageList_AddIcon(g_hImageList, hClosedFolder);
  DestroyIcon(hClosedFolder);
} else {
  // Fallback to SHGetFileInfo if Shell32 load fails
  SHGetFileInfoW(L"dummy", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                 SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
  ImageList_AddIcon(g_hImageList, sfi.hIcon);
  DestroyIcon(sfi.hIcon);
}
```

#### Open Folder Icon (Index 2)
```cpp
// Load Shell32 icon #5 (open folder)
HICON hOpenFolder = (HICON)LoadImageW(
    GetModuleHandleW(L"shell32.dll"), 
    MAKEINTRESOURCEW(5), 
    IMAGE_ICON, 16, 16, 0);

if (hOpenFolder) {
  ImageList_AddIcon(g_hImageList, hOpenFolder);
  DestroyIcon(hOpenFolder);
} else {
  // Fallback to SHGetFileInfo with OPENICON
  SHGetFileInfoW(L"dummy", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                 SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON |
                     SHGFI_OPENICON);
  ImageList_AddIcon(g_hImageList, sfi.hIcon);
  DestroyIcon(sfi.hIcon);
}
```

## Visual Difference

### Closed Folder Icon (#4)
- 📁 Classic yellow folder
- Folder flap **closed**
- Completely sealed appearance
- Standard "at rest" folder icon

### Open Folder Icon (#5)
- 📂 Classic yellow folder
- Folder flap **open**
- **Contents visible** inside folder
- Clearly indicates expanded state

## How It Works

### Icon Loading Process
1. Attempts to load the classic icon from Shell32.dll using `LoadImageW`
2. If successful, adds icon to image list
3. If Shell32 load fails, falls back to `SHGetFileInfoW` method
4. Ensures icons are always available (robust fallback)

### Icon Switching (Already Implemented)
The `WM_NOTIFY` handler automatically switches icons when folders expand/collapse:

```cpp
if (nm->code == TVN_ITEMEXPANDEDW) {
  if (pnmtv->action == TVE_EXPAND) {
    // Change to OPEN icon (index 2)
    tvi.iImage = IDX_FOLDER_OPEN;
    tvi.iSelectedImage = IDX_FOLDER_OPEN;
    TreeView_SetItem(hTree, &tvi);
  } else if (pnmtv->action == TVE_COLLAPSE) {
    // Change to CLOSED icon (index 1)
    tvi.iImage = IDX_FOLDER_CLOSED;
    tvi.iSelectedImage = IDX_FOLDER_CLOSED;
    TreeView_SetItem(hTree, &tvi);
  }
}
```

## Benefits

✅ **Instantly Recognizable**: Classic Windows folder icons everyone knows  
✅ **Clear Visual Difference**: Open folder shows visible contents  
✅ **Professional Appearance**: Uses standard Windows UI conventions  
✅ **Fallback Support**: Gracefully handles Shell32 load failures  
✅ **Consistent**: Works across all Windows versions  

## Shell32 Icon Resources Used

| Resource ID | Icon Type | Description |
|-------------|-----------|-------------|
| #4 | Closed Folder | Yellow folder, closed flap |
| #5 | Open Folder | Yellow folder, open flap with contents |

These are the classic Windows icons used throughout Windows Explorer and have been stable since Windows 95.

## Testing

### Verify Icon Switching
1. Run the application
2. Navigate to Source or Destination tree view
3. Click to expand a folder
   - **Expected**: Icon changes from closed (📁) to open (📂)
4. Click to collapse the folder
   - **Expected**: Icon changes back from open (📂) to closed (📁)

### Visual Check
- **Closed folders**: Should show sealed yellow folder
- **Expanded folders**: Should show open yellow folder with visible paper/contents

## Technical Notes

- **Icon Size**: 16×16 pixels (standard small icon size)
- **Color Depth**: ILC_COLOR32 (32-bit with alpha channel)
- **Module Handle**: Uses `GetModuleHandleW(L"shell32.dll")` - no need to load/free
- **Resource IDs**: Using `MAKEINTRESOURCEW()` for proper resource ID casting
- **Memory Management**: Icons properly destroyed with `DestroyIcon()` after adding to image list

## Build Status
✅ **Compiled Successfully**  
✅ **No New Warnings**  
✅ **Executable**: `D:\ws\SureBackup\build\Release\SureBackup.exe`

---

## Summary

The folder icon visibility issue has been resolved by using Shell32.dll's classic, highly distinct folder icons. Users can now clearly see which folders are expanded (open folder icon) versus collapsed (closed folder icon) in the tree view panels.
