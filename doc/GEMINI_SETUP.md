# Ecode Gemini Integration Setup

This guide explains how to configure and use the Gemini AI integration within the Ecode app. Since Ecode uses a JavaScript engine (`quickjs`) for extensibility, you can configure your Gemini settings directly in your initialization scripts.

## 1. Setting Up `api_key` and `api_base`

To make Ecode use Gemini, you need to provide your API key and the API base URL. This is done by defining a global configuration object in your Ecode initialization script.

### Configuration via `aiconfig.js`

To keep your credentials separate from your main initialization script, it is recommended to store them in a dedicated `aiconfig.js` file.

1. Create a new file at `%APPDATA%\Ecode\aiconfig.js`.
2. Add the following JavaScript code to set up your credentials:

```javascript
// Setup Gemini AI parameters (aiconfig.js)
var GeminiConfig = {
    api_key: "YOUR_GEMINI_API_KEY",
    api_base: "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions",
    model: "gemini-1.5-pro"
    // You can change api_base if you are using a proxy or a different model endpoint
};
```

3. Open your main initialization script at `%APPDATA%\Ecode\ecodeinit.js`.
4. Add the following line at the top to load your AI configuration:

```javascript
load(Editor.getAppDataPath() + "/aiconfig.js");
```

## 2. Using Gemini in Ecode

Ecode's scripting engine can execute commands to interact with the Gemini API. You can create a macro to send the current selection to Gemini and paste the response back into the editor.

### Example Macro: `gemini.js`

Save this script in your `scripts` directory and load it via `ecodeinit.js` (`load('scripts/gemini.js')`):

```javascript
// A simple macro to send text to Gemini and append the response
function gemini_ask() {
    if (!GeminiConfig || !GeminiConfig.api_key) {
        Editor.setStatusText("Gemini Error: api_key not set in GeminiConfig!");
        return;
    }

    var text = "";
    if (Editor.hasSelection && Editor.hasSelection()) {
        var start = Editor.getSelectionStart ? Editor.getSelectionStart() : Editor.getCaretPos();
        var end = Editor.getSelectionEnd ? Editor.getSelectionEnd() : Editor.getSelectionAnchor ? Editor.getSelectionAnchor() : Editor.getCaretPos();
        if (start > end) { var t = start; start = end; end = t; }
        text = Editor.getText(start, end - start);
    }

    if (!text) {
        Editor.setStatusText("Gemini: No text selected.");
        return;
    }

    Editor.setStatusText("Asking Gemini...");
    
    // Construct the payload body
    var modelName = GeminiConfig.model || "gemini-3-flash-preview";
    var jsonBody = JSON.stringify({
        model: modelName,
        messages: [{ role: "user", content: prompt }],
        stream: false // Set to true if you implement streaming parsing
    });

    // Write jsonBody to a temporary file
    var tempFile = "ecode_prompt.json";
    Editor.execSync("echo " + jsonBody.replace(/"/g, '\\"') + " > " + tempFile);

    // Execute curl synchronously
    var curlCmd = "curl -s " + GeminiConfig.api_base +
                  " -H \"Content-Type: application/json\"" +
                  " -H \"Authorization: Bearer " + GeminiConfig.api_key + "\"" +
                  " -d @" + tempFile;
    
    var responseStr = Editor.execSync(curlCmd);
    Editor.execSync("del " + tempFile); // Cleanup temp file
    
    Editor.setStatusText("Parsing response...");
    
    try {
        var responseObj = JSON.parse(responseStr);
        if (responseObj.choices && responseObj.choices.length > 0) {
            var answer = responseObj.choices[0].message.content;
            
            // Insert the answer into the current buffer
            Editor.insert(Editor.getCaretPos(), "\n\n=== AI Response ===\n");
            Editor.insert(Editor.getCaretPos(), answer + "\n=====================\n\n");
            Editor.setStatusText("Response added.");
        } else if (responseObj.error) {
            Editor.setStatusText("API Error: " + responseObj.error.message);
        } else {
            Editor.setStatusText("Failed to parse response structure.");
        }
    } catch(e) {
        Editor.setStatusText("JSON Parse error: " + e.toString());
    }
}
```

### Can I use Claude, Codex, or OpenAI?
Yes! Since this macro leverages standard `curl` and the widely-used OpenAI REST JSON structure (`messages` and `role`), you can easily swap out the parameters in `aiconfig.js` to target Anthropic's Claude, OpenAI's GPT models, or local servers like Ollama/LMStudio. 

Simply change the `api_base` to their respective endpoint, update the `model` parameter, and ensure the authorization header format matches what that provider expects!

// Bind to a hotkey, e.g., Ctrl+J
Editor.setKeyBinding("Ctrl+J", "gemini_ask");
```

## 3. Running the Integration

1. Restart Ecode to load the updated `ecodeinit.js`.
2. Select some text in the editor.
3. Press `Ctrl+J` (or your assigned hotkey) to trigger the Gemini macro.
4. Output will be displayed in the editor or the `*Messages*` buffer based on your macro implementation.
