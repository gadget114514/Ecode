'use strict';
const { app, BrowserWindow, ipcMain, dialog, Menu, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');
const https = require('https');
const http = require('http');
const { execFile, spawn } = require('child_process');

// Resolve frontend root: packaged app uses process.resourcesPath/frontend,
// dev run uses the sibling ../frontend directory.
const FRONTEND_ROOT = app.isPackaged
    ? path.join(process.resourcesPath, 'frontend')
    : path.join(__dirname, '..', 'frontend');

// ============================================================
// Paths
// ============================================================
function getAppDataPath() {
    return path.join(app.getPath('appData'), 'Ecode', 'Prompts');
}

// ============================================================
// Utilities
// ============================================================
function jsonEscape(s) {
    return s.replace(/\\/g, '\\\\').replace(/"/g, '\\"')
            .replace(/\n/g, '\\n').replace(/\r/g, '\\r').replace(/\t/g, '\\t');
}

function ensureDir(p) {
    try { fs.mkdirSync(p, { recursive: true }); } catch {}
}

function readJson(filePath, fallback = null) {
    try { return JSON.parse(fs.readFileSync(filePath, 'utf8')); }
    catch { return fallback; }
}

function writeJson(filePath, obj) {
    ensureDir(path.dirname(filePath));
    fs.writeFileSync(filePath, JSON.stringify(obj, null, 2), 'utf8');
}

function nowIso() {
    return new Date().toISOString().replace(/\.\d+Z$/, 'Z');
}

function generateRunId() {
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    const ts = `${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
    return ts + '_' + (Math.random() * 1e6 | 0);
}

// ============================================================
// HTTP helper
// ============================================================
function httpRequest(url, method, headers, body, timeoutMs = 60000) {
    return new Promise((resolve, reject) => {
        const u = new URL(url);
        const mod = u.protocol === 'https:' ? https : http;
        const opts = {
            hostname: u.hostname,
            port: u.port || (u.protocol === 'https:' ? 443 : 80),
            path: u.pathname + u.search,
            method,
            headers: { ...(body ? { 'Content-Length': Buffer.byteLength(body) } : {}), ...headers },
            timeout: timeoutMs,
        };
        const req = mod.request(opts, res => {
            const chunks = [];
            res.on('data', c => chunks.push(c));
            res.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
        });
        req.on('error', reject);
        req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
        if (body) req.write(body);
        req.end();
    });
}

// ============================================================
// AI Providers
// ============================================================
class OpenAIProvider {
    constructor(apiKey, baseUrl) {
        this.apiKey = apiKey;
        this.baseUrl = baseUrl || 'https://api.openai.com';
    }
    name() { return 'openai'; }
    defaultModels() { return ['gpt-4.1', 'gpt-4o-mini']; }

    _buildBody(req) {
        const messages = [];
        if (req.systemPrompt) messages.push({ role: 'system', content: req.systemPrompt });
        messages.push({ role: 'user', content: req.userPrompt });
        return JSON.stringify({
            model: req.model,
            messages,
            temperature: req.temperature ?? 0.7,
            max_tokens: req.maxTokens ?? 4096,
            stream: false,
        });
    }

    async call(req) {
        const body = this._buildBody(req);
        const raw = await httpRequest(
            this.baseUrl + '/v1/chat/completions', 'POST',
            { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + this.apiKey },
            body);
        const j = JSON.parse(raw);
        return { content: j.choices?.[0]?.message?.content ?? '[OpenAI: no content]', model: req.model };
    }

    async listModels() {
        try {
            const raw = await httpRequest(this.baseUrl + '/v1/models', 'GET',
                { 'Authorization': 'Bearer ' + this.apiKey }, null);
            const j = JSON.parse(raw);
            if (j.data) return j.data.map(m => m.id).sort();
        } catch {}
        return this.defaultModels();
    }

    async testConnection() {
        try {
            const raw = await httpRequest(this.baseUrl + '/v1/models', 'GET',
                { 'Authorization': 'Bearer ' + this.apiKey }, null);
            const j = JSON.parse(raw);
            if (j.error) return j.error.message;
            if (j.data) return '';
            return 'Unexpected response';
        } catch (e) { return e.message; }
    }
}

class AnthropicProvider {
    constructor(apiKey, baseUrl) {
        this.apiKey = apiKey;
        this.baseUrl = baseUrl || 'https://api.anthropic.com';
    }
    name() { return 'anthropic'; }
    defaultModels() { return ['claude-sonnet-4-6', 'claude-haiku-4-5']; }

    _buildBody(req) {
        return JSON.stringify({
            model: req.model,
            max_tokens: req.maxTokens ?? 4096,
            system: req.systemPrompt || '',
            messages: [{ role: 'user', content: req.userPrompt }],
            stream: false,
        });
    }

    async call(req) {
        const body = this._buildBody(req);
        const raw = await httpRequest(
            this.baseUrl + '/v1/messages', 'POST',
            { 'Content-Type': 'application/json', 'x-api-key': this.apiKey, 'anthropic-version': '2023-06-01' },
            body);
        const j = JSON.parse(raw);
        const content = j.content?.[0]?.text ?? '[Anthropic: no content]';
        return { content, model: req.model };
    }

    async listModels() {
        try {
            const raw = await httpRequest(this.baseUrl + '/v1/models', 'GET',
                { 'x-api-key': this.apiKey, 'anthropic-version': '2023-06-01' }, null);
            const j = JSON.parse(raw);
            if (j.data) return j.data.map(m => m.id).sort();
        } catch {}
        return this.defaultModels();
    }

    async testConnection() {
        try {
            const raw = await httpRequest(this.baseUrl + '/v1/models', 'GET',
                { 'x-api-key': this.apiKey, 'anthropic-version': '2023-06-01' }, null);
            const j = JSON.parse(raw);
            if (j.error) return j.error.message;
            if (j.data) return '';
            return 'Unexpected response';
        } catch (e) { return e.message; }
    }
}

class GeminiProvider {
    constructor(apiKey, baseUrl) {
        this.apiKey = apiKey;
        this.baseUrl = baseUrl || 'https://generativelanguage.googleapis.com';
    }
    name() { return 'gemini'; }
    defaultModels() { return ['gemini-2.5-flash', 'gemini-2.5-pro']; }

    _buildBody(req) {
        return JSON.stringify({
            contents: [{ parts: [{ text: req.userPrompt }] }],
            generationConfig: { temperature: req.temperature ?? 0.7, maxOutputTokens: req.maxTokens ?? 4096 },
            systemInstruction: req.systemPrompt ? { parts: [{ text: req.systemPrompt }] } : undefined,
        });
    }

    async call(req) {
        const body = this._buildBody(req);
        const url = `${this.baseUrl}/v1beta/models/${req.model}:generateContent?key=${this.apiKey}`;
        const raw = await httpRequest(url, 'POST', { 'Content-Type': 'application/json' }, body);
        const j = JSON.parse(raw);
        const content = j.candidates?.[0]?.content?.parts?.[0]?.text ?? '[Gemini: no content]';
        return { content, model: req.model };
    }

    async listModels() {
        try {
            const raw = await httpRequest(`${this.baseUrl}/v1beta/models?key=${this.apiKey}`, 'GET', {}, null);
            const j = JSON.parse(raw);
            if (j.models) return j.models.map(m => m.name.split('/').pop()).sort();
        } catch {}
        return this.defaultModels();
    }

    async testConnection() {
        try {
            const raw = await httpRequest(`${this.baseUrl}/v1beta/models?key=${this.apiKey}`, 'GET', {}, null);
            const j = JSON.parse(raw);
            if (j.error) return j.error.message;
            if (j.models) return '';
            return 'Unexpected response';
        } catch (e) { return e.message; }
    }
}

class OllamaProvider {
    constructor(apiKey, baseUrl) {
        this.apiKey = apiKey;
        this.baseUrl = baseUrl || 'http://localhost:11434';
    }
    name() { return 'ollama'; }
    defaultModels() { return ['llama3.2', 'mistral']; }

    _buildBody(req) {
        return JSON.stringify({
            model: req.model,
            system: req.systemPrompt || '',
            prompt: req.userPrompt,
            options: { temperature: req.temperature ?? 0.7 },
            stream: false,
        });
    }

    async call(req) {
        const body = this._buildBody(req);
        const raw = await httpRequest(this.baseUrl + '/api/generate', 'POST',
            { 'Content-Type': 'application/json' }, body);
        const j = JSON.parse(raw);
        return { content: j.response ?? '[Ollama: no content]', model: req.model };
    }

    async listModels() {
        try {
            const raw = await httpRequest(this.baseUrl + '/api/tags', 'GET', {}, null);
            const j = JSON.parse(raw);
            if (j.models) return j.models.map(m => m.name).sort();
        } catch {}
        return this.defaultModels();
    }

    async testConnection() {
        try {
            const raw = await httpRequest(this.baseUrl + '/api/tags', 'GET', {}, null);
            const j = JSON.parse(raw);
            if (j.error) return j.error;
            if (j.models) return '';
            return 'Unexpected response';
        } catch (e) { return e.message; }
    }
}

function createProvider(type, apiKey, baseUrl) {
    switch (type) {
        case 'openai':    return new OpenAIProvider(apiKey, baseUrl);
        case 'anthropic': return new AnthropicProvider(apiKey, baseUrl);
        case 'gemini':    return new GeminiProvider(apiKey, baseUrl);
        case 'ollama':    return new OllamaProvider(apiKey, baseUrl);
        default:          return null;
    }
}

// ============================================================
// Storage
// ============================================================
class Storage {
    constructor() {
        this.basePath = '';
    }

    init(basePath) {
        this.basePath = basePath;
        ensureDir(path.join(basePath, 'data'));
        ensureDir(path.join(basePath, 'blobs'));
        ensureDir(path.join(basePath, 'history'));
        return true;
    }

    dataPath(rel) {
        return path.join(this.basePath, 'data', rel);
    }

    blobPath(rel) {
        return path.join(this.basePath, 'blobs', rel);
    }

    getBasePath() { return this.basePath; }

    // Session
    loadSession() {
        return readJson(path.join(this.basePath, 'session.json'), { tabs: [] });
    }

    saveSession(session) {
        writeJson(path.join(this.basePath, 'session.json'), session);
    }

    // Tab data (nodes)
    loadTabData(filePath) {
        const full = filePath.includes(path.sep) ? filePath : this.dataPath(filePath);
        return readJson(full, { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [] });
    }

    saveTabData(filePath, root) {
        const full = filePath.includes(path.sep) ? filePath : this.dataPath(filePath);
        writeJson(full, root);
    }

    // Blobs
    loadBlob(relativePath) {
        try { return fs.readFileSync(this.blobPath(relativePath), 'base64'); } catch { return ''; }
    }

    saveBlob(data, ext) {
        const name = Date.now() + '_' + Math.random().toString(36).slice(2) + ext;
        fs.writeFileSync(this.blobPath(name), Buffer.from(data, 'base64'));
        return name;
    }

    removeBlob(relativePath) {
        try { fs.unlinkSync(this.blobPath(relativePath)); } catch {}
    }

    garbageCollectBlobs(referencedPaths) {
        try {
            const all = fs.readdirSync(path.join(this.basePath, 'blobs'));
            for (const f of all) {
                if (!referencedPaths.includes(f)) {
                    try { fs.unlinkSync(this.blobPath(f)); } catch {}
                }
            }
        } catch {}
    }

    getTabFiles() {
        try {
            return fs.readdirSync(path.join(this.basePath, 'data'))
                     .filter(f => f.endsWith('.json'));
        } catch { return []; }
    }

    getFileTreeJson() {
        const files = this.getTabFiles();
        return JSON.stringify(files.map(f => ({ name: f, path: f })));
    }

    // History
    saveHistory(recordJson) {
        try {
            const obj = JSON.parse(recordJson);
            const id = obj.id || generateRunId();
            const fileName = `run_${id}.json`;
            writeJson(path.join(this.basePath, 'history', fileName), obj);
            this._trimHistory();
        } catch (e) { console.error('saveHistory error:', e); }
    }

    _trimHistory() {
        try {
            const dir = path.join(this.basePath, 'history');
            const files = fs.readdirSync(dir)
                .filter(f => f.startsWith('run_') && f.endsWith('.json'))
                .map(f => ({ f, mtime: fs.statSync(path.join(dir, f)).mtimeMs }))
                .sort((a, b) => b.mtime - a.mtime);
            const max = this.maxHistoryRuns || 50;
            for (let i = max; i < files.length; i++) {
                try { fs.unlinkSync(path.join(dir, files[i].f)); } catch {}
            }
        } catch {}
    }

    updateHistoryEvaluation(filename, evaluation) {
        const p = path.join(this.basePath, 'history', filename);
        const obj = readJson(p, null);
        if (obj) { obj.evaluation = evaluation; writeJson(p, obj); }
    }

    listHistory() {
        try {
            return fs.readdirSync(path.join(this.basePath, 'history'))
                .filter(f => f.startsWith('run_') && f.endsWith('.json'))
                .sort().reverse();
        } catch { return []; }
    }

    loadHistoryRecord(filename) {
        try {
            return fs.readFileSync(path.join(this.basePath, 'history', filename), 'utf8');
        } catch { return ''; }
    }

    // Providers
    loadProviders() {
        return readJson(path.join(this.basePath, 'providers.json'), {});
    }

    saveProviders(providers) {
        writeJson(path.join(this.basePath, 'providers.json'), providers);
        return true;
    }

    // Pipelines
    loadPipelines() {
        const obj = readJson(path.join(this.basePath, 'pipelines.json'), { pipelines: [] });
        return obj.pipelines || obj || [];
    }

    savePipelines(pipelines) {
        writeJson(path.join(this.basePath, 'pipelines.json'), { pipelines });
    }

    // Recent files
    loadRecentFiles() {
        return readJson(path.join(this.basePath, 'recent_files.json'), []);
    }

    saveRecentFiles(files) {
        writeJson(path.join(this.basePath, 'recent_files.json'), files);
    }

    // General config
    loadGeneralConfig() {
        return readJson(path.join(this.basePath, 'config.json'), {
            historyRetention: 50, defaultProvider: 'openai', defaultModel: ''
        });
    }

    saveGeneralConfig(cfg) {
        writeJson(path.join(this.basePath, 'config.json'), cfg);
        this.maxHistoryRuns = cfg.historyRetention || 50;
        return true;
    }

    // Named chests
    _chestPath(name) {
        ensureDir(path.join(this.basePath, 'chests'));
        return path.join(this.basePath, 'chests', name + '.txt');
    }

    saveToNamedChest(name, content) {
        fs.writeFileSync(this._chestPath(name), content, 'utf8');
    }

    loadFromNamedChest(name) {
        try { return fs.readFileSync(this._chestPath(name), 'utf8'); } catch { return ''; }
    }

    chestExists(name) {
        return fs.existsSync(this._chestPath(name));
    }

    listNamedChests() {
        try {
            return fs.readdirSync(path.join(this.basePath, 'chests'))
                .filter(f => f.endsWith('.txt'))
                .map(f => f.slice(0, -4));
        } catch { return []; }
    }

    // Recipes
    loadRecipes() {
        return readJson(path.join(this.basePath, 'recipes.json'), []);
    }

    saveRecipes(recipes) {
        writeJson(path.join(this.basePath, 'recipes.json'), recipes);
        return true;
    }

    // Optimizer data
    saveOptimizerData(relativePath, json) {
        ensureDir(path.join(this.basePath, 'optimizer'));
        fs.writeFileSync(path.join(this.basePath, 'optimizer', relativePath), json, 'utf8');
    }

    loadOptimizerData(relativePath) {
        try { return fs.readFileSync(path.join(this.basePath, 'optimizer', relativePath), 'utf8'); }
        catch { return ''; }
    }

    // Wizard data
    loadWizardData(wizardName) {
        const candidates = [
            path.join(FRONTEND_ROOT, 'wizards', wizardName + '.json'),
        ];
        for (const p of candidates) {
            try { return fs.readFileSync(p, 'utf8'); } catch {}
        }
        return '';
    }

    // Run state
    saveRunState(runId, stateJson) {
        ensureDir(path.join(this.basePath, 'runs'));
        fs.writeFileSync(path.join(this.basePath, 'runs', runId + '_state.json'), stateJson, 'utf8');
    }

    loadRunState(runId) {
        try { return fs.readFileSync(path.join(this.basePath, 'runs', runId + '_state.json'), 'utf8'); }
        catch { return ''; }
    }

    scanIncompleteRuns() { return []; }
    closeRun(runId) {}
    discardRun(runId) {}

    ensureDirectory(p) {
        ensureDir(p);
        return true;
    }

    resolveProjectPath(rel) {
        const full = path.resolve(path.join(this.basePath, 'data', rel));
        if (!full.startsWith(path.join(this.basePath, 'data'))) return '';
        return full;
    }

    setMaxHistoryRuns(n) { this.maxHistoryRuns = n; }
    getMaxHistoryRuns() { return this.maxHistoryRuns || 50; }
}

// ============================================================
// Pipeline Runner
// ============================================================
class PipelineRunner {
    constructor() {
        this.running = false;
        this.cancelled = false;
        this.bridgeCb = null;
        this.providers = {};
        this.historySteps = [];
        this.currentStepIndex = -1;
        this.pendingSteps = [];
        this.inputContent = '';
        this.inputAttachments = [];
        this.outputMode = 'child';
        this.pipelineName = '';
        this.runId = '';
        this.startedAt = '';
        this.waitingForManual = false;
        this.waitingForWizard = false;
        this.waitingForFilter = false;
        this.wizardValues = {};
        this.filterApproved = [];
        this.filterRejected = [];
        this.inputSourceOverridden = false;
        this.inputSourceContent = '';
        this._manualResolve = null;
        this._wizardResolve = null;
        this._filterResolve = null;
    }

    setBridgeCallback(cb) { this.bridgeCb = cb; }

    registerProvider(type, apiKey, baseUrl) {
        const p = createProvider(type, apiKey, baseUrl);
        if (p) this.providers[type] = p;
    }

    postBridge(type, json) {
        if (this.bridgeCb) this.bridgeCb(type, json);
    }

    setExternalInput(content) {
        this.inputSourceOverridden = true;
        this.inputSourceContent = content;
    }

    getRunId() { return this.runId; }
    isRunning() { return this.running; }

    cancel() {
        this.cancelled = true;
        this.running = false;
        this.pendingSteps = [];
        if (this._manualResolve) { this._manualResolve(null); this._manualResolve = null; }
        if (this._wizardResolve) { this._wizardResolve(null); this._wizardResolve = null; }
        if (this._filterResolve) { this._filterResolve(null); this._filterResolve = null; }
    }

    resumeManual(content) {
        if (this._manualResolve) { this._manualResolve(content); this._manualResolve = null; }
    }

    cancelManual() {
        if (this._manualResolve) { this._manualResolve(null); this._manualResolve = null; }
    }

    resumeWizard(valuesJson) {
        if (this._wizardResolve) { this._wizardResolve(valuesJson); this._wizardResolve = null; }
    }

    resumeFilter(decisionJson) {
        if (this._filterResolve) { this._filterResolve(decisionJson); this._filterResolve = null; }
    }

    run(pipelineName, steps, inputContent, inputAttachments, outputMode) {
        if (this.running) return;
        this.pipelineName = pipelineName;
        this.inputContent = inputContent;
        this.inputAttachments = inputAttachments || [];
        this.outputMode = outputMode || 'child';
        this.cancelled = false;
        this.running = true;
        this.runId = generateRunId();
        this.startedAt = nowIso();
        this.historySteps = steps.map((s, i) => ({
            index: i, name: s.name, type: s.type, input: i === 0 ? inputContent : '',
            output: '', status: 'pending', promptTokens: 0, completionTokens: 0,
            parallelBranches: {}, retries: 0, iterations: 0,
        }));
        this.currentStepIndex = -1;
        this.pendingSteps = [...steps];
        this.inputSourceOverridden = false;
        this.inputSourceContent = '';
        this.postBridge('step_started', JSON.stringify({ index: 0, name: steps[0]?.name || '' }));
        this._runNext().catch(e => {
            this.running = false;
            this.postBridge('pipeline_error', JSON.stringify({ message: String(e) }));
        });
    }

    _currentContent() {
        if (this.inputSourceOverridden) return this.inputSourceContent;
        if (this.currentStepIndex > 0 && this.historySteps[this.currentStepIndex - 1])
            return this.historySteps[this.currentStepIndex - 1].output;
        return this.inputContent;
    }

    async _runNext() {
        if (this.cancelled || this.pendingSteps.length === 0) {
            this.running = false;
            if (!this.cancelled) {
                this.postBridge('pipeline_completed', this._buildMeta());
            } else {
                this.postBridge('pipeline_error', JSON.stringify({ message: 'Canceled' }));
            }
            return;
        }

        this.currentStepIndex++;
        const step = this.pendingSteps.shift();

        // Update input for this step
        if (this.currentStepIndex < this.historySteps.length) {
            this.historySteps[this.currentStepIndex].input = this._currentContent();
            this.historySteps[this.currentStepIndex].status = 'running';
        }

        this.postBridge('step_started', JSON.stringify({ index: this.currentStepIndex, name: step.name }));

        try {
            await this._executeStep(step);
        } catch (e) {
            this.running = false;
            this.postBridge('pipeline_error', JSON.stringify({ message: String(e) }));
            return;
        }

        if (!this.waitingForManual && !this.waitingForWizard && !this.waitingForFilter && this.running) {
            await this._runNext();
        }
    }

    async _executeStep(step) {
        const type = step.type;
        const idx = this.currentStepIndex;

        if (type === 'ai') {
            const providerName = step.params?.provider || 'openai';
            const provider = this.providers[providerName];
            if (!provider) {
                throw new Error('Provider not configured: ' + providerName);
            }

            let userPrompt = step.params?.userPrompt || '{content}';
            userPrompt = userPrompt.replace(/\{content\}/g, this.inputContent)
                                   .replace(/\{result\}/g, this._currentContent());

            const req = {
                model: step.params?.model || 'gpt-4.1',
                systemPrompt: step.params?.systemPrompt || '',
                userPrompt,
                temperature: parseFloat(step.params?.temperature || '0.7'),
                maxTokens: parseInt(step.params?.maxTokens || '4096'),
            };

            const resp = await provider.call(req);
            if (idx < this.historySteps.length) {
                this.historySteps[idx].output = resp.content;
                this.historySteps[idx].status = 'completed';
            }
            this.postBridge('step_done', JSON.stringify({ index: idx, tokens: resp.completionTokens || 0 }));

        } else if (type === 'manual') {
            const mode = step.params?.mode || 'view';
            const prompt = step.params?.prompt || '';
            const content = this._currentContent();
            this.waitingForManual = true;

            if (mode === 'compare') {
                const branches = idx > 0 && this.historySteps[idx - 1]
                    ? Object.entries(this.historySteps[idx - 1].parallelBranches || {}).map(([name, c]) => ({ name, content: c }))
                    : [];
                this.postBridge('manual_step_pause', JSON.stringify({ index: idx, mode: 'compare', prompt, branches }));
            } else {
                const choices = step.params?.choices ? JSON.parse(step.params.choices) : [];
                this.postBridge('manual_step_pause', JSON.stringify({ index: idx, mode, prompt, content, choices }));
            }

            const result = await new Promise(res => { this._manualResolve = res; });
            this.waitingForManual = false;

            if (idx < this.historySteps.length) {
                this.historySteps[idx].output = result ?? content;
                this.historySteps[idx].status = 'completed';
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'command') {
            const cmd = step.params?.command || '';
            const argsStr = step.params?.args || '[]';
            const workDir = step.params?.workingDir || '';
            const timeoutSec = parseInt(step.params?.timeout || '60');
            const content = this._currentContent();
            const args = JSON.parse(argsStr);

            // Write content to temp file
            const tmpFile = path.join(os.tmpdir(), 'prompts_' + Date.now() + '.tmp');
            fs.writeFileSync(tmpFile, content, 'utf8');

            const resolvedArgs = args.map(a =>
                a.replace('{content_file}', tmpFile)
                 .replace('{content}', content)
                 .replace('{result}', content));

            let output = '';
            await new Promise((resolve) => {
                const proc = spawn(cmd, resolvedArgs, {
                    cwd: workDir || undefined,
                    shell: false,
                    timeout: timeoutSec * 1000,
                });
                proc.stdout.on('data', chunk => {
                    const text = chunk.toString('utf8');
                    output += text;
                    this.postBridge('stream_chunk', JSON.stringify({ stepIndex: idx, text }));
                });
                proc.stderr.on('data', chunk => {
                    const text = chunk.toString('utf8');
                    output += text;
                    this.postBridge('stream_chunk', JSON.stringify({ stepIndex: idx, text }));
                });
                proc.on('close', () => resolve());
                proc.on('error', e => { output += '[error: ' + e.message + ']'; resolve(); });
            });

            try { fs.unlinkSync(tmpFile); } catch {}

            if (idx < this.historySteps.length) {
                this.historySteps[idx].output = output;
                this.historySteps[idx].status = 'completed';
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'parallel') {
            const branchesVal = JSON.parse(step.params?.branches || '[]');
            const inputForBranches = this._currentContent();
            const branchResults = {};

            for (const branch of branchesVal) {
                const branchName = branch.name || 'branch';
                const subSteps = branch.steps || [];
                let branchContent = inputForBranches;
                for (const subStep of subSteps) {
                    if (subStep.type === 'ai') {
                        const providerName = subStep.provider || 'openai';
                        const provider = this.providers[providerName];
                        if (!provider) continue;
                        let userPrompt = (subStep.userPrompt || '{content}')
                            .replace('{content}', inputForBranches)
                            .replace('{result}', branchContent);
                        const resp = await provider.call({
                            model: subStep.model || 'gpt-4.1',
                            systemPrompt: subStep.systemPrompt || '',
                            userPrompt,
                            temperature: parseFloat(subStep.temperature || '0.7'),
                            maxTokens: 4096,
                        });
                        branchContent = resp.content;
                    }
                }
                branchResults[branchName] = branchContent;
            }

            if (idx < this.historySteps.length) {
                this.historySteps[idx].parallelBranches = branchResults;
                this.historySteps[idx].output = JSON.stringify(branchResults);
                this.historySteps[idx].status = 'completed';
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'wizard') {
            const wizardName = step.params?.wizard || '';
            const wizardData = step.params?.wizardData || '{}';
            const content = this._currentContent();
            this.waitingForWizard = true;

            this.postBridge('wizard_step_pause', JSON.stringify({
                index: idx, wizard: wizardName,
                wizardData: JSON.parse(wizardData), content
            }));

            const valuesJson = await new Promise(res => { this._wizardResolve = res; });
            this.waitingForWizard = false;

            if (idx < this.historySteps.length) {
                this.historySteps[idx].output = valuesJson || '{}';
                this.historySteps[idx].status = 'completed';
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'filter') {
            const content = this._currentContent();
            const mode = step.params?.mode || 'manual';
            const splitBy = step.params?.splitBy || '';
            this.waitingForFilter = true;

            let blocks;
            if (splitBy && content) {
                blocks = content.split(splitBy);
            } else {
                blocks = [content];
            }
            const outputs = blocks.map((c, i) => ({ index: i, content: c }));

            if (mode === 'auto') {
                this.waitingForFilter = false;
                if (idx < this.historySteps.length) {
                    this.historySteps[idx].status = 'completed';
                    this.historySteps[idx].output = content;
                }
                this.postBridge('step_done', JSON.stringify({ index: idx }));
                return;
            }

            this.postBridge('step_filter_pause', JSON.stringify({ index: idx, mode, outputs }));
            const decision = await new Promise(res => { this._filterResolve = res; });
            this.waitingForFilter = false;

            if (idx < this.historySteps.length) {
                this.historySteps[idx].status = 'completed';
                this.historySteps[idx].output = content;
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'evaluate') {
            const content = this._currentContent();
            const criteria = step.params?.criteria || '';
            const rubric = step.params?.rubric || '1-10';
            if (idx < this.historySteps.length) {
                this.historySteps[idx].status = 'completed';
                this.historySteps[idx].output = content;
            }
            this.postBridge('evaluate_result', JSON.stringify({ stepIndex: idx, content, criteria, rubric }));
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'chest') {
            const chestName = step.params?.chestName || '';
            const mode = step.params?.mode || 'put';
            const content = this._currentContent();

            if (mode === 'put') {
                this.postBridge('chest_put', JSON.stringify({ chestName, content }));
            } else if (mode === 'take') {
                this.postBridge('chest_take', JSON.stringify({ chestName }));
            }
            if (idx < this.historySteps.length) {
                this.historySteps[idx].status = 'completed';
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'condition') {
            const content = this._currentContent();
            if (idx < this.historySteps.length) {
                this.historySteps[idx].status = 'completed';
                this.historySteps[idx].output = content;
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else {
            this.postBridge('log', JSON.stringify({ message: '⚠ Unimplemented step type: ' + type + ' — skipped' }));
            if (idx < this.historySteps.length) {
                this.historySteps[idx].status = 'skipped';
            }
        }
    }

    _buildMeta() {
        return JSON.stringify({
            id: this.runId,
            pipelineName: this.pipelineName,
            startedAt: this.startedAt,
            status: 'completed',
            outputMode: this.outputMode,
            steps: this.historySteps.map(s => ({
                index: s.index, name: s.name, type: s.type,
                input: s.input, output: s.output, status: s.status,
                promptTokens: s.promptTokens, completionTokens: s.completionTokens,
                parallelBranches: s.parallelBranches,
            })),
        });
    }
}

// ============================================================
// Pipeline Version Manager (simplified)
// ============================================================
class PipelineVersionManager {
    constructor(storage) {
        this.storage = storage;
        this.cursors = {};
    }

    _getVersionsPath(name) {
        return path.join(this.storage.basePath, 'optimizer', name + '_versions.json');
    }

    _loadVersions(name) {
        const p = this._getVersionsPath(name);
        return readJson(p, { versions: [], currentVersion: 0, headVersion: 0 });
    }

    _saveVersions(name, data) {
        ensureDir(path.join(this.storage.basePath, 'optimizer'));
        writeJson(this._getVersionsPath(name), data);
    }

    ensureBaseVersion(name, pipeline) {
        const data = this._loadVersions(name);
        if (data.versions.length === 0) {
            data.versions.push({ version: 1, pipeline: JSON.parse(JSON.stringify(pipeline)), timestamp: nowIso(), label: 'Base' });
            data.currentVersion = 1;
            data.headVersion = 1;
            this._saveVersions(name, data);
        }
        return data;
    }

    commitVersion(name, pipeline, sessionId, label, proposals) {
        const data = this._loadVersions(name);
        const next = data.headVersion + 1;
        data.versions.push({ version: next, pipeline: JSON.parse(JSON.stringify(pipeline)), timestamp: nowIso(), label, sessionId });
        data.currentVersion = next;
        data.headVersion = next;
        this._saveVersions(name, data);
        return next;
    }

    getCursor(name) {
        const data = this._loadVersions(name);
        return {
            pipelineName: name,
            currentVersion: data.currentVersion,
            headVersion: data.headVersion,
            entries: data.versions.map(v => ({ version: v.version, timestamp: v.timestamp, label: v.label, sessionId: v.sessionId || '' })),
        };
    }

    _findPipeline(name, version) {
        const data = this._loadVersions(name);
        const entry = data.versions.find(v => v.version === version);
        return entry ? entry.pipeline : null;
    }

    undo(name) {
        const data = this._loadVersions(name);
        if (data.currentVersion <= 1) return null;
        data.currentVersion--;
        this._saveVersions(name, data);
        return this._findPipeline(name, data.currentVersion);
    }

    redo(name) {
        const data = this._loadVersions(name);
        if (data.currentVersion >= data.headVersion) return null;
        data.currentVersion++;
        this._saveVersions(name, data);
        return this._findPipeline(name, data.currentVersion);
    }

    checkoutVersion(name, version) {
        const data = this._loadVersions(name);
        const entry = data.versions.find(v => v.version === version);
        if (!entry) return null;
        data.currentVersion = version;
        this._saveVersions(name, data);
        return entry.pipeline;
    }

    reapplyVersion(name, version, current) {
        return this._findPipeline(name, version) || current;
    }
}

// ============================================================
// Pipeline Optimizer (stub — calls AI for proposals)
// ============================================================
class PipelineOptimizer {
    constructor(storage) {
        this.storage = storage;
    }

    loadRejectedBuffer(name) {
        const raw = this.storage.loadOptimizerData(name + '_rejected.json');
        return raw ? JSON.parse(raw) : [];
    }

    saveRejectedBuffer(name, buffer) {
        this.storage.saveOptimizerData(name + '_rejected.json', JSON.stringify(buffer));
    }

    async startSession(name, pipeline, historyLimit, maxEdits, providerName, apiKey, baseUrl, model, callback) {
        const sessionId = generateRunId();
        callback('log', JSON.stringify({ message: '🔍 Analyzing pipeline for optimization...' }));

        const provider = createProvider(providerName, apiKey, baseUrl);
        if (!provider) {
            callback('optimize_error', JSON.stringify({ message: 'Unknown provider: ' + providerName }));
            return;
        }

        try {
            const pipelineJson = JSON.stringify(pipeline, null, 2);
            const resp = await provider.call({
                model,
                systemPrompt: 'You are a pipeline optimization expert. Analyze the pipeline and suggest improvements.',
                userPrompt: `Analyze this pipeline and suggest improvements as JSON proposals:\n${pipelineJson}\n\nRespond with JSON: {"proposals": [{"op": "modify", "stepName": "...", "field": "...", "oldValue": "...", "newValue": "...", "rationale": "..."}]}`,
                temperature: 0.7,
                maxTokens: 2048,
            });

            let proposals = [];
            try {
                const parsed = JSON.parse(resp.content);
                proposals = parsed.proposals || [];
            } catch {
                proposals = [{ op: 'note', stepName: '', field: '', oldValue: '', newValue: resp.content, rationale: 'AI analysis' }];
            }

            callback('optimize_proposals', JSON.stringify({ sessionId, proposals }));
        } catch (e) {
            callback('optimize_error', JSON.stringify({ message: String(e) }));
        }
    }

    static applyApprovals(pipeline, approved, rejected, session) {
        const updated = JSON.parse(JSON.stringify(pipeline));
        for (const idx of approved) {
            const prop = session.proposals[idx];
            if (!prop) continue;
            const step = updated.steps?.find(s => s.name === prop.stepName);
            if (step && prop.field && prop.newValue !== undefined) {
                if (!step.params) step.params = {};
                step.params[prop.field] = prop.newValue;
            }
        }
        return updated;
    }
}

// ============================================================
// App State
// ============================================================
const storage = new Storage();
const runner = new PipelineRunner();
const versionMgr = new PipelineVersionManager(storage);
const optimizer = new PipelineOptimizer(storage);

let mainWindow = null;
let appDataPath = '';
let recentFiles = [];
let activeOptSession = null;
let localization = { lang: 'en' };
let embedded = false;

function postToJS(type, payload) {
    if (type === 'log') {
        try {
            const msg = typeof payload === 'string' ? JSON.parse(payload) : payload;
            console.log('[Prompts]', msg.message || payload);
        } catch { console.log('[Prompts]', payload); }
    }
    if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send('bridge-message', JSON.stringify({ type, payload: typeof payload === 'string' ? JSON.parse(payload) : payload }));
    }
}

runner.setBridgeCallback((type, json) => {
    if (type === 'pipeline_completed') storage.saveHistory(json);
    postToJS('log', JSON.stringify({ message: '🏃 Pipeline: ' + type }));
    postToJS(type, JSON.parse(json));
});

// ============================================================
// Full Init
// ============================================================
function sendFullInit() {
    postToJS('log', JSON.stringify({ message: '[TRACE] SendFullInit: loading session...' }));

    let session = storage.loadSession();
    if (!session.tabs || session.tabs.length === 0) {
        const tab = { name: 'General', file: 'general.json' };
        storage.saveTabData('general.json', { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [] });
        session = { tabs: [tab] };
        storage.saveSession(session);
    }

    const nodes = {};
    for (const tab of session.tabs) {
        nodes[tab.file] = storage.loadTabData(tab.file);
    }

    const pipelines = storage.loadPipelines();

    const providers = storage.loadProviders();
    for (const [type, cfg] of Object.entries(providers)) {
        runner.registerProvider(type, cfg.apiKey, cfg.baseUrl);
    }

    recentFiles = storage.loadRecentFiles();

    const generalCfg = storage.loadGeneralConfig();
    storage.maxHistoryRuns = generalCfg.historyRetention || 50;

    const chestList = storage.listNamedChests();
    const recipes = storage.loadRecipes();

    postToJS('init', {
        language: localization.lang,
        embedded,
        tabs: session.tabs,
        nodes,
        pipelines,
        providers,
        recentFiles,
        config: { historyRetention: generalCfg.historyRetention, chestList, defaultProvider: generalCfg.defaultProvider, defaultModel: generalCfg.defaultModel },
        recipes,
    });

    postToJS('log', JSON.stringify({ message: '[TRACE] SendFullInit: init posted' }));
}

// ============================================================
// Bridge Message Handler
// ============================================================
function handleBridgeMessage(type, payload) {
    switch (type) {
        case 'get_file_tree': {
            const tree = storage.getFileTreeJson();
            postToJS('file_tree_result', { tree: JSON.parse(tree) });
            break;
        }
        case 'load_file_data': {
            if (payload?.path) {
                const root = storage.loadTabData(payload.path);
                postToJS('file_data_result', { path: payload.path, root });
            }
            break;
        }
        case 'save_node': {
            if (payload?.tabFile && payload?.root) {
                storage.saveTabData(payload.tabFile, payload.root);
            }
            break;
        }
        case 'set_language': {
            if (payload?.language) localization.lang = payload.language;
            break;
        }
        case 'save_session': {
            if (payload?.tabs) storage.saveSession({ tabs: payload.tabs });
            break;
        }
        case 'run_pipeline': {
            if (payload?.pipelineName) {
                const pipelines = storage.loadPipelines();
                const pipeline = pipelines.find(p => p.name === payload.pipelineName);
                if (pipeline) {
                    for (const step of pipeline.steps) {
                        if (step.type === 'wizard' && step.params?.wizard && !step.params?.wizardData) {
                            const wd = storage.loadWizardData(step.params.wizard);
                            if (wd) step.params.wizardData = wd;
                        }
                    }
                    runner.run(pipeline.name, pipeline.steps, payload.content || '', [], pipeline.outputMode || 'child');
                }
            }
            break;
        }
        case 'run_prompt_process': {
            if (payload?.content) {
                const step = {
                    name: 'Process Prompt',
                    type: 'ai',
                    params: {
                        provider: payload.provider || 'openai',
                        model: payload.model || 'gpt-4.1',
                        systemPrompt: payload.systemPrompt || '',
                        userPrompt: payload.userPrompt || '{content}',
                        temperature: String(payload.temperature ?? 0.7),
                    },
                };
                runner.run('Process Prompt', [step], payload.content, [], 'child');
            }
            break;
        }
        case 'cancel_pipeline':
            runner.cancel();
            break;
        case 'wizard_step_resume':
            runner.resumeWizard(JSON.stringify(payload?.values || {}));
            break;
        case 'manual_step_resume':
            runner.resumeManual(payload?.content ?? '');
            break;
        case 'manual_step_cancel':
            runner.cancelManual();
            break;
        case 'step_filter_resume':
            runner.resumeFilter(typeof payload === 'string' ? payload : JSON.stringify(payload));
            break;
        case 'save_pipeline':
            handleSavePipeline(payload);
            break;
        case 'delete_pipeline':
            handleDeletePipeline(payload);
            break;
        case 'open_file':
            openFileDialog();
            break;
        case 'save_file_as':
            saveFileDialog();
            break;
        case 'get_providers': {
            const providers = storage.loadProviders();
            postToJS('providers_result', providers);
            break;
        }
        case 'save_providers': {
            const providers = payload || {};
            storage.saveProviders(providers);
            for (const [type, cfg] of Object.entries(providers)) {
                runner.registerProvider(type, cfg.apiKey || '', cfg.baseUrl || '');
            }
            break;
        }
        case 'test_provider_connection': {
            const { provider: prov, apiKey, baseUrl } = payload || {};
            const p = createProvider(prov, apiKey, baseUrl);
            if (p) {
                p.testConnection().then(err => {
                    postToJS('test_connection_result', {
                        provider: prov,
                        success: !err,
                        message: err || 'Connection OK',
                    });
                });
            } else {
                postToJS('test_connection_result', { provider: prov, success: false, message: 'Unknown provider' });
            }
            break;
        }
        case 'fetch_models': {
            const prov = payload?.provider;
            if (prov) {
                const providers = storage.loadProviders();
                const cfg = providers[prov] || {};
                const p = createProvider(prov, cfg.apiKey, cfg.baseUrl);
                if (p) {
                    p.listModels().then(models => {
                        postToJS('model_list', { provider: prov, models });
                    });
                } else {
                    postToJS('model_list', { provider: prov, models: [] });
                }
            }
            break;
        }
        case 'history_list':
            handleHistoryList();
            break;
        case 'history_detail':
            handleHistoryDetail(payload);
            break;
        case 'evaluate_node':
            handleEvaluateNode(payload);
            break;
        case 'evaluate_history_step':
            handleEvaluateHistoryStep(payload);
            break;
        case 'evaluate_history_run':
            handleEvaluateHistoryRun(payload);
            break;
        case 'optimize_pipeline':
            handleOptimizePipeline(payload);
            break;
        case 'optimize_apply':
            handleOptimizeApply(payload);
            break;
        case 'optimize_undo':
            handleOptimizeUndo(payload);
            break;
        case 'optimize_redo':
            handleOptimizeRedo(payload);
            break;
        case 'optimize_checkout':
            handleOptimizeCheckout(payload);
            break;
        case 'optimize_reapply':
            handleOptimizeReapply(payload);
            break;
        case 'optimize_version_list':
            handleOptimizeVersionList(payload);
            break;
        case 'send_to_chest': {
            if (payload?.chestName && payload?.content != null) {
                storage.saveToNamedChest(payload.chestName, payload.content);
            }
            break;
        }
        case 'select_input_source': {
            const src = payload?.source;
            if (src === 'chest' && payload?.chestName) {
                runner.setExternalInput(storage.loadFromNamedChest(payload.chestName));
            } else if (src === 'manual' && payload?.content != null) {
                runner.setExternalInput(payload.content);
            } else {
                runner.setExternalInput('');
            }
            break;
        }
        case 'save_config': {
            const cfg = { historyRetention: payload?.historyRetention || 50, defaultProvider: payload?.defaultProvider || 'openai', defaultModel: payload?.defaultModel || '' };
            storage.saveGeneralConfig(cfg);
            break;
        }
        case 'save_recipes': {
            storage.saveRecipes(payload || []);
            break;
        }
        case 'set_history_retention': {
            if (payload?.maxRuns) {
                storage.setMaxHistoryRuns(payload.maxRuns);
                const cfg = storage.loadGeneralConfig();
                cfg.historyRetention = payload.maxRuns;
                storage.saveGeneralConfig(cfg);
            }
            break;
        }
        case 'select_project': {
            if (payload?.projectName) {
                const newPath = path.join(appDataPath, 'projects', payload.projectName);
                storage.init(newPath);
                const pipelines = storage.loadPipelines();
                postToJS('project_changed', { projectName: payload.projectName, pipelines });
            }
            break;
        }
        case 'create_project': {
            if (payload?.projectName) {
                const projPath = path.join(appDataPath, 'projects', payload.projectName);
                storage.init(projPath);
                postToJS('project_changed', { projectName: payload.projectName, tabs: [], pipelines: [] });
            }
            break;
        }
        case 'save_run_state': {
            const runId = runner.getRunId();
            if (runId) storage.saveRunState(runId, JSON.stringify(payload));
            break;
        }
        case 'resume_run': {
            const action = payload?.action;
            const runId = payload?.runId;
            if (action === 'keep') storage.closeRun(runId);
            else if (action === 'discard') storage.discardRun(runId);
            break;
        }
        default:
            postToJS('log', JSON.stringify({ message: '[bridge] unhandled type: ' + type }));
    }
}

// ============================================================
// Pipeline CRUD
// ============================================================
function handleSavePipeline(payload) {
    const name = payload?.name;
    if (!name) return;
    const pipelines = storage.loadPipelines();
    const idx = pipelines.findIndex(p => p.name === name);
    if (idx >= 0) {
        Object.assign(pipelines[idx], payload);
    } else {
        pipelines.push(payload);
    }
    storage.savePipelines(pipelines);
    postToJS('pipeline_list', { pipelines });
}

function handleDeletePipeline(payload) {
    const name = payload?.name;
    if (!name) return;
    const pipelines = storage.loadPipelines().filter(p => p.name !== name);
    storage.savePipelines(pipelines);
    postToJS('pipeline_list', { pipelines });
}

// ============================================================
// History
// ============================================================
function handleHistoryList() {
    const files = storage.listHistory();
    const items = [];
    let limit = 100;
    for (const file of files) {
        if (limit-- <= 0) break;
        const raw = storage.loadHistoryRecord(file);
        if (!raw) continue;
        try {
            const obj = JSON.parse(raw);
            if (!obj.pipelineName) continue;
            items.push({
                id: obj.id || '',
                pipelineName: obj.pipelineName || '',
                startedAt: obj.startedAt || obj.executedAt || '',
                status: obj.status || 'completed',
                evaluation: obj.evaluation || '',
                stepCount: (obj.steps || []).length,
            });
        } catch {}
    }
    postToJS('history_list_result', { items });
}

function handleHistoryDetail(payload) {
    const id = payload?.id;
    if (!id) { postToJS('history_detail_result', {}); return; }
    const raw = storage.loadHistoryRecord('run_' + id + '.json');
    if (!raw) { postToJS('history_detail_result', {}); return; }
    try { postToJS('history_detail_result', JSON.parse(raw)); }
    catch { postToJS('history_detail_result', {}); }
}

function handleEvaluateNode(payload) {
    const { nodeId, tabFile, evaluation, note } = payload || {};
    if (!nodeId || !tabFile) return;
    const root = storage.loadTabData(tabFile);

    function findNode(n, nodePath) {
        if (!nodePath) return n;
        const parts = nodePath.split('.');
        let cur = n;
        for (const p of parts) {
            const i = parseInt(p);
            if (!cur.children || i >= cur.children.length) return null;
            cur = cur.children[i];
        }
        return cur;
    }

    const target = findNode(root, nodeId);
    if (!target) { postToJS('evaluation_saved', { error: 'node not found' }); return; }
    target.evaluation = evaluation;
    target.evaluatedAt = nowIso();
    target.evaluationNote = note || '';
    storage.saveTabData(tabFile, root);
    postToJS('evaluation_saved', { targetType: 'node', id: nodeId, evaluation });
}

function handleEvaluateHistoryStep(payload) {
    const { runId, stepIndex, evaluation, note } = payload || {};
    if (!runId || stepIndex == null) return;
    const filename = 'run_' + runId + '.json';
    const raw = storage.loadHistoryRecord(filename);
    if (!raw) return;
    const obj = JSON.parse(raw);
    if (obj.steps?.[stepIndex]) {
        obj.steps[stepIndex].evaluation = evaluation;
        obj.steps[stepIndex].evaluationNote = note || '';
    }
    storage.saveHistory(JSON.stringify(obj));
    postToJS('evaluation_saved', { targetType: 'step', id: runId + '.' + stepIndex, evaluation });
}

function handleEvaluateHistoryRun(payload) {
    const { runId, evaluation } = payload || {};
    if (!runId) return;
    storage.updateHistoryEvaluation('run_' + runId + '.json', evaluation);
    postToJS('evaluation_saved', { targetType: 'run', id: runId, evaluation });
}

// ============================================================
// Optimizer
// ============================================================
async function handleOptimizePipeline(payload) {
    const { pipelineName, historyLimit, maxEditsPerStep, provider: prov, model } = payload || {};
    if (!pipelineName || !prov || !model) {
        postToJS('optimize_error', { message: 'Missing pipelineName, provider, or model' });
        return;
    }
    const pipelines = storage.loadPipelines();
    const pipeline = pipelines.find(p => p.name === pipelineName);
    if (!pipeline) { postToJS('optimize_error', { message: 'Pipeline not found: ' + pipelineName }); return; }

    versionMgr.ensureBaseVersion(pipelineName, pipeline);
    const providers = storage.loadProviders();
    const cfg = providers[prov] || {};

    activeOptSession = { pipelineName, sessionId: '', proposals: [], rejectedBuffer: optimizer.loadRejectedBuffer(pipelineName) };

    await optimizer.startSession(pipelineName, pipeline, historyLimit, maxEditsPerStep, prov, cfg.apiKey, cfg.baseUrl, model, (type, json) => {
        if (type === 'optimize_proposals') {
            const v = JSON.parse(json);
            if (activeOptSession) {
                activeOptSession.sessionId = v.sessionId || '';
                activeOptSession.proposals = v.proposals || [];
            }
        }
        postToJS(type, JSON.parse(json));
    });
}

function handleOptimizeApply(payload) {
    const { pipelineName, approved, rejected } = payload || {};
    if (!pipelineName || !activeOptSession || activeOptSession.pipelineName !== pipelineName) {
        postToJS('optimize_error', { message: 'No active optimization session' });
        return;
    }
    const pipelines = storage.loadPipelines();
    const idx = pipelines.findIndex(p => p.name === pipelineName);
    if (idx < 0) { postToJS('optimize_error', { message: 'Pipeline not found' }); return; }

    const updated = PipelineOptimizer.applyApprovals(pipelines[idx], approved || [], rejected || [], activeOptSession);
    optimizer.saveRejectedBuffer(pipelineName, activeOptSession.rejectedBuffer);

    const label = 'Optimize (' + (approved?.length || 0) + ' edits)';
    const newVersion = versionMgr.commitVersion(pipelineName, updated, activeOptSession.sessionId, label, []);

    pipelines[idx] = updated;
    storage.savePipelines(pipelines);
    activeOptSession = null;

    const cursor = versionMgr.getCursor(pipelineName);
    postToJS('optimize_applied', { pipelineName, approvedCount: (approved||[]).length, rejectedCount: (rejected||[]).length, version: newVersion });
    postToJS('pipeline_list', { pipelines });
    postToJS('optimize_version_changed', { pipelineName, version: cursor.currentVersion, canUndo: cursor.currentVersion > 1, canRedo: cursor.currentVersion < cursor.headVersion });
}

function handleOptimizeUndo(payload) {
    const name = payload?.pipelineName;
    if (!name) return;
    const restored = versionMgr.undo(name);
    if (!restored) { postToJS('optimize_error', { message: 'Already at earliest version' }); return; }
    const pipelines = storage.loadPipelines();
    const idx = pipelines.findIndex(p => p.name === name);
    if (idx >= 0) pipelines[idx] = restored;
    storage.savePipelines(pipelines);
    const cursor = versionMgr.getCursor(name);
    postToJS('pipeline_list', { pipelines });
    postToJS('optimize_version_changed', { pipelineName: name, version: cursor.currentVersion, canUndo: cursor.currentVersion > 1, canRedo: cursor.currentVersion < cursor.headVersion });
}

function handleOptimizeRedo(payload) {
    const name = payload?.pipelineName;
    if (!name) return;
    const restored = versionMgr.redo(name);
    if (!restored) { postToJS('optimize_error', { message: 'Already at latest version' }); return; }
    const pipelines = storage.loadPipelines();
    const idx = pipelines.findIndex(p => p.name === name);
    if (idx >= 0) pipelines[idx] = restored;
    storage.savePipelines(pipelines);
    const cursor = versionMgr.getCursor(name);
    postToJS('pipeline_list', { pipelines });
    postToJS('optimize_version_changed', { pipelineName: name, version: cursor.currentVersion, canUndo: cursor.currentVersion > 1, canRedo: cursor.currentVersion < cursor.headVersion });
}

function handleOptimizeCheckout(payload) {
    const { pipelineName: name, version } = payload || {};
    if (!name || !version) return;
    const restored = versionMgr.checkoutVersion(name, version);
    if (!restored) { postToJS('optimize_error', { message: 'Version not found' }); return; }
    const pipelines = storage.loadPipelines();
    const idx = pipelines.findIndex(p => p.name === name);
    if (idx >= 0) pipelines[idx] = restored;
    storage.savePipelines(pipelines);
    const cursor = versionMgr.getCursor(name);
    postToJS('pipeline_list', { pipelines });
    postToJS('optimize_version_changed', { pipelineName: name, version: cursor.currentVersion, canUndo: cursor.currentVersion > 1, canRedo: cursor.currentVersion < cursor.headVersion });
}

function handleOptimizeReapply(payload) {
    const { pipelineName: name, version } = payload || {};
    if (!name || !version) return;
    const pipelines = storage.loadPipelines();
    const idx = pipelines.findIndex(p => p.name === name);
    if (idx < 0) { postToJS('optimize_error', { message: 'Pipeline not found' }); return; }
    const updated = versionMgr.reapplyVersion(name, version, pipelines[idx]);
    pipelines[idx] = updated;
    storage.savePipelines(pipelines);
    const cursor = versionMgr.getCursor(name);
    postToJS('optimize_applied', { pipelineName: name, approvedCount: 0, rejectedCount: 0, version: cursor.currentVersion });
    postToJS('pipeline_list', { pipelines });
    postToJS('optimize_version_changed', { pipelineName: name, version: cursor.currentVersion, canUndo: cursor.currentVersion > 1, canRedo: cursor.currentVersion < cursor.headVersion });
}

function handleOptimizeVersionList(payload) {
    const name = payload?.pipelineName;
    if (!name) return;
    const cursor = versionMgr.getCursor(name);
    postToJS('optimize_version_list_result', { pipelineName: name, cursor });
}

// ============================================================
// File Dialogs
// ============================================================
async function openFileDialog() {
    if (!mainWindow) return;
    const result = await dialog.showOpenDialog(mainWindow, {
        filters: [{ name: 'JSON Files', extensions: ['json'] }, { name: 'All Files', extensions: ['*'] }],
        properties: ['openFile'],
    });
    if (!result.canceled && result.filePaths.length > 0) {
        const filePath = result.filePaths[0];
        addRecentFile(filePath);
        postToJS('open_file_result', { path: filePath });
    }
}

async function saveFileDialog() {
    if (!mainWindow) return;
    const result = await dialog.showSaveDialog(mainWindow, {
        filters: [{ name: 'JSON Files', extensions: ['json'] }, { name: 'All Files', extensions: ['*'] }],
    });
    if (!result.canceled && result.filePath) {
        addRecentFile(result.filePath);
        postToJS('save_as_result', { path: result.filePath });
    }
}

function addRecentFile(filePath) {
    recentFiles = recentFiles.filter(f => f !== filePath);
    recentFiles.unshift(filePath);
    if (recentFiles.length > 10) recentFiles.length = 10;
    storage.saveRecentFiles(recentFiles);
    updateRecentFilesMenu();
}

// ============================================================
// Menu
// ============================================================
function buildMenu() {
    const send = (action) => () => postToJS('menu_command', { action });
    const template = [
        {
            label: 'File',
            submenu: [
                { label: 'New Tab', accelerator: 'CmdOrCtrl+N', click: send('new_tab') },
                { label: 'Open...', accelerator: 'CmdOrCtrl+O', click: () => openFileDialog() },
                { label: 'Save', accelerator: 'CmdOrCtrl+S', click: send('save') },
                { label: 'Save As...', accelerator: 'CmdOrCtrl+Shift+S', click: () => saveFileDialog() },
                { type: 'separator' },
                { label: 'Import ZIP', click: send('import_zip') },
                { label: 'Export Node', click: send('export_node') },
                { type: 'separator' },
                { id: 'recent-placeholder', label: '(No Recent Files)', enabled: false },
                { type: 'separator' },
                { label: 'Exit', accelerator: 'Alt+F4', role: 'quit' },
            ],
        },
        {
            label: 'Pipeline',
            submenu: [
                { label: 'Run Pipeline', accelerator: 'F5', click: send('run_pipeline') },
                { label: 'Pipeline Manager', click: send('pipeline_manager') },
                { label: 'History', click: send('pipeline_history') },
                { label: 'Cancel', click: () => runner.cancel() },
            ],
        },
        {
            label: 'Providers',
            submenu: [
                { label: 'Configure...', click: send('config') },
                { label: 'Test Connection', click: send('test_connection') },
            ],
        },
        {
            label: 'Recipes',
            submenu: [
                { label: 'Recipe Manager', click: send('recipe_manager') },
                { label: 'Configure...', click: send('config') },
            ],
        },
        {
            label: 'View',
            submenu: [
                { label: 'Tree', click: () => postToJS('menu_command', { action: 'toggle_pane', pane: 'tree' }) },
                { label: 'List', click: () => postToJS('menu_command', { action: 'toggle_pane', pane: 'list' }) },
                { label: 'Editor', click: () => postToJS('menu_command', { action: 'toggle_pane', pane: 'editor' }) },
                { label: 'Messages', click: () => postToJS('menu_command', { action: 'toggle_pane', pane: 'messages' }) },
                { type: 'separator' },
                { label: 'Toggle Fullscreen', accelerator: 'F11', role: 'togglefullscreen' },
            ],
        },
        {
            label: 'Help',
            submenu: [
                { label: 'Welcome Wizard', click: send('welcome_wizard') },
                { label: 'Reset Welcome Wizard', click: send('reset_wizard') },
                { label: 'Setup Wizard', click: send('setup_wizard') },
                { label: 'Documentation', click: () => shell.openExternal('https://github.com/gadget114514/Ecode') },
                { label: 'About', click: send('about') },
            ],
        },
    ];
    return template;
}

function updateRecentFilesMenu() {
    if (!mainWindow || embedded) return;
    const menu = Menu.getApplicationMenu();
    if (!menu) return;
    // Rebuild menu with recent files
    const template = buildMenu();
    const fileMenu = template[0].submenu;
    // Remove placeholder, add recent files before the last separator+exit
    fileMenu.splice(8, 1); // remove placeholder
    const recent = recentFiles.slice(0, 5);
    if (recent.length > 0) {
        recent.forEach((f, i) => {
            fileMenu.splice(8 + i, 0, {
                label: `&${i + 1} ${f}`,
                click: () => {
                    addRecentFile(f);
                    postToJS('open_file_result', { path: f });
                },
            });
        });
    } else {
        fileMenu.splice(8, 0, { label: '(No Recent Files)', enabled: false });
    }
    Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

// ============================================================
// Window creation
// ============================================================
function createWindow() {
    const iconPath = path.join(__dirname, '..', 'resources', 'app.ico');
    mainWindow = new BrowserWindow({
        width: 1000,
        height: 700,
        title: 'Prompts',
        icon: fs.existsSync(iconPath) ? iconPath : undefined,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
        },
    });

    const frontendPath = path.join(FRONTEND_ROOT, 'index.html');
    mainWindow.loadFile(frontendPath);

    if (!embedded) {
        const template = buildMenu();
        Menu.setApplicationMenu(Menu.buildFromTemplate(template));
    } else {
        Menu.setApplicationMenu(null);
    }

    mainWindow.webContents.on('did-finish-load', () => {
        mainWindow.webContents.openDevTools({ mode: 'detach' });
        postToJS('ready', {});
    });

    mainWindow.on('closed', () => { mainWindow = null; });
}

// ============================================================
// IPC
// ============================================================
ipcMain.on('bridge', (_event, msg) => {
    let obj = msg;
    if (typeof msg === 'string') {
        try { obj = JSON.parse(msg); } catch { return; }
    }
    const type = obj.type;
    const payload = obj.payload;

    if (type === 'init_complete') {
        postToJS('log', JSON.stringify({ message: '[TRACE] init_complete received from JS, calling SendFullInit' }));
        sendFullInit();
        return;
    }

    handleBridgeMessage(type, payload);
});

// ============================================================
// App lifecycle
// ============================================================
app.whenReady().then(() => {
    appDataPath = getAppDataPath();
    storage.init(appDataPath);

    // Check for --embedded flag
    const args = process.argv.slice(2);
    embedded = args.includes('--embedded');

    createWindow();

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});
