var writerWizardStep = 0;
var writerConfig = {};

function run_writer_wizard() {
    writerConfig = {};
    writerWizardStep = 0;
    console.log("Starting Writer Setup Wizard...");
    Editor.setStatusText("Writer Wizard: Step 1/7");
    Editor.showMinibuffer("Step 1/7: Language (0:EN, 1:JA, 2:FR, 3:ES, 4:PT, 5:DE):", "callback", "writer_step_lang");
}

function writer_step_lang(val) {
    if (val === "") val = "0";
    var langMap = {"0":"en","1":"ja","2":"fr","3":"es","4":"pt","5":"de"};
    writerConfig.language = langMap[val] || "en";
    Editor.setLanguage(writerConfig.language);
    writerWizardStep = 1;
    Editor.setStatusText("Writer Wizard: Step 2/7");
    Editor.showMinibuffer("Step 2/7: Writing Type (blog, technical, fiction, academic, business, general):", "callback", "writer_step_type");
}

function writer_step_type(val) {
    if (val === "") val = "general";
    writerConfig.writingType = val.toLowerCase();
    writerWizardStep = 2;
    Editor.setStatusText("Writer Wizard: Step 3/7");
    Editor.showMinibuffer("Step 3/7: AI Provider (gemini, openai, anthropic, ollama):", "callback", "writer_step_ai");
}

function writer_step_ai(val) {
    if (val === "") val = "gemini";
    writerConfig.aiProvider = val.toLowerCase();
    writerWizardStep = 3;
    Editor.setStatusText("Writer Wizard: Step 4/7");
    Editor.showMinibuffer("Step 4/7: AI Model:", "callback", "writer_step_model");
}

function writer_step_model(val) {
    var modelMap = {"gemini":"gemini-1.5-flash","openai":"gpt-4o","anthropic":"claude-sonnet-4-6","ollama":"llama3"};
    writerConfig.aiModel = val || modelMap[writerConfig.aiProvider] || "gpt-4o";
    writerWizardStep = 4;
    Editor.setStatusText("Writer Wizard: Step 5/7");
    Editor.showMinibuffer("Step 5/7: API Key (empty for Ollama):", "callback", "writer_step_key");
}

function writer_step_key(val) {
    writerConfig.aiKey = val;
    writerWizardStep = 5;
    Editor.setStatusText("Writer Wizard: Step 6/7");
    Editor.showMinibuffer("Step 6/7: Project Directory:", "callback", "writer_step_dir");
}

function writer_step_dir(val) {
    writerConfig.projectDir = val || "C:/writing";
    writerWizardStep = 6;
    Editor.setStatusText("Writer Wizard: Step 7/7");
    Editor.showMinibuffer("Step 7/7: Enable Spell Check? (y/n):", "callback", "writer_step_spellcheck");
}

function writer_step_spellcheck(val) {
    writerConfig.spellCheck = val === "" || val.toLowerCase() === "y";
    finishWriterWizard();
}

function finishWriterWizard() {
    var aiConfig = getAiConfig() || {};
    aiConfig.activeServer = writerConfig.aiProvider;
    aiConfig.allowedProjectDir = writerConfig.projectDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    var srv = aiConfig.servers[writerConfig.aiProvider];
    if (!srv) { srv = { provider: writerConfig.aiProvider }; aiConfig.servers[writerConfig.aiProvider] = srv; }
    srv.apiKey = writerConfig.aiKey;
    srv.model = writerConfig.aiModel;
    if (!aiConfig.agents) aiConfig.agents = {};
    aiConfig.agents["writer_agent"] = {
        name: "Writer",
        systemPrompt: "You are a professional " + writerConfig.writingType + " writer. Help with writing, editing, grammar, and style. Adapt tone to the audience and purpose. Provide constructive feedback on drafts."
    };
    aiConfig.activeAgent = "writer_agent";
    saveAiConfig(aiConfig);
    Editor.saveSettings();

    Editor.newFile("*Welcome Writer*");
    Editor.setScratch(true);
    Editor.insert(0,
        "# Writing Environment Ready\n\n" +
        "- Writing Type: " + writerConfig.writingType +
        "\n- AI: " + writerConfig.aiProvider + " / " + writerConfig.aiModel +
        "\n- Spell Check: " + (writerConfig.spellCheck ? "On" : "Off") +
        "\n- Project: " + writerConfig.projectDir +
        "\n\nPress Alt+A for AI Writing Assistant\nHappy Writing!"
    );
    Editor.setStatusText("Writer Setup Complete! Type: " + writerConfig.writingType + ", AI: " + writerConfig.aiProvider);
    console.log("Writer wizard finished.");
}

function setup_writer_wizard() {
    run_writer_wizard();
}
