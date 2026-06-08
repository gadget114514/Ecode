var setupStep = 0;
var setupConfig = {};
var setupTotalSteps = 0;
var setupSteps = [];

function run_setup_wizard() {
    setupConfig = {};
    setupSteps = [
        { name: "language", prompt: "Step 1/8: Select Language (0:EN, 1:JP, 2:ES, 3:FR, 4:DE, 5:PT):", callback: "wizard_step_language", validate: function(v) { return true; } },
        { name: "use_case", prompt: "Step 2/8: Use Case (0:General, 1:Developer, 2:Writer, 3:Researcher, 4:Data Scientist, 5:Japanese Dev):", callback: "wizard_step_usecase", validate: function(v) { return true; } },
        { name: "ai_provider", prompt: "Step 3/8: AI Provider (gemini, openai, ollama, anthropic):", callback: "wizard_step_ai_provider", validate: function(v) { return true; } },
        { name: "ai_model", prompt: "Step 4/8: AI Model (e.g., gemini-1.5-flash, gpt-4o, claude-sonnet-4-6, llama3):", callback: "wizard_step_ai_model", validate: function(v) { return true; } },
        { name: "ai_key", prompt: "Step 5/8: AI API Key (leave empty for local Ollama):", callback: "wizard_step_ai_key", validate: function(v) { return true; } },
        { name: "lsp", prompt: "Step 6/8: LSP Server Path (enter 0 to skip, e.g., clangd, pyright, gopls):", callback: "wizard_step_lsp", validate: function(v) { return true; } },
        { name: "project_dir", prompt: "Step 7/8: Allowed Project Directory (Ecode will only edit files here):", callback: "wizard_step_project_dir", validate: function(v) { return true; } },
        { name: "theme", prompt: "Step 8/8: Theme (0:Default Dark, 1:Light, 2:Monokai, 3:One Dark, 4:Nord, 5:Custom):", callback: "wizard_step_theme", validate: function(v) { return true; } },
    ];
    setupTotalSteps = setupSteps.length;
    setupStep = 0;
    console.log("Starting Enhanced Ecode Setup Wizard...");
    nextSetupStep();
}

function nextSetupStep() {
    if (setupStep >= setupTotalSteps) {
        finishSetupWizard();
        return;
    }
    var s = setupSteps[setupStep];
    Editor.showMinibuffer(s.prompt, "callback", s.callback);
}

function wizard_step_language(val) {
    if (val === "") val = "0";
    var langMap = { "0": "en", "1": "ja", "2": "es", "3": "fr", "4": "de", "5": "pt" };
    setupConfig.language = langMap[val] || "en";
    Editor.setLanguage(setupConfig.language);
    console.log("  Language set to: " + setupConfig.language);
    setupStep++;
    nextSetupStep();
}

function wizard_step_usecase(val) {
    if (val === "") val = "0";
    var ucMap = { "0": "general", "1": "developer", "2": "writer", "3": "researcher", "4": "data_scientist", "5": "japanese_dev" };
    setupConfig.useCase = ucMap[val] || "general";
    console.log("  Use Case: " + setupConfig.useCase);
    setupStep++;
    nextSetupStep();
}

function wizard_step_ai_provider(val) {
    if (val === "") val = "gemini";
    var provider = val.toLowerCase();
    var validProviders = ["gemini", "openai", "ollama", "anthropic"];
    if (validProviders.indexOf(provider) === -1) {
        Editor.showMinibuffer("Invalid provider. Choose from: gemini, openai, ollama, anthropic:", "callback", "wizard_step_ai_provider");
        return;
    }
    setupConfig.aiProvider = provider;
    var defaults = {
        "gemini": { model: "gemini-1.5-flash", baseUrl: "https://generativelanguage.googleapis.com" },
        "openai": { model: "gpt-4o", baseUrl: "https://api.openai.com/v1" },
        "ollama": { model: "llama3", baseUrl: "http://localhost:11434/v1" },
        "anthropic": { model: "claude-sonnet-4-6", baseUrl: "https://api.anthropic.com" }
    };
    setupConfig.aiModel = defaults[provider] ? defaults[provider].model : "gpt-4o";
    setupConfig.aiBaseUrl = defaults[provider] ? defaults[provider].baseUrl : "";
    console.log("  AI Provider: " + setupConfig.aiProvider);
    setupStep++;
    nextSetupStep();
}

function wizard_step_ai_model(val) {
    if (val !== "") setupConfig.aiModel = val;
    console.log("  AI Model: " + setupConfig.aiModel);
    setupStep++;
    nextSetupStep();
}

function wizard_step_ai_key(val) {
    setupConfig.aiKey = val;
    console.log("  API Key: " + (val ? "[set]" : "[empty - using local Ollama]"));
    setupStep++;
    nextSetupStep();
}

function wizard_step_lsp(val) {
    if (val === "0") {
        setupConfig.lspPath = "";
    } else if (val) {
        setupConfig.lspPath = val;
    }
    setupStep++;
    nextSetupStep();
}

function wizard_step_project_dir(val) {
    setupConfig.projectDir = val || "C:/";
    setupStep++;
    nextSetupStep();
}

function wizard_step_theme(val) {
    if (val === "") val = "0";
    var themeMap = { "0": "default_dark", "1": "light", "2": "monokai", "3": "one_dark", "4": "nord", "5": "custom" };
    setupConfig.theme = themeMap[val] || "default_dark";
    console.log("  Theme: " + setupConfig.theme);
    setupStep++;
    nextSetupStep();
}

function finishSetupWizard() {
    var aiConfig = getAiConfig() || {};
    aiConfig.activeServer = setupConfig.aiProvider;
    aiConfig.allowedProjectDir = setupConfig.projectDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    var srv = aiConfig.servers[setupConfig.aiProvider];
    if (!srv) {
        srv = { provider: setupConfig.aiProvider };
        aiConfig.servers[setupConfig.aiProvider] = srv;
    }

    srv.apiKey = setupConfig.aiKey;
    srv.model = setupConfig.aiModel;
    if (setupConfig.aiBaseUrl) srv.apiBase = setupConfig.aiBaseUrl;

    // Use-case-specific AI agent setup
    var agentName = "";
    var agentPrompt = "";
    switch (setupConfig.useCase) {
        case "developer":
            agentName = "Developer";
            agentPrompt = "You are a senior software engineer. Help write, debug, and refactor code. Provide clear explanations and follow best practices.";
            break;
        case "writer":
            agentName = "Writer";
            agentPrompt = "You are a professional writer and editor. Help with writing, editing, grammar, style, and content creation.";
            break;
        case "researcher":
            agentName = "Researcher";
            agentPrompt = "You are a research assistant. Help analyze information, summarize findings, and explore topics in depth.";
            break;
        case "data_scientist":
            agentName = "Data Scientist";
            agentPrompt = "You are a data science expert. Help with data analysis, statistics, machine learning, and visualization.";
            break;
        case "japanese_dev":
            agentName = "Japanese Developer";
            agentPrompt = "あなたはシニアソフトウェアエンジニアです。日本語でコードの作成、デバッグ、リファクタリングを支援します。";
            break;
        default:
            agentName = "General Coder";
            agentPrompt = "You are a helpful AI assistant. Help with coding, writing, and problem-solving.";
    }

    if (!aiConfig.agents) aiConfig.agents = {};
    aiConfig.agents["use_case_agent"] = { name: agentName, systemPrompt: agentPrompt };
    aiConfig.activeAgent = "use_case_agent";

    saveAiConfig(aiConfig);

    if (setupConfig.lspPath) {
        Editor.lspStart(setupConfig.lspPath, setupConfig.projectDir);
    }

    Editor.saveSettings();
    Editor.setStatusText("Setup Complete! Use Case: " + setupConfig.useCase + ", AI: " + setupConfig.aiProvider);

    Editor.newFile("*Welcome*");
    Editor.setScratch(true);
    Editor.insert(0,
        "# Welcome to Ecode!\n\n" +
        "Your " + setupConfig.useCase.replace("_", " ") + " environment is ready.\n\n" +
        "- Language: " + setupConfig.language +
        "\n- Use Case: " + setupConfig.useCase +
        "\n- AI Provider: " + setupConfig.aiProvider +
        "\n- AI Model: " + setupConfig.aiModel +
        "\n- Theme: " + setupConfig.theme +
        "\n- Project Root: " + setupConfig.projectDir +
        "\n- LSP: " + (setupConfig.lspPath || "none") +
        "\n\nPress Alt+A for AI Completion or Alt+I for AI Console.\nHappy Coding!"
    );
    console.log("Setup Wizard completed successfully for use case: " + setupConfig.useCase);
}

function setup_wizard() {
    run_setup_wizard();
}
