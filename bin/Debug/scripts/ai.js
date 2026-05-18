/**
 * @file ai.js
 * @description USER CONFIGURATION LAYER
 * Divide variables which user should define and the javascript code which are provided by editor.
 * 
 * --- USERS SHOULD DEFINE THE FOLLOWING VARIABLES ---
 */

var AI_API_KEY = ""; // Prefer environment variable ECODE_AI_KEY

/**
 * AI Configuration Defaults
 * Users can customize their agents and server definitions here.
 */
function getAiConfig() {
    try {
        var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
        var configFile = appData + "\\Ecode\\ai_config.json";
        var content = Editor.runCommand('powershell -Command "if(Test-Path \'' + configFile + '\'){ Get-Content \'' + configFile + '\' }"').trim();
        if (content && content.indexOf("{") === 0) {
            return JSON.parse(content);
        }
    } catch (e) { }

    // Fallback Setup
    return {
        activeServer: "gemini",
        activeAgent: "coder",
        allowedProjectDir: "",
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

/**
 * Configuration Save Utility
 */
function saveAiConfig(config) {
    try {
        var appData = Editor.runCommand('powershell -Command "$env:APPDATA"').trim();
        var configFile = appData + "\\Ecode\\ai_config.json";
        var json = JSON.stringify(config);
        var escapedJson = json.replace(/'/g, "''");
        Editor.runCommand('powershell -Command "Set-Content -Path \'' + configFile + '\' -Value \'' + escapedJson + '\'"');
        return true;
    } catch (e) { return false; }
}

// =============================================================================
// --- EDITOR PROVIDED CODE (LOADED AS A LIBRARY) ---
// =============================================================================

try {
    Editor.loadScript("scripts/ai_core.js");
} catch (e) {
    console.log("Error: Failed to load scripts/ai_core.js. AI features will not work.");
}

// --- USER CUSTOMIZATION HOOKS & BRIDGE ---

function set_ai_key() {
    Editor.showMinibuffer("Enter Gemini API Key:", "callback", "setAiKey");
}

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

function switchAgent(agentId) {
    var config = getAiConfig();
    if (config.agents && config.agents[agentId]) {
        config.activeAgent = agentId;
        saveAiConfig(config);
        Editor.setStatusText("Agent: " + config.agents[agentId].name);
        return true;
    }
    return false;
}

function switchAiServer(serverName) {
    var config = getAiConfig();
    if (config.servers && config.servers[serverName]) {
        config.activeServer = serverName;
        saveAiConfig(config);
        Editor.setStatusText("Server: " + serverName);
    }
}

// --- INITIALIZATION & BINDINGS ---

Editor.setKeyBinding("Alt+A", "aiComplete");
Editor.setKeyBinding("Alt+I", "openAiConsole");
Editor.setKeyHandler("aiKeyHandler");

console.log("Ecode AI Configuration Layer Loaded.");
