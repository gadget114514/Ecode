# 🚀 Ecode: High-Performance Native Text Editor

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-windows-blue)
![License](https://img.shields.io/badge/license-MIT-orange)
![Duktape](https://img.shields.io/badge/scripting-duktape-purple)

**Ecode** is a blazing-fast, native Win32 text editor designed for efficiency, visual excellence, and extreme programmability. Built from the ground up for modern developers who demand performance without compromising on customization.

---

## ✨ Key Technical Pillars

-   **⚡ Unmatched Performance**: Leveraging a **Piece Table** data structure for $O(1)$ edit performance, even with gigabyte-sized files.
-   **💾 Huge File Support**: Instant file opening via **Win32 Memory Mapping (MMF)**. If your OS can see it, Ecode can edit it.
-   **🎨 Visual Excellence**: Hardware-accelerated text rendering using **DirectWrite** and **Direct2D** for crisp, smooth typography.
-   **📜 Extreme Programmability**: Embedded **Duktape JS Engine** allows for live macros, custom key bindings, and editor extensions.
-   **🌍 Multi-lingual**: Built-in support for English, Japanese, Spanish, French, and German.

---

## 🛠️ Features

### 🧩 Core Engine
*   **Piece Table Implementation**: Efficient internal representation of text edits.
*   **Atomic Save Strategy**: "Save-to-temp-and-rename" ensures zero data loss during power failures or crashes.
*   **Tabbed Interface**: Manage multiple huge buffers simultaneously with ease.

### ⌨️ Scripting & Macros
*   **Live Scratch Buffer**: Evaluate JavaScript on the fly to manipulate text or automate tasks.
*   **JS-Invokable Key Bindings**: Bind any JavaScript function to custom key chords (e.g., `Ctrl+Alt+S`).
*   **Rich JS API**: Access buffer contents, length, and editing functions directly from scripts.

### 🌐 Global Ready
*   **Multi-language UI**: Switch between English, 日本語, Español, Français, and Deutsch at runtime.
*   **UTF-8 / UCS-2 Support**: Full compatibility with modern text encodings.

---

## 🚀 Getting Started

### Prerequisites
*   Windows 10/11
*   Visual Studio 2022 (with C++ Desktop Development)
*   Powershell (for build scripts)

### Building from Source
1.  **Clone the repository**:
    ```bash
    git clone https://github.com/user/Ecode.git
    cd Ecode
    ```
2.  **Initialize environment**: Open a Developer Command Prompt.
3.  **Run Build**:
    ```powershell
    mkdir build
    cd build
    cmake ..
    cmake --build . --config Release --target installer
    ```
    This will compile the editor and generate the installer in `bin/EcodeSetup.exe`.

---

## 📖 Feature Spotlight: The Scratch Buffer

Want to automate a repetitive task? Open a **Scratch Buffer**, write some JS, and execute it instantly with `Ctrl+Enter`.

```javascript
// Duplicate the current line 10 times
function duplicate() {
  let text = Editor.getText(0, Editor.getLength());
  for(let i=0; i<10; i++) Editor.insert(Editor.getLength(), text);
}
Editor.setKeyBinding("Ctrl+D", "duplicate");
```

---

## 📈 Project Status

-   [x] Piece Table & Memory Mapping
-   [x] DirectWrite Rendering Pipeline
-   [x] Duktape Integration (JS API)
-   [x] Localization (v1.0)
-   [ ] Caret & Selection Logic (In Progress)
-   [ ] Theming System
-   [ ] Search & Regex

---

## 📜 License
This project is licensed under the MIT License. See [LICENSE.txt](LICENSE.txt) for details.

---

*Built with ❤️ for the performance-obsessed developer.*
