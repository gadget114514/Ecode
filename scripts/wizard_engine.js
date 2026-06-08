/**
 * Wizard Engine — JSON-driven wizard system
 * 
 * Usage (standalone):
 *   load_wizard("developer")   // loads frontend/wizards/developer.json
 *   load_wizard("writer")
 *   load_wizard("researcher")
 *   load_wizard("data_scientist")
 *   load_wizard("japanese_dev")
 *   load_wizard("C:/path/to/custom.json")  // absolute path
 * 
 * Pipeline integration:
 *   Pipeline step { type: "wizard", wizard: "developer" }
 *   → C++ PipelineRunner resolves wizard data → JS renders modal
 */

// ── Wizard loading ───────────────────────────────────────────────

function load_wizard(nameOrPath) {
    // If it's an absolute path, try to load directly
    if (nameOrPath.indexOf(":") >= 0 || nameOrPath.indexOf("/") === 0 || nameOrPath.indexOf("\\") === 0) {
        try {
            var content = Editor.readFile(nameOrPath);
            if (content) return startWizardFromJson(JSON.parse(content));
        } catch (e) {
            console.log("Failed to load wizard from path: " + nameOrPath + " - " + e);
        }
        return;
    }

    // Try built-in paths
    var paths = [
        "Application/Prompts/frontend/wizards/" + nameOrPath + ".json",
        "scripts/wizards/" + nameOrPath + ".json",
    ];

    for (var i = 0; i < paths.length; i++) {
        try {
            var content = Editor.readFile(paths[i]);
            if (content) {
                startWizardFromJson(JSON.parse(content));
                return;
            }
        } catch (e) {
            // continue trying next path
        }
    }

    console.log("Wizard not found: " + nameOrPath);
    Editor.showMinibuffer("Wizard '" + nameOrPath + "' not found. Enter JSON file path:", "callback", "load_wizard_callback");
}

function load_wizard_callback(val) {
    if (val) load_wizard(val);
}

function startWizardFromJson(wizardData) {
    if (!wizardData || !wizardData.steps || wizardData.steps.length === 0) {
        console.log("Invalid wizard definition");
        return;
    }
    console.log("Starting wizard: " + (wizardData.name || wizardData.id || "unnamed"));
    wizardEngineState = {
        data: wizardData,
        step: 0,
        values: {},
        totalSteps: wizardData.steps.length
    };
    runWizardStep();
}

// ── Wizard state ─────────────────────────────────────────────────

var wizardEngineState = null;

function runWizardStep() {
    var state = wizardEngineState;
    if (!state || state.step >= state.totalSteps) {
        finishWizard();
        return;
    }

    var stepDef = state.data.steps[state.step];
    var defaultVal = stepDef.default || "";
    var currentVal = state.values[stepDef.id] !== undefined ? state.values[stepDef.id] : defaultVal;
    var prompt = stepDef.prompt || ("Step " + (state.step + 1) + "/" + state.totalSteps + ": Enter value for " + stepDef.id);

    // Set current value hint
    if (currentVal) {
        prompt += " [" + currentVal + "]";
    }

    // For password type, the value should not echo
    if (stepDef.type === "password") {
        prompt += " (leave empty to keep current)";
    }

    Editor.showMinibuffer(prompt, "callback", "wizardEngineStepCallback");
}

function wizardEngineStepCallback(val) {
    var state = wizardEngineState;
    if (!state) return;

    var stepDef = state.data.steps[state.step];
    var resolved = val || stepDef.default || "";

    // Validate
    if (stepDef.validate && resolved) {
        try {
            var re = new RegExp(stepDef.validate);
            if (!re.test(resolved)) {
                Editor.showMinibuffer("Invalid input. " + stepDef.prompt, "callback", "wizardEngineStepCallback");
                return;
            }
        } catch (e) { }
    }

    // Apply action
    if (stepDef.action === "setLanguage") {
        Editor.setLanguage(resolved);
    }

    state.values[stepDef.id] = resolved;
    state.step++;
    runWizardStep();
}

function finishWizard() {
    var state = wizardEngineState;
    if (!state) return;
    wizardEngineState = null;

    // Apply output mapping
    if (state.data.outputMapping) {
        for (var targetField in state.data.outputMapping) {
            var mapping = state.data.outputMapping[targetField];
            var sourceVal = state.values[mapping.source];
            if (sourceVal && mapping.map && mapping.map[sourceVal]) {
                state.values[targetField] = mapping.map[sourceVal];
            }
        }
    }

    // Apply AI config
    applyWizardAiConfig(state);
    applyWizardWelcome(state);

    console.log("Wizard '" + (state.data.name || "unnamed") + "' completed.");
    Editor.setStatusText("Wizard complete: " + (state.data.name || "unnamed"));
}

function applyWizardAiConfig(state) {
    var aiConfig = getAiConfig();
    var provider = state.values.ai_provider || "gemini";
    var model = state.values.ai_model || "";
    var apiKey = state.values.api_key || "";

    aiConfig.activeServer = provider;
    if (state.values.project_dir) aiConfig.allowedProjectDir = state.values.project_dir;

    if (!aiConfig.servers) aiConfig.servers = {};
    if (!aiConfig.servers[provider]) {
        aiConfig.servers[provider] = { provider: provider };
    }
    if (apiKey) aiConfig.servers[provider].apiKey = apiKey;
    if (model) aiConfig.servers[provider].model = model;

    // Apply AI agent
    if (state.data.aiAgent) {
        if (!aiConfig.agents) aiConfig.agents = {};
        aiConfig.agents["wizard_agent"] = {
            name: state.data.aiAgent.name || "Wizard Agent",
            systemPrompt: state.data.aiAgent.systemPrompt || ""
        };
        aiConfig.activeAgent = "wizard_agent";
    }

    saveAiConfig(aiConfig);

    // Start LSP
    if (state.values.lspPath && state.values.project_dir) {
        Editor.lspStart(state.values.lspPath, state.values.project_dir);
    }
    Editor.saveSettings();
}

function applyWizardWelcome(state) {
    var msg = state.data.welcomeMessage || "# Wizard Complete\n\nSetup finished.";
    // Expand placeholders
    for (var key in state.values) {
        var re = new RegExp("\\{" + key + "\\}", "g");
        msg = msg.replace(re, state.values[key] || "");
    }

    Editor.newFile("*Wizard Complete*");
    Editor.setScratch(true);
    Editor.insert(0, msg);
}

// ── Global commands ──────────────────────────────────────────────

function setup_wizard() {
    load_wizard("developer");
}

function setup_writer_wizard() {
    load_wizard("writer");
}

function setup_researcher_wizard() {
    load_wizard("researcher");
}

function setup_data_scientist_wizard() {
    load_wizard("data_scientist");
}

function setup_japanese_dev_wizard() {
    load_wizard("japanese_dev");
}
