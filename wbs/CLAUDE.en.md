# WBS Tool - CLAUDE.en.md

> **English translation of [`CLAUDE.md`](CLAUDE.md) (the Japanese original is the primary spec).**
> **Sync rule: whenever the spec changes, update `CLAUDE.md` and `CLAUDE.en.md` in the same commit.**

A text (JSON) driven WBS / Gantt + inazuma-line tool.
`wbs.json` (data) is the single source of truth; the self-contained single HTML `wbs_viewer.html` renders it.

---

## Handling principles (important)

- **Do not touch `wbs_viewer.html` (the viewer) as a rule.** The view logic is complete.
- **Changes mean editing `wbs.json` (data) only.** When asking an AI to do work, it should edit the JSON.
  - Touch the HTML only when the display spec itself must change (and only on explicit request).

## Archiving (pruning past tasks)

To keep the file from growing forever, **move old completed tasks out of the current file**:

- **Trigger**: when completed tasks from past months (e.g., May 2026 and earlier) have scrolled into the past on the Gantt.
- **Steps**:
  1. **Create a backup in the same folder** (e.g., collect the moved tasks into `wbs-archive-2026-05.json`).
  2. **Delete those completed tasks** from `wbs.json`.
- → The current `wbs.json` stays focused on "now and the future"; history is preserved in archive JSONs.
- Scale is managed in two layers: **collapsing (display) + this archiving (data)**.

---

## Requirements
- **Google Chrome (latest) recommended**. **Chromium-based browsers (e.g., Edge) work too** (testing is done on Chrome). Firefox/Safari are not supported (no File System Access API).
- On corporate-managed browsers, editing is unavailable if File System Access is disabled by policy (viewing still works).
- No HTTP server, no external libraries / CDN. Open `wbs_viewer.html` **directly via file://**.

## Opening / reloading
1. Open `wbs_viewer.html` in Chrome.
2. Pick `wbs.json` via **Open file** (or **drag & drop** onto that same button).
3. Edit `wbs.json` and save → press **Reload** to re-read and re-render (no re-selection needed / File System Access API).
4. Collapsing: click a **◆project / L1 / L2 `▼/▶`–name**.
   The **`▼/▶` in the Task column header** expands/collapses everything (▼ = all open → click collapses all; ▶ = something closed → click expands all).
   An accidental header action can be **restored with Ctrl+Z** (inactive while an input has focus).
5. Scroll position is **preserved across collapsing and reloads**; **only loading a new file re-centers on today and resets collapse state**.

## In-browser editing (edit mode)
In addition to text / AI editing, `wbs.json` can be **edited directly on screen** (optional feature).
- The **Edit button** toggles ON/OFF (green = ON). When ON, the left table becomes inputs + action buttons.
- Supported: **inline editing of every field** (No.=id / task name / qty / hours / assignee / plan & actual dates / notes),
  **adding leaves** (row `＋` = sibling below; project row `+Task`; ids are minted collision-free; adding to a collapsed project auto-expands it),
  **deletion** (`✕`, with confirmation, including children), **reordering among siblings** (`▲▼`).
- **Autosave**: changes are written back to `wbs.json` after a ~0.4 s debounce (File System Access API).
  Writes are serialized through a single queue. Only the internal derived values `_calc`/`_leaf` are stripped (**user keys starting with `_` are preserved**).
  Save status (Unsaved changes… / Saved HH:MM:SS / Save failed) is **always visible at the top right**.
- **External-change detection**: before each write the file's mtime is checked; if it changed outside the tool (e.g., AI editing), an **overwrite confirmation** is shown.
  When asking an AI to edit, it is safest to turn edit mode OFF first.
- Prerequisites: saving requires **write permission**. Only files opened with a handle (Open file or D&D) are editable.
  **file:// pages cannot show write-permission prompts**, so when turning Edit ON you **re-select the same wbs.json in a save dialog** (the selection itself grants permission).
  ⚠ Chrome **truncates the file the moment it is picked**, so the current data is **written immediately after selection** (never left empty — #29).
  If told to "press Edit again", do so (browser gesture limitation; the second click opens the dialog directly).
  The permission is **session-scoped** (after restarting Chrome, re-select once per Edit ON).
  **Loading a new file automatically turns edit mode OFF** (turn it ON again to grant permission for the new file). On a failed load the handle is not replaced (prevents overwriting the wrong file).
- **Legacy format** (`{project,tasks}`): on Edit ON, after confirmation it is **converted to the `projects[]` format** before editing.
- **Date cells are `YYYY-MM-DD` text inputs plus a 📅 calendar button**. `2026/07/01`-style input is accepted and normalized to `-`.
  (The display format of `input type=date` follows the browser UI language and cannot be controlled, so the display is fixed to ISO — #33.)
- Input guards: dates are **valid only within 1900–2099** (out-of-range / partial input is ignored, not saved). Qty / hours accept decimals (`step="any"`).
  Mouse-wheel changes on number inputs are disabled (prevents accidental edits). Closing the tab with unsaved changes prompts for confirmation.
- Out of scope (by design): drag-and-drop reordering / moving across parents / automatic renumbering / leaf↔summary conversion / adding projects. Use JSON or AI editing for those.
- Effort / progress / inazuma line recompute automatically as before (re-rendering is deferred while an input has focus).

## Data (wbs.json)
- **Multi-project**. Top level is `projects: [{ name, milestones, tasks }]`.
  - The legacy format (single `{ project, milestones, tasks }`) is **still readable** (backward compatible).
- Each project's `tasks` nest parent-child (max 3 levels). With `children` = summary node; without = leaf (carries effort).
- Leaf fields:
  ```
  id, name, qty, hours (per unit), assignee,
  plan:   { start, end }        planned, "YYYY-MM-DD"
  actual: { start, end }        actual (null if undecided)
  note
  _anyName                      "_"-prefixed = custom key (optional, ignored by viewer, preserved on save)
  ```
- **Effort and progress are not stored in the data** (all derived; computed by the JS in the HTML).
- **Keys starting with `_` may be added freely** (ignored by the viewer, preserved by edit-mode saves).
  Use them for metadata (e.g., `_ai` in `wbs_roadmap.json` = recorded AI effort in tokens; entirely optional. `wbs_sample.json` is the minimal example without custom keys).
- `milestones: [{ date, label, color }]` is optional per project.

## Adding / updating data (how-to)

Edit `wbs.json` only. Save → press **Reload** in the viewer.

### ① Status updates (daily operation — most important)
Progress, effort, and the inazuma line are **computed automatically**. You only touch the **actual dates**:
- **Work started** → set `actual.start` on the leaf (→ the Actual-End column shows "active"; reflected in the inazuma line)
- **Work finished** → set `actual.end` (→ row turns gray with `✓`; the inazuma point sits on the today line)
- **Plan changed** → fix `plan.start` / `plan.end`

### ② Adding a task
Append a leaf to a summary node's `children`.
**Keep `name` a concise label (guideline: ~14 full-width chars; put details and context in `note`)** —
long names don't break anything but get truncated in the task column (full text on hover).
```json
{ "id": "2.1.3", "name": "New task", "qty": 1, "hours": 16, "assignee": "piguo",
  "plan":   { "start": "2026-07-01", "end": "2026-07-05" },
  "actual": { "start": null, "end": null }, "note": "" }
```
- To add an intermediate phase, add a **summary node** with `children` and put leaves under it (max 3 levels).

### ③ Adding a project
Append to the `projects` array:
```json
{ "name": "New project", "milestones": [],
  "tasks": [ { "id": "1", "name": "Phase 1", "children": [ /* leaves… */ ] } ] }
```

### ④ Adding a milestone
Append to the project's `milestones`:
```json
{ "date": "2026-09-30", "label": "Release", "color": "#ef4444" }
```

### ⑤ Example requests to an AI (have it edit the JSON)
- "Mark the design review as **completed today**" → set `actual.end` of that leaf to today
- "Component placement has **started**" → set `actual.start`
- "**Add** a testing phase" → append a summary node + leaves
- "**Archive** everything completed before May" → archiving procedure above (backup + delete)

## Computation (deterministic, in the HTML)
- **Effort (person-days) = `qty × hours ÷ 8`**. Parent = sum of descendant leaves.
- Parent plan/actual period = min start – max end of descendant leaves.
- Progress (reference date = today):
  - Not started (no actual.start) = 0% / Completed (actual.end set) = 100%
  - Active = `clamp((today − actual.start) ÷ (plan.end − plan.start) × 100, 0, 100)`
  - Parent = effort-weighted average of descendant leaves' (progress × effort).
- Header shows "project count / total effort (person-days)".
- Progress is computed internally and **used only for the inazuma line and parent aggregation** (time-based, so it is misleading as a number and never displayed).

## Display
- **UI is Japanese/English switchable** (the "EN / 日本語" toolbar button). Default Japanese; the choice is stored in localStorage.
  All UI strings live in the `I18N` table inside the HTML (data = the contents of wbs.json is never translated). **When adding a UI string, add it to both ja and en.**
- Column headers are centered. The left info table (No.–Notes) is fixed; **only the Gantt scrolls horizontally**.
- **Plan/Actual date columns have a two-level header**: "Plan" / "Actual" on top, "Start / End" beneath (adjacent columns grouped by the `group` property in `COLS`).
- Row layer colors: **◆project (with separators) > L1 > L2 > L3**.
- Gantt: date + weekday (year-month header), weekends shaded; active tasks extend the actual bar to today.
  **Plan = translucent solid outline (front, no fill) / actual = filled bar (behind), overlaid in the same lane**:
  the actual bar sticking past the outline's right edge = deadline overrun; empty space at the outline's left = late start — both directly visible.
- Date columns show `5/11` style. Initial view centers near today.
- **Completed tasks**: darker gray row + strikethrough + leading `✓`.
- **URLs inside notes are auto-linked** (`http(s)://` only, opens in a new tab). Put plain URLs in `note` to jump to issues or specs.
- Overlay (SVG):
  - **Inazuma line** (red, **one line across all projects**). Each terminal row's point is placed as follows (**bulging left = behind schedule**):
    - Completed = on the today line
    - **Deadline overrun** (started, unfinished, today > `plan.end`) = at the **planned end date** (overrun takes priority)
    - **Start delay** (not started, today > `plan.start`) = at the **planned start date**
    - Active (within deadline) = on the today line (start drift is not shown)
    - Everything else (not started, not yet due, etc.) = on the today line
    - A collapsed node contributes a single aggregated point. No explicit today line is drawn (today is the reference).
  - **Milestone lines** (per-project `milestones`, arbitrary color).

## Broken input (graceful degradation)

**Policy: broken input must never crash the viewer.** Invalid values render as ignored / 0 / empty.
Avoid the following when entering data (nothing crashes, but display degrades).

| Input (anomaly) | Viewer behavior |
|---|---|
| Invalid date (not `"YYYY-MM-DD"`, or **outside 1900–2099**) | **Ignored** (plan/actual/milestones alike). Date cell shows `—` |
| Milestone without `label` | **Rendered with an empty label** (no crash) |
| Active but `plan.end` missing | Progress **0%** (avoids NaN) |
| End < start (inverted period) | Bar not drawn / looks odd |
| Actual start in the future | Progress 0 |
| `qty` / `hours` missing or 0 | Effort 0 (shown empty) |
| Decimal `qty` (e.g., 0.5) | Decimal person-days (1.5 etc.) |
| Nesting beyond 4 levels | Colors stop at **L3** (no breakage) |
| Out-of-range / colorless / invalid-date milestones | **Not drawn** (ignored) |
| Milestone `color` not `#hex` | **Ignored, default color** (prevents attribute injection) |
| **Duplicate ids** within one project | Collapse keys collide (same ids open/close together) |
| `projects: []` / `tasks: []` (empty) | Empty view (no crash) |
| Top level `null` / number / non-object | Empty view (no crash) |
| Invalid JSON (D&D / file picker / reload) | Alert shown (no silent failure) |

- Verification data lives in **`tests/異常_*.json`** (broken) and **`tests/正常_*.json`** (normal); see `tests/INDEX.md`.
- "Never crashes" is verified by loading every test file in headless Chromium and asserting **no JS errors and no `NaN`**.

## Known limitations
- **Initial rendering slows with thousands of rows** (full innerHTML rebuild each render). Mitigation = collapsing + archiving. Virtualization only if it ever truly hurts.
- **Identically-named projects share collapse state** (they open/close together). Keep project names unique.
- **`null` elements inside arrays** (null tasks/projects) are unsupported (a non-object top level degrades to an empty view).
- **No keyboard navigation / screen-reader support** (mouse-first personal tool).
- **Renaming a project or editing an id in edit mode resets its collapse state** (collapse keys derive from name/id).
- **Multi-line `note` values get flattened to one line** when touched in edit mode (inputs cannot hold line breaks).

## Notes
- Chromium-based browsers (Chrome recommended), file:// based (uses the File System Access API).
- Your own `wbs.json` is **gitignored by default** (prevents accidental commits). `wbs-archive-*.json` is ignored too. Sample = `wbs_sample.json`; development plan = `wbs_roadmap.json`.
