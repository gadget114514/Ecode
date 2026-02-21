/**
 * Ecode Setup Wizard
 * Guides the user through basic editor, AI, and LSP configuration.
 */

var setupStep = 0;
var setupConfig = {};

function run_setup_wizard() {
    setupStep = 0;
    setupConfig = {};
    console.log("Starting Ecode Setup Wizard...");
    nextSetupStep();
}

function nextSetupStep() {
    setupStep++;
    switch (setupStep) {
        case 1:
            Editor.showMinibuffer("Step 1/5: Select Language (0:EN, 1:JP, 2:ES, 3:FR, 4:DE):", "callback", "setup_step_lang");
            break;
        case 2:
            Editor.showMinibuffer("Step 2/5: AI Provider (gemini, openai, ollama):", "callback", "setup_step_ai_provider");
            break;
        case 3:
            Editor.showMinibuffer("Step 3/5: AI API Key (leave empty for local Ollama):", "callback", "setup_step_ai_key");
            break;
        case 4:
            Editor.showMinibuffer("Step 4/5: LSP Server Path (e.g., clangd, pyright, gopls):", "callback", "setup_step_lsp");
            break;
        case 5:
            Editor.showMinibuffer("Step 5/5: Allowed Project Directory (Ecode will only edit files here):", "callback", "setup_step_project_dir");
            break;
        default:
            finishSetupWizard();
            break;
    }
}

function setup_step_lang(val) {
    if (val === "") val = "0";
    setupConfig.language = parseInt(val);
    Editor.setLanguage(setupConfig.language);
    nextSetupStep();
}

function setup_step_ai_provider(provider) {
    if (provider === "") provider = "gemini";
    setupConfig.aiProvider = provider.toLowerCase();
    nextSetupStep();
}

function setup_step_ai_key(key) {
    setupConfig.aiKey = key;
    nextSetupStep();
}

function setup_step_lsp(lspPath) {
    setupConfig.lspPath = lspPath || "clangd";
    nextSetupStep();
}

function setup_step_project_dir(dir) {
    setupConfig.projectDir = dir || "C:/";
    nextSetupStep();
}

function finishSetupWizard() {
    // Apply AI Config
    var aiConfig = getAiConfig();
    aiConfig.activeServer = setupConfig.aiProvider;
    aiConfig.allowedProjectDir = setupConfig.projectDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    if (!aiConfig.servers[setupConfig.aiProvider]) {
        aiConfig.servers[setupConfig.aiProvider] = { provider: setupConfig.aiProvider };
    }
    aiConfig.servers[setupConfig.aiProvider].apiKey = setupConfig.aiKey;

    // Set default models if new
    if (setupConfig.aiProvider === "gemini" && !aiConfig.servers.gemini.model) aiConfig.servers.gemini.model = "gemini-1.5-flash";
    if (setupConfig.aiProvider === "openai" && !aiConfig.servers.openai.model) aiConfig.servers.openai.model = "gpt-4o";
    if (setupConfig.aiProvider === "ollama") {
        aiConfig.servers.ollama.model = "llama3";
        aiConfig.servers.ollama.apiBase = "http://localhost:11434/v1";
    }

    saveAiConfig(aiConfig);

    // Start LSP if path provided
    if (setupConfig.lspPath) {
        Editor.lspStart(setupConfig.lspPath, setupConfig.projectDir);
    }

    // Save general settings
    Editor.saveSettings();

    Editor.setStatusText("Setup Complete! AI and LSP configured.");
    console.log("Setup Wizard finished successfully.");

    // Open a welcome message
    Editor.newFile("*Welcome*");
    Editor.setScratch(true);
    Editor.insert(0, "# Welcome to Ecode!\n\nYour environment is now configured.\n\n- Language: " + setupConfig.language +
        "\n- AI Server: " + setupConfig.aiProvider +
        "\n- Project Root: " + setupConfig.projectDir +
        "\n- LSP Server: " + setupConfig.lspPath +
        "\n\nPress Alt+A for AI Completion or Alt+I for AI Console.\nHappy Coding!");
}

// Global command
function setup_wizard() {
    run_setup_wizard();
}
