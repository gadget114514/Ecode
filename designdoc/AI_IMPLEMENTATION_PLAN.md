# 🤖 AI-Assisted Coding Implementation Plan

This document outlines the architecture and implementation details for the AI-assisted coding functionality in Ecode.

## 1. Architecture Overview
The system follows a hybrid approach:
- **C++ Backend**: Provides the heavy lifting for file I/O, process execution, and editor state management.
- **JavaScript Layer (Duktape)**: Handles the high-level logic, prompt construction, API communication (via shell), and parsing of AI responses.

## 2. Core Components

### 2.1 Backend: Shell APIs
- **`Editor.runCommand`**: A synchronous API exposed to JavaScript that allows executing shell commands.
  - **Status**: ✅ Implemented in `JsApi_ShellAndMinibuffer.inl`.
  - **Purpose**: Enables calling `curl` or `powershell` for network requests without linking heavy HTTP libraries into the core editor.
- **`Editor.runAsync`**: An asynchronous API that launches external programs without blocking the UI.
  - **Status**: ✅ Implemented in `JsApi_ShellAndMinibuffer.inl`.
  - **Purpose**: Allows AI macros to perform long-running tasks and receive results via a JavaScript callback.

### 2.2 Secure Persona & Key Management
- **Security**: API keys are stored in JSON format in `%APPDATA%/Ecode/ai_config.json` rather than a plain text file.
- **Access**: Accessed via `Editor.runCommand` to read the environment or local credential store.
- **Token & Cost Optimization**: 
  - **Sliding History**: Only the last `maxHistoryItems` are kept for standard providers to prevent context bloat.
  - **Gemini Context Caching**: ✅ Automatically creates and uses server-side caches for conversations with Gemini 1.5 models (>= 4 turns), drastically reducing token costs and improving response speed.
  - **Configurable Context**: Custom limits (`contextBefore`/`contextAfter`) for text sent before/after the cursor.
- **Status**: ✅ Implemented in `scripts/ai.js`.

### 2.3 The AI Console (`*AI*` Buffer)
A specialized scratch buffer for conversational AI.
- **Logic**:
    - `Alt+I` to open/switch to the console.
    - `Alt+Enter` to send the current "instruction" line (prefixed with `> `).
    - Displays "Thinking..." state and streams (or blocks) results.
- **Status**: ✅ Core logic in `scripts/ai.js`.

### 2.4 Prompt Engineering & Context
- **Selective Context**: Automatically grabs 2000 characters before and 1000 characters after the caret to provide localized context to the LLM.
- **Instruction Support**: Supports both selected-text modification and minibuffer-prompted instructions.
- **Status**: ✅ Implemented in `scripts/ai.js`.

### 2.5 Automated File Updating (Multi-File AI)
The AI is instructed to use a specific protocol for structured edits:
```text
@@@REPLACE [path] [start_offset] [length]@@@
[new_code]
@@@END@@@
```
- **Parsing**: The JS layer parses these blocks, switches buffers, applies edits, and switches back.
- **Status**: ✅ Implemented in `scripts/ai.js`.

## 3. Implementation Roadmap

### Phase 1: Core Integration (Complete)
- [x] Expose `runCommand` to JS.
- [x] Create `ai.js` with basic completion (`Alt+A`).
- [x] Add menu items to the Tools menu.

### Phase 2: User Experience (Complete)
- [x] Implement AI Console for chat-like interaction (`Alt+I`).
- [x] Add secure API key input via minibuffer (`set_ai_key`).
- [x] Enable **Gemini Context Caching** for long conversations and cost efficiency.
- [x] **Asynchronous Execution**: Launched `runAsync` for non-blocking background tasks.
- [x] **Local LLM Support**: Support `ollama` or `llama.cpp` locally via OpenAI-compatible API and `api_base` config.
- [x] **@buffer workspace references**: Use `@filename` in prompts to include other open files as context.
- [ ] Add support for "Streaming" output (currently waiting for process termination).

### Phase 3: Advanced Features (Planned)
- [ ] **LSP Integration**: Provide type/schema information in the prompt for better accuracy.
- [ ] **Debugging Integration**: Send compiler errors/lints to the AI for automated fix suggestions.

## 4. Usage Instructions
1. Press `Alt+X` and type `set_ai_key` to set your API key (if needed).
2. Use `Alt+A` on any line to get a completion or instruction prompt.
3. Use `Alt+I` to open the full AI control console (`*AI*` buffer).
   - Type `/server [name]` to switch providers.
   - Type `/server_add [name] [provider] [api_base] [model]` to add a local or custom endpoint.
   - Type `/clear` to reset the conversation context.
4. Use `@filename` in your prompts to reference other open files.
5. Use `Alt+Shift+Drag` (mouse) or `Alt+Shift+Arrows` (keyboard) for **Box Selection**.
