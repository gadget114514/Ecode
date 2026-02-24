var AI_API_KEY = ""; // Prefer environment variable ECODE_AI_KEY
var aiContexts = {}; // Key: buffer path, Value: array of {role: string, content: string}
var aiCaches = {};   // Key: buffer path, Value: cacheName

/**
 * Call the AI API using curl for better performance on Win32
 */
function callAiApi(url, method, headers, payload) {
    var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
    var tmpFile = appData + "\\Ecode\\ai_payload.json";

    var jsonPayload = JSON.stringify(payload);
    if (Editor.writeFile(tmpFile, jsonPayload)) {
        var logHArgs = "";
        var realHArgs = "";
        for (var h in headers) {
            var val = headers[h];
            realHArgs += ' -H "' + h + ': ' + val + '"';

            // Mask API keys in logs
            if (h.toLowerCase().indexOf("authorization") !== -1 || h.toLowerCase().indexOf("key") !== -1) {
                val = "********";
            }
            logHArgs += ' -H "' + h + ': ' + val + '"';
        }

        console.log("AI API Request: " + method + " " + url);
        console.log("AI API Headers: " + logHArgs);
        console.log("AI API Payload (excerpt): " + jsonPayload.substring(0, 500) + (jsonPayload.length > 500 ? "..." : ""));

        var cmd = 'curl -s -X ' + method + ' "' + url + '"' + realHArgs + ' -d "@' + tmpFile + '"';
        var result = Editor.runCommand(cmd);

        console.log("AI API Response (excerpt): " + result.substring(0, 1000) + (result.length > 1000 ? "..." : ""));
        return result;
    }
    console.log("AI API Error: Could not write temporary payload file.");
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
        // Try to load from config
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
        // Keep last 50 for file size sanity, but in-memory it's unlimited
        config.histories[path] = aiContexts[path].slice(-50);
    }
    saveAiConfig(config);
}

function clearBufferHistory() {
    var path = getBufferPath();
    aiContexts[path] = [];
    if (aiCaches[path]) {
        aiCaches[path] = null;
    }
    saveBufferHistory();
}

function getAiConfig() {
    try {
        var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
        var configFile = appData + "\\Ecode\\ai_config.json";
        var content = Editor.runCommand('powershell -Command "if(Test-Path \'' + configFile + '\'){ Get-Content \'' + configFile + '\' }"').trim();
        if (content && content.indexOf("{") === 0) {
            return JSON.parse(content);
        }
    } catch (e) { console.log("Error reading AI config: " + e); }

    // Default config if missing
    return {
        activeServer: "gemini",
        maxHistoryItems: 6,
        contextBefore: 2000,
        contextAfter: 1000,
        allowedProjectDir: "",
        activeAgent: "coder",
        agents: {
            "coder": {
                "name": "General Coder",
                "systemPrompt": "You are an AI coding assistant. You help write, refactor, and explain code with precision."
            },
            "architect": {
                "name": "Software Architect",
                "systemPrompt": "You are a Senior Software Architect. Focus on system design, patterns, and long-term maintainability."
            },
            "bug_hunter": {
                "name": "Bug Hunter",
                "systemPrompt": "You are an expert debugger. Focus on finding edge cases, security vulnerabilities, and logic errors."
            }
        },
        servers: {
            "gemini": {
                "provider": "gemini",
                "model": "gemini-1.5-flash",
                "apiKey": ""
            }
        }
    };
}

function isPathAllowed(path) {
    var config = getAiConfig();
    var allowedDir = config.allowedProjectDir;
    if (!allowedDir) return true; // No restriction if not specified

    var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
    var normAppData = appData.replace(/\\/g, "/").toLowerCase();

    // Normalize path for comparison
    var normPath = path.replace(/\\/g, "/").toLowerCase();

    // Always allow app data for internal config
    if (normPath.indexOf(normAppData) === 0) return true;

    var normAllowed = allowedDir.replace(/\\/g, "/").toLowerCase();
    if (normAllowed.lastIndexOf("/") !== normAllowed.length - 1) normAllowed += "/";

    // Handle relative paths by assuming they are relative to the active buffer or workspace root
    // For now, if it doesn't have a drive letter or start with /, we check if allowedDir + path is okay
    if (normPath.indexOf(":") === -1 && normPath.indexOf("/") !== 0) {
        // Assume relative to allowedDir if it's set
        return true;
    }

    if (path === "active") {
        var activePath = getBufferPath();
        if (activePath === "untitled") return true;
        return activePath.replace(/\\/g, "/").toLowerCase().indexOf(normAllowed) === 0;
    }

    return normPath.indexOf(normAllowed) === 0;
}

/**
 * Wrapped Editor.writeFile to enforce restrictions
 */
function safeWriteFile(path, content) {
    if (!isPathAllowed(path)) {
        console.log("AI Warning: Blocked Editor.writeFile to unauthorized path: " + path);
        return false;
    }
    return Editor.writeFile(path, content);
}

function saveAiConfig(config) {
    try {
        var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
        var configFile = appData + "\\Ecode\\ai_config.json";
        var json = JSON.stringify(config);
        var escapedJson = json.replace(/'/g, "''");
        Editor.runCommand('powershell -Command "Set-Content -Path \'' + configFile + '\' -Value \'' + escapedJson + '\'"');
        return true;
    } catch (e) {
        console.log("Error saving AI config: " + e);
        return false;
    }
}

function getActiveServer() {
    var config = getAiConfig();
    var serverName = config.activeServer || "gemini";
    var server = config.servers ? config.servers[serverName] : null;
    if (!server && serverName === "gemini") {
        // Fallback for old configs
        server = { provider: "gemini", model: "gemini-1.5-flash", apiKey: config.apiKey || "" };
    }
    return server;
}

function getActiveAgent() {
    var config = getAiConfig();
    var agentId = config.activeAgent || "coder";
    if (!config.agents) {
        return { name: "General Coder", systemPrompt: "You are an AI coding assistant." };
    }
    return config.agents[agentId] || config.agents["coder"];
}

function switchAgent(agentId) {
    var config = getAiConfig();
    if (config.agents && config.agents[agentId]) {
        config.activeAgent = agentId;
        saveAiConfig(config);
        Editor.setStatusText("Agent switched to: " + config.agents[agentId].name);
        return true;
    }
    return false;
}

/**
 * Securely set the AI API key for active server
 */
function setAiKey(key) {
    if (!key) return;
    var config = getAiConfig();
    var serverName = config.activeServer || "gemini";
    if (!config.servers) config.servers = {};
    if (!config.servers[serverName]) config.servers[serverName] = { provider: serverName };

    config.servers[serverName].apiKey = key;
    if (saveAiConfig(config)) {
        Editor.setStatusText("AI Key saved for " + serverName);
        AI_API_KEY = key;
    }
}

/**
 * Switch active AI server
 */
function switchAiServer(serverName) {
    var config = getAiConfig();
    if (config.servers && config.servers[serverName]) {
        config.activeServer = serverName;
        saveAiConfig(config);
        Editor.setStatusText("Switched AI to " + serverName);
    } else {
        // Create a skeleton if it doesn't exist? (Optional)
        Editor.setStatusText("Server " + serverName + " not found in config.");
    }
}

/**
 * AI Console implementation
 */
function openAiConsole() {
    var buffers = Editor.getBuffers();
    var aiBufferIndex = -1;
    for (var i = 0; i < buffers.length; i++) {
        if (buffers[i].path === "*AI*") {
            aiBufferIndex = i;
            break;
        }
    }

    if (aiBufferIndex === -1) {
        Editor.newFile("*AI*");
        Editor.setScratch(true);
    } else {
        Editor.switchBuffer(aiBufferIndex);
    }

    // If it's empty, add instructions
    if (Editor.getLength() === 0) {
        var config = getAiConfig();
        var activeServer = config.activeServer || "gemini";
        var activeAgent = config.activeAgent || "coder";
        Editor.insert(0, "// Ecode AI Console (Server: " + activeServer + ", Agent: " + activeAgent + ")\n// Commands:\n//   /server [name] - Switch active server\n//   /server_add [name] [provider] [api_base] [model] - Add new server\n//   /agent [id] - Switch active agent persona\n//   /agent_add [id] [name] [prompt] - Add new agent persona\n//   /clear - Clear conversation history for this buffer\n// Tip: Use @filename in your prompt to include file context.\n\n> ");
        Editor.setCaretPos(Editor.getLength());
    }
}

// Global hook for key events
var originalKeyHandler = null; // We can't easily get it, so we'll just chain it if we set one ourselves later.

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
                if (instruction.indexOf("/server ") === 0) {
                    var newServer = instruction.substring(8).trim();
                    switchAiServer(newServer);
                    Editor.insert(Editor.getLength(), "\nSwitched to " + newServer + "\n\n> ");
                    return true;
                }
                if (instruction.indexOf("/server_add ") === 0) {
                    var parts = instruction.substring(12).split(" ");
                    if (parts.length >= 2) {
                        var name = parts[0];
                        var config = getAiConfig();
                        if (!config.servers) config.servers = {};
                        var serverData = { provider: parts[1] };
                        if (parts[2]) {
                            if (parts[2].indexOf("http") === 0) {
                                if (parts[2].indexOf("/chat/completions") !== -1) serverData.url = parts[2];
                                else serverData.apiBase = parts[2];
                            } else {
                                serverData.model = parts[2];
                            }
                        }
                        if (parts[3]) serverData.model = parts[3];
                        config.servers[name] = serverData;
                        saveAiConfig(config);
                        Editor.insert(Editor.getLength(), "\nAdded server " + name + " (Provider: " + parts[1] + "). Use set_ai_key to set its key.\n\n> ");
                        return true;
                    }
                }
                if (instruction.indexOf("/agent ") === 0) {
                    var newAgent = instruction.substring(7).trim();
                    if (switchAgent(newAgent)) {
                        Editor.insert(Editor.getLength(), "\nSwitched to agent: " + newAgent + "\n\n> ");
                    } else {
                        Editor.insert(Editor.getLength(), "\nAgent " + newAgent + " not found.\n\n> ");
                    }
                    return true;
                }
                if (instruction.indexOf("/agent_add ") === 0) {
                    var parts = instruction.substring(11).split(" ");
                    if (parts.length >= 3) {
                        var id = parts[0];
                        var name = parts[1];
                        var prompt = parts.slice(2).join(" ");
                        var config = getAiConfig();
                        if (!config.agents) config.agents = {};
                        config.agents[id] = { name: name, systemPrompt: prompt };
                        saveAiConfig(config);
                        Editor.insert(Editor.getLength(), "\nAgent " + name + " (" + id + ") added.\n\n> ");
                    } else {
                        Editor.insert(Editor.getLength(), "\nUsage: /agent_add [id] [name] [prompt...]\n\n> ");
                    }
                    return true;
                }
                if (instruction === "/clear") {
                    clearBufferHistory();
                    Editor.insert(Editor.getLength(), "\nContext cleared for this buffer.\n\n> ");
                    return true;
                }
                if (instruction === "/cache_clear") {
                    var path = getBufferPath();
                    aiCaches[path] = null;
                    Editor.insert(Editor.getLength(), "\nLocal cache reference cleared (Remote cache will expire).\n\n> ");
                    return true;
                }
                if (instruction) {
                    Editor.insert(Editor.getLength(), "\nThinking...\n");
                    aiExecute(instruction, "", "", -1, -1, true);
                    return true;
                }
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

            // Look for (id) or sid patterns
            var agentMatch = lineText.match(/\(([^)]+)\)\s*\[ACTIVE\]/); // Already active
            if (!agentMatch) agentMatch = lineText.match(/### .*\(([^)]+)\)/); // Agent id in header
            if (agentMatch) {
                switchAgent(agentMatch[1]);
                open_ai_agent_manager(); // Refresh
                return true;
            }

            var serverMatch = lineText.match(/^- ([^ ]+) \(/); // Server id
            if (serverMatch) {
                switchAiServer(serverMatch[1]);
                open_ai_agent_manager(); // Refresh
                return true;
            }

            if (lineText.indexOf("[Add New Agent Persona]") !== -1) {
                Editor.showMinibuffer("New Agent ID (lowercase, no spaces):", "callback", "manager_add_agent_step1");
                return true;
            }
        }
        if (key === "r" || key === "R") {
            open_ai_agent_manager();
            return true;
        }
        if (key === "d" || key === "D") {
            // Delete logic? Maybe too dangerous for a single key. 
            // Let's stick to activation for now.
        }
        return true; // Consume keys in manager
    }

    // Fallback to other logic if needed (e.g. if we had a previous handler)
    return false;
}

Editor.setKeyHandler(aiKeyHandler);

/**
 * Core AI Execution
 * @param {boolean} isConsole 
 */
function aiExecute(instruction, before, after, replaceStart, replaceEnd, isConsole) {
    var server = getActiveServer();
    var isLocal = server && server.url && (server.url.indexOf("http://localhost") === 0 || server.url.indexOf("http://127.0.0.1") === 0);

    if (!server || (!server.apiKey && !isLocal)) {
        Editor.setStatusText("AI Error: API Key or Server not configured.");
        if (isConsole) Editor.insert(Editor.getLength(), "Error: API Key or Server not configured.\n> ");
        return;
    }

    var agent = getActiveAgent();
    var systemPrompt = "You are an AI coding assistant in Ecode.\n" +
        (agent ? agent.systemPrompt + "\n" : "") +
        "You can modify files. If you want to update a file, use the format:\n" +
        "@@@REPLACE [path] [start_offset] [length]@@@\n[new_code]\n@@@END@@@\n\n" +
        "Otherwise, just output the code to be inserted at the cursor.";

    var config = getAiConfig();
    if (config.allowedProjectDir) {
        systemPrompt += "\nIMPORTANT: You are only allowed to modify files under the following directory: " + config.allowedProjectDir +
            "\nPlease ensure any @@@REPLACE ... @@@ commands use paths within this directory.";
    }

    var history = getBufferHistory();
    history.push({ role: "user", content: instruction });

    // AI logic: unlimited in memory, but sliding window for the API prompt
    var maxWindow = config.maxHistoryItems || 10;
    var promptHistory = history;
    if (maxWindow > 0 && history.length > maxWindow) {
        promptHistory = history.slice(history.length - maxWindow);
    }

    var psCmd = "";
    var apiKey = server.apiKey;
    var targetUrl = server.url || (server.provider === "gemini" ? "https://generativelanguage.googleapis.com/v1beta/models/" + (server.model || "gemini-1.5-flash") + ":generateContent" : "https://api.openai.com/v1/chat/completions");
    var isGeminiOpenAI = (server.provider === "openai" || server.provider === "custom") &&
        (targetUrl.indexOf("generativelanguage.googleapis.com") !== -1);

    var contextText = getLspContext(); // Incorporate LSP diagnostics
    if (before || after) {
        contextText += "CONTEXT BEFORE:\n" + before + "\nCONTEXT AFTER:\n" + after + "\n\n";
    }

    var workspaceContext = "";
    var mentionRegex = /@([\w\.\-\\]+)/g;
    var match;
    var mentionedIndices = [];
    // Reset regex lastIndex because we might reuse it if we move it to global
    mentionRegex.lastIndex = 0;
    while ((match = mentionRegex.exec(instruction)) !== null) {
        var mention = match[1];
        var buffers = Editor.getBuffers();
        for (var i = 0; i < buffers.length; i++) {
            var b = buffers[i];
            var bName = b.path.split(/[\\\/]/).pop();
            // Match by filename or full path
            if (bName === mention || b.path.indexOf(mention) !== -1) {
                if (mentionedIndices.indexOf(b.index) === -1) {
                    mentionedIndices.push(b.index);
                    var currentIdx = Editor.getActiveBufferIndex();
                    Editor.switchBuffer(b.index);
                    // Read full text (limit to 10k chars to avoid blowing up context?)
                    var len = Editor.getLength();
                    var text = Editor.getText(0, Math.min(len, 10000));
                    Editor.switchBuffer(currentIdx);
                    workspaceContext += "ADDITIONAL CONTEXT FILE: " + b.path + "\n---\n" + text + (len > 10000 ? "\n[Truncated...]" : "") + "\n---\n\n";
                }
                break;
            }
        }
    }

    if (workspaceContext) {
        contextText = workspaceContext + contextText;
    }

    var cachedContentName = null;

    if (server.provider === "gemini" || isGeminiOpenAI) {
        var baseUrl = "https://generativelanguage.googleapis.com/v1beta";
        var modelName = (server.model && server.model.indexOf("models/") === 0) ? server.model : ("models/" + (server.model || "gemini-1.5-flash"));
        var bufferPath = getBufferPath();
        var cacheEnabled = (config.useCaching !== false);
        cachedContentName = aiCaches[bufferPath];

        if (cacheEnabled && history.length >= 4) {
            if (!cachedContentName) {
                var cacheContents = [{ role: "user", parts: [{ text: systemPrompt }] }];
                for (var i = 0; i < history.length - 1; i++) {
                    var role = (history[i].role === "user") ? "user" : "model";
                    cacheContents.push({ role: role, parts: [{ text: history[i].content }] });
                }

                var cacheResponse = callAiApi(baseUrl + "/cachedContents?key=" + apiKey, "POST", { "Content-Type": "application/json" }, {
                    model: modelName,
                    contents: cacheContents,
                    ttl: "600s"
                });

                try {
                    var respObj = JSON.parse(cacheResponse);
                    if (respObj && respObj.name) {
                        aiCaches[bufferPath] = respObj.name;
                        cachedContentName = respObj.name;
                    }
                } catch (e) { console.log("Cache creation failed: " + cacheResponse); }
            }
        }

        if (isGeminiOpenAI) {
            var messages = [{ role: "system", content: systemPrompt }];
            if (!cachedContentName && contextText) {
                messages.push({ role: "system", content: "File context:\n" + contextText });
            }
            for (var i = 0; i < promptHistory.length; i++) {
                messages.push({ role: promptHistory[i].role, content: promptHistory[i].content });
            }

            var payload = { model: server.model || "gemini-1.5-flash", messages: messages };
            if (cachedContentName) payload.cached_content = cachedContentName;

            var response = callAiApi(targetUrl + (targetUrl.indexOf("?") === -1 ? "?key=" + apiKey : "&key=" + apiKey), "POST", { "Content-Type": "application/json" }, payload);
            try {
                var respObj = JSON.parse(response);
                aiText = respObj.choices[0].message.content;
            } catch (e) { aiText = "Error parsing response: " + response; }
        } else {
            var currentContents = [];
            var lastTurn = history[history.length - 1];
            currentContents.push({ role: "user", parts: [{ text: (contextText + lastTurn.content) }] });

            var payload = { contents: currentContents };
            if (cachedContentName) {
                payload.cachedContent = cachedContentName;
            } else {
                var fullContents = [{ role: "user", parts: [{ text: systemPrompt }] }];
                if (contextText) {
                    fullContents.push({ role: "user", parts: [{ text: "File context loaded." }] });
                    fullContents.push({ role: "model", parts: [{ text: "Understood." }] });
                }
                for (var i = 0; i < promptHistory.length; i++) {
                    var role = (promptHistory[i].role === "user") ? "user" : "model";
                    fullContents.push({ role: role, parts: [{ text: promptHistory[i].content }] });
                }
                payload.contents = fullContents;
            }

            var nativeUrl = targetUrl + (targetUrl.indexOf("?") === -1 ? "?key=" + apiKey : "&key=" + apiKey);
            if (nativeUrl.indexOf(":generateContent") === -1) nativeUrl = nativeUrl.replace(/\?/, ":generateContent?");

            var response = callAiApi(nativeUrl, "POST", { "Content-Type": "application/json" }, payload);
            try {
                var respObj = JSON.parse(response);
                aiText = respObj.candidates[0].content.parts[0].text;
            } catch (e) { aiText = "Error parsing response: " + response; }
        }
    } else if (server.provider === "openai" || server.provider === "custom" || server.provider === "ollama") {
        var url = server.url;
        if (!url && server.apiBase) {
            url = server.apiBase;
            if (url.lastIndexOf("/") !== url.length - 1) url += "/";
            url += "chat/completions";
        }
        if (!url) url = "https://api.openai.com/v1/chat/completions";

        var messages = [{ role: "system", content: systemPrompt }];
        if (contextText) messages.push({ role: "system", content: "File context:\n" + contextText });
        for (var i = 0; i < promptHistory.length; i++) messages.push({ role: promptHistory[i].role, content: promptHistory[i].content });

        var payload = { model: server.model || "gpt-4o", messages: messages };
        var headers = { "Content-Type": "application/json" };
        if (server.apiKey && server.apiKey !== "local") {
            headers["Authorization"] = "Bearer " + server.apiKey;
        }

        var response = callAiApi(url, "POST", headers, payload);
        try {
            var respObj = JSON.parse(response);
            if (respObj.choices && respObj.choices[0] && respObj.choices[0].message) {
                aiText = respObj.choices[0].message.content;
            } else if (respObj.error) {
                aiText = "API Error: " + respObj.error.message;
            } else {
                aiText = "Error: Unexpected API response format.";
            }
        } catch (e) { aiText = "Error parsing response: " + response; }
    }

    if (aiText && aiText.indexOf("Error:") === -1) {
        var hist = getBufferHistory();
        hist.push({ role: "assistant", content: aiText });
        saveBufferHistory(); // Persist the interaction
        processAiResponse(aiText, replaceStart, replaceEnd, isConsole);
    } else {
        var err = "AI Error: " + aiText;
        if (isConsole) Editor.insert(Editor.getLength(), err + "\n\n> ");
        Editor.setStatusText("AI Error.");
    }
}

function processAiResponse(text, replaceStart, replaceEnd, isConsole) {
    // Check for @@@REPLACE ... @@@ pattern
    var replaceRegex = /@@@REPLACE (.*?) (\d+) (\d+)@@@\n([\s\S]*?)\n@@@END@@@/g;
    var match;
    var appliedCount = 0;

    var remainingText = text;
    while ((match = replaceRegex.exec(text)) !== null) {
        var path = match[1];
        var start = parseInt(match[2]);
        var len = parseInt(match[3]);
        var newCode = match[4];

        applyUpdate(path, start, len, newCode);
        appliedCount++;
        remainingText = remainingText.replace(match[0], "[Applied update to " + path + "]");
    }

    if (appliedCount > 0) {
        Editor.setStatusText("Applied " + appliedCount + " updates.");
    }

    if (isConsole) {
        Editor.insert(Editor.getLength(), remainingText + "\n\n> ");
        Editor.setCaretPos(Editor.getLength());
    } else if (remainingText.trim()) {
        if (replaceStart !== -1) {
            Editor.delete(replaceStart, replaceEnd - replaceStart);
            Editor.insert(replaceStart, remainingText.trim());
        } else {
            Editor.insert(Editor.getCaretPos(), remainingText.trim());
        }
    }
}

function applyUpdate(path, start, len, newCode) {
    if (!isPathAllowed(path)) {
        console.log("AI Warning: Blocked write attempt to unauthorized path: " + path);
        return;
    }

    var targetIdx = -1;
    var buffers = Editor.getBuffers();
    for (var i = 0; i < buffers.length; i++) {
        if (buffers[i].path === path || (path === "active" && i === Editor.getActiveBufferIndex())) {
            targetIdx = i;
            break;
        }
    }

    // If not open, try to open it
    if (targetIdx === -1 && path !== "active") {
        if (Editor.open(path)) {
            var newBuffers = Editor.getBuffers();
            for (var j = 0; j < newBuffers.length; j++) {
                if (newBuffers[j].path === path) {
                    targetIdx = j;
                    break;
                }
            }
        } else {
            // If it doesn't exist, try to create it
            if (safeWriteFile(path, "")) {
                if (Editor.open(path)) {
                    var newBuffers2 = Editor.getBuffers();
                    for (var k = 0; k < newBuffers2.length; k++) {
                        if (newBuffers2[k].path === path) {
                            targetIdx = k;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (targetIdx !== -1) {
        var currentIdx = Editor.getActiveBufferIndex();
        Editor.switchBuffer(targetIdx);

        var totalLen = Editor.getLength();
        var safeStart = Math.min(start, totalLen);
        var safeLen = Math.min(len, totalLen - safeStart);

        Editor.delete(safeStart, safeLen);
        Editor.insert(safeStart, newCode);
        Editor.switchBuffer(currentIdx);
    }
}

function aiComplete() {
    var anchor = Editor.getSelectionAnchor();
    var caret = Editor.getCaretPos();
    var start = Math.min(anchor, caret);
    var end = Math.max(anchor, caret);
    var selection = "";

    if (start != end) {
        selection = Editor.getText(start, end - start);
    }

    if (selection === "") {
        Editor.showMinibuffer("AI Instruction (Alt+A):", "callback", "aiCompleteWithInstruction");
        return;
    }

    var config = getAiConfig();
    var limitBefore = config.contextBefore || 2000;
    var limitAfter = config.contextAfter || 1000;

    var contextBefore = Editor.getText(Math.max(0, start - limitBefore), start - Math.max(0, start - limitBefore));
    var contextAfter = Editor.getText(end, Math.min(Editor.getLength() - end, limitAfter));

    aiExecute(selection, contextBefore, contextAfter, start, end, false);
}

function aiCompleteWithInstruction(instruction) {
    if (!instruction) return;
    var pos = Editor.getCaretPos();
    aiExecute(instruction, "", "", pos, pos, false);
}

// Bindings
Editor.setKeyBinding("Alt+A", "aiComplete");
Editor.setKeyBinding("Alt+I", "openAiConsole");

// Global command to set key
function set_ai_key() {
    Editor.showMinibuffer("Enter Gemini API Key:", "callback", "setAiKey");
}

/**
 * Helper to get LSP context for the current buffer
 */
function getLspContext() {
    var path = getBufferPath();
    if (path === "untitled" || path === "*AI*") return "";

    // Sync with server
    var content = Editor.getText(0, Editor.getLength());
    Editor.lspNotify("textDocument/didOpen", JSON.stringify({
        textDocument: { uri: "file:///" + path.replace(/\\/g, "/"), languageId: "cpp", version: 1, text: content }
    }));

    // Get diagnostics
    var diagRaw = Editor.lspGetDiagnostics();
    if (diagRaw) {
        try {
            var diag = JSON.parse(diagRaw);
            if (diag.params && diag.params.diagnostics && diag.params.diagnostics.length > 0) {
                var out = "LSP DIAGNOSTICS for " + path + ":\n";
                diag.params.diagnostics.forEach(function (d) {
                    out += "- Line " + (d.range.start.line + 1) + ": " + d.message + " (" + d.severity + ")\n";
                });
                return out + "\n";
            }
        } catch (e) { }
    }
    return "";
}

/**
 * Start LSP for current project
 */
function lsp_start(serverPath) {
    if (!serverPath) {
        Editor.showMinibuffer("LSP Server Path (e.g. clangd):", "callback", "lsp_start");
        return;
    }
    var config = getAiConfig();
    var root = config.allowedProjectDir || "C:/";
    if (Editor.lspStart(serverPath, root)) {
        Editor.setStatusText("LSP started: " + serverPath);
    } else {
        Editor.setStatusText("LSP failed to start.");
    }
}

function open_ai_agent_manager() {
    var buffers = Editor.getBuffers();
    var managerIdx = -1;
    for (var i = 0; i < buffers.length; i++) {
        if (buffers[i].path === "*AI Manager*") {
            managerIdx = i;
            break;
        }
    }

    if (managerIdx === -1) {
        Editor.newFile("*AI Manager*");
        Editor.setScratch(true);
    } else {
        Editor.switchBuffer(managerIdx);
    }

    var config = getAiConfig();
    var out = "# AI Agent Manager Dashboard\n";
    out += "--------------------------------------------------\n";
    out += "Active Persona: " + (config.activeAgent || "coder") + "\n";
    out += "Active Server:  " + (config.activeServer || "gemini") + "\n";
    out += "Project Root:   " + (config.allowedProjectDir || "NOT SET") + "\n";
    out += "--------------------------------------------------\n\n";

    out += "## Available Agents (Personas)\n";
    out += "Move cursor to an agent name and press [Enter] to Activate.\n";
    out += ">> [Add New Agent Persona] <<\n\n";
    for (var aid in config.agents) {
        var agent = config.agents[aid];
        var active = (config.activeAgent === aid) ? " [ACTIVE]" : " [Selectable]";
        out += "### " + agent.name + " (" + aid + ")" + active + "\n";
        out += "> " + agent.systemPrompt.substring(0, 120) + (agent.systemPrompt.length > 120 ? "..." : "") + "\n\n";
    }

    out += "## Configured AI Servers\n";
    out += "Move cursor to a server name and press [Enter] to Switch.\n\n";
    for (var sid in config.servers) {
        var s = config.servers[sid];
        var activeS = (config.activeServer === sid) ? " [ACTIVE]" : " [Selectable]";
        out += "- " + sid + " (" + s.provider + (s.model ? ", " + s.model : "") + ")" + activeS + "\n";
    }

    out += "\n--- \n";
    out += "Keyboard Controls:\n";
    out += " [Enter] : Activate item under cursor\n";
    out += " [R]     : Refresh this dashboard\n";
    out += " [Alt+I] : Open AI Console\n";
    out += " [Alt+A] : Contextual Completion\n";

    Editor.delete(0, Editor.getLength());
    Editor.insert(0, out);
    Editor.setCaretPos(0);
}

var tempNewAgent = {};
function manager_add_agent_step1(id) {
    if (!id) return;
    tempNewAgent.id = id.toLowerCase().replace(/ /g, "_");
    Editor.showMinibuffer("Agent Display Name:", "callback", "manager_add_agent_step2");
}

function manager_add_agent_step2(name) {
    if (!name) return;
    tempNewAgent.name = name;
    Editor.showMinibuffer("System Prompt (Specialty):", "callback", "manager_add_agent_step3");
}

function manager_add_agent_step3(prompt) {
    if (!prompt) return;
    var config = getAiConfig();
    if (!config.agents) config.agents = {};
    config.agents[tempNewAgent.id] = { name: tempNewAgent.name, systemPrompt: prompt };
    saveAiConfig(config);
    open_ai_agent_manager(); // Refresh
    Editor.setStatusText("Agent created: " + tempNewAgent.name);
}

console.log("AI Assistant loaded. Alt+A for completion, Alt+I for Console.");
