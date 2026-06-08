var devWizardStep = 0;
var devConfig = {};

function run_developer_wizard() {
    devConfig = {};
    devWizardStep = 0;
    console.log("Starting Developer Setup Wizard...");
    Editor.setStatusText("Developer Wizard: Step 1/7");
    Editor.showMinibuffer("Step 1/7: Language (0:EN, 1:JP):", "callback", "dev_step_lang");
}

function dev_step_lang(val) {
    if (val === "") val = "0";
    devConfig.language = val === "0" ? "en" : "ja";
    Editor.setLanguage(devConfig.language);
    devWizardStep = 1;
    Editor.setStatusText("Developer Wizard: Step 2/7");
    Editor.showMinibuffer("Step 2/7: Main Language/Framework (js, ts, python, rust, cpp, go, java, other):", "callback", "dev_step_primary_lang");
}

function dev_step_primary_lang(val) {
    if (val === "") val = "js";
    devConfig.primaryLang = val.toLowerCase();
    var lspMap = {
        "js": "typescript-language-server --stdio",
        "ts": "typescript-language-server --stdio",
        "python": "pyright-langserver --stdio",
        "rust": "rust-analyzer",
        "cpp": "clangd",
        "go": "gopls",
        "java": "jdtls"
    };
    devConfig.lspPath = lspMap[devConfig.primaryLang] || val;
    devWizardStep = 2;
    Editor.setStatusText("Developer Wizard: Step 3/7");
    Editor.showMinibuffer("Step 3/7: AI Provider (gemini, openai, ollama, anthropic):", "callback", "dev_step_ai_provider");
}

function dev_step_ai_provider(val) {
    if (val === "") val = "gemini";
    devConfig.aiProvider = val.toLowerCase();
    devWizardStep = 3;
    Editor.setStatusText("Developer Wizard: Step 4/7");
    Editor.showMinibuffer("Step 4/7: AI Model:", "callback", "dev_step_ai_model");
}

function dev_step_ai_model(val) {
    if (!val) {
        var modelMap = { "gemini": "gemini-1.5-flash", "openai": "gpt-4o", "ollama": "codellama", "anthropic": "claude-sonnet-4-6" };
        devConfig.aiModel = modelMap[devConfig.aiProvider] || "gpt-4o";
    } else {
        devConfig.aiModel = val;
    }
    devWizardStep = 4;
    Editor.setStatusText("Developer Wizard: Step 5/7");
    Editor.showMinibuffer("Step 5/7: API Key (leave empty for Ollama):", "callback", "dev_step_ai_key");
}

function dev_step_ai_key(val) {
    devConfig.aiKey = val;
    devWizardStep = 5;
    Editor.setStatusText("Developer Wizard: Step 6/7");
    Editor.showMinibuffer("Step 6/7: Project Directory:", "callback", "dev_step_project_dir");
}

function dev_step_project_dir(val) {
    devConfig.projectDir = val || "C:/dev";
    devWizardStep = 6;
    Editor.setStatusText("Developer Wizard: Step 7/7");
    Editor.showMinibuffer("Step 7/7: Enable Git Integration? (y/n):", "callback", "dev_step_git");
}

function dev_step_git(val) {
    devConfig.gitEnabled = val === "" || val.toLowerCase() === "y";
    finishDeveloperWizard();
}

function finishDeveloperWizard() {
    var aiConfig = getAiConfig() || {};
    aiConfig.activeServer = devConfig.aiProvider;
    aiConfig.allowedProjectDir = devConfig.projectDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    var srv = aiConfig.servers[devConfig.aiProvider];
    if (!srv) {
        srv = { provider: devConfig.aiProvider };
        aiConfig.servers[devConfig.aiProvider] = srv;
    }
    srv.apiKey = devConfig.aiKey;
    srv.model = devConfig.aiModel;
    if (!aiConfig.agents) aiConfig.agents = {};
    aiConfig.agents["dev_agent"] = {
        name: "Developer",
        systemPrompt: "You are a senior software engineer. Help write, debug, and refactor code. Provide clear explanations, follow best practices, and suggest optimizations. Focus on " + devConfig.primaryLang + "."
    };
    aiConfig.activeAgent = "dev_agent";
    saveAiConfig(aiConfig);

    if (devConfig.lspPath) {
        Editor.lspStart(devConfig.lspPath, devConfig.projectDir);
    }
    Editor.saveSettings();

    Editor.newFile("*Welcome Developer*");
    Editor.setScratch(true);
    Editor.insert(0,
        "# Developer Environment Ready\n\n" +
        "- Language: " + devConfig.primaryLang +
        "\n- LSP: " + devConfig.lspPath +
        "\n- AI: " + devConfig.aiProvider + " / " + devConfig.aiModel +
        "\n- Project: " + devConfig.projectDir +
        "\n- Git: " + (devConfig.gitEnabled ? "Enabled" : "Disabled") +
        "\n\nPress Alt+A for AI Completion\nHappy Coding!"
    );
    Editor.setStatusText("Developer Setup Complete! Primary: " + devConfig.primaryLang + ", AI: " + devConfig.aiProvider);
    console.log("Developer wizard finished.");
}

function setup_developer_wizard() {
    run_developer_wizard();
}
