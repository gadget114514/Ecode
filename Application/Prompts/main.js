const { app, BrowserWindow, ipcMain, dialog, Menu } = require('electron');
const path = require('path');
const fs = require('fs');
const storage = require('./src/js/storage');
const runner = require('./src/js/runner');
const optimizer = require('./src/js/optimizer');
const versionMgr = require('./src/js/versionManager');
const { setHttpLogCallback } = require('./src/js/ai');

let win = null;
let isEmbedded = false;
let appLang = 'en';

// Parse command line arguments
const args = process.argv;
if (args.includes('--embedded')) {
    isEmbedded = true;
}
const langIdx = args.indexOf('--lang');
if (langIdx !== -1 && langIdx + 1 < args.length) {
    appLang = args[langIdx + 1];
}

// Initialize storage paths
const appDataPath = path.join(app.getPath('appData'), 'Ecode', 'Prompts');
storage.init(appDataPath);

// Setup HTTP log callback to post directly back to JS
setHttpLogCallback((logHtml) => {
    postToJS('log', { message: logHtml });
});

function postToJS(type, payload) {
    if (win && win.webContents) {
        win.webContents.send('webview-message', {
            type,
            payload
        });
    }
}

function sendAck(reqId, success = true) {
    if (reqId) {
        postToJS('ack', { _reqId: reqId, success });
    }
}

function sendFullInit() {
    const session = storage.loadSession();

    // Ensure at least one tab
    if (!session.tabs || session.tabs.length === 0) {
        session.tabs = [{ name: 'General', file: 'general.json' }];
        storage.saveSession(session);
    }

    // Load nodes for each tab
    const nodes = {};
    for (const tab of session.tabs) {
        nodes[tab.file] = storage.loadTabData(tab.file);
    }

    // Load pipelines
    const pipelines = storage.loadPipelines();

    // Load providers
    const providers = storage.loadProviders();
    // Register loaded providers with runner
    for (const [pName, pCfg] of Object.entries(providers)) {
        runner.registerProvider(pName, pCfg.apiKey || '', pCfg.baseUrl || '');
    }

    // Recent files
    const recent = storage.loadRecentFiles();

    // Config
    const generalCfg = storage.loadGeneralConfig();
    const chestNames = storage.listNamedChests();
    const config = {
        historyRetention: generalCfg.historyRetention || 50,
        chestList: chestNames,
        defaultProvider: generalCfg.defaultProvider || 'openai',
        defaultModel: generalCfg.defaultModel || ''
    };

    // Recipes
    const recipes = storage.loadRecipes();

    // Run garbage collection of blobs
    const referencedBlobs = [];
    const collectRefs = (node) => {
        if (node.attachments) {
            for (const a of node.attachments) {
                if (a.file) referencedBlobs.push(a.file);
            }
        }
        if (node.children) {
            for (const c of node.children) collectRefs(c);
        }
    };
    for (const tab of session.tabs) {
        collectRefs(nodes[tab.file]);
    }
    storage.garbageCollectBlobs(referencedBlobs);

    postToJS('init', {
        language: appLang,
        embedded: isEmbedded,
        tabs: session.tabs,
        nodes,
        pipelines,
        providers,
        recentFiles: recent,
        config,
        recipes
    });
}

// Set up Runner callback
runner.setBridgeCallback((type, dataStr) => {
    let data = {};
    try {
        data = JSON.parse(dataStr);
    } catch (e) {
        data = dataStr;
    }

    if (type === 'pipeline_completed') {
        storage.saveHistory(data);
    }
    postToJS('log', { message: `🏃 PipelineRunner Callback: type=${type}` });
    postToJS(type, data);
});

function buildMenu() {
    if (isEmbedded) {
        Menu.setApplicationMenu(null);
        return;
    }

    const template = [
        {
            label: '&File',
            submenu: [
                {
                    label: '&New Tab',
                    accelerator: 'CmdOrCtrl+T',
                    click: () => postToJS('menu_command', { action: 'new_tab' })
                },
                {
                    label: '&Open File...',
                    accelerator: 'CmdOrCtrl+O',
                    click: () => handleOpenFileDialog()
                },
                {
                    label: '&Save',
                    accelerator: 'CmdOrCtrl+S',
                    click: () => postToJS('menu_command', { action: 'save' })
                },
                {
                    label: 'Save &As...',
                    accelerator: 'CmdOrCtrl+Shift+S',
                    click: () => handleSaveFileDialog()
                },
                { type: 'separator' },
                {
                    label: 'Exit',
                    click: () => app.quit()
                }
            ]
        },
        {
            label: '&Pipeline',
            submenu: [
                {
                    label: '&Run Pipeline',
                    accelerator: 'F5',
                    click: () => postToJS('menu_command', { action: 'run_pipeline' })
                },
                {
                    label: '&Cancel Pipeline',
                    click: () => runner.cancel()
                },
                { type: 'separator' },
                {
                    label: 'Pipeline &Manager',
                    click: () => postToJS('menu_command', { action: 'pipeline_manager' })
                },
                {
                    label: 'Pipeline &History',
                    click: () => postToJS('menu_command', { action: 'pipeline_history' })
                }
            ]
        },
        {
            label: 'P&roviders',
            submenu: [
                {
                    label: '&Configure Providers...',
                    click: () => postToJS('menu_command', { action: 'config' })
                },
                {
                    label: '&Test Connection',
                    click: () => postToJS('menu_command', { action: 'test_connection' })
                }
            ]
        },
        {
            label: '&Help',
            submenu: [
                {
                    label: 'Welcome &Wizard',
                    accelerator: 'F1',
                    click: () => postToJS('menu_command', { action: 'welcome_wizard' })
                },
                {
                    label: '&Setup Wizard',
                    click: () => postToJS('menu_command', { action: 'setup_wizard' })
                },
                { type: 'separator' },
                {
                    label: '&About Prompts',
                    click: () => postToJS('menu_command', { action: 'about' })
                }
            ]
        }
    ];

    const menu = Menu.buildFromTemplate(template);
    Menu.setApplicationMenu(menu);
}

function handleOpenFileDialog() {
    const paths = dialog.showOpenDialogSync(win, {
        filters: [{ name: 'JSON Files', extensions: ['json'] }, { name: 'All Files', extensions: ['*'] }],
        properties: ['openFile']
    });
    if (paths && paths.length > 0) {
        const filePath = paths[0];
        let recent = storage.loadRecentFiles();
        recent = recent.filter(f => f !== filePath && f !== 'data/' + filePath);
        recent.unshift(filePath);
        if (recent.length > 5) recent = recent.slice(0, 5);
        storage.saveRecentFiles(recent);
        
        postToJS('open_file_result', { path: filePath });
    }
}

function handleSaveFileDialog() {
    const filePath = dialog.showSaveDialogSync(win, {
        filters: [{ name: 'JSON Files', extensions: ['json'] }, { name: 'All Files', extensions: ['*'] }],
        defaultPath: 'pipeline.json'
    });
    if (filePath) {
        let recent = storage.loadRecentFiles();
        recent = recent.filter(f => f !== filePath && f !== 'data/' + filePath);
        recent.unshift(filePath);
        if (recent.length > 5) recent = recent.slice(0, 5);
        storage.saveRecentFiles(recent);

        postToJS('save_as_result', { path: filePath });
    }
}

function createWindow() {
    win = new BrowserWindow({
        width: 1000,
        height: 700,
        frame: !isEmbedded,
        autoHideMenuBar: isEmbedded,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false
        }
    });

    win.loadFile(path.join(__dirname, 'frontend', 'index.html'));

    buildMenu();

    // Hook native Windows messages for Ecode synchronization
    if (process.platform === 'win32') {
        const WM_RELOAD_SESSION = 0x8002;
        const WM_RELOAD_GLOBAL_DATA = 0x8003;

        win.hookWindowMessage(WM_RELOAD_SESSION, () => {
            postToJS('log', { message: '[TRACE] hookWindowMessage: WM_RELOAD_SESSION received' });
            sendFullInit();
        });

        win.hookWindowMessage(WM_RELOAD_GLOBAL_DATA, () => {
            postToJS('log', { message: '[TRACE] hookWindowMessage: WM_RELOAD_GLOBAL_DATA received' });
            sendFullInit();
        });
    }

    win.on('closed', () => {
        win = null;
    });
}

app.whenReady().then(() => {
    createWindow();

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) {
            createWindow();
        }
    });
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

// Helper to notify Ecode wrapper to reload (writes to stdout which the C++ wrapper reads)
function notifyEcodeReload() {
    console.log('NOTIFY_ECODE_RELOAD_GLOBAL_DATA');
}

// IPC Message handling (routes frontend chrome.webview.postMessage)
ipcMain.on('webview-message', async (event, message) => {
    const { type, _reqId } = message;
    const payload = message.payload;

    postToJS('log', { message: `📥 WebMessage Received: type=${type}` });

    if (type === 'init_complete') {
        postToJS('ready', {});
        sendFullInit();
        return;
    }

    try {
        switch (type) {
            case 'get_file_tree': {
                const tree = JSON.parse(storage.getFileTreeJson());
                postToJS('file_tree_result', { tree });
                break;
            }
            case 'load_file_data': {
                const tabPath = payload.path;
                const root = storage.loadTabData(tabPath);
                postToJS('file_data_result', { path: tabPath, root });
                break;
            }
            case 'save_node': {
                storage.saveTabData(payload.tabFile, payload.root);
                sendAck(_reqId, true);
                break;
            }
            case 'set_language': {
                appLang = payload.language || 'en';
                sendAck(_reqId, true);
                break;
            }
            case 'save_session': {
                storage.saveSession({ tabs: payload.tabs });
                sendAck(_reqId, true);
                break;
            }
            case 'run_pipeline': {
                const pipelineName = payload.pipelineName;
                const content = payload.content || '';
                const pipelines = storage.loadPipelines();
                const p = pipelines.find(x => x.name === pipelineName);
                if (p) {
                    // Pre-resolve wizardData
                    const steps = JSON.parse(JSON.stringify(p.steps));
                    for (const step of steps) {
                        if (step.type === 'wizard' && step.wizard && !step.wizardData) {
                            const wizJson = storage.loadWizardData(step.wizard);
                            if (wizJson) {
                                step.wizardData = JSON.parse(wizJson);
                            }
                        }
                    }
                    runner.run(pipelineName, steps, content, [], p.outputMode);
                }
                break;
            }
            case 'run_prompt_process': {
                const steps = [{
                    name: new Date().toISOString(),
                    type: 'ai',
                    provider: payload.provider || 'openai',
                    model: payload.model || 'gpt-4o-mini',
                    systemPrompt: payload.systemPrompt || '',
                    userPrompt: payload.userPrompt || '{content}',
                    temperature: String(payload.temperature || 0.7)
                }];
                runner.run(new Date().toISOString(), steps, payload.content || '', [], 'child');
                break;
            }
            case 'cancel_pipeline': {
                runner.cancel();
                break;
            }
            case 'wizard_step_resume': {
                runner.resumeWizard(JSON.stringify(payload.values || {}));
                break;
            }
            case 'manual_step_resume': {
                runner.resumeManual(payload.content || '');
                break;
            }
            case 'manual_step_cancel': {
                runner.cancelManual();
                break;
            }
            case 'save_pipeline': {
                const pipelines = storage.loadPipelines();
                let idx = pipelines.findIndex(x => x.name === payload.name);
                const newPipe = {
                    name: payload.name,
                    mode: payload.mode || 'basic',
                    outputMode: payload.outputMode || 'child',
                    outputNaming: payload.outputNaming || '{pipeline_name}_{timestamp}',
                    retryCount: payload.retryCount || 0,
                    retryDelayMs: payload.retryDelayMs || 0,
                    multiMedia: payload.multiMedia || 'text',
                    steps: payload.steps || []
                };
                if (idx !== -1) {
                    pipelines[idx] = newPipe;
                } else {
                    pipelines.push(newPipe);
                }
                storage.savePipelines(pipelines);
                notifyEcodeReload();
                postToJS('pipeline_list', { pipelines });
                sendAck(_reqId, true);
                break;
            }
            case 'delete_pipeline': {
                let pipelines = storage.loadPipelines();
                pipelines = pipelines.filter(x => x.name !== payload.name);
                storage.savePipelines(pipelines);
                notifyEcodeReload();
                postToJS('pipeline_list', { pipelines });
                sendAck(_reqId, true);
                break;
            }
            case 'open_file': {
                handleOpenFileDialog();
                break;
            }
            case 'save_file_as': {
                handleSaveFileDialog();
                break;
            }
            case 'get_providers': {
                const provs = storage.loadProviders();
                postToJS('providers_result', provs);
                break;
            }
            case 'history_list': {
                const files = storage.listHistory();
                const items = [];
                for (const f of files.slice(0, 100)) {
                    const recJson = storage.loadHistoryRecord(f);
                    if (!recJson) continue;
                    try {
                        const rec = JSON.parse(recJson);
                        items.push({
                            id: rec.id || '',
                            pipelineName: rec.pipelineName || '',
                            startedAt: rec.startedAt || rec.executedAt || '',
                            status: rec.status || 'completed',
                            evaluation: rec.evaluation || '',
                            stepCount: rec.steps ? rec.steps.length : 0
                        });
                    } catch (e) {}
                }
                postToJS('history_list_result', { items });
                break;
            }
            case 'history_detail': {
                const recJson = storage.loadHistoryRecord(`run_${payload.id}.json`);
                if (recJson) {
                    postToJS('history_detail_result', JSON.parse(recJson));
                } else {
                    postToJS('history_detail_result', {});
                }
                break;
            }
            case 'evaluate_node': {
                const root = storage.loadTabData(payload.tabFile);
                const findNode = (node, pathStr) => {
                    if (!pathStr) return node;
                    const parts = pathStr.split('.');
                    const idx = parseInt(parts[0]);
                    if (isNaN(idx) || idx < 0 || idx >= node.children.length) return null;
                    return findNode(node.children[idx], parts.slice(1).join('.'));
                };
                const target = findNode(root, payload.nodeId);
                if (target) {
                    target.evaluation = payload.evaluation;
                    target.evaluatedAt = new Date().toISOString();
                    target.evaluationNote = payload.note || '';
                    storage.saveTabData(payload.tabFile, root);
                    postToJS('evaluation_saved', {
                        targetType: 'node',
                        id: payload.nodeId,
                        evaluation: payload.evaluation
                    });
                } else {
                    postToJS('evaluation_saved', { error: 'node not found' });
                }
                break;
            }
            case 'evaluate_history_step': {
                const filename = `run_${payload.runId}.json`;
                const recordStr = storage.loadHistoryRecord(filename);
                if (recordStr) {
                    const record = JSON.parse(recordStr);
                    const step = record.steps[payload.stepIndex];
                    if (step) {
                        step.evaluation = payload.evaluation;
                        step.evaluationNote = payload.note || '';
                        storage.saveHistory(record);
                        postToJS('evaluation_saved', {
                            targetType: 'step',
                            id: `${payload.runId}.${payload.stepIndex}`,
                            evaluation: payload.evaluation
                        });
                    }
                }
                break;
            }
            case 'evaluate_history_run': {
                storage.updateHistoryEvaluation(`run_${payload.runId}.json`, payload.evaluation);
                postToJS('evaluation_saved', {
                    targetType: 'run',
                    id: payload.runId,
                    evaluation: payload.evaluation
                });
                break;
            }
            case 'optimize_pipeline': {
                const name = payload.pipelineName;
                const historyLimit = payload.historyLimit || 10;
                const maxEdits = payload.maxEditsPerStep || 3;
                const prov = payload.provider;
                const model = payload.model;

                const pipelines = storage.loadPipelines();
                const p = pipelines.find(x => x.name === name);
                if (p) {
                    const providers = storage.loadProviders();
                    const apiKey = providers[prov]?.apiKey || '';
                    const baseUrl = providers[prov]?.baseUrl || '';

                    // Active session
                    global.activeOptSession = {
                        pipelineName: name,
                        proposals: [],
                        rejectedBuffer: optimizer.loadRejectedBuffer(name)
                    };

                    optimizer.startSession(name, p, historyLimit, maxEdits, prov, apiKey, baseUrl, model, (type, json) => {
                        if (type === 'optimize_proposals') {
                            const data = JSON.parse(json);
                            if (global.activeOptSession) {
                                global.activeOptSession.sessionId = data.sessionId;
                                global.activeOptSession.proposals = data.proposals;
                            }
                        }
                        postToJS(type, JSON.parse(json));
                    });
                }
                break;
            }
            case 'optimize_apply': {
                const name = payload.pipelineName;
                if (!global.activeOptSession || global.activeOptSession.pipelineName !== name) {
                    postToJS('optimize_error', { message: 'No active optimization session' });
                    break;
                }

                const approved = payload.approved || [];
                const rejected = payload.rejected || [];

                const pipelines = storage.loadPipelines();
                let idx = pipelines.findIndex(x => x.name === name);
                if (idx !== -1) {
                    const updated = optimizer.applyApprovals(pipelines[idx], approved, rejected, global.activeOptSession);
                    
                    // Save rejected buffer
                    optimizer.saveRejectedBuffer(name, global.activeOptSession.rejectedBuffer);

                    // Commit version
                    const approvedProps = approved.map(i => global.activeOptSession.proposals[i]).filter(Boolean);
                    versionMgr.createVersion(name, approvedProps, updated);

                    pipelines[idx] = updated;
                    storage.savePipelines(pipelines);
                    notifyEcodeReload();

                    global.activeOptSession = null;
                    sendAck(_reqId, true);
                }
                break;
            }
            case 'optimize_undo': {
                const name = payload.pipelineName;
                const reverted = versionMgr.undo(name);
                if (reverted) {
                    const pipelines = storage.loadPipelines();
                    const idx = pipelines.findIndex(x => x.name === name);
                    if (idx !== -1) {
                        pipelines[idx] = reverted;
                        storage.savePipelines(pipelines);
                        notifyEcodeReload();
                        
                        const cursor = versionMgr.loadCursor(name);
                        postToJS('optimize_version_changed', {
                            pipelineName: name,
                            version: cursor.currentVersion,
                            canUndo: cursor.currentVersion > 0,
                            canRedo: cursor.currentVersion < cursor.headVersion
                        });
                        postToJS('pipeline_list', { pipelines });
                    }
                }
                break;
            }
            case 'optimize_redo': {
                const name = payload.pipelineName;
                const advanced = versionMgr.redo(name);
                if (advanced) {
                    const pipelines = storage.loadPipelines();
                    const idx = pipelines.findIndex(x => x.name === name);
                    if (idx !== -1) {
                        pipelines[idx] = advanced;
                        storage.savePipelines(pipelines);
                        notifyEcodeReload();

                        const cursor = versionMgr.loadCursor(name);
                        postToJS('optimize_version_changed', {
                            pipelineName: name,
                            version: cursor.currentVersion,
                            canUndo: cursor.currentVersion > 0,
                            canRedo: cursor.currentVersion < cursor.headVersion
                        });
                        postToJS('pipeline_list', { pipelines });
                    }
                }
                break;
            }
            case 'optimize_checkout': {
                const name = payload.pipelineName;
                const ver = payload.version;
                const checkedOut = versionMgr.checkout(name, ver);
                if (checkedOut) {
                    const pipelines = storage.loadPipelines();
                    const idx = pipelines.findIndex(x => x.name === name);
                    if (idx !== -1) {
                        pipelines[idx] = checkedOut;
                        storage.savePipelines(pipelines);
                        notifyEcodeReload();

                        const cursor = versionMgr.loadCursor(name);
                        postToJS('optimize_version_changed', {
                            pipelineName: name,
                            version: cursor.currentVersion,
                            canUndo: cursor.currentVersion > 0,
                            canRedo: cursor.currentVersion < cursor.headVersion
                        });
                        postToJS('pipeline_list', { pipelines });
                    }
                }
                break;
            }
            case 'optimize_version_list': {
                const name = payload.pipelineName;
                const cursor = versionMgr.loadCursor(name);
                postToJS('optimize_version_list_result', cursor);
                break;
            }
            case 'save_providers': {
                const provs = payload;
                storage.saveProviders(provs);
                notifyEcodeReload();
                sendAck(_reqId, true);
                break;
            }
            case 'test_provider_connection': {
                const { provider, apiKey, baseUrl } = payload;
                const p = AIProvider.create(provider, apiKey, baseUrl);
                if (p) {
                    postToJS('log', { message: `Testing connection to ${provider}...` });
                    const err = await p.testConnection();
                    if (!err) {
                        postToJS('test_connection_result', { provider, success: true, message: 'Connection OK' });
                    } else {
                        postToJS('test_connection_result', { provider, success: false, message: err });
                    }
                } else {
                    postToJS('test_connection_result', { provider, success: false, message: `Unknown provider: ${provider}` });
                }
                break;
            }
            case 'step_filter_resume': {
                runner.resumeFilter(JSON.stringify(payload));
                break;
            }
            case 'fetch_models': {
                const providerType = payload.provider;
                const providers = storage.loadProviders();
                const apiKey = providers[providerType]?.apiKey || '';
                const baseUrl = providers[providerType]?.baseUrl || '';
                const p = AIProvider.create(providerType, apiKey, baseUrl);
                if (p) {
                    const list = p.listModels();
                    postToJS('model_list', { provider: providerType, models: list });
                }
                break;
            }
            case 'send_to_chest': {
                storage.saveToNamedChest(payload.chestName, payload.content);
                sendAck(_reqId, true);
                break;
            }
            case 'select_input_source': {
                const source = payload.source;
                if (source === 'chest') {
                    const content = storage.loadFromNamedChest(payload.chestName);
                    runner.setExternalInput(content);
                } else if (source === 'manual') {
                    runner.setExternalInput(payload.content || '');
                } else if (source === 'checkpoint') {
                    const cp = storage.loadCheckpointOutput(runner.getRunId(), 0);
                    if (cp) runner.setExternalInput(cp);
                } else {
                    runner.setExternalInput('');
                }
                break;
            }
            case 'select_project': {
                const projName = payload.projectName;
                const newPath = path.join(appDataPath, 'projects', projName);
                storage.init(newPath);
                sendFullInit();
                sendAck(_reqId, true);
                break;
            }
            case 'create_project': {
                const projName = payload.projectName;
                const projPath = path.join(appDataPath, 'projects', projName);
                fs.mkdirSync(path.join(projPath, 'data'), { recursive: true });
                fs.mkdirSync(path.join(projPath, 'blobs'), { recursive: true });
                fs.mkdirSync(path.join(projPath, 'history'), { recursive: true });
                storage.init(projPath);
                sendFullInit();
                sendAck(_reqId, true);
                break;
            }
            case 'save_run_state': {
                const runId = runner.getRunId();
                if (runId) {
                    storage.saveRunState(runId, JSON.stringify(payload));
                }
                sendAck(_reqId, true);
                break;
            }
            case 'resume_run': {
                const { runId, action } = payload;
                if (action === 'continue') {
                    postToJS('log', { message: `▶ Resuming run: ${runId}` });
                } else if (action === 'keep') {
                    storage.closeRun(runId);
                } else if (action === 'discard') {
                    storage.discardRun(runId);
                }
                break;
            }
            case 'set_history_retention': {
                const max = parseInt(payload.maxRuns);
                storage.maxHistoryRuns = max;
                const cfg = storage.loadGeneralConfig();
                cfg.historyRetention = max;
                storage.saveGeneralConfig(cfg);
                notifyEcodeReload();
                sendAck(_reqId, true);
                break;
            }
            case 'save_config': {
                const cfg = storage.loadGeneralConfig();
                if (payload.historyRetention) cfg.historyRetention = parseInt(payload.historyRetention);
                if (payload.defaultProvider) cfg.defaultProvider = payload.defaultProvider;
                if (payload.defaultModel) cfg.defaultModel = payload.defaultModel;
                storage.saveGeneralConfig(cfg);
                notifyEcodeReload();
                sendAck(_reqId, true);
                break;
            }
            case 'save_recipes': {
                storage.saveRecipes(payload);
                notifyEcodeReload();
                sendAck(_reqId, true);
                break;
            }
            default:
                postToJS('log', { message: `⚠ Unhandled message type: ${type}` });
        }
    } catch (e) {
        postToJS('log', { message: `❌ Error handling webmessage type=${type}: ${e.message}` });
        sendAck(_reqId, false);
    }
});
