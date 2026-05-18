# Features and Usage

## Scratch Buffer & JavaScript Execution

Ecode supports a specialized **Scratch Buffer** for executing JavaScript code directly within the editor.

### How to use:
1.  **Open Scratch Buffer**: Select `File` -> `New Scratch Buffer` from the menu.
2.  **Type JS Code**: Write any JavaScript code (e.g., `1 + 2 * 3` or `Math.random()`).
3.  **Execute**: Press `Ctrl + Enter`.
4.  **Result**: The result of the evaluation will be appended to the buffer as a comment.

## Key Bindings (JS Invokable)

You can bind JavaScript functions to key chords:
-   **Registering**: In the scratch buffer (or eventually `ecodeinit.js`), use:
    ```javascript
    function myHelloWorld() {
      Editor.insert(Editor.getLength(), "\nHello from JS!");
    }
    Editor.setKeyBinding("Ctrl+H", "myHelloWorld");
    ```
-   **Executing**: Once evaluated, pressing `Ctrl + H` will execute the `myHelloWorld` function.
-   **Supported Chords**: `Ctrl+`, `Shift+`, `Alt+` combinations with `A-Z`, `F1-F12`, and `Enter`.

## Localization

You can switch the editor's UI language at runtime:
-   Select `Language` from the menu.
-   Choose from: English, Japanese (日本語), Spanish (Español), French (Français), or German (Deutsch).
-   The menu titles and window header will update instantly.

## High Performance

Ecode is built for efficiency:
-   **Piece Table**: Handles huge files without copying data.
-   **Memory Mapping**: Loads files of any size almost instantly.
-   **DirectWrite**: Hardware-accelerated text rendering.
