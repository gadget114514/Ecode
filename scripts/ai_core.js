/**
 * AI CORE ENGINE - Provided by Editor
 * Contains the functional logic for AI integration.
 */

var aiContexts = {}; // Key: buffer path, Value: array of {role: string, content: string}
var aiCaches = {};   // Key: buffer path, Value: cacheName

function callAiApi(url, method, headers, payload) {
    var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
    var tmpFile = appData + "\\Ecode\\ai_payload.json";

    var jsonPayload = JSON.stringify(payload);
    if (Editor.writeFile(tmpFile, jsonPayload)) {
        var realHArgs = "";
        for (var h in headers) {
            realHArgs += ' -H "' + h + ': ' + headers[h] + '"';
        }
        var cmd = 'curl -s -X ' + method + ' "' + url + '"' + realHArgs + ' -d "@' + tmpFile + '"';
        return Editor.runCommand(cmd);
    }
    return "Error: Could not write temporary payload file.";
}

function getBufferPath() {
    var buffers = Editor.getBuffers();
    var idx = Editor.getActiveBufferIndex();
    return (buffers[idx] && buffers[idx].path) ? buffers[idx].path : "untitled";
}

function getBufferHistory() {
    var path = getBufferPath();
    if (!aiContexts[path]) {
        var config = getAiConfig();
        if (config.histories && config.histories[path]) {
            aiContexts[path] = config.histories[path];
        } else {
            aiContexts[path] = [];
        }
    }
    return aiContexts[path];
}

function saveBufferHistory() {
    var config = getAiConfig();
    if (!config.histories) config.histories = {};
    for (var path in aiContexts) {
        config.histories[path] = aiContexts[path].slice(-50);
    }
    saveAiConfig(config);
}

function clearBufferHistory() {
    var path = getBufferPath();
    aiContexts[path] = [];
    if (aiCaches[path]) aiCaches[path] = null;
    saveBufferHistory();
}

function isPathAllowed(path) {
    var config = getAiConfig();
    var allowedDir = config.allowedProjectDir;
    if (!allowedDir) return true;
    var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
    var normAppData = appData.replace(/\\/g, "/").toLowerCase();
    var normPath = path.replace(/\\/g, "/").toLowerCase();
    if (normPath.indexOf(normAppData) === 0) return true;
    var normAllowed = allowedDir.replace(/\\/g, "/").toLowerCase();
    if (normAllowed.lastIndexOf("/") !== normAllowed.length - 1) normAllowed += "/";
    if (normPath.indexOf(":") === -1 && normPath.indexOf("/") !== 0) return true;
    if (path === "active") {
        var activePath = getBufferPath();
        if (activePath === "untitled") return true;
        return activePath.replace(/\\/g, "/").toLowerCase().indexOf(normAllowed) === 0;
    }
    return normPath.indexOf(normAllowed) === 0;
}

function safeWriteFile(path, content) {
    if (!isPathAllowed(path)) return false;
    return Editor.writeFile(path, content);
}

function getActiveServer() {
    var config = getAiConfig();
    var serverName = config.activeServer || "gemini";
    return config.servers ? config.servers[serverName] : { provider: "gemini", model: "gemini-1.5-flash", apiKey: config.apiKey || "" };
}

function getActiveAgent() {
    var config = getAiConfig();
    var agentId = config.activeAgent || "coder";
    return (config.agents && config.agents[agentId]) ? config.agents[agentId] : { name: "General Coder", systemPrompt: "You are an AI coding assistant." };
}

function openAiConsole() {
    var buffers = Editor.getBuffers();
    var aiBufferIndex = -1;
    for (var i = 0; i < buffers.length; i++) {
        if (buffers[i].path === "*AI*") { aiBufferIndex = i; break; }
    }
    if (aiBufferIndex === -1) { Editor.newFile("*AI*"); Editor.setScratch(true); }
    else Editor.switchBuffer(aiBufferIndex);

    if (Editor.getLength() === 0) {
        var config = getAiConfig();
        Editor.insert(0, "// Ecode AI Console (Server: " + (config.activeServer || "gemini") + ")\n//   /server [name], /agent [id], /clear\n\n> ");
        Editor.setCaretPos(Editor.getLength());
    }
}

function aiKeyHandler(key, isChar) {
    var buffers = Editor.getBuffers();
    var currentIdx = Editor.getActiveBufferIndex();
    var currentPath = buffers[currentIdx] ? buffers[currentIdx].path : "";

    if (currentPath === "*AI*") {
        if (key === "Alt+Enter") {
            var pos = Editor.getCaretPos();
            var lineIdx = Editor.getLineAtOffset(pos);
            var lineOffset = Editor.getLineOffset(lineIdx);
            var lineText = Editor.getText(lineOffset, pos - lineOffset).trim();
            if (lineText.indexOf("> ") === 0) {
                var instruction = lineText.substring(2).trim();
                if (instruction.indexOf("/server ") === 0) { switchAiServer(instruction.substring(8).trim()); Editor.insert(Editor.getLength(), "\nServer Switched.\n\n> "); return true; }
                if (instruction.indexOf("/agent ") === 0) { switchAgent(instruction.substring(7).trim()); Editor.insert(Editor.getLength(), "\nAgent Switched.\n\n> "); return true; }
                if (instruction === "/clear") { clearBufferHistory(); Editor.insert(Editor.getLength(), "\nContext cleared.\n\n> "); return true; }
                if (instruction) { Editor.insert(Editor.getLength(), "\nThinking...\n"); aiExecute(instruction, "", "", -1, -1, true); return true; }
            }
        }
    }

    if (currentPath === "*AI Manager*") {
        if (key === "Enter") {
            var pos = Editor.getCaretPos();
            var lineIdx = Editor.getLineAtOffset(pos);
            var lineOffset = Editor.getLineOffset(lineIdx);
            var nextOffset = Editor.getLineOffset(lineIdx + 1);
            var lineText = Editor.getText(lineOffset, nextOffset - lineOffset).trim();
            var agentMatch = lineText.match(/\(([^)]+)\)\s*\[ACTIVE\]/);
            if (!agentMatch) agentMatch = lineText.match(/### .*\(([^)]+)\)/);
            if (agentMatch) { switchAgent(agentMatch[1]); open_ai_agent_manager(); return true; }
            var serverMatch = lineText.match(/^- ([^ ]+) \(/);
            if (serverMatch) { switchAiServer(serverMatch[1]); open_ai_agent_manager(); return true; }
        }
        if (key === "r" || key === "R") { open_ai_agent_manager(); return true; }
        return true;
    }
    return false;
}

function aiExecute(instruction, before, after, replaceStart, replaceEnd, isConsole) {
    var server = getActiveServer();
    var config = getAiConfig();
    var agent = getActiveAgent();
    var systemPrompt = "You are an AI coding assistant in Ecode.\n" + agent.systemPrompt + "\nFormat changes as: @@@REPLACE [path] [start] [len]@@@\n[code]\n@@@END@@@\n";

    var history = getBufferHistory();
    history.push({ role: "user", content: instruction });

    var contextText = getLspContext();
    if (config.allowedProjectDir) contextText += getUnityContext(config.allowedProjectDir);
    if (before || after) contextText += "CONTEXT BEFORE:\n" + before + "\nCONTEXT AFTER:\n" + after + "\n\n";

    var apiKey = server.apiKey;
    var targetUrl = server.url || (server.provider === "gemini" ? "https://generativelanguage.googleapis.com/v1beta/models/" + (server.model || "gemini-1.5-flash") + ":generateContent" : "https://api.openai.com/v1/chat/completions");

    var aiText = "";
    if (server.provider === "gemini") {
        var payload = { contents: [{ role: "user", parts: [{ text: systemPrompt + "\n" + contextText + "\n" + instruction }] }] };
        var response = callAiApi(targetUrl + "?key=" + apiKey, "POST", { "Content-Type": "application/json" }, payload);
        try { var obj = JSON.parse(response); aiText = obj.candidates[0].content.parts[0].text; } catch (e) { aiText = "Error: " + response; }
    } else {
        var messages = [{ role: "system", content: systemPrompt }, { role: "user", content: contextText + "\n" + instruction }];
        var payload = { model: server.model || "gpt-4o", messages: messages };
        var response = callAiApi(targetUrl, "POST", { "Content-Type": "application/json", "Authorization": "Bearer " + apiKey }, payload);
        try { var obj = JSON.parse(response); aiText = obj.choices[0].message.content; } catch (e) { aiText = "Error: " + response; }
    }

    if (aiText && aiText.indexOf("Error:") === -1) {
        history.push({ role: "assistant", content: aiText });
        processAiResponse(aiText, replaceStart, replaceEnd, isConsole);
    } else {
        if (isConsole) Editor.insert(Editor.getLength(), aiText + "\n\n> ");
    }
}

function processAiResponse(text, replaceStart, replaceEnd, isConsole) {
    var replaceRegex = /@@@REPLACE (.*?) (\d+) (\d+)@@@\n([\s\S]*?)\n@@@END@@@/g;
    var match;
    var remainingText = text;
    while ((match = replaceRegex.exec(text)) !== null) {
        applyUpdate(match[1], parseInt(match[2]), parseInt(match[3]), match[4]);
        remainingText = remainingText.replace(match[0], "[Applied update to " + match[1] + "]");
    }
    if (isConsole) {
        Editor.insert(Editor.getLength(), remainingText + "\n\n> ");
        Editor.setCaretPos(Editor.getLength());
    } else if (remainingText.trim()) {
        if (replaceStart !== -1) { Editor.delete(replaceStart, replaceEnd - replaceStart); Editor.insert(replaceStart, remainingText.trim()); }
        else { Editor.insert(Editor.getCaretPos(), remainingText.trim()); }
    }
}

function applyUpdate(path, start, len, newCode) {
    if (!isPathAllowed(path)) return;
    var buffers = Editor.getBuffers();
    var targetIdx = -1;
    for (var i = 0; i < buffers.length; i++) {
        if (buffers[i].path === path || (path === "active" && i === Editor.getActiveBufferIndex())) { targetIdx = i; break; }
    }
    if (targetIdx === -1 && path !== "active") {
        if (Editor.open(path)) {
            var nb = Editor.getBuffers();
            for (var j = 0; j < nb.length; j++) if (nb[j].path === path) targetIdx = j;
        }
    }
    if (targetIdx !== -1) {
        var curr = Editor.getActiveBufferIndex();
        Editor.switchBuffer(targetIdx);
        Editor.delete(Math.min(start, Editor.getLength()), Math.min(len, Editor.getLength() - start));
        Editor.insert(Math.min(start, Editor.getLength()), newCode);
        Editor.switchBuffer(curr);
    }
}

function aiComplete() {
    var anchor = Editor.getSelectionAnchor();
    var caret = Editor.getCaretPos();
    var start = Math.min(anchor, caret);
    var end = Math.max(anchor, caret);
    var selection = (start != end) ? Editor.getText(start, end - start) : "";
    if (selection === "") { Editor.showMinibuffer("AI Instruction:", "callback", "aiCompleteWithInstruction"); return; }
    aiExecute(selection, "", "", start, end, false);
}

function aiCompleteWithInstruction(instruction) {
    if (!instruction) return;
    aiExecute(instruction, "", "", Editor.getCaretPos(), Editor.getCaretPos(), false);
}

function getLspContext() {
    var path = getBufferPath();
    if (path === "untitled" || path === "*AI*") return "";
    var content = Editor.getText(0, Editor.getLength());
    Editor.lspNotify("textDocument/didOpen", JSON.stringify({ textDocument: { uri: "file:///" + path.replace(/\\/g, "/"), languageId: "cpp", version: 1, text: content } }));
    var diagRaw = Editor.lspGetDiagnostics();
    if (diagRaw) {
        try {
            var diag = JSON.parse(diagRaw);
            if (diag.params && diag.params.diagnostics && diag.params.diagnostics.length > 0) {
                var out = "LSP DIAGNOSTICS:\n";
                diag.params.diagnostics.forEach(function (d) { out += "- Line " + (d.range.start.line + 1) + ": " + d.message + "\n"; });
                return out + "\n";
            }
        } catch (e) { }
    }
    return "";
}

function getUnityContext(root) {
    try {
        var assetsExists = Editor.runCommand('powershell -Command "Test-Path \'' + root + '\\Assets\'"').trim();
        if (assetsExists === "True") {
            var out = "PROJECT TYPE: Unity\nASSETS: " + root + "\\Assets\n";
            var pkgs = Editor.runCommand('powershell -Command "Test-Path \'' + root + '\\Packages\'"').trim();
            if (pkgs === "True") out += "PACKAGES DETECTED\n";
            return out + "\n";
        }
    } catch (e) { }
    return "";
}

function open_ai_agent_manager() {
    var managerIdx = -1;
    var buffers = Editor.getBuffers();
    for (var i = 0; i < buffers.length; i++) if (buffers[i].path === "*AI Manager*") { managerIdx = i; break; }
    if (managerIdx === -1) { Editor.newFile("*AI Manager*"); Editor.setScratch(true); }
    else Editor.switchBuffer(managerIdx);

    var config = getAiConfig();
    var out = "# AI Agent Manager\nActive Persona: " + (config.activeAgent || "coder") + "\nActive Server: " + (config.activeServer || "gemini") + "\n\n## Agents\n";
    for (var aid in config.agents) out += "### " + config.agents[aid].name + " (" + aid + ")" + (config.activeAgent === aid ? " [ACTIVE]" : "") + "\n> " + config.agents[aid].systemPrompt.substring(0, 100) + "...\n\n";
    out += "## Servers\n";
    for (var sid in config.servers) out += "- " + sid + (config.activeServer === sid ? " [ACTIVE]" : "") + "\n";
    Editor.delete(0, Editor.getLength());
    Editor.insert(0, out);
    Editor.setCaretPos(0);
}
