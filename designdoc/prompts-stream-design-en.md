# Prompts Stream Model — Design Document

> Using Factorio's conveyor belt system as a metaphor.
> The user acts as a factory manager: monitoring machine I/O, adjusting recipes, and sorting outputs.

---

## Factorio Analogy Table

| Factorio | Prompts |
|----------|---------|
| **Machine** (assembler / chemical plant / refinery) | **Node** or **Pipeline step** |
| **Conveyor belt** | **Stream** (data flow between steps) |
| **Material** (iron plate / copper plate) | **Input** (content fed into a step) |
| **Recipe** (gear recipe / processing unit recipe) | **Prompt/Params** (step config: systemPrompt, userPrompt, model, temperature) |
| **Module** (Speed module / Productivity module) | Step-type-specific parameters (attachMedia, retry, timeout, etc.) |
| **Product** (gear wheel / plastic bar) | **Output** (step result) |
| **Opening a machine's GUI** | Clicking a node/step in the Tree |
| **Machine input slot** | Input Pane (shows what's coming in; allows manual replacement) |
| **Machine recipe screen** | Prompt/Params Pane (shows current recipe; editing is done elsewhere) |
| **Machine output slot** | Output Pane (shows what was produced) |
| **Filter inserter** (pick only iron plates) | **Filter** step type (✔ save / ✕ discard) |
| **Circuit network combinator** (evaluate signals) | **Evaluate** step type (AI absolute scoring) |
| **Splitter** (split belt into two directions) | **Condition** step type (conditional branching) |
| **Steel chest** (simple temporary storage) | **Steel Chest** (standard named shared buffer) |
| **Requester chest** (auto-collect items) | **Requester Chest** (data-detection auto-pipeline trigger) |
| **Storage chest** (overflow/trash storage) | **Storage Chest** (unwanted data / GC-targeted intermediate product holding area) |
| **Creative mode chest** (infinite supply) | **External file / manual input** (Input Pane source selection) |
| **Circuit conditions controlling machines** | **Dynamic input source selection** (referencing past checkpoints) |
| **Power outage → restart** | **Checkpoint persistence** → **Recovery flow** |
| **Drag items with mouse (move to chest)** | Manually sending intermediate products or results to chest / loading from chest |

---

## Overview

Redesign Prompts using Factorio's factory metaphor.

- Each **node** is a machine (assembler). Content (material) goes in, a recipe (pipeline) processes it, and a product (child node) comes out.
- During pipeline execution, each **step** is a machine connected by conveyor belts. The previous machine's output automatically flows into the next machine's input.
- The user acts as a factory manager: **clicking machines to inspect I/O**, **adjusting recipes**, **filtering outputs**, and **having AI evaluate quality**.
- Each machine's I/O is persisted as **checkpoints** sequentially. If a power outage occurs, the factory resumes from the last checkpoint.
- Cross-project data transfer is only possible indirectly through global shared buffers called **"Chests"**.
- Multiple chest types with different roles (**Steel Chest**, **Requester Chest**, **Storage Chest**) enable manual and automatic data sharing, event-driven cross-project automation, and clean storage management.

---

## UI Architecture — 5-Pane Layout

The current 4-pane layout (`Tree | List | Editor | Messages`) becomes 5 panes:
`Tree | Input | Prompt/Params | Output | Messages`.

**Opening a machine in Factorio is now the main content area of Prompts.**

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Toolbar  [Project: default ▾]                                           │
├───────┬────────────────┬──────────────────────────┬──────────────────────┤
│ Tree  │ Input          │ Prompt/Params            │ Output               │
│ (Factory│ (Machine      │ (Machine                 │ (Machine             │
│  Map)  │  Input Slot)  │  Recipe Settings)         │  Output Slot +       │
│       │                │                          │  Sorting/Evaluation)  │
│ 📂Parent│ Input Content │ Type-specific config     │ Output Content       │
│  ├📄Child│ (editable)   │                          │                      │
│  ├📄Child│              │ ai:                      │ [✔Save] [✕Discard]   │
│       │ ── Source ──   │  ├ systemPrompt          │                      │
│ 📂Parent│ [Prev Step]  │  ├ userPrompt            │ ★ Score: 8.5/10     │
│  ├✨AI│ [Checkpoint ▾] │  ├ model                 │ (AI Evaluation)      │
│  │    │ [📦 Named     │  └ temperature           │                      │
│  │    │   Chest ▾]    │                          │ [📦 Send to Chest]   │
│  │    │ [External File]│ chest:                   │                      │
│  │    │ [Manual Input] │  ├ chestName: "trans"    │ ── Metadata ──       │
│  │    │                │  ├ chestType: "steel"    │ tokens: 312          │
│  │    │ [▶ Process]   │  └ mode: "put"           │ model: gpt-4o        │
│  │    │                │                          │ duration: 2.3s       │
│  │    │                │                          │                      │
├───────┴────────────────┴──────────────────────────┴──────────────────────┤
│  Messages (Factory operation log)                                        │
└──────────────────────────────────────────────────────────────────────────┘
```

### Pane Summary

| Pane | Factorio Analogy | Role | Content |
|------|------------------|------|---------|
| **Tree** | Factory map. Click to inspect a machine | Full node tree. Node list in View mode, step list in Pipeline mode | Hierarchical list of nodes or steps. Click to select → other panes update |
| **Input** | Machine input slot. See what's inside; manually replace materials | Shows the input of the selected node/step. Allows source replacement | Editable textarea / image display for content. Source dropdown (prev step / checkpoint / named chest / file / manual) |
| **Prompt/Params** | Machine recipe screen. See current recipe configuration | Shows the current step's settings by type. Read-only; editing is done in Pipeline Manager | Formatted display of step.params key-values keyed by step type |
| **Output** | Machine output slot. Inspect products, sort, evaluate quality | Shows the output of the selected node/step. Save/discard decisions, evaluation, comparison, chest transfer | Output content + [✔Save] [✕] + score. [📦 Send to Chest] button. Multiple outputs: side-by-side cards + Diff/Evaluate comparison UI |
| **Messages** | Factory operation log | Operation log and pipeline streaming output | Log entry list |

---

## Stream Model — Checkpoint System

### Core Concept

Each step's input and output are sequentially persisted as **checkpoints** in the project's execution history directory. Saved checkpoints can be freely reused as input sources. This is the Factorio equivalent of **"logging every machine's I/O so the factory can resume after a power outage."**

### Input Side — Free Source Selection

The Input Pane lets you freely choose the input source for a step. Unless explicitly specified, data from other projects never mixes in.

| Source | Factorio | Scope |
|--------|----------|-------|
| **Previous step output** (default) | Flowing from the previous machine on the belt | `historySteps_[currentStepIndex_ - 1].output` (same Run) |
| **Same-project past checkpoint** | Retrieving past products from storage | `projects/{current_project}/history/*/checkpoint_N/output.json` |
| **Named Chest** | **(explicit only)** Taking named materials from shared chest | User selects "Load from chest" and specifies `chestName`. **Direct cross-project access is forbidden.** |
| **External file** | Buying iron plates from outside | Text/image file selected via file dialog |
| **Manual input** | Placing items directly into the input slot with the mouse | Inline textarea in Input Pane |

---

## Project Concept & Isolation

### 1. Directory Structure Isolation

Instead of a flat `%APPDATA%/Ecode/Prompts/` layout, introduce `projects/` subfolders for physical isolation, a shared `chests/` area, and a `storage_chest/` for GC-targeted data.

```
%APPDATA%/Ecode/Prompts/
├── providers.json
├── session.json
├── chests/                      ← Shared chest area (steel / requester)
│   ├── chest_jp_translations.json
│   └── ...
├── storage_chest/               ← New: Storage chest (GC trash bin)
│   ├── deleted_checkpoint_xyz.json
│   └── ...
└── projects/
    ├── default/
    │   ├── pipeline.json
    │   ├── data/
    │   ├── blobs/
    │   └── history/
    └── project_A/
```

### 2. Three-Layer Isolation Guarantee — No Mixing Ever

To **absolutely prevent** data mixing between projects and nodes, isolation is guaranteed at three layers.

#### Layer 1: Filesystem Isolation (Physical)
- Each project's data is **physically separated** under `projects/{project_name}/`.
- Project A's code can **never open** Project B's `data/`, `blobs/`, or `history/` directories via file paths (Storage strictly restricts paths).
- Storage blocks all file reads outside the `%APPDATA%/Ecode/Prompts/projects/` directory tree.

#### Layer 2: Access Control in Storage Layer (Logical)
- All file I/O methods (`Storage::LoadTabData()`, `LoadCheckpoint()`, `SaveHistory()`, etc.) **only allow access to the currently active project's path**.
- The internal base path switches only when `select_project` changes the active project.
- `Storage::LoadFromNamedChest()` is the sole exception: it accesses only `chests/`. It **never accesses another project's `projects/` subtree**.

#### Layer 3: Route Restriction in C++ ↔ JS Bridge (Communication)
- Bridge messages (`search`, `history_list`, `history_detail`, `evaluate_node`, etc.) are **always scoped to the current project only**.
- If JS sends `{ type: "load_tab", project: "project_B", file: "..." }` to access another project, C++'s `HandleBridgeMessage` **rejects all access outside the current project**.

#### Explicit Anti-Mixing Rules

| Situation | Behavior | Guarantee |
|-----------|----------|-----------|
| Pipeline ▶ Run in Project A | All reads/writes inside `projects/A/` | ✅ Fully isolated |
| Browsing past checkpoints in Project A | Search limited to `projects/A/history/` | ✅ Other projects invisible |
| Manually sending to chest from Project A | Data is **copied** to `chests/chest_xxx.json` | ✅ Original data stays |
| Manually loading from chest in Project B | Data is **copied** from `chests/chest_xxx.json` | ✅ Chest data remains |
| Search in Project A | Targets only `projects/A/` | ✅ Project B data not hit |
| Recovering from Storage Chest | Copied from `storage_chest/` to current project | ✅ Original stays |
| External file as input | Only the path selected in file dialog | ✅ User responsibility |

### 3. Data Transfer via Named Chests (Automatic & Manual)

Direct cross-project data access is completely forbidden. All inter-project data transfer must go through the **Chest** shared area. Chest operations support both **pipeline-automated** and **UI-manual** methods.

- **A. Pipeline-automated (Chest step)**:
  - Use the `type: "chest"` step (params: `chestName`, `chestType`, `mode`).
  - `mode: "put"`: saves the upstream output to `chests/chest_{chestName}.json`.
  - `mode: "take"`: reads from the named chest and passes data downstream.
- **B. UI-manual (Send to / Take from Chest)**:
  - **Send to Chest**: With intermediate output or a checkpoint displayed, click the **`[📦 Send to Chest]`** button in the Output or Input Pane. Enter a chest name; the data is copied to `chests/chest_{chestName}.json`.
  - **Take from Chest**: In the Input Pane source dropdown, select "Load from named chest" and enter/select a chest name to load data into the current project.
- **Effect**: Manual chest operations allow **in-flight intermediate data (checkpoints)** to be safely and easily exported to another project by giving it a name.

---

## Chest Types

Following Factorio's chests, introduce role-specific chests for advanced cross-project workflows.

### 1. Steel Chest
- **Role**: Standard static data buffer. Data is persisted as a file and held until another project manually loads it or a pipeline `take` step reads it.

### 2. Requester Chest 🌟 [Event-Driven Integration]
- **Role**: **Auto-pipeline trigger** chest.
- **Behavior**:
  - A Chest step with `mode: "take"` and `chestType: "requester"` monitors `chests/chest_{chestName}.json` for updates (file write detection).
  - When another project writes to the target chest, it triggers the requesting pipeline to **automatically run in the background** with the new data.
- **Use case**: Project A (drafting) writes a draft to a chest → Project B (proofreading/translation) auto-starts processing.

### 3. Storage Chest 🧹 [Clean Storage Management]
- **Role**: Trash/overflow chest for unwanted intermediate data and old checkpoints.
- **Behavior**:
  - Outputs marked "discard (✕)" and old `checkpoint_N/` directories exceeding the retention limit are not immediately deleted. Instead, they are **automatically moved** to the global `storage_chest/` directory.
  - Users can search the storage chest and manually recover ("I discarded it then, but now I need it").
  - To free disk space, execute "Empty Storage Chest" from the toolbar for a bulk physical delete.

---

## Step Types

### Existing Steps

| type | Factorio Analogy | Prompt/Params Display | Output Display |
|------|------------------|----------------------|----------------|
| `ai` | AI assembler | provider, model, systemPrompt, userPrompt, temperature, maxTokens, attachMedia | AI response text |
| `manual` | Human inspection station | mode (view/edit/select), prompt, choices | User selection result |
| `command` | Specialized machine tool | command, args, workingDir, timeout, resultAs | stdout |
| `tool` | External machine startup | command, args, waitForExit, resultAs | Tool output |
| `fetch` | External material procurement | url, method, auth, resultAs | HTTP response |
| `condition` | Quality gate / combinator | expression, operator, value, onTrue, onFalse | Evaluation (true/false) |
| `transform` | Material forming press | engine, expression, input | Transformed text |
| `call_pipeline` | Sub-factory invocation | pipelineName, input, inheritAttachments | Subroutine output |
| `foreach` | Mass production line | input, itemVariable, concurrency | Concatenated results |
| `parallel` | Parallel production lines | branches[], outputMode | JSON of all branch outputs |
| `wait` | Curing/cooling time | durationMs, until, pollIntervalMs, timeoutMs | (none) |
| `history` | Warehouse retrieval | runId, stepIndex, field | Retrieved content |
| `wizard` | New machine setup wizard | wizard (JSON name), wizardData | Input values JSON |

### New Steps

#### `filter` — Filter Inserter (decide whether to save output)

Factorio's filter inserter picks specific items from the belt. This is not cherry-picking (removal) — it's **"should I keep this in storage?"**

```json
{
  "type": "filter",
  "name": "Choose output to keep",
  "mode": "manual",
  "splitBy": "\n---\n",
  "actions": ["approve", "reject"]
}
```

| mode | Factorio | Behavior |
|------|----------|----------|
| `auto` | Unfiltered inserter (all pass) | Single output: auto-save. No manual prompt |
| `auto_pass` | Just let it through | Single output: do NOT save. Do not prompt |
| `manual` | Human presses ✔/✕ | User decides whether to save each output |
| `manual_split` | Sorting conveyor | Split output by `splitBy` delimiter, evaluate each block independently |

#### `evaluate` — AI Quality Evaluation (absolute scoring)

Like a circuit network combinator connected to a quality sensor: **absolute score, not relative ranking**. Each output gets an independent score.

```json
{
  "type": "evaluate",
  "name": "Evaluate translation quality",
  "criteria": "Accuracy and fluency",
  "rubric": "1-10 scale",
  "outputScore": true
}
```

- Passes the previous step's output to an evaluation AI and gets an absolute score (★ n/10)
- The score is always visible in the Output Pane (like a quality label)
- Multiple outputs each receive independent scores (e.g., Claude ★7, GPT-4 ★9)
- Scores are saved as `evaluate_N.json` and embedded into saved output node metadata

#### `chest` — Named Chest (store/retrieve via chest)

Transfer data between projects asynchronously via a named chest.

```json
{
  "type": "chest",
  "name": "Japan Translation Box",
  "params": {
    "chestName": "jp_translations",
    "chestType": "steel",
    "mode": "put"
  }
}
```

| parameter | Role | Example |
|-----------|------|---------|
| `chestName` | Chest identifier | `"jp_translations"`, `"code_assets"` |
| `chestType` | Chest type | `"steel"` (standard buffer) \| `"requester"` (auto-trigger) |
| `mode` | Put or take | `"put"` (store) \| `"take"` (load) |

---

## Bridge Message Extensions

### New Messages

| Direction | type | payload | Description |
|-----------|------|---------|-------------|
| JS→C++ | `select_project` | `{projectName}` | Switch project |
| JS→C++ | `create_project` | `{projectName}` | Create new project |
| C++→JS | `project_changed` | `{projectName, tabs[], pipelines[]}` | Project switch complete, apply new data |
| JS→C++ | `send_to_chest` | `{content, chestName, chestType?}` | Manual chest put |
| JS→C++ | `select_input_source` | `{stepIndex, source, chestName?}` | Change input source; specify `chestName` for chest take |
| C++→JS | `input_source_changed` | `{stepIndex, content}` | Input source change reflected in UI |

### Modified Messages

| type | Change |
|------|--------|
| `init` | Added `incompleteRuns[]` field to payload |
| `pipeline_completed` | Includes paths to all checkpoints on completion |

### Step Filter Messages

| Direction | type | payload | Factorio |
|-----------|------|---------|----------|
| C++→JS | `step_filter_pause` | `{stepIndex, mode, splitBy?, outputs[]}` | Inserter pauses: "Which item do I pick?" |
| JS→C++ | `step_filter_resume` | `{stepIndex, approved[], rejected[]}` | Human responds: "Take these, discard those" |
| C++→JS | `checkpoint_ready` | `{runId, stepIndex}` | Machine log file written |
| C++→JS | `evaluation_result` | `{stepIndex, score, rationale}` | Quality combinator outputs score |
| C++→JS | `incomplete_run_detected` | `{runId, pipelineName, lastCompletedStep}` | Power outage recovery: "Here are the production logs" |
| JS→C++ | `resume_run` | `{runId, action}` | Recovery action (continue/discard) |

---

## Power Outage Recovery Flow

Factorio power outage: all machines stop. On restart, you resume from the last save.
Prompts uses checkpoints saved after each step, so recovery is step-granular.

On startup:

1. Scan `history/` for runs where `state.json` status is `"running"`
2. For each: send `incomplete_run_detected` to JS
3. JS shows a modal:

```
📋 Incomplete pipeline run detected
   Pipeline: "Translate → Review"
   Step 2/3 completed (Translate done, Review interrupted by outage)
   Last checkpoint: run_20260608_120000/checkpoint_1
   Pipeline definition: (display)

   [▶ Resume from Step 2]  [📝 Keep results so far]  [🗑 Discard]
```

4. "Resume": `ResumePartialRun(runId)` loads checkpoints and continues from the step after the last completed one
5. "Keep results": closes the run with completed checkpoints preserved (unfinished outputs discarded)
6. "Discard": deletes all run data

---

## Design Decisions

### 1. Checkpoint Retention Policy (Garbage Collection) [Configurable]
To conserve disk space, the number of execution histories (`checkpoint_N/`) retained per project is user-configurable in the Config screen's General tab.
Default: 50 runs. Minimum: 10. Maximum: 500.
Excess runs are automatically moved to the Storage Chest.

### 2. Binary/Media File (Blob) Management
Binary output files such as images and audio are stored separately within each project's `blobs/` directory.

### 3. Isolated Sharing via Chest Steps & Manual Chest Operations
- Projects must never directly reference each other's private data (tabs, history, intermediate checkpoints).
- All cross-project data transfer must go through **named chests** stored in the global `chests/` buffer.
  - **Automatic**: `chest` step (`mode: "put"` / `"take"`) handles pipeline-driven transfers.
  - **Manual**: Any output or **intermediate product (checkpoint)** can be sent to a named chest via the `[📦 Send to Chest]` button in the UI, then manually loaded by another project.

### 4. Requester Chest Reactive Auto-Trigger
- A pipeline with a `mode: "take"` step using `chestType: "requester"` is auto-triggered by the backend's file-watch mechanism the moment data is written to the target chest.

---

## Implementation Phases

### Phase 1 — Project Isolation & 5-Pane UI + Checkpoint Persistence

**Goal**: Factorio machines can now be opened. I/O and recipes are visible in one screen. The factory survives power outages.

| # | File | Content |
|---|------|---------|
| 1 | `designdoc/prompts-stream-design-en.md` | This design document (English) |
| 2 | `frontend/style.css` | 5-pane grid layout (Tree 20% / Input 27% / Prompt 27% / Output 26%) |
| 3 | `frontend/index.html` | Replace List/Editor panes with Input/Prompt/Output panes |
| 4 | `frontend/app.js` | Implement `renderInput()`, `renderPrompt()`, `renderOutput()`; `viewMode` management; `selectNode()` / `selectPipelineStep()` pane switching |
| 5 | `src/Storage.h/cpp` | `SaveCheckpoint(runId, stepIndex, input, output, meta)`, `LoadCheckpoint()`, `ScanIncompleteRuns()`, `CloseRun()`, `DiscardRun()`; directory restructure for `projects/`, `chests/`, `storage_chest/` |
| 6 | `src/PipelineRunner.h/cpp` | Call `Storage::SaveCheckpoint` on step completion; add `ResumePartialRun()` |
| 7 | `src/App.cpp` | Startup `ScanIncompleteRuns()` → user confirmation UI; `resume_run` handler |
| 8 | `frontend/app.js` | Project management: project selector in toolbar, project create/switch UI |

### Phase 2 — Input Source Selection & Chest Step

**Goal**: Mouse-driven input slot manipulation. Select materials from past checkpoints, external files, or named chests. Chest step for cross-project data flow.

| # | File | Content |
|---|------|---------|
| 1 | `src/Storage.h/cpp` | `SaveToNamedChest(chestName, content)` / `LoadFromNamedChest(chestName)`; initialize `chests/` and `storage_chest/` directories |
| 2 | `src/App.cpp` | `send_to_chest` message handler; chest file monitoring skeleton (for requester chest) |
| 3 | `frontend/app.js` | Input Pane source dropdown (prev step / checkpoint / named chest / file / manual); `[📦 Send to Chest]` button in Output and Input panes with name dialog; "Load from named chest" option in Input |
| 4 | `src/PipelineRunner.cpp` | `type: "chest"` handler: `mode: "put"` calls `SaveToNamedChest`, `mode: "take"` calls `LoadFromNamedChest` |
| 5 | `src/PipelineRunner.cpp` | `inputFrom` resolution: resolve any checkpoint / external file / string as `{content}` |

### Phase 3 — Filter Step Type & Compare UI

**Goal**: Filter inserters are now placeable. ✔/✕ buttons appear in Output Pane. Multiple outputs can be compared side-by-side.

| # | File | Content |
|---|------|---------|
| 1 | `src/NodeData.h` | `FilterConfig` struct |
| 2 | `src/PipelineRunner.cpp` | `type: "filter"` handler in `ExecuteStep` |
| 3 | `src/App.cpp` | `step_filter_resume` handler |
| 4 | `frontend/app.js` | `step_filter_pause` → ✔/✕ buttons in Output Pane; side-by-side card display for multiple outputs; 📊 Diff button (line-level); 🤖 Evaluate button |
| 5 | `frontend/style.css` | Filter UI styles (✔/✕ buttons, split cards); comparison cards and diff display |

### Phase 4 — Evaluate Step Type

**Goal**: Quality evaluation machines are placeable. AI scores are displayed on every output as ★ n/10.

| # | File | Content |
|---|------|---------|
| 1 | `src/PipelineRunner.cpp` | `type: "evaluate"` handler in `ExecuteStep` |
| 2 | `src/App.cpp` | `evaluation_result` bridge message sender |
| 3 | `frontend/app.js` | Score display (★ n/10) in Output Pane; evaluate step UI |
| 4 | `frontend/app.js` | Config General tab: history retention setting (default 50, min 10, max 500) |
| 5 | `src/Storage.h/cpp` | `maxHistoryRuns_` field; config serialization/loading |
| 6 | `src/App.cpp` | `set_history_retention` bridge handler |
| 7 | `frontend/style.css` | Score badge, storage chest management UI |

---

## Data Structures

### FilterConfig (NodeData.h)

```cpp
struct FilterConfig {
    std::string mode;          // "auto" | "auto_pass" | "manual" | "manual_split"
    std::string splitBy;       // delimiter (for manual_split)
    std::vector<std::string> actions;  // allowed actions
};
```

### CheckpointMeta (Storage.h)

```cpp
struct CheckpointMeta {
    int stepIndex;
    std::string stepName;
    std::string stepType;
    std::string status;        // "completed" | "running" | "pending"
    int promptTokens;
    int completionTokens;
    double durationMs;
    bool saved;                // ✔ marked for save?
    double score;              // evaluate score (-1 = unevaluated)
    std::string evaluationNote;
};
```

### IncompleteRun (Storage.h)

```cpp
struct IncompleteRun {
    std::string runId;
    std::string pipelineName;
    int lastCompletedStep;
    int totalSteps;
    std::string startedAt;
    std::string status;        // "running" | "interrupted"
};
```

---

## Open Questions

1. **Blob management in checkpoints** — Should image outputs in checkpoints be stored as files in the project's `blobs/` directory, or as binaries inside the checkpoint directory itself?
2. **Resume-time param editing** — Should the user be allowed to modify step settings (model, prompt, etc.) when resuming an interrupted run? Currently: "continue with original definition only." If needed, deferred to Phase 5+.
3. **Compare vs Evaluate relationship** — Should `compare` be an independent step type, or should it be expressed as a combination of `evaluate` + `filter`?
