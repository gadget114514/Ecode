var jpWizardStep = 0;
var jpConfig = {};

function run_japanese_dev_wizard() {
    jpConfig = {};
    jpWizardStep = 0;
    console.log("Japanese Developer Setup Wizard started...");
    Editor.setStatusText("日本語開発者ウィザード: ステップ 1/8");
    Editor.showMinibuffer("Step 1/8: 言語 / Language (0:日本語, 1:English):", "callback", "jp_step_lang");
}

function jp_step_lang(val) {
    if (val === "") val = "0";
    jpConfig.language = val === "0" ? "ja" : "en";
    Editor.setLanguage(jpConfig.language);
    jpWizardStep = 1;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 2/8");
    Editor.showMinibuffer("Step 2/8: 開発言語 / Primary Language (js, ts, python, rust, cpp, go):", "callback", "jp_step_plang");
}

function jp_step_plang(val) {
    if (val === "") val = "ts";
    jpConfig.primaryLang = val.toLowerCase();
    var lspMap = {"js":"typescript-language-server --stdio","ts":"typescript-language-server --stdio","python":"pyright-langserver --stdio","rust":"rust-analyzer","cpp":"clangd","go":"gopls"};
    jpConfig.lspPath = lspMap[jpConfig.primaryLang] || jpConfig.primaryLang;
    jpWizardStep = 2;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 3/8");
    Editor.showMinibuffer("Step 3/8: AI プロバイダ (gemini, openai, anthropic, ollama):", "callback", "jp_step_ai");
}

function jp_step_ai(val) {
    if (val === "") val = "gemini";
    jpConfig.aiProvider = val.toLowerCase();
    jpWizardStep = 3;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 4/8");
    Editor.showMinibuffer("Step 4/8: AI モデル (デフォルト推奨):", "callback", "jp_step_model");
}

function jp_step_model(val) {
    var modelMap = {"gemini":"gemini-1.5-flash","openai":"gpt-4o","anthropic":"claude-sonnet-4-6","ollama":"llama3"};
    jpConfig.aiModel = val || modelMap[jpConfig.aiProvider] || "gpt-4o";
    jpWizardStep = 4;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 5/8");
    Editor.showMinibuffer("Step 5/8: API キー (Ollamaの場合は空欄):", "callback", "jp_step_key");
}

function jp_step_key(val) {
    jpConfig.aiKey = val;
    jpWizardStep = 5;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 6/8");
    Editor.showMinibuffer("Step 6/8: プロジェクトディレクトリ:", "callback", "jp_step_dir");
}

function jp_step_dir(val) {
    jpConfig.projectDir = val || "C:/dev";
    jpWizardStep = 6;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 7/8");
    Editor.showMinibuffer("Step 7/8: 日本語対応の強調表示を有効にしますか？ (y/n):", "callback", "jp_step_jp_hl");
}

function jp_step_jp_hl(val) {
    jpConfig.jpHighlight = val === "" || val.toLowerCase() === "y";
    jpWizardStep = 7;
    Editor.setStatusText("日本語開発者ウィザード: ステップ 8/8");
    Editor.showMinibuffer("Step 8/8: Git 統合を有効にしますか？ (y/n):", "callback", "jp_step_git");
}

function jp_step_git(val) {
    jpConfig.gitEnabled = val === "" || val.toLowerCase() === "y";
    finishJapaneseDevWizard();
}

function finishJapaneseDevWizard() {
    var aiConfig = getAiConfig() || {};
    aiConfig.activeServer = jpConfig.aiProvider;
    aiConfig.allowedProjectDir = jpConfig.projectDir;
    if (!aiConfig.servers) aiConfig.servers = {};
    var srv = aiConfig.servers[jpConfig.aiProvider];
    if (!srv) { srv = { provider: jpConfig.aiProvider }; aiConfig.servers[jpConfig.aiProvider] = srv; }
    srv.apiKey = jpConfig.aiKey;
    srv.model = jpConfig.aiModel;
    if (!aiConfig.agents) aiConfig.agents = {};
    aiConfig.agents["jp_dev_agent"] = {
        name: "Japanese Developer",
        systemPrompt: "あなたはシニアソフトウェアエンジニアです。" + jpConfig.primaryLang + " を専門としています。コードの作成、デバッグ、リファクタリングを日本語で支援します。ベストプラクティスに従い、最適化を提案してください。"
    };
    aiConfig.activeAgent = "jp_dev_agent";
    saveAiConfig(aiConfig);

    if (jpConfig.lspPath) {
        Editor.lspStart(jpConfig.lspPath, jpConfig.projectDir);
    }
    Editor.saveSettings();

    var welcomeMsg = jpConfig.language === "ja"
        ? "# 日本語開発環境のセットアップが完了しました\n\n"
          + "- 言語: " + jpConfig.primaryLang
          + "\n- LSP: " + jpConfig.lspPath
          + "\n- AI: " + jpConfig.aiProvider + " / " + jpConfig.aiModel
          + "\n- プロジェクト: " + jpConfig.projectDir
          + "\n- Git: " + (jpConfig.gitEnabled ? "有効" : "無効")
          + "\n\nAlt+A で AI 補完、Alt+I で AI コンソールを開けます。\n良いコーディングを！"
        : "# Japanese Developer Environment Ready\n\n"
          + "- Language: " + jpConfig.primaryLang
          + "\n- LSP: " + jpConfig.lspPath
          + "\n- AI: " + jpConfig.aiProvider + " / " + jpConfig.aiModel
          + "\n- Project: " + jpConfig.projectDir
          + "\n- Git: " + (jpConfig.gitEnabled ? "Enabled" : "Disabled")
          + "\n\nPress Alt+A for AI Completion\nHappy Coding!";

    Editor.newFile("*Welcome*");
    Editor.setScratch(true);
    Editor.insert(0, welcomeMsg);
    Editor.setStatusText("日本語開発者セットアップ完了！");
    console.log("Japanese developer wizard finished.");
}

function setup_japanese_dev_wizard() {
    run_japanese_dev_wizard();
}
