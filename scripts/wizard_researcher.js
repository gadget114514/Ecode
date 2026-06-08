var resWizardStep = 0;
var resConfig = {};

function run_researcher_wizard() {
    resConfig = {};
    resWizardStep = 0;
    console.log("Starting Researcher Setup Wizard...");
    Editor.setStatusText("Researcher Wizard: Step 1/6");
    Editor.showMinibuffer("Step 1/6: Language (0:EN, 1:JA, 2:FR, 3:ES):", "callback", "res_step_lang");
}

function res_step_lang(val) {
    if (val === "") val = "0";
    var langMap = {"0":"en","1":"ja","2":"fr","3":"es"};
    resConfig.language = langMap[val] || "en";
    Editor.setLanguage(resConfig.language);
    resWizardStep = 1;
    Editor.setStatusText("Researcher Wizard: Step 2/6");
    Editor.showMinibuffer("Step 2/6: Research Domain (general, science, tech, medicine, humanities):", "callback", "res_step_domain");
}

function res_step_domain(val) {
    resConfig.domain = val || "general";
    resWizardStep = 2;
    Editor.setStatusText("Researcher Wizard: Step 3/6");
    Editor.showMinibuffer("Step 3/6: AI Provider (gemini, openai, anthropic, ollama):", "callback", "res_step_ai");
}

function res_step_ai(val) {
    if (val === "") val = "gemini";
    resConfig.aiProvider = val.toLowerCase();
    resWizardStep = 3;
    Editor.setStatusText("Researcher Wizard: Step 4/6");
    Editor.showMinibuffer("Step 4/6: AI Model:", "callback", "res_step_model");
}

function res_step_model(val) {
    var modelMap = {"gemini":"gemini-1.5-flash","openai":"gpt-4o","anthropic":"claude-sonnet-4-6","ollama":"llama3"};
    resConfig.aiModel = val || modelMap[resConfig.aiProvider] || "gpt-4o";
    resWizardStep = 4;
    Editor.setStatusText("Researcher Wizard: Step 5/6");
    Editor.showMinibuffer("Step 5/6: API Key (empty for Ollama):", "callback", "res_step_key");
}

function res_step_key(val) {
    resConfig.aiKey = val;
    resWizardStep = 5;
    Editor.setStatusText("Researcher Wizard: Step 6/6");
    Editor.showMinibuffer("Step 6/6: Research Notes Directory:", "callback", "res_step_dir");
}

function res_step_dir(val) {
    resConfig.notesDir = val || "C:/research";
    finishResearcherWizard();
}

function finishResearcherWizard() {
    var aiConfig = getAiConfig() || {};
    aiConfig.activeServer = resConfig.aiProvider;
    aiConfig.allowedProjectDir = resConfig.notesDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    var srv = aiConfig.servers[resConfig.aiProvider];
    if (!srv) { srv = { provider: resConfig.aiProvider }; aiConfig.servers[resConfig.aiProvider] = srv; }
    srv.apiKey = resConfig.aiKey;
    srv.model = resConfig.aiModel;
    if (!aiConfig.agents) aiConfig.agents = {};
    aiConfig.agents["researcher_agent"] = {
        name: "Research Assistant",
        systemPrompt: "You are an expert research assistant in " + resConfig.domain + ". Help analyze information, summarize findings, identify patterns, and explore topics in depth. Cite sources and note uncertainties. Think step by step."
    };
    aiConfig.activeAgent = "researcher_agent";
    saveAiConfig(aiConfig);
    Editor.saveSettings();

    Editor.newFile("*Welcome Researcher*");
    Editor.setScratch(true);
    Editor.insert(0,
        "# Research Environment Ready\n\n" +
        "- Domain: " + resConfig.domain +
        "\n- AI: " + resConfig.aiProvider + " / " + resConfig.aiModel +
        "\n- Notes: " + resConfig.notesDir +
        "\n\nPress Alt+A for AI Research Assistant\nHappy Researching!"
    );
    Editor.setStatusText("Researcher Setup Complete! Domain: " + resConfig.domain + ", AI: " + resConfig.aiProvider);
    console.log("Researcher wizard finished.");
}

function setup_researcher_wizard() {
    run_researcher_wizard();
}
