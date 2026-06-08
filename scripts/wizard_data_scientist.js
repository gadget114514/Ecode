var dsWizardStep = 0;
var dsConfig = {};

function run_data_scientist_wizard() {
    dsConfig = {};
    dsWizardStep = 0;
    console.log("Starting Data Scientist Setup Wizard...");
    Editor.setStatusText("Data Scientist Wizard: Step 1/8");
    Editor.showMinibuffer("Step 1/8: Language (0:EN, 1:JP):", "callback", "ds_step_lang");
}

function ds_step_lang(val) {
    if (val === "") val = "0";
    dsConfig.language = val === "0" ? "en" : "ja";
    Editor.setLanguage(dsConfig.language);
    dsWizardStep = 1;
    Editor.setStatusText("Data Scientist Wizard: Step 2/8");
    Editor.showMinibuffer("Step 2/8: Primary Tool (python, julia, r, matlab):", "callback", "ds_step_tool");
}

function ds_step_tool(val) {
    if (val === "") val = "python";
    dsConfig.tool = val.toLowerCase();
    var lspMap = {"python": "pyright-langserver --stdio", "julia": "julia-language-server", "r": "languageserver", "matlab": ""};
    dsConfig.lspPath = lspMap[dsConfig.tool] || "";
    dsWizardStep = 2;
    Editor.setStatusText("Data Scientist Wizard: Step 3/8");
    Editor.showMinibuffer("Step 3/8: AI Provider (gemini, openai, anthropic, ollama):", "callback", "ds_step_ai");
}

function ds_step_ai(val) {
    if (val === "") val = "gemini";
    dsConfig.aiProvider = val.toLowerCase();
    dsWizardStep = 3;
    Editor.setStatusText("Data Scientist Wizard: Step 4/8");
    Editor.showMinibuffer("Step 4/8: AI Model:", "callback", "ds_step_model");
}

function ds_step_model(val) {
    var modelMap = {"gemini":"gemini-1.5-flash","openai":"gpt-4o","anthropic":"claude-sonnet-4-6","ollama":"llama3"};
    dsConfig.aiModel = val || modelMap[dsConfig.aiProvider] || "gpt-4o";
    dsWizardStep = 4;
    Editor.setStatusText("Data Scientist Wizard: Step 5/8");
    Editor.showMinibuffer("Step 5/8: API Key (empty for Ollama):", "callback", "ds_step_key");
}

function ds_step_key(val) {
    dsConfig.aiKey = val;
    dsWizardStep = 5;
    Editor.setStatusText("Data Scientist Wizard: Step 6/8");
    Editor.showMinibuffer("Step 6/8: Project Directory:", "callback", "ds_step_dir");
}

function ds_step_dir(val) {
    dsConfig.projectDir = val || "C:/data_science";
    dsWizardStep = 6;
    Editor.setStatusText("Data Scientist Wizard: Step 7/8");
    Editor.showMinibuffer("Step 7/8: Enable Jupyter-like Scratch Buffers? (y/n):", "callback", "ds_step_jupyter");
}

function ds_step_jupyter(val) {
    dsConfig.jupyterMode = val === "" || val.toLowerCase() === "y";
    dsWizardStep = 7;
    Editor.setStatusText("Data Scientist Wizard: Step 8/8");
    Editor.showMinibuffer("Step 8/8: Enable Statistical Analysis Tools? (y/n):", "callback", "ds_step_stats");
}

function ds_step_stats(val) {
    dsConfig.statsTools = val === "" || val.toLowerCase() === "y";
    finishDataScientistWizard();
}

function finishDataScientistWizard() {
    var aiConfig = getAiConfig() || {};
    aiConfig.activeServer = dsConfig.aiProvider;
    aiConfig.allowedProjectDir = dsConfig.projectDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    var srv = aiConfig.servers[dsConfig.aiProvider];
    if (!srv) { srv = { provider: dsConfig.aiProvider }; aiConfig.servers[dsConfig.aiProvider] = srv; }
    srv.apiKey = dsConfig.aiKey;
    srv.model = dsConfig.aiModel;
    if (!aiConfig.agents) aiConfig.agents = {};
    aiConfig.agents["datascientist_agent"] = {
        name: "Data Scientist",
        systemPrompt: "You are an expert data scientist specializing in " + dsConfig.tool + ". Help with data analysis, statistical modeling, machine learning, and creating insightful visualizations. Explain statistical concepts clearly and suggest appropriate methodologies for the data at hand."
    };
    aiConfig.activeAgent = "datascientist_agent";
    saveAiConfig(aiConfig);

    if (dsConfig.lspPath) {
        Editor.lspStart(dsConfig.lspPath, dsConfig.projectDir);
    }
    Editor.saveSettings();

    Editor.newFile("*Welcome Data Scientist*");
    Editor.setScratch(true);
    Editor.insert(0,
        "# Data Science Environment Ready\n\n" +
        "- Tool: " + dsConfig.tool +
        "\n- LSP: " + (dsConfig.lspPath || "none") +
        "\n- AI: " + dsConfig.aiProvider + " / " + dsConfig.aiModel +
        "\n- Jupyter Mode: " + (dsConfig.jupyterMode ? "Yes" : "No") +
        "\n- Stats Tools: " + (dsConfig.statsTools ? "Yes" : "No") +
        "\n- Project: " + dsConfig.projectDir +
        "\n\nPress Alt+A for AI Data Science Assistant\nHappy Analyzing!"
    );
    Editor.setStatusText("Data Scientist Setup Complete! Tool: " + dsConfig.tool + ", AI: " + dsConfig.aiProvider);
    console.log("Data scientist wizard finished.");
}

function setup_data_scientist_wizard() {
    run_data_scientist_wizard();
}
