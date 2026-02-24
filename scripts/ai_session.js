// scripts/ai_session.js

var ActiveSessions = ActiveSessions || {};

function ai_provider_ask() {
    var bufName = "Unknown Session";
    if (Editor.getBufferName) bufName = Editor.getBufferName();

    var sessionConfig = ActiveSessions[bufName];
    if (!sessionConfig) {
        Editor.setStatusText("Error: Not a valid AI session buffer.");
        if (Editor.logMessage) Editor.logMessage("Error: '" + bufName + "' is not a valid AI session buffer.");
        return;
    }

    var text = "";
    if (Editor.hasSelection && Editor.hasSelection()) {
        var start = Editor.getSelectionStart ? Editor.getSelectionStart() : Editor.getCaretPos();
        var end = Editor.getSelectionEnd ? Editor.getSelectionEnd() : Editor.getSelectionAnchor ? Editor.getSelectionAnchor() : Editor.getCaretPos();
        if (start > end) { var t = start; start = end; end = t; }
        text = Editor.getText(start, end - start);
    } else {
        text = Editor.getText(0, Editor.getLength());
        var promptMarker = "\n> ";
        var offset = text.lastIndexOf(promptMarker);
        if (offset !== -1) {
            text = text.substring(offset + promptMarker.length);
        } else {
            var firstIdx = text.lastIndexOf("> ");
            if (firstIdx !== -1) {
                text = text.substring(firstIdx + 2);
            }
        }
        text = text.trim();
    }

    if (!text || text.trim() === "") {
        Editor.setStatusText("AI: No text to send.");
        return;
    }

    Editor.setStatusText("Asking " + sessionConfig.vendor + "...");

    var tempFile = Editor.getAppDataPath() + "/ecode_prompt.json";
    var logFile = Editor.getAppDataPath() + "/ecode_curl.log";
    var curlOpts = " -v --stderr \"" + logFile + "\" ";
    var curlCmd = "";

    if (sessionConfig.vendor === "Gemini") {
        var isCompat = (typeof GeminiConfig !== 'undefined' && GeminiConfig.api_base && GeminiConfig.api_base.indexOf("/openai/") !== -1);
        var jsonBody = "";

        if (isCompat) {
            jsonBody = JSON.stringify({
                model: sessionConfig.model,
                messages: [{ role: "user", content: text }]
            });
            if (Editor.writeFile) Editor.writeFile(tempFile, jsonBody);

            curlCmd = "curl -s" + curlOpts + GeminiConfig.api_base +
                " -H \"Content-Type: application/json\"" +
                " -H \"Authorization: Bearer " + sessionConfig.apiKey + "\"" +
                " -d \"@" + tempFile + "\"";
        } else {
            jsonBody = JSON.stringify({
                contents: [{ parts: [{ text: text }] }]
            });
            if (Editor.writeFile) Editor.writeFile(tempFile, jsonBody);

            curlCmd = "curl -s" + curlOpts + "\"https://generativelanguage.googleapis.com/v1beta/models/" + sessionConfig.model + ":generateContent?key=" + sessionConfig.apiKey + "\"" +
                " -H \"Content-Type: application/json\"" +
                " -d \"@" + tempFile + "\"";
        }

    } else if (sessionConfig.vendor === "Anthropic") {
        var jsonBody = JSON.stringify({
            model: sessionConfig.model,
            max_tokens: 4096,
            messages: [{ role: "user", content: text }]
        });
        var psCmd = "powershell -NoProfile -Command \"Set-Content -Path '" + tempFile + "' -Value '" + jsonBody.replace(/'/g, "''") + "' -Encoding UTF8\"";
        Editor.execSync(psCmd);

        curlCmd = "curl -s" + curlOpts + "https://api.anthropic.com/v1/messages" +
            " -H \"x-api-key: " + sessionConfig.apiKey + "\"" +
            " -H \"anthropic-version: 2023-06-01\"" +
            " -H \"content-type: application/json\"" +
            " -d \"@" + tempFile + "\"";

    } else { // OpenAI
        var jsonBody = JSON.stringify({
            model: sessionConfig.model,
            messages: [{ role: "user", content: text }]
        });
        var psCmd = "powershell -NoProfile -Command \"Set-Content -Path '" + tempFile + "' -Value '" + jsonBody.replace(/'/g, "''") + "' -Encoding UTF8\"";
        Editor.execSync(psCmd);

        curlCmd = "curl -s" + curlOpts + "https://api.openai.com/v1/chat/completions" +
            " -H \"Content-Type: application/json\"" +
            " -H \"Authorization: Bearer " + sessionConfig.apiKey + "\"" +
            " -d \"@" + tempFile + "\"";
    }

    if (Editor.logMessage) {
        Editor.logMessage("Executing API Request to " + sessionConfig.vendor + " via " + tempFile);
        Editor.logMessage("Detailed communication log: " + logFile);
    }

    var responseStr = "";
    try {
        responseStr = Editor.execSync(curlCmd);
    } catch (err) {
        if (Editor.logMessage) Editor.logMessage("execSync Error: " + err);
    }

    // We intentionally don't delete the temp file immediately if there's an error so the user can inspect it
    // Editor.execSync("del " + tempFile);

    Editor.setStatusText("Parsing response...");

    try {
        var responseObj = JSON.parse(responseStr);
        var answer = "";
        var errStr = "";

        var isCompat = (typeof GeminiConfig !== 'undefined' && GeminiConfig.api_base && GeminiConfig.api_base.indexOf("/openai/") !== -1);

        if (sessionConfig.vendor === "Gemini") {
            if (isCompat && responseObj.choices && responseObj.choices.length > 0) {
                answer = responseObj.choices[0].message.content;
            } else if (!isCompat && responseObj.candidates && responseObj.candidates.length > 0) {
                answer = responseObj.candidates[0].content.parts[0].text;
            } else if (responseObj.error) {
                errStr = responseObj.error.message;
            }
        } else if (sessionConfig.vendor === "Anthropic") {
            if (responseObj.content && responseObj.content.length > 0) {
                answer = responseObj.content[0].text;
            } else if (responseObj.error) {
                errStr = responseObj.error.message;
            }
        } else { // OpenAI
            if (responseObj.choices && responseObj.choices.length > 0) {
                answer = responseObj.choices[0].message.content;
            } else if (responseObj.error) {
                errStr = responseObj.error.message;
            }
        }

        if (answer !== "") {
            var pos = Editor.getLength();
            Editor.insert(pos, "\n\n=== AI Response ===\n");
            Editor.insert(Editor.getLength(), answer + "\n=====================\n\n> ");
            Editor.moveCaret(Editor.getLength(), false);
            Editor.setStatusText("Response added.");
        } else if (errStr !== "") {
            Editor.setStatusText("API Error: " + errStr);
            Editor.logMessage("API Error: " + errStr);
        } else {
            Editor.setStatusText("Failed to parse response structure.");
            Editor.logMessage("Unknown Response: " + responseStr);
        }
    } catch (e) {
        Editor.setStatusText("Error parsing JSON response.");
        if (Editor.logMessage) {
            Editor.logMessage("JSON Parse Error: " + e);
            Editor.logMessage("Response was: " + responseStr);
            Editor.logMessage("cURL Command: " + curlCmd);
        }
    }
}

function startAISession() {
    var vendor = Editor.getAIVendor ? Editor.getAIVendor() : "Gemini";
    var model = Editor.getAIModel ? Editor.getAIModel() : "gemini-1.5-pro";
    if (vendor === "Gemini" && typeof GeminiConfig !== 'undefined' && GeminiConfig.model) {
        model = GeminiConfig.model;
    }
    var apiKey = Editor.getAIApiKey ? Editor.getAIApiKey(vendor) : "";

    if (!apiKey || apiKey === "") {
        Editor.logMessage("Warning: API Key for " + vendor + " is blank. Set it in Config -> AI Settings.");
    }

    Editor.newFile();
    var bufName = vendor + " (" + model + ") Session";

    // Add uniqueness if opened multiple times
    var finalBufName = bufName;
    var counter = 1;
    while (ActiveSessions[finalBufName]) {
        finalBufName = bufName + " " + counter;
        counter++;
    }

    ActiveSessions[finalBufName] = {
        vendor: vendor,
        model: model,
        apiKey: apiKey
    };

    if (Editor.setBufferName) {
        Editor.setBufferName(finalBufName);
    }

    var header = "// Ecode AI Session: " + vendor + " - " + model + "\n" +
        "// Type your prompt below and press Ctrl+Enter to send.\n\n" +
        "> ";

    // Set buffer focus and binding
    Editor.insert(0, header);
    Editor.moveCaret(Editor.getLength(), false);

    // Bind the local key.
    Editor.setKeyBinding("Ctrl+Enter", "ai_provider_ask");
    Editor.setKeyBinding("Ctrl+Return", "ai_provider_ask");

    if (Editor.logMessage) {
        Editor.logMessage("Started AI Session buffer: " + finalBufName + " and bound Ctrl+Enter.");
    }
}
