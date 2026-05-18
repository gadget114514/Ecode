# 🤖 AI Agent Development & Protocol Guide

This document defines the architecture, methodology, and communication protocols for building autonomous AI coding agents within the Ecode ecosystem.

---

## 1. The Agentic Loop (How to Use AI)
Building a reliable agent requires moving beyond simple "chat." You must implement a **control loop** where the AI can observe the results of its own actions and self-correct.

### The 5-Step Agentic Cycle:
1.  **Perception**: The agent gathers data from the environment. In Ecode, this includes reading the codebase via `Editor.getBuffers()`, checking the cursor location, and observing compiler/shell output via `Editor.runCommand`.
2.  **Planning**: Before writing code, the agent SHOULD generate a internal "Technical Plan" or "Task List." This reduces halluciation in complex refactors.
3.  **Action**: The agent executes a tool or sends a modification request. (See Section 3: Action Protocol).
4.  **Feedback**: The software returns the result of the action (e.g., `Exit code 1: Syntax Error` or `File not found`).
5.  **Iteration**: The agent analyzes the feedback, updates its state, and restarts from Step 2.

---

## 2. Communication Patterns

To ensure high reliability, communication with the Reasoning Engine (LLM) must be structured.

### 2. system Prompting & SOPs
Standard Operating Procedures (SOPs) should be baked into the system prompt:
- **RFC 2119 Keywords**: Use strict terms like **MUST**, **SHOULD**, and **MAY** to define behavioral boundaries.
- **Role Definition**: Define the agent's persona (e.g., "Senior Systems Architect") to shift its latent space toward high-precision output.

### 2.2 Tool Use (Function Calling)
This is the "hand" of the agent. Within Ecode, you provide the AI with definitions of functions it can trigger. 
- **Example Tools**: `read_file(path)`, `list_directory(path)`, `run_terminal_command(cmd)`.
- **Protocol**: The AI outputs a structured JSON block or a specific tag (like `@@@REPLACE`), which the Ecode JS layer executes.

---

## 3. The Action Protocol (Ecode Native)

This is the bridge between the AI's reasoning and the actual file system.

### 3.1 The `@@@REPLACE` Standard
Ecode's native agent logic parses the following block to apply precision edits:

```text
@@@REPLACE [path_or_active] [byte_offset] [length_to_remove]@@@
[new_code_to_insert]
@@@END@@@
```

- **[path_or_active]**: Target file path or the keyword `active`.
- **[byte_offset]**: Byte-level index where the edit starts.
- **[length_to_remove]**: Count of characters to delete.
- **[new_code_to_insert]**: The content to be written.

### 3.2 Security Boundary
All agent actions are restricted by the **Allowed Project Directory**. Edits targeting paths outside this boundary MUST be blocked by the software layer.

---

## 4. Industry Standards & Advanced Protocols

For advanced agents, Ecode is designed to align with emerging industry standards:

### 4.1 Model Context Protocol (MCP)
Ecode aims to support **MCP** as the gold standard for connecting agents to external data.
- **Unified Interface**: Use MCP to connect the agent to GitHub, Slack, or Google Drive.
- **Context Fetching**: Standardize how code segments are fetched to stay within token limits.

### 4.2 NLIP (Natural Language Interaction Protocol)
Aligned with **ECMA-430**, this protocol handles:
- Multi-turn conversational state management.
- Binary data transfer (images, logs) within the stream.

---

## 5. Technical Stack Recommendation

| Component | Recommendation |
| :--- | :--- |
| **Brain** | Claude 3.5 Sonnet or GPT-4o (Superior reasoning/tool-use) |
| **Connectivity** | **Model Context Protocol (MCP)** |
| **Orchestration** | LangGraph or PydanticAI (For robust state management) |
| **Execution** | Ecode Host (Win32) / Dockerized Sandboxes for safety |
| **Interface** | **LSP (Language Server Protocol)** for type-safe context |

---

## 6. Summary for Script Developers

To build a new agent in Ecode:
1.  **Observe**: Use `Editor.getBuffers()` to see open files.
2.  **Plan**: Ask the AI to output a `<PLAN>` before code.
3.  **Execute**: Use `@@@REPLACE` for file edits.
4.  **Verify**: Call `Editor.runCommand` to run tests and feed errors back to the AI.
