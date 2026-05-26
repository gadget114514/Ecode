# Worker Progress Bars - Smart Visibility

## ✅ Implementation Complete

Worker progress bars now intelligently show/hide based on window mode for optimal space usage.

---

## How It Works

### 📊 Maximized Mode (Full Details)
**When user clicks "MAXIMIZE LOG":**
- ✅ Worker progress bars **VISIBLE**
- ✅ 4 compact progress bars shown (one per thread)
- ✅ Each shows: `[Worker Label] [Progress Bar] [X Button]`
- Layout: 22px per row = 88px total for 4 workers
- Log text positioned below worker progress

**Layout:**
```
┌─────────────────────────────────────────┐
│ Main Progress: [████████░░] 80%         │
├─────────────────────────────────────────┤
│ Worker 1: file.txt    [███░░] [X]       │ ← 18px
│ Worker 2: photo.jpg   [█████] [X]       │ ← 18px
│ Worker 3: doc.pdf     [██░░░] [X]       │ ← 18px
│ Worker 4: Idle        [     ] [X]       │ ← 18px
├─────────────────────────────────────────┤
│ Log Text Window                          │
│ (Full details of all operations)        │
│                                          │
└─────────────────────────────────────────┘
```

### 📝 Normal Mode (Space Optimized)
**When window is in regular view:**
- ❌ Worker progress bars **HIDDEN**
- ✅ Log text gets maximum space
- Still tracks progress internally (just not displayed)

**Layout:**
```
┌────────┬───────────┬───────────┐
│Backup  │ Source    │ Target    │
│Sets/   │ Tree      │ Tree      │
│Units   │ View      │ View      │
└────────┴───────────┴───────────┘
┌─────────────────────────────────┐
│ Main Progress: [████] 50%       │
├─────────────────────────────────┤
│ Log Text Window                 │
│ (Maximum vertical space)        │
│                                 │
│                                 │
│                                 │
└─────────────────────────────────┘
```

---

## Code Implementation

### Control Creation (Lines 1365-1384)
```cpp
for (int i = 0; i < MAX_WORKERS; ++i) {
  // Worker status label
  hWorkerLabels[i] = CreateWindowW(L"STATIC", L"Worker Idle", ...);
  
  // Compact progress bar (18px height)
  hWorkerProgs[i] = CreateWindowW(PROGRESS_CLASSW, NULL, ...);
  
  // Stop button (compact "X")
  hWorkerBtns[i] = CreateWindowW(L"BUTTON", L"X", ...);
}
```

### Maximized Mode Layout (Lines 1622-1637)
```cpp
// SHOW worker controls
for (int i = 0; i < MAX_WORKERS; ++i) {
  ShowWindow(hWorkerLabels[i], SW_SHOW);
  ShowWindow(hWorkerProgs[i], SW_SHOW);
  ShowWindow(hWorkerBtns[i], SW_SHOW);
  
  // Position compactly (22px per row)
  int rowY = 95 + (i * 22);
  MoveWindow(hWorkerLabels[i], 20, rowY, 250, 18, TRUE);
  MoveWindow(hWorkerProgs[i], 275, rowY, W - 355, 18, TRUE);
  MoveWindow(hWorkerBtns[i], W - 75, rowY, 55, 18, TRUE);
}

// Log below workers
int logStartY = 95 + (MAX_WORKERS * 22) + 5;
MoveWindow(hLogEdit, 20, logStartY, W - 40, H - logStartY - 15, TRUE);
```

### Normal Mode Layout (Lines 1657-1665)
```cpp
// HIDE worker controls (save space)
for (int i = 0; i < MAX_WORKERS; ++i) {
  ShowWindow(hWorkerLabels[i], SW_HIDE);
  ShowWindow(hWorkerProgs[i], SW_HIDE);
  ShowWindow(hWorkerBtns[i], SW_HIDE);
}

// Log gets full space
MoveWindow(hLogEdit, 20, 100 + listH, W - 40, logH - 45, TRUE);
```

---

## Benefits

### ✅ Space Efficiency
- **Normal mode**: Maximum log space (no clutter)
- **Maximized mode**: Full thread monitoring

### ✅ User Experience
- Automatic - no manual configuration needed
- Shows detailed progress when space available
- Hides complexity when space limited

### ✅ Parallel Backup Monitoring
When using parallel backup mode:
- See all 4 threads simultaneously
- Track individual file progress
- Cancel specific workers with "X" button
- Monitor thread utilization

### ✅ Clean Design
- Compact 18px rows (vs previous 30px)
- Aligned labels, progress bars, buttons
- Professional appearance
- No wasted space

---

## Technical Details

### Control Dimensions
- **Label**: 250px wide, 18px tall
- **Progress Bar**: Dynamic width (W - 355px), 18px tall
- **Button**: 55px wide ("X"), 18px tall
- **Row Height**: 22px (18px control + 4px spacing)

### Position Calculation
**Maximized:**
- Worker Start Y: 95 (below main progress)
- Each Row: 95 + (workerIndex * 22)
- Log Start: 95 + (4 * 22) + 5 = 188

**Normal:**
- Workers hidden (no space calculation)
- Log Start: 100 + listH (full available space)

### WindowLogger Integration
- Still receives `OnWorkerProgress()` calls in both modes
- Controls exist but are hidden in normal mode
- No null checks needed - controls always exist

---

## Build Status: ✅ SUCCESS

**Executable:** `D:\ws\SureBackup\build\Release\SureBackup.exe`  
**Warnings:** Same as before (no new issues)

---

## Summary

✅ **Worker progress bars visible ONLY in maximized mode**  
✅ **Hidden in normal mode for space efficiency**  
✅ **Compact 18px design (was 30px)**  
✅ **Smooth show/hide on mode toggle**  
✅ **Full parallel backup monitoring support**

The log window now intelligently adapts to provide either detailed multi-thread monitoring (maximized) or maximum log viewing space (normal). Perfect balance of detail and simplicity! 🎊
