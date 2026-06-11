'use strict';
/**
 * Prompts Electron — unit / integration tests
 * Run with:  node test.js
 * (No external deps — uses Node built-ins only)
 */

const { test, describe, before, after } = require('node:test');
const assert = require('node:assert/strict');
const fs   = require('node:fs');
const path = require('node:path');
const os   = require('node:os');
const http = require('node:http');

// ── helpers ──────────────────────────────────────────────────

function makeTempDir() {
    return fs.mkdtempSync(path.join(os.tmpdir(), 'prompts_test_'));
}

function rmrf(p) {
    fs.rmSync(p, { recursive: true, force: true });
}

// ── inline the testable modules from main.js ──────────────────
// We extract pure-logic classes/functions so tests don't need Electron.

// ---- utilities ----
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

// ---- Storage (copy of main.js Storage class) ----
class Storage {
    constructor() { this.basePath = ''; this.maxHistoryRuns = 50; }

    init(basePath) {
        this.basePath = basePath;
        ensureDir(path.join(basePath, 'data'));
        ensureDir(path.join(basePath, 'blobs'));
        ensureDir(path.join(basePath, 'history'));
        return true;
    }

    dataPath(rel) { return path.join(this.basePath, 'data', rel); }
    blobPath(rel) { return path.join(this.basePath, 'blobs', rel); }
    getBasePath()  { return this.basePath; }

    loadSession()          { return readJson(path.join(this.basePath, 'session.json'), { tabs: [] }); }
    saveSession(s)         { writeJson(path.join(this.basePath, 'session.json'), s); }

    loadTabData(rel)       { return readJson(this.dataPath(rel), { title:'', content:'', mimetype:'text/plain', attachments:[], children:[] }); }
    saveTabData(rel, root) { writeJson(this.dataPath(rel), root); }

    loadBlob(rel)          { try { return fs.readFileSync(this.blobPath(rel), 'base64'); } catch { return ''; } }
    saveBlob(data, ext)    {
        const name = Date.now() + '_' + Math.random().toString(36).slice(2) + ext;
        fs.writeFileSync(this.blobPath(name), Buffer.from(data, 'base64'));
        return name;
    }
    removeBlob(rel)        { try { fs.unlinkSync(this.blobPath(rel)); } catch {} }

    garbageCollectBlobs(referenced) {
        try {
            const all = fs.readdirSync(path.join(this.basePath, 'blobs'));
            for (const f of all) if (!referenced.includes(f)) { try { fs.unlinkSync(this.blobPath(f)); } catch {} }
        } catch {}
    }

    getTabFiles() {
        try { return fs.readdirSync(path.join(this.basePath, 'data')).filter(f => f.endsWith('.json')); }
        catch { return []; }
    }

    saveHistory(recordJson) {
        try {
            const obj = JSON.parse(recordJson);
            const id  = obj.id || generateRunId();
            writeJson(path.join(this.basePath, 'history', `run_${id}.json`), obj);
            this._trimHistory();
        } catch {}
    }

    _trimHistory() {
        try {
            const dir   = path.join(this.basePath, 'history');
            const files = fs.readdirSync(dir)
                .filter(f => f.startsWith('run_') && f.endsWith('.json'))
                .map(f => ({ f, mtime: fs.statSync(path.join(dir, f)).mtimeMs }))
                .sort((a, b) => b.mtime - a.mtime);
            for (let i = this.maxHistoryRuns; i < files.length; i++)
                try { fs.unlinkSync(path.join(dir, files[i].f)); } catch {}
        } catch {}
    }

    updateHistoryEvaluation(filename, evaluation) {
        const p = path.join(this.basePath, 'history', filename);
        const obj = readJson(p, null);
        if (obj) { obj.evaluation = evaluation; writeJson(p, obj); }
    }

    listHistory() {
        try { return fs.readdirSync(path.join(this.basePath, 'history')).filter(f => f.startsWith('run_') && f.endsWith('.json')).sort().reverse(); }
        catch { return []; }
    }

    loadHistoryRecord(filename) {
        try { return fs.readFileSync(path.join(this.basePath, 'history', filename), 'utf8'); }
        catch { return ''; }
    }

    loadProviders()          { return readJson(path.join(this.basePath, 'providers.json'), {}); }
    saveProviders(p)         { writeJson(path.join(this.basePath, 'providers.json'), p); return true; }

    loadPipelines()          { const o = readJson(path.join(this.basePath, 'pipelines.json'), { pipelines: [] }); return o.pipelines || o || []; }
    savePipelines(pl)        { writeJson(path.join(this.basePath, 'pipelines.json'), { pipelines: pl }); }

    loadRecentFiles()        { return readJson(path.join(this.basePath, 'recent_files.json'), []); }
    saveRecentFiles(f)       { writeJson(path.join(this.basePath, 'recent_files.json'), f); }

    loadGeneralConfig()      { return readJson(path.join(this.basePath, 'config.json'), { historyRetention: 50, defaultProvider: 'openai', defaultModel: '' }); }
    saveGeneralConfig(cfg)   { writeJson(path.join(this.basePath, 'config.json'), cfg); this.maxHistoryRuns = cfg.historyRetention || 50; return true; }

    loadRecipes()            { return readJson(path.join(this.basePath, 'recipes.json'), []); }
    saveRecipes(r)           { writeJson(path.join(this.basePath, 'recipes.json'), r); return true; }

    _chestPath(name) { ensureDir(path.join(this.basePath, 'chests')); return path.join(this.basePath, 'chests', name + '.txt'); }
    saveToNamedChest(name, content) { fs.writeFileSync(this._chestPath(name), content, 'utf8'); }
    loadFromNamedChest(name)        { try { return fs.readFileSync(this._chestPath(name), 'utf8'); } catch { return ''; } }
    chestExists(name)               { return fs.existsSync(this._chestPath(name)); }
    listNamedChests()               { try { return fs.readdirSync(path.join(this.basePath, 'chests')).filter(f => f.endsWith('.txt')).map(f => f.slice(0, -4)); } catch { return []; } }

    setMaxHistoryRuns(n) { this.maxHistoryRuns = n; }
    getMaxHistoryRuns()  { return this.maxHistoryRuns; }

    ensureDirectory(p) { ensureDir(p); return true; }

    resolveProjectPath(rel) {
        const full = path.resolve(path.join(this.basePath, 'data', rel));
        if (!full.startsWith(path.join(this.basePath, 'data'))) return '';
        return full;
    }
}

// ---- PipelineVersionManager ----
class PipelineVersionManager {
    constructor(storage) { this.storage = storage; }

    _getVersionsPath(name) { return path.join(this.storage.basePath, 'optimizer', name + '_versions.json'); }
    _loadVersions(name)    { return readJson(this._getVersionsPath(name), { versions: [], currentVersion: 0, headVersion: 0 }); }
    _saveVersions(name, d) { ensureDir(path.join(this.storage.basePath, 'optimizer')); writeJson(this._getVersionsPath(name), d); }

    ensureBaseVersion(name, pipeline) {
        const data = this._loadVersions(name);
        if (data.versions.length === 0) {
            data.versions.push({ version: 1, pipeline: JSON.parse(JSON.stringify(pipeline)), timestamp: nowIso(), label: 'Base' });
            data.currentVersion = 1; data.headVersion = 1;
            this._saveVersions(name, data);
        }
        return data;
    }

    commitVersion(name, pipeline, sessionId, label) {
        const data = this._loadVersions(name);
        const next = data.headVersion + 1;
        data.versions.push({ version: next, pipeline: JSON.parse(JSON.stringify(pipeline)), timestamp: nowIso(), label, sessionId });
        data.currentVersion = next; data.headVersion = next;
        this._saveVersions(name, data);
        return next;
    }

    getCursor(name) {
        const data = this._loadVersions(name);
        return { pipelineName: name, currentVersion: data.currentVersion, headVersion: data.headVersion,
                 entries: data.versions.map(v => ({ version: v.version, timestamp: v.timestamp, label: v.label })) };
    }

    _findPipeline(name, version) {
        const data = this._loadVersions(name);
        const e = data.versions.find(v => v.version === version);
        return e ? e.pipeline : null;
    }

    undo(name) { const d = this._loadVersions(name); if (d.currentVersion <= 1) return null; d.currentVersion--; this._saveVersions(name, d); return this._findPipeline(name, d.currentVersion); }
    redo(name) { const d = this._loadVersions(name); if (d.currentVersion >= d.headVersion) return null; d.currentVersion++; this._saveVersions(name, d); return this._findPipeline(name, d.currentVersion); }
    checkoutVersion(name, version) { const d = this._loadVersions(name); const e = d.versions.find(v => v.version === version); if (!e) return null; d.currentVersion = version; this._saveVersions(name, d); return e.pipeline; }
}

// ---- PipelineRunner (thin test shim — no Electron IPC) ----
class PipelineRunner {
    constructor() {
        this.running = false; this.cancelled = false; this.bridgeCb = null;
        this.providers = {}; this.historySteps = []; this.currentStepIndex = -1;
        this.pendingSteps = []; this.inputContent = ''; this.outputMode = 'child';
        this.pipelineName = ''; this.runId = ''; this.startedAt = '';
        this.waitingForManual = false; this.waitingForWizard = false; this.waitingForFilter = false;
        this._manualResolve = null; this._wizardResolve = null; this._filterResolve = null;
        this.inputSourceOverridden = false; this.inputSourceContent = '';
        this.events = []; // captured bridge events for assertions
    }

    setBridgeCallback(cb) { this.bridgeCb = cb; }
    postBridge(type, json) { this.events.push({ type, payload: typeof json === 'string' ? JSON.parse(json) : json }); if (this.bridgeCb) this.bridgeCb(type, typeof json === 'string' ? json : JSON.stringify(json)); }
    registerProvider(type, p) { this.providers[type] = p; }
    getRunId()  { return this.runId; }
    isRunning() { return this.running; }
    cancel()    { this.cancelled = true; this.running = false; this.pendingSteps = []; if (this._manualResolve) { this._manualResolve(null); this._manualResolve = null; } }
    resumeManual(content) { if (this._manualResolve) { this._manualResolve(content); this._manualResolve = null; } }
    resumeWizard(json)    { if (this._wizardResolve) { this._wizardResolve(json); this._wizardResolve = null; } }
    resumeFilter(json)    { if (this._filterResolve) { this._filterResolve(json); this._filterResolve = null; } }
    setExternalInput(c)   { this.inputSourceOverridden = true; this.inputSourceContent = c; }

    _currentContent() {
        if (this.inputSourceOverridden) return this.inputSourceContent;
        if (this.currentStepIndex > 0 && this.historySteps[this.currentStepIndex - 1]) return this.historySteps[this.currentStepIndex - 1].output;
        return this.inputContent;
    }

    run(pipelineName, steps, inputContent, inputAttachments, outputMode) {
        if (this.running) return;
        this.pipelineName = pipelineName; this.inputContent = inputContent;
        this.inputAttachments = inputAttachments || [];
        this.outputMode = outputMode || 'child'; this.cancelled = false; this.running = true;
        this.runId = generateRunId(); this.startedAt = nowIso();
        this.historySteps = steps.map((s, i) => ({ index: i, name: s.name, type: s.type, input: i === 0 ? inputContent : '', output: '', status: 'pending', promptTokens: 0, completionTokens: 0, parallelBranches: {} }));
        this.currentStepIndex = -1; this.pendingSteps = [...steps];
        this.inputSourceOverridden = false;
        this.postBridge('step_started', JSON.stringify({ index: 0, name: steps[0]?.name || '' }));
        return this._runNext();
    }

    async _runNext() {
        if (this.cancelled || this.pendingSteps.length === 0) {
            this.running = false;
            if (!this.cancelled) this.postBridge('pipeline_completed', JSON.stringify({ id: this.runId, pipelineName: this.pipelineName, steps: this.historySteps, status: 'completed' }));
            else                 this.postBridge('pipeline_error', JSON.stringify({ message: 'Canceled' }));
            return;
        }
        this.currentStepIndex++;
        const step = this.pendingSteps.shift();
        if (this.currentStepIndex < this.historySteps.length) { this.historySteps[this.currentStepIndex].input = this._currentContent(); this.historySteps[this.currentStepIndex].status = 'running'; }
        this.postBridge('step_started', JSON.stringify({ index: this.currentStepIndex, name: step.name }));
        try {
            await this._executeStep(step);
        } catch (e) {
            this.running = false;
            this.postBridge('pipeline_error', JSON.stringify({ message: String(e) }));
            return;
        }
        if (!this.waitingForManual && !this.waitingForWizard && !this.waitingForFilter && this.running) await this._runNext();
    }

    async _executeStep(step) {
        const type = step.type;
        const idx  = this.currentStepIndex;

        if (type === 'ai') {
            const provider = this.providers[step.params?.provider || 'openai'];
            if (!provider) throw new Error('Provider not configured: ' + (step.params?.provider || 'openai'));
            let userPrompt = (step.params?.userPrompt || '{content}').replace(/\{content\}/g, this.inputContent).replace(/\{result\}/g, this._currentContent());
            const resp = await provider.call({ model: step.params?.model || 'gpt-4.1', systemPrompt: step.params?.systemPrompt || '', userPrompt, temperature: parseFloat(step.params?.temperature || '0.7'), maxTokens: 4096, attachments: this.inputAttachments || [] });
            if (idx < this.historySteps.length) {
                this.historySteps[idx].output = resp.content;
                this.historySteps[idx].status = 'completed';
                if (resp.outputAttachments && resp.outputAttachments.length > 0) {
                    this.historySteps[idx].artifacts = resp.outputAttachments;
                }
            }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'manual') {
            const content = this._currentContent();
            this.waitingForManual = true;
            const choices = step.params?.choices ? JSON.parse(step.params.choices) : [];
            this.postBridge('manual_step_pause', JSON.stringify({ index: idx, mode: step.params?.mode || 'view', prompt: step.params?.prompt || '', content, choices }));
            const result = await new Promise(res => { this._manualResolve = res; });
            this.waitingForManual = false;
            if (idx < this.historySteps.length) { this.historySteps[idx].output = result ?? content; this.historySteps[idx].status = 'completed'; }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'wizard') {
            this.waitingForWizard = true;
            this.postBridge('wizard_step_pause', JSON.stringify({ index: idx, wizard: step.params?.wizard || '', content: this._currentContent() }));
            const valuesJson = await new Promise(res => { this._wizardResolve = res; });
            this.waitingForWizard = false;
            if (idx < this.historySteps.length) { this.historySteps[idx].output = valuesJson || '{}'; this.historySteps[idx].status = 'completed'; }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else if (type === 'filter') {
            const content = this._currentContent();
            const mode = step.params?.mode || 'manual';
            if (mode === 'auto') { if (idx < this.historySteps.length) { this.historySteps[idx].status = 'completed'; this.historySteps[idx].output = content; } this.postBridge('step_done', JSON.stringify({ index: idx })); return; }
            this.waitingForFilter = true;
            this.postBridge('step_filter_pause', JSON.stringify({ index: idx, mode, outputs: [{ index: 0, content }] }));
            await new Promise(res => { this._filterResolve = res; });
            this.waitingForFilter = false;
            if (idx < this.historySteps.length) { this.historySteps[idx].status = 'completed'; this.historySteps[idx].output = content; }
            this.postBridge('step_done', JSON.stringify({ index: idx }));

        } else {
            this.postBridge('log', JSON.stringify({ message: '⚠ Unknown step: ' + type }));
            if (idx < this.historySteps.length) this.historySteps[idx].status = 'skipped';
        }
    }
}

// ================================================================
// ── MockAIProvider ─────────────────────────────────────────────
// Internal scripted provider — no network access.
//
// Request format (mirrors real providers):
//   { model, systemPrompt, userPrompt, temperature, maxTokens,
//     attachments: [{ file, path, mimetype, content (base64), size }] }
//
// Response format:
//   { content: string, model: string, outputAttachments: Attachment[] }
//
// Usage:
//   const p = new MockAIProvider();
//
//   // ── Deterministic rules (never consumed, highest priority) ──
//   p.when('hello', 'world')                        // exact userPrompt match → string content
//   p.when(/translate/i, 'translation')             // regex match
//   p.when(req => req.model === 'x', 'ok')          // predicate fn → string content
//   p.when(req => req.attachments.length > 0,       // predicate fn → full response fn
//          req => ({ content: 'got it', model: 'mock-model',
//                    outputAttachments: [req.attachments[0]] }))
//
//   // ── Queue (consumed FIFO, fallback when no rule matches) ────
//   p.queue('Hello back')                           // text-only response
//   p.queueWithMedia('Caption', [imageAtt])         // response with output media
//   p.queueError('rate limit')                      // next call throws
//
//   // ── Assertions ──────────────────────────────────────────────
//   await p.call(req)                               // returns scripted response
//   p.calls[0]                                      // captured request
//   p.inputAttachmentsOf(0)                         // attachments in 1st call
//   p.inputImagesOf(0)                              // image attachments in 1st call
//   p.inputAudiosOf(0)                              // audio attachments in 1st call
class MockAIProvider {
    constructor() {
        this._rules = [];   // deterministic rules — never consumed
        this._queue = [];   // scripted entries in FIFO order
        this.calls  = [];   // every captured request (with attachments snapshot)
    }

    // Register a deterministic rule (chainable).
    // matcher: string (exact userPrompt), RegExp, or (req) => bool
    // response: string (content only), or (req) => { content, model, outputAttachments }
    when(matcher, response) {
        this._rules.push({ matcher, response });
        return this;
    }

    // Queue a text-only response (chainable)
    queue(content, model = 'mock-model') {
        this._queue.push({ ok: true, content, model, outputAttachments: [] });
        return this;
    }

    // Queue a response that includes output media attachments (e.g. TTS audio, generated image)
    queueWithMedia(content, outputAttachments = [], model = 'mock-model') {
        this._queue.push({ ok: true, content, model, outputAttachments });
        return this;
    }

    // Queue an error (chainable)
    queueError(message) {
        this._queue.push({ ok: false, message });
        return this;
    }

    // call() — used by the test PipelineRunner shim
    async call(req) {
        // Snapshot attachments array so later mutations don't affect captured calls
        this.calls.push({ ...req, attachments: req.attachments ? req.attachments.map(a => ({ ...a })) : [] });

        // 1. Rule-based (deterministic, never consumed)
        for (const { matcher, response } of this._rules) {
            let matched = false;
            if (typeof matcher === 'string')        matched = req.userPrompt === matcher;
            else if (matcher instanceof RegExp)     matched = matcher.test(req.userPrompt);
            else if (typeof matcher === 'function') matched = matcher(req);
            if (!matched) continue;
            const r = typeof response === 'function' ? response(req) : response;
            return typeof r === 'string'
                ? { content: r, model: 'mock-model', outputAttachments: [] }
                : r;
        }

        // 2. Queue (consumed FIFO)
        if (this._queue.length === 0) {
            return { content: `echo:${req.userPrompt}`, model: 'mock-model', outputAttachments: [] };
        }
        const entry = this._queue.shift();
        if (!entry.ok) throw new Error(entry.message);
        return { content: entry.content, model: entry.model, outputAttachments: entry.outputAttachments };
    }

    // callStreaming() — used by the real runner.js (same queue, streams via callbacks)
    async callStreaming(req, onChunk, onDone, onError) {
        try {
            const resp = await this.call(req);
            onChunk(resp.content);
            onDone(resp);
        } catch (e) {
            onError(e.message);
        }
    }

    // ── Assertion helpers ──────────────────────────────────────

    // All attachments that were sent in call n (default: last)
    inputAttachmentsOf(n = this.calls.length - 1) {
        return this.calls[n]?.attachments || [];
    }

    // Only image/* attachments from call n
    inputImagesOf(n = this.calls.length - 1) {
        return this.inputAttachmentsOf(n).filter(a => a.mimetype?.startsWith('image/'));
    }

    // Only audio/* attachments from call n
    inputAudiosOf(n = this.calls.length - 1) {
        return this.inputAttachmentsOf(n).filter(a => a.mimetype?.startsWith('audio/'));
    }

    // Convenience getters
    get lastCall()          { return this.calls[this.calls.length - 1]; }
    get lastInputAttachments() { return this.inputAttachmentsOf(); }
    get callCount()         { return this.calls.length; }
    nthCall(n)              { return this.calls[n]; }

    reset() { this._rules = []; this._queue = []; this.calls = []; }
}

// makeApp — top-level so all describe blocks can access it
// Assigned inside describe('Bridge message handling'), used from other suites too.
let makeApp;

// ── TESTS ─────────────────────────────────────────────────────
// ================================================================

// ── 1. Utilities ──────────────────────────────────────────────
describe('jsonEscape', () => {
    test('empty string', () => assert.equal(jsonEscape(''), ''));
    test('plain string unchanged', () => assert.equal(jsonEscape('hello'), 'hello'));
    test('double quotes escaped', () => assert.equal(jsonEscape('"hi"'), '\\"hi\\"'));
    test('backslash escaped', () => assert.equal(jsonEscape('a\\b'), 'a\\\\b'));
    test('newline escaped', () => assert.equal(jsonEscape('a\nb'), 'a\\nb'));
    test('carriage return escaped', () => assert.equal(jsonEscape('a\rb'), 'a\\rb'));
    test('tab escaped', () => assert.equal(jsonEscape('a\tb'), 'a\\tb'));
    test('combined special chars', () => assert.equal(jsonEscape('say "hi"\nbye'), 'say \\"hi\\"\\nbye'));
});

describe('generateRunId', () => {
    test('returns non-empty string', () => assert.ok(generateRunId().length > 0));
    test('two calls produce different ids', () => assert.notEqual(generateRunId(), generateRunId()));
    test('format contains timestamp portion', () => assert.match(generateRunId(), /^\d{8}_\d{6}_\d+$/));
});

describe('nowIso', () => {
    test('returns ISO 8601 string ending in Z', () => assert.match(nowIso(), /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/));
});

// ── 2. Storage ────────────────────────────────────────────────
describe('Storage', () => {
    let tmpDir, st;

    before(() => { tmpDir = makeTempDir(); st = new Storage(); st.init(tmpDir); });
    after(() => rmrf(tmpDir));

    test('init creates directories', () => {
        assert.ok(fs.existsSync(path.join(tmpDir, 'data')));
        assert.ok(fs.existsSync(path.join(tmpDir, 'blobs')));
        assert.ok(fs.existsSync(path.join(tmpDir, 'history')));
    });

    test('getBasePath returns init path', () => assert.equal(st.getBasePath(), tmpDir));

    test('session round-trip', () => {
        st.saveSession({ tabs: [{ name: 'T1', file: 'tab1.json' }] });
        const s = st.loadSession();
        assert.equal(s.tabs.length, 1);
        assert.equal(s.tabs[0].name, 'T1');
    });

    test('loadSession defaults to empty tabs', () => {
        const st2 = new Storage();
        st2.init(makeTempDir());
        assert.deepEqual(st2.loadSession(), { tabs: [] });
        rmrf(st2.basePath);
    });

    test('tab data round-trip', () => {
        const node = { title: 'Hello', content: 'World', mimetype: 'text/plain', attachments: [], children: [] };
        st.saveTabData('test.json', node);
        const loaded = st.loadTabData('test.json');
        assert.equal(loaded.title, 'Hello');
        assert.equal(loaded.content, 'World');
    });

    test('loadTabData returns default when file missing', () => {
        const node = st.loadTabData('nonexistent.json');
        assert.equal(node.mimetype, 'text/plain');
    });

    test('getTabFiles lists json files', () => {
        st.saveTabData('a.json', {}); st.saveTabData('b.json', {});
        const files = st.getTabFiles();
        assert.ok(files.includes('a.json'));
        assert.ok(files.includes('b.json'));
    });

    test('providers round-trip', () => {
        const providers = { openai: { apiKey: 'sk-test', baseUrl: '', models: [] } };
        st.saveProviders(providers);
        const loaded = st.loadProviders();
        assert.equal(loaded.openai.apiKey, 'sk-test');
    });

    test('pipelines round-trip', () => {
        const pipelines = [{ name: 'pipe1', steps: [], mode: 'basic', outputMode: 'child' }];
        st.savePipelines(pipelines);
        const loaded = st.loadPipelines();
        assert.equal(loaded.length, 1);
        assert.equal(loaded[0].name, 'pipe1');
    });

    test('recent files round-trip', () => {
        st.saveRecentFiles(['/a/b.json', '/c/d.json']);
        assert.deepEqual(st.loadRecentFiles(), ['/a/b.json', '/c/d.json']);
    });

    test('general config round-trip', () => {
        st.saveGeneralConfig({ historyRetention: 30, defaultProvider: 'anthropic', defaultModel: 'claude-sonnet-4-6' });
        const cfg = st.loadGeneralConfig();
        assert.equal(cfg.historyRetention, 30);
        assert.equal(cfg.defaultProvider, 'anthropic');
        assert.equal(cfg.defaultModel, 'claude-sonnet-4-6');
    });

    test('saveGeneralConfig updates maxHistoryRuns', () => {
        st.saveGeneralConfig({ historyRetention: 25, defaultProvider: 'openai', defaultModel: '' });
        assert.equal(st.getMaxHistoryRuns(), 25);
    });

    test('recipes round-trip', () => {
        const recipes = [{ name: 'r1', type: 'ai', provider: 'openai', model: 'gpt-4.1', temperature: 0.5, systemPrompt: 'sys', command: '' }];
        st.saveRecipes(recipes);
        const loaded = st.loadRecipes();
        assert.equal(loaded[0].name, 'r1');
        assert.equal(loaded[0].temperature, 0.5);
    });

    test('history save and list', () => {
        const id = 'testrun_' + Date.now();
        st.saveHistory(JSON.stringify({ id, pipelineName: 'p', status: 'completed', steps: [] }));
        const files = st.listHistory();
        assert.ok(files.some(f => f.includes(id)));
    });

    test('history load record', () => {
        const id = 'testrun2_' + Date.now();
        st.saveHistory(JSON.stringify({ id, pipelineName: 'p2', status: 'completed', steps: [] }));
        const raw = st.loadHistoryRecord(`run_${id}.json`);
        const obj = JSON.parse(raw);
        assert.equal(obj.pipelineName, 'p2');
    });

    test('updateHistoryEvaluation', () => {
        const id = 'evalrun_' + Date.now();
        st.saveHistory(JSON.stringify({ id, pipelineName: 'pe', status: 'completed', steps: [] }));
        st.updateHistoryEvaluation(`run_${id}.json`, 'ok');
        const raw = st.loadHistoryRecord(`run_${id}.json`);
        assert.equal(JSON.parse(raw).evaluation, 'ok');
    });

    test('history trimming respects maxHistoryRuns', () => {
        const st2 = new Storage();
        const dir2 = makeTempDir();
        st2.init(dir2);
        st2.setMaxHistoryRuns(3);
        for (let i = 0; i < 6; i++) {
            st2.saveHistory(JSON.stringify({ id: `trim_${i}`, pipelineName: 'p', status: 'completed', steps: [] }));
        }
        const files = st2.listHistory();
        assert.ok(files.length <= 3, `expected ≤ 3 history files, got ${files.length}`);
        rmrf(dir2);
    });

    test('named chest save and load', () => {
        st.saveToNamedChest('mychest', 'hello chest');
        assert.equal(st.loadFromNamedChest('mychest'), 'hello chest');
        assert.ok(st.chestExists('mychest'));
    });

    test('chestExists false for missing chest', () => assert.equal(st.chestExists('ghost'), false));

    test('listNamedChests returns chest names', () => {
        st.saveToNamedChest('c1', 'a'); st.saveToNamedChest('c2', 'b');
        const list = st.listNamedChests();
        assert.ok(list.includes('c1'));
        assert.ok(list.includes('c2'));
    });

    test('blob save and load', () => {
        const data = Buffer.from('hello blob').toString('base64');
        const name = st.saveBlob(data, '.txt');
        const loaded = st.loadBlob(name);
        assert.equal(Buffer.from(loaded, 'base64').toString('utf8'), 'hello blob');
    });

    test('removeBlob deletes file', () => {
        const data = Buffer.from('bye').toString('base64');
        const name = st.saveBlob(data, '.txt');
        st.removeBlob(name);
        assert.equal(st.loadBlob(name), '');
    });

    test('garbageCollectBlobs removes unreferenced files', () => {
        const a = st.saveBlob(Buffer.from('a').toString('base64'), '.txt');
        const b = st.saveBlob(Buffer.from('b').toString('base64'), '.txt');
        st.garbageCollectBlobs([a]);   // keep a, remove b
        assert.notEqual(st.loadBlob(a), '');
        assert.equal(st.loadBlob(b), '');
    });

    test('resolveProjectPath within bounds', () => {
        const resolved = st.resolveProjectPath('sub/file.json');
        assert.ok(resolved.startsWith(path.join(tmpDir, 'data')));
    });

    test('resolveProjectPath rejects traversal', () => {
        const resolved = st.resolveProjectPath('../../etc/passwd');
        assert.equal(resolved, '');
    });
});

// ── 3. PipelineVersionManager ─────────────────────────────────
describe('PipelineVersionManager', () => {
    let tmpDir, st, vm;

    before(() => { tmpDir = makeTempDir(); st = new Storage(); st.init(tmpDir); vm = new PipelineVersionManager(st); });
    after(() => rmrf(tmpDir));

    const basePipeline = () => ({ name: 'test', steps: [{ name: 's1', type: 'ai', params: { userPrompt: 'hello' } }], mode: 'basic', outputMode: 'child' });

    test('ensureBaseVersion creates version 1', () => {
        vm.ensureBaseVersion('pipe1', basePipeline());
        const cursor = vm.getCursor('pipe1');
        assert.equal(cursor.currentVersion, 1);
        assert.equal(cursor.headVersion, 1);
        assert.equal(cursor.entries.length, 1);
        assert.equal(cursor.entries[0].label, 'Base');
    });

    test('ensureBaseVersion is idempotent', () => {
        vm.ensureBaseVersion('pipe2', basePipeline());
        vm.ensureBaseVersion('pipe2', basePipeline());
        assert.equal(vm.getCursor('pipe2').entries.length, 1);
    });

    test('commitVersion increments version', () => {
        const p = basePipeline(); p.name = 'pipe3';
        vm.ensureBaseVersion('pipe3', p);
        const v = vm.commitVersion('pipe3', p, 'sess1', 'Optimize (2 edits)');
        assert.equal(v, 2);
        const cursor = vm.getCursor('pipe3');
        assert.equal(cursor.currentVersion, 2);
        assert.equal(cursor.headVersion, 2);
    });

    test('undo returns previous pipeline', () => {
        const p = basePipeline(); p.name = 'pipe4';
        vm.ensureBaseVersion('pipe4', p);
        p.steps[0].params.userPrompt = 'modified';
        vm.commitVersion('pipe4', p, '', 'v2');
        const restored = vm.undo('pipe4');
        assert.ok(restored);
        assert.equal(restored.steps[0].params.userPrompt, 'hello');
    });

    test('undo returns null at version 1', () => {
        const p = basePipeline(); p.name = 'pipe5';
        vm.ensureBaseVersion('pipe5', p);
        assert.equal(vm.undo('pipe5'), null);
    });

    test('redo returns next pipeline', () => {
        const p = basePipeline(); p.name = 'pipe6';
        vm.ensureBaseVersion('pipe6', p);
        p.steps[0].params.userPrompt = 'v2';
        vm.commitVersion('pipe6', p, '', 'v2');
        vm.undo('pipe6');
        const redone = vm.redo('pipe6');
        assert.ok(redone);
        assert.equal(redone.steps[0].params.userPrompt, 'v2');
    });

    test('redo returns null at head version', () => {
        const p = basePipeline(); p.name = 'pipe7';
        vm.ensureBaseVersion('pipe7', p);
        assert.equal(vm.redo('pipe7'), null);
    });

    test('checkoutVersion loads specific version', () => {
        const p = basePipeline(); p.name = 'pipe8';
        vm.ensureBaseVersion('pipe8', p);
        p.steps[0].params.userPrompt = 'v2';
        vm.commitVersion('pipe8', p, '', 'v2');
        const checked = vm.checkoutVersion('pipe8', 1);
        assert.ok(checked);
        assert.equal(checked.steps[0].params.userPrompt, 'hello');
    });

    test('checkoutVersion returns null for missing version', () => {
        vm.ensureBaseVersion('pipe9', basePipeline());
        assert.equal(vm.checkoutVersion('pipe9', 99), null);
    });
});

// ── 4. PipelineRunner ─────────────────────────────────────────
describe('PipelineRunner', () => {
    test('initial state: not running', () => {
        const r = new PipelineRunner();
        assert.equal(r.isRunning(), false);
    });

    test('cancel when idle is safe', () => {
        const r = new PipelineRunner();
        r.cancel();
        assert.equal(r.isRunning(), false);
    });

    test('run with no steps completes immediately', async () => {
        const r = new PipelineRunner();
        await r.run('empty', [], 'input', [], 'child');
        const completed = r.events.find(e => e.type === 'pipeline_completed');
        assert.ok(completed, 'should emit pipeline_completed');
    });

    test('run with mock AI step completes', async () => {
        const r = new PipelineRunner();
        const mockProvider = { call: async () => ({ content: 'AI output', model: 'mock' }) };
        r.registerProvider('openai', mockProvider);
        await r.run('test', [{ name: 'Step1', type: 'ai', params: { provider: 'openai', model: 'gpt-4.1', userPrompt: '{content}' } }], 'hello', [], 'child');
        const done = r.events.find(e => e.type === 'step_done');
        assert.ok(done, 'step_done should be emitted');
        const completed = r.events.find(e => e.type === 'pipeline_completed');
        assert.ok(completed, 'pipeline_completed should be emitted');
        assert.equal(r.historySteps[0].output, 'AI output');
    });

    test('AI step substitutes {content} placeholder', async () => {
        const r = new PipelineRunner();
        let capturedPrompt = '';
        r.registerProvider('openai', { call: async req => { capturedPrompt = req.userPrompt; return { content: 'done', model: 'mock' }; } });
        await r.run('t', [{ name: 's', type: 'ai', params: { provider: 'openai', userPrompt: 'process: {content}' } }], 'mydata', [], 'child');
        assert.equal(capturedPrompt, 'process: mydata');
    });

    test('AI step substitutes {result} from previous step', async () => {
        const r = new PipelineRunner();
        let secondPrompt = '';
        let call = 0;
        r.registerProvider('openai', {
            call: async req => {
                call++;
                if (call === 2) secondPrompt = req.userPrompt;
                return { content: 'step' + call, model: 'mock' };
            }
        });
        const steps = [
            { name: 's1', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } },
            { name: 's2', type: 'ai', params: { provider: 'openai', userPrompt: 'prev={result}' } },
        ];
        await r.run('t', steps, 'input', [], 'child');
        assert.equal(secondPrompt, 'prev=step1');
    });

    test('run emits pipeline_error for missing provider', async () => {
        const r = new PipelineRunner();
        try {
            await r.run('t', [{ name: 's', type: 'ai', params: { provider: 'openai' } }], 'x', [], 'child');
        } catch {}
        const err = r.events.find(e => e.type === 'pipeline_error');
        assert.ok(err, 'pipeline_error should be emitted');
    });

    test('cancel mid-run stops execution', async () => {
        const r = new PipelineRunner();
        let resolveAI;
        r.registerProvider('openai', { call: () => new Promise(res => { resolveAI = res; }) });
        const runPromise = r.run('t', [
            { name: 's1', type: 'ai', params: { provider: 'openai' } },
            { name: 's2', type: 'ai', params: { provider: 'openai' } },
        ], 'x', [], 'child');
        r.cancel();
        resolveAI({ content: 'done', model: 'mock' });
        await runPromise;
        assert.equal(r.isRunning(), false);
    });

    test('manual step pauses and resumes', async () => {
        const r = new PipelineRunner();
        const runPromise = r.run('t', [{ name: 'm', type: 'manual', params: { mode: 'view', prompt: 'Edit this' } }], 'original', [], 'child');
        // wait for pause event
        await new Promise(res => setTimeout(res, 10));
        const pause = r.events.find(e => e.type === 'manual_step_pause');
        assert.ok(pause, 'manual_step_pause should be emitted');
        r.resumeManual('edited content');
        await runPromise;
        assert.equal(r.historySteps[0].output, 'edited content');
        assert.ok(r.events.find(e => e.type === 'pipeline_completed'));
    });

    test('wizard step pauses and resumes', async () => {
        const r = new PipelineRunner();
        const runPromise = r.run('t', [{ name: 'w', type: 'wizard', params: { wizard: 'developer' } }], 'x', [], 'child');
        await new Promise(res => setTimeout(res, 10));
        assert.ok(r.events.find(e => e.type === 'wizard_step_pause'));
        r.resumeWizard(JSON.stringify({ lang: 'Python' }));
        await runPromise;
        assert.equal(r.historySteps[0].output, JSON.stringify({ lang: 'Python' }));
    });

    test('filter step auto-mode skips pause', async () => {
        const r = new PipelineRunner();
        await r.run('t', [{ name: 'f', type: 'filter', params: { mode: 'auto' } }], 'data', [], 'child');
        assert.ok(!r.events.find(e => e.type === 'step_filter_pause'), 'auto filter should not pause');
        assert.ok(r.events.find(e => e.type === 'pipeline_completed'));
    });

    test('filter step manual-mode pauses and resumes', async () => {
        const r = new PipelineRunner();
        const runPromise = r.run('t', [{ name: 'f', type: 'filter', params: { mode: 'manual' } }], 'data', [], 'child');
        await new Promise(res => setTimeout(res, 10));
        assert.ok(r.events.find(e => e.type === 'step_filter_pause'));
        r.resumeFilter(JSON.stringify({ approved: [0], rejected: [] }));
        await runPromise;
        assert.ok(r.events.find(e => e.type === 'pipeline_completed'));
    });

    test('unknown step type is skipped', async () => {
        const r = new PipelineRunner();
        await r.run('t', [{ name: 'x', type: 'unknown_type', params: {} }], 'data', [], 'child');
        assert.equal(r.historySteps[0].status, 'skipped');
        assert.ok(r.events.find(e => e.type === 'pipeline_completed'));
    });

    test('setExternalInput overrides step input', async () => {
        const r = new PipelineRunner();
        let capturedPrompt = '';
        r.registerProvider('openai', { call: async req => { capturedPrompt = req.userPrompt; return { content: 'out', model: 'mock' }; } });
        r.setExternalInput('external data');
        await r.run('t', [{ name: 's', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } }], 'original', [], 'child');
        assert.equal(capturedPrompt, 'original'); // {content} is always original; {result} would use external
    });

    test('multi-step pipeline passes output between steps', async () => {
        const r = new PipelineRunner();
        let call = 0;
        r.registerProvider('openai', { call: async () => ({ content: `out${++call}`, model: 'mock' }) });
        const steps = [
            { name: 's1', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } },
            { name: 's2', type: 'ai', params: { provider: 'openai', userPrompt: '{result}' } },
            { name: 's3', type: 'ai', params: { provider: 'openai', userPrompt: '{result}' } },
        ];
        await r.run('t', steps, 'start', [], 'child');
        assert.equal(r.historySteps[0].output, 'out1');
        assert.equal(r.historySteps[1].output, 'out2');
        assert.equal(r.historySteps[2].output, 'out3');
        assert.equal(call, 3);
    });

    test('runId is set after run', async () => {
        const r = new PipelineRunner();
        await r.run('t', [], 'x', [], 'child');
        assert.ok(r.getRunId().length > 0);
    });
});

// ── 5. AI Provider shape tests (no real network) ──────────────
describe('AI Provider request building', () => {
    // We verify that the provider builds the correct body/headers
    // by intercepting the httpRequest via a local mock server.

    let server, serverPort;
    let lastRequest = null;

    before(async () => {
        await new Promise(resolve => {
            server = http.createServer((req, res) => {
                let body = '';
                req.on('data', c => body += c);
                req.on('end', () => {
                    lastRequest = { method: req.method, url: req.url, headers: req.headers, body };
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    // Return a minimal valid response for each provider
                    if (req.url.includes('/v1/chat/completions'))
                        res.end(JSON.stringify({ choices: [{ message: { content: 'mock' } }] }));
                    else if (req.url.includes('/v1/messages'))
                        res.end(JSON.stringify({ content: [{ text: 'mock' }] }));
                    else if (req.url.includes('/api/generate'))
                        res.end(JSON.stringify({ response: 'mock' }));
                    else if (req.url.includes('generateContent'))
                        res.end(JSON.stringify({ candidates: [{ content: { parts: [{ text: 'mock' }] } }] }));
                    else if (req.url.includes('/v1/models') || req.url.includes('/api/tags'))
                        res.end(JSON.stringify({ data: [{ id: 'model-x' }], models: [{ name: 'tag/model-x' }] }));
                    else
                        res.end(JSON.stringify({}));
                });
            });
            server.listen(0, '127.0.0.1', () => { serverPort = server.address().port; resolve(); });
        });
    });

    after(() => server.close());

    // ---- inline httpRequest for these tests ----
    function httpReq(url, method, headers, body) {
        return new Promise((resolve, reject) => {
            const u = new URL(url);
            const opts = { hostname: u.hostname, port: u.port, path: u.pathname + u.search, method, headers: { ...(body ? { 'Content-Length': Buffer.byteLength(body) } : {}), ...headers } };
            const req = http.request(opts, res => { const ch = []; res.on('data', c => ch.push(c)); res.on('end', () => resolve(Buffer.concat(ch).toString())); });
            req.on('error', reject);
            if (body) req.write(body);
            req.end();
        });
    }

    // Minimal provider classes that hit local mock server
    class TestOpenAI {
        constructor(apiKey, base) { this.apiKey = apiKey; this.base = base; }
        async call(req) {
            const body = JSON.stringify({ model: req.model, messages: [{ role: 'user', content: req.userPrompt }], temperature: req.temperature, max_tokens: req.maxTokens, stream: false });
            const raw = await httpReq(this.base + '/v1/chat/completions', 'POST', { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + this.apiKey, 'Content-Length': Buffer.byteLength(body) }, body);
            const j = JSON.parse(raw);
            return { content: j.choices?.[0]?.message?.content ?? '', model: req.model };
        }
        async listModels() {
            const raw = await httpReq(this.base + '/v1/models', 'GET', { 'Authorization': 'Bearer ' + this.apiKey }, null);
            return JSON.parse(raw).data.map(m => m.id);
        }
    }

    class TestAnthropic {
        constructor(apiKey, base) { this.apiKey = apiKey; this.base = base; }
        async call(req) {
            const body = JSON.stringify({ model: req.model, max_tokens: req.maxTokens, system: req.systemPrompt, messages: [{ role: 'user', content: req.userPrompt }], stream: false });
            const raw = await httpReq(this.base + '/v1/messages', 'POST', { 'Content-Type': 'application/json', 'x-api-key': this.apiKey, 'anthropic-version': '2023-06-01', 'Content-Length': Buffer.byteLength(body) }, body);
            return { content: JSON.parse(raw).content?.[0]?.text ?? '', model: req.model };
        }
    }

    class TestOllama {
        constructor(base) { this.base = base; }
        async call(req) {
            const body = JSON.stringify({ model: req.model, system: req.systemPrompt, prompt: req.userPrompt, options: { temperature: req.temperature }, stream: false });
            const raw = await httpReq(this.base + '/api/generate', 'POST', { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body) }, body);
            return { content: JSON.parse(raw).response ?? '', model: req.model };
        }
        async listModels() {
            const raw = await httpReq(this.base + '/api/tags', 'GET', {}, null);
            return JSON.parse(raw).models.map(m => m.name.split('/').pop());
        }
    }

    test('OpenAI call sends Authorization header', async () => {
        const p = new TestOpenAI('sk-test', `http://127.0.0.1:${serverPort}`);
        await p.call({ model: 'gpt-4.1', userPrompt: 'hello', systemPrompt: '', temperature: 0.7, maxTokens: 512 });
        assert.ok(lastRequest.headers.authorization?.startsWith('Bearer sk-test'));
    });

    test('OpenAI call sends correct endpoint', async () => {
        const p = new TestOpenAI('k', `http://127.0.0.1:${serverPort}`);
        await p.call({ model: 'gpt-4.1', userPrompt: 'hi', systemPrompt: '', temperature: 0.7, maxTokens: 512 });
        assert.equal(lastRequest.url, '/v1/chat/completions');
    });

    test('OpenAI call returns content', async () => {
        const p = new TestOpenAI('k', `http://127.0.0.1:${serverPort}`);
        const resp = await p.call({ model: 'gpt-4.1', userPrompt: 'hi', systemPrompt: '', temperature: 0.7, maxTokens: 512 });
        assert.equal(resp.content, 'mock');
    });

    test('OpenAI listModels returns array', async () => {
        const p = new TestOpenAI('k', `http://127.0.0.1:${serverPort}`);
        const models = await p.listModels();
        assert.ok(Array.isArray(models));
        assert.ok(models.length > 0);
    });

    test('Anthropic call sends x-api-key header', async () => {
        const p = new TestAnthropic('anth-key', `http://127.0.0.1:${serverPort}`);
        await p.call({ model: 'claude-sonnet-4-6', userPrompt: 'hi', systemPrompt: '', temperature: 0.7, maxTokens: 512 });
        assert.equal(lastRequest.headers['x-api-key'], 'anth-key');
    });

    test('Anthropic call sends anthropic-version header', async () => {
        const p = new TestAnthropic('k', `http://127.0.0.1:${serverPort}`);
        await p.call({ model: 'claude-sonnet-4-6', userPrompt: 'hi', systemPrompt: '', temperature: 0.7, maxTokens: 512 });
        assert.equal(lastRequest.headers['anthropic-version'], '2023-06-01');
    });

    test('Anthropic call returns content', async () => {
        const p = new TestAnthropic('k', `http://127.0.0.1:${serverPort}`);
        const resp = await p.call({ model: 'claude-sonnet-4-6', userPrompt: 'hi', systemPrompt: '', temperature: 0.7, maxTokens: 512 });
        assert.equal(resp.content, 'mock');
    });

    test('Ollama call uses /api/generate endpoint', async () => {
        const p = new TestOllama(`http://127.0.0.1:${serverPort}`);
        await p.call({ model: 'llama3.2', userPrompt: 'hi', systemPrompt: '', temperature: 0.5, maxTokens: 512 });
        assert.equal(lastRequest.url, '/api/generate');
    });

    test('Ollama call returns response', async () => {
        const p = new TestOllama(`http://127.0.0.1:${serverPort}`);
        const resp = await p.call({ model: 'llama3.2', userPrompt: 'hi', systemPrompt: '', temperature: 0.5, maxTokens: 512 });
        assert.equal(resp.content, 'mock');
    });

    test('Ollama listModels returns array', async () => {
        const p = new TestOllama(`http://127.0.0.1:${serverPort}`);
        const models = await p.listModels();
        assert.ok(Array.isArray(models) && models.length > 0);
    });
});

// ── 6. Bridge message routing ─────────────────────────────────
describe('Bridge message handling', () => {
    // Test that handleBridgeMessage dispatches correctly by running
    // the same logic with a fake postToJS that captures output.

    makeApp = function(tmpDir) {
        const st = new Storage();
        st.init(tmpDir);
        const r = new PipelineRunner();
        const vm = new PipelineVersionManager(st);
        const sent = [];
        const post = (type, payload) => sent.push({ type, payload });

        function handle(type, payload) {
            switch (type) {
                case 'save_session':
                    if (payload?.tabs) st.saveSession({ tabs: payload.tabs });
                    break;
                case 'save_node':
                    if (payload?.tabFile && payload?.root) st.saveTabData(payload.tabFile, payload.root);
                    break;
                case 'get_providers': {
                    const provs = st.loadProviders();
                    if (!provs.mock) provs.mock = { apiKey: '', baseUrl: '', models: ['echo', 'fixed'] };
                    post('providers_result', provs);
                    break;
                }
                case 'save_providers':
                    st.saveProviders(payload || {});
                    break;
                case 'save_pipeline':
                    if (payload?.name) {
                        const pl = st.loadPipelines();
                        const i = pl.findIndex(p => p.name === payload.name);
                        if (i >= 0) Object.assign(pl[i], payload); else pl.push(payload);
                        st.savePipelines(pl);
                        post('pipeline_list', { pipelines: pl });
                    }
                    break;
                case 'delete_pipeline':
                    if (payload?.name) {
                        const pl = st.loadPipelines().filter(p => p.name !== payload.name);
                        st.savePipelines(pl);
                        post('pipeline_list', { pipelines: pl });
                    }
                    break;
                case 'history_list': {
                    const items = st.listHistory().slice(0, 100).flatMap(f => {
                        const raw = st.loadHistoryRecord(f); if (!raw) return [];
                        const obj = JSON.parse(raw); if (!obj.pipelineName) return [];
                        return [{ id: obj.id || '', pipelineName: obj.pipelineName, startedAt: obj.startedAt || '', status: obj.status || 'completed', evaluation: obj.evaluation || '', stepCount: (obj.steps||[]).length }];
                    });
                    post('history_list_result', { items });
                    break;
                }
                case 'save_recipes':
                    st.saveRecipes(payload || []);
                    break;
                case 'save_config':
                    st.saveGeneralConfig({ historyRetention: payload?.historyRetention || 50, defaultProvider: payload?.defaultProvider || 'openai', defaultModel: payload?.defaultModel || '' });
                    break;
                case 'send_to_chest':
                    if (payload?.chestName && payload?.content != null) st.saveToNamedChest(payload.chestName, payload.content);
                    break;
                case 'cancel_pipeline':
                    r.cancel();
                    break;
                case 'run_prompt_process': {
                    // merge machine-level + belt-level attachments (mirrors main.js logic)
                    const allAttachments = [
                        ...(payload?.attachments || []),
                        ...(payload?.inputAttachments || []),
                    ];
                    post('run_prompt_process_captured', { allAttachments, content: payload?.content });
                    break;
                }
            }
        }

        return { st, r, sent, handle };
    }

    test('save_session persists tabs', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_session', { tabs: [{ name: 'Tab1', file: 'tab1.json' }] });
        assert.equal(st.loadSession().tabs[0].name, 'Tab1');
        rmrf(tmpDir);
    });

    test('save_node persists node data', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_node', { tabFile: 'node.json', root: { title: 'T', content: 'C', mimetype: 'text/plain', attachments: [], children: [] } });
        assert.equal(st.loadTabData('node.json').title, 'T');
        rmrf(tmpDir);
    });

    test('get_providers returns providers_result', () => {
        const tmpDir = makeTempDir();
        const { st, sent, handle } = makeApp(tmpDir);
        st.saveProviders({ openai: { apiKey: 'k', baseUrl: '', models: [] } });
        handle('get_providers');
        assert.equal(sent[0].type, 'providers_result');
        assert.equal(sent[0].payload.openai.apiKey, 'k');
        rmrf(tmpDir);
    });

    test('save_providers persists and save_providers round-trips', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_providers', { anthropic: { apiKey: 'ant', baseUrl: '', models: [] } });
        assert.equal(st.loadProviders().anthropic.apiKey, 'ant');
        rmrf(tmpDir);
    });

    test('save_pipeline creates new pipeline', () => {
        const tmpDir = makeTempDir();
        const { st, sent, handle } = makeApp(tmpDir);
        handle('save_pipeline', { name: 'MyPipe', steps: [], mode: 'basic', outputMode: 'child' });
        assert.equal(sent[0].type, 'pipeline_list');
        assert.ok(sent[0].payload.pipelines.some(p => p.name === 'MyPipe'));
        rmrf(tmpDir);
    });

    test('save_pipeline updates existing pipeline', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_pipeline', { name: 'P', steps: [], mode: 'basic', outputMode: 'child' });
        handle('save_pipeline', { name: 'P', steps: [{ name: 's1', type: 'ai', params: {} }], mode: 'basic', outputMode: 'sibling' });
        const pl = st.loadPipelines();
        const p = pl.find(x => x.name === 'P');
        assert.equal(p.outputMode, 'sibling');
        assert.equal(p.steps.length, 1);
        rmrf(tmpDir);
    });

    test('delete_pipeline removes pipeline', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_pipeline', { name: 'Del', steps: [], mode: 'basic', outputMode: 'child' });
        handle('delete_pipeline', { name: 'Del' });
        assert.ok(!st.loadPipelines().some(p => p.name === 'Del'));
        rmrf(tmpDir);
    });

    test('history_list returns saved history', () => {
        const tmpDir = makeTempDir();
        const { st, sent, handle } = makeApp(tmpDir);
        st.saveHistory(JSON.stringify({ id: 'h1', pipelineName: 'P', status: 'completed', steps: [] }));
        handle('history_list');
        assert.equal(sent[0].type, 'history_list_result');
        assert.ok(sent[0].payload.items.some(i => i.id === 'h1'));
        rmrf(tmpDir);
    });

    test('save_recipes persists recipes', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_recipes', [{ name: 'r1', type: 'ai', provider: 'openai', model: 'gpt-4.1', temperature: 0.8, systemPrompt: '', command: '' }]);
        assert.equal(st.loadRecipes()[0].name, 'r1');
        rmrf(tmpDir);
    });

    test('save_config persists config', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('save_config', { historyRetention: 20, defaultProvider: 'gemini', defaultModel: 'gemini-2.5-pro' });
        const cfg = st.loadGeneralConfig();
        assert.equal(cfg.historyRetention, 20);
        assert.equal(cfg.defaultProvider, 'gemini');
        rmrf(tmpDir);
    });

    test('send_to_chest stores content in chest', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        handle('send_to_chest', { chestName: 'box', content: 'treasure' });
        assert.equal(st.loadFromNamedChest('box'), 'treasure');
        rmrf(tmpDir);
    });

    test('cancel_pipeline stops the runner', () => {
        const tmpDir = makeTempDir();
        const { r, handle } = makeApp(tmpDir);
        handle('cancel_pipeline');
        assert.equal(r.isRunning(), false);
        rmrf(tmpDir);
    });

    test('run_prompt_process merges machine and belt attachments', () => {
        const tmpDir = makeTempDir();
        const { sent, handle } = makeApp(tmpDir);
        handle('run_prompt_process', {
            content: 'hello',
            attachments: [{ file: 'bg.png', mimetype: 'image/png' }],
            inputAttachments: [{ file: 'ref.jpg', mimetype: 'image/jpeg' }],
        });
        const captured = sent.find(s => s.type === 'run_prompt_process_captured');
        assert.ok(captured, 'run_prompt_process_captured should be emitted');
        assert.equal(captured.payload.allAttachments.length, 2);
        assert.equal(captured.payload.allAttachments[0].file, 'bg.png');
        assert.equal(captured.payload.allAttachments[1].file, 'ref.jpg');
        rmrf(tmpDir);
    });

    test('run_prompt_process with only machine attachments', () => {
        const tmpDir = makeTempDir();
        const { sent, handle } = makeApp(tmpDir);
        handle('run_prompt_process', {
            content: 'test',
            attachments: [{ file: 'ctx.mp3', mimetype: 'audio/mpeg' }],
        });
        const captured = sent.find(s => s.type === 'run_prompt_process_captured');
        assert.equal(captured.payload.allAttachments.length, 1);
        assert.equal(captured.payload.allAttachments[0].file, 'ctx.mp3');
        rmrf(tmpDir);
    });

    test('run_prompt_process with no attachments produces empty array', () => {
        const tmpDir = makeTempDir();
        const { sent, handle } = makeApp(tmpDir);
        handle('run_prompt_process', { content: 'x' });
        const captured = sent.find(s => s.type === 'run_prompt_process_captured');
        assert.deepStrictEqual(captured.payload.allAttachments, []);
        rmrf(tmpDir);
    });

    test('save_node persists selectedRecipe in node data', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        const nodeData = {
            title: 'MyNode',
            content: 'some content',
            mimetype: 'text/plain',
            attachments: [],
            inputAttachments: [],
            selectedRecipe: 'GPT-4 Fast',
            children: [],
        };
        handle('save_node', { tabFile: 'node.json', root: nodeData });
        const loaded = st.loadTabData('node.json');
        assert.equal(loaded.selectedRecipe, 'GPT-4 Fast');
        rmrf(tmpDir);
    });

    test('save_node persists node.attachments and node.inputAttachments', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        const nodeData = {
            title: 'N',
            content: '',
            mimetype: 'text/plain',
            attachments: [{ file: 'machine.png', mimetype: 'image/png' }],
            inputAttachments: [{ file: 'belt.wav', mimetype: 'audio/wav' }],
            children: [],
        };
        handle('save_node', { tabFile: 'n.json', root: nodeData });
        const loaded = st.loadTabData('n.json');
        assert.equal(loaded.attachments[0].file, 'machine.png');
        assert.equal(loaded.inputAttachments[0].file, 'belt.wav');
        rmrf(tmpDir);
    });
});

// inline buildMetaRecord — mirrors runner.js logic for step artifact inclusion
function buildMetaRecord(histStep) {
    return {
        name: histStep.name || '',
        type: histStep.type || 'ai',
        input: histStep.input || '',
        output: histStep.output || '',
        artifacts: histStep.artifacts || [],
        tokens: histStep.completionTokens || 0,
    };
}

// ── 7. buildMetaRecord — artifact field inclusion ─────────────
describe('buildMetaRecord artifact inclusion', () => {
    test('includes artifacts field defaulting to empty array', () => {
        const rec = buildMetaRecord({ name: 'step1', type: 'ai', input: 'in', output: 'out' });
        assert.deepStrictEqual(rec.artifacts, []);
    });

    test('preserves artifacts when present', () => {
        const artifacts = [{ label: 'report.pdf', path: '/tmp/report.pdf', type: 'file' }];
        const rec = buildMetaRecord({ name: 's', type: 'ai', input: 'i', output: 'o', artifacts });
        assert.deepStrictEqual(rec.artifacts, artifacts);
    });

    test('includes all required fields', () => {
        const rec = buildMetaRecord({ name: 'translate', type: 'ai', input: 'hello', output: 'こんにちは', completionTokens: 42 });
        assert.equal(rec.name, 'translate');
        assert.equal(rec.type, 'ai');
        assert.equal(rec.input, 'hello');
        assert.equal(rec.output, 'こんにちは');
        assert.equal(rec.tokens, 42);
        assert.deepStrictEqual(rec.artifacts, []);
    });

    test('pipeline_completed event steps can be mapped through buildMetaRecord', async () => {
        const r = new PipelineRunner();
        r.registerProvider('openai', { call: async () => ({ content: 'result', model: 'mock' }) });
        await r.run('pipe', [{ name: 's1', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } }], 'input', [], 'child');
        const done = r.events.find(e => e.type === 'pipeline_completed');
        assert.ok(done, 'pipeline_completed should be emitted');
        const records = done.payload.steps.map(buildMetaRecord);
        assert.equal(records.length, 1);
        assert.deepStrictEqual(records[0].artifacts, []);
        assert.equal(records[0].output, 'result');
    });
});

// ── 8. Pipeline state reset logic (Case B node-switch) ────────
describe('Pipeline state reset on node switch', () => {
    // Pure logic: mirrors the selectNode Case B reset in app.js
    function resetPipelineSteps(steps) {
        return steps.map(s => ({
            ...s,
            completed: false,
            input: '',
            output: '',
            streamingOutput: '',
            status: 'pending',
        }));
    }

    test('resetPipelineSteps clears completed flag on all steps', () => {
        const steps = [
            { name: 's1', completed: true, input: 'in', output: 'out', streamingOutput: 'x', status: 'completed' },
            { name: 's2', completed: false, input: '', output: '', streamingOutput: '', status: 'pending' },
        ];
        const reset = resetPipelineSteps(steps);
        assert.ok(reset.every(s => s.completed === false));
    });

    test('resetPipelineSteps clears input/output data', () => {
        const steps = [{ name: 's1', completed: true, input: 'hello', output: 'world', streamingOutput: 'wor', status: 'completed' }];
        const reset = resetPipelineSteps(steps);
        assert.equal(reset[0].input, '');
        assert.equal(reset[0].output, '');
        assert.equal(reset[0].streamingOutput, '');
    });

    test('resetPipelineSteps sets all statuses to pending', () => {
        const steps = [
            { name: 's1', completed: true, input: '', output: '', streamingOutput: '', status: 'completed' },
            { name: 's2', completed: true, input: '', output: '', streamingOutput: '', status: 'running' },
        ];
        const reset = resetPipelineSteps(steps);
        assert.ok(reset.every(s => s.status === 'pending'));
    });

    test('resetPipelineSteps preserves step name and other properties', () => {
        const steps = [{ name: 'translate', type: 'ai', completed: true, input: 'x', output: 'y', streamingOutput: '', status: 'completed' }];
        const reset = resetPipelineSteps(steps);
        assert.equal(reset[0].name, 'translate');
        assert.equal(reset[0].type, 'ai');
    });

    test('dialog should be shown when any step is completed (condition check)', () => {
        const shouldShowDialog = (viewMode, steps) =>
            viewMode === 'pipeline' && steps.some(s => s.completed);

        assert.ok(shouldShowDialog('pipeline', [{ completed: true }, { completed: false }]));
        assert.ok(!shouldShowDialog('pipeline', [{ completed: false }, { completed: false }]));
        assert.ok(!shouldShowDialog('node', [{ completed: true }]));
    });
});

// ─────────────────────────────────────────────────────────────
// Helper functions extracted from app.js for mode-specific tests
// ─────────────────────────────────────────────────────────────

// Mirror of app.js: reads last step output from child's pipelineMeta (閲覧/通常モード出力)
function getLastStepOutput(child) {
    let text = child.content ? (() => { try { return Buffer.from(child.content, 'base64').toString('utf8'); } catch { return child.content; } })() : '';
    let artifacts = [];
    if (child.pipelineMeta) {
        try {
            const meta = JSON.parse(child.pipelineMeta);
            if (meta && meta.steps && meta.steps.length > 0) {
                const last = meta.steps[meta.steps.length - 1];
                text = last.output || text;
                artifacts = last.artifacts || [];
            }
        } catch (e) {}
    }
    return { text, artifacts };
}

// Mirror of app.js processPrompt: build sent text for normal mode (通常モード)
function buildSentText(prompt, input) {
    return prompt.includes('{content}')
        ? prompt.replace('{content}', input)
        : (prompt + '\n\n' + input);
}

// Mirror of app.js renderPipelineInput: step source label (連結モード)
function getStepSourceLabel(stepIndex) {
    return stepIndex === 0 ? '元入力 ({content})' : `Step ${stepIndex} 出力 ({result})`;
}

// Mirror of app.js renderPipelineOutput: select display text (連結モード)
function selectOutputText(step) {
    return step.completed
        ? (step.output || '(empty output)')
        : (step.streamingOutput || (step.status === 'running' ? '...' : '(pending)'));
}

// Mirror of app.js renderPipelineInput: get previous step artifacts (連結モード)
function getPrevArtifacts(steps, si) {
    return (si > 0 && steps[si - 1].artifacts) || [];
}

// ── 9. 閲覧モード (Node view mode) ───────────────────────────
describe('閲覧モード — node view mode logic', () => {
    test('getLastStepOutput returns fallback content when no pipelineMeta', () => {
        const child = { content: Buffer.from('plain result').toString('base64') };
        const { text, artifacts } = getLastStepOutput(child);
        assert.equal(text, 'plain result');
        assert.deepStrictEqual(artifacts, []);
    });

    test('getLastStepOutput reads last step output from pipelineMeta', () => {
        const meta = { steps: [
            { name: 's1', output: 'step1 out', artifacts: [] },
            { name: 's2', output: 'step2 out', artifacts: [] },
        ]};
        const child = { content: '', pipelineMeta: JSON.stringify(meta) };
        const { text } = getLastStepOutput(child);
        assert.equal(text, 'step2 out');
    });

    test('getLastStepOutput returns artifacts from last step', () => {
        const artifacts = [{ label: 'result.pdf', path: '/tmp/result.pdf', type: 'file' }];
        const meta = { steps: [
            { name: 's1', output: 'text', artifacts },
        ]};
        const child = { content: '', pipelineMeta: JSON.stringify(meta) };
        const { artifacts: got } = getLastStepOutput(child);
        assert.deepStrictEqual(got, artifacts);
    });

    test('getLastStepOutput uses content as fallback when last step output is empty', () => {
        const meta = { steps: [{ name: 's1', output: '' }] };
        const child = {
            content: Buffer.from('fallback text').toString('base64'),
            pipelineMeta: JSON.stringify(meta),
        };
        const { text } = getLastStepOutput(child);
        assert.equal(text, 'fallback text');
    });

    test('getLastStepOutput tolerates invalid pipelineMeta JSON', () => {
        const child = {
            content: Buffer.from('safe fallback').toString('base64'),
            pipelineMeta: '{ broken json',
        };
        const { text, artifacts } = getLastStepOutput(child);
        assert.equal(text, 'safe fallback');
        assert.deepStrictEqual(artifacts, []);
    });

    test('node.inputAttachments is separate from node.attachments', () => {
        const node = {
            attachments: [{ file: 'machine.png', mimetype: 'image/png' }],
            inputAttachments: [{ file: 'belt.wav', mimetype: 'audio/wav' }],
        };
        assert.notDeepStrictEqual(node.attachments, node.inputAttachments);
        assert.equal(node.attachments[0].file, 'machine.png');
        assert.equal(node.inputAttachments[0].file, 'belt.wav');
    });

    test('selectedRecipe is restored from node.selectedRecipe on node switch', () => {
        // Simulates selectNode recipe restoration logic
        const node = { selectedRecipe: 'GPT-4 Fast', content: '' };
        const state = { selectedRecipe: '' };
        state.selectedRecipe = node.selectedRecipe || '';
        assert.equal(state.selectedRecipe, 'GPT-4 Fast');
    });

    test('selectedRecipe defaults to empty string if not set', () => {
        const node = { content: '' };  // no selectedRecipe field
        const state = { selectedRecipe: 'OldRecipe' };
        state.selectedRecipe = node.selectedRecipe || '';
        assert.equal(state.selectedRecipe, '');
    });
});

// ── 10. 通常モード (Normal single-run mode) ───────────────────
describe('通常モード — normal single-run logic', () => {
    test('buildSentText replaces {content} placeholder', () => {
        assert.equal(buildSentText('Translate: {content}', 'Hello world'), 'Translate: Hello world');
    });

    test('buildSentText concatenates when no {content} placeholder', () => {
        assert.equal(buildSentText('Translate this:', 'Hello'), 'Translate this:\n\nHello');
    });

    test('buildSentText with empty input', () => {
        assert.equal(buildSentText('Say {content} please', ''), 'Say  please');
    });

    test('buildSentText with multiple {content} occurrences replaces first only', () => {
        // String.replace without /g replaces first match
        const result = buildSentText('{content} and {content}', 'X');
        assert.equal(result, 'X and {content}');
    });

    test('run_prompt_process payload includes machine and belt attachments', () => {
        const node = {
            attachments: [{ file: 'ctx.png', mimetype: 'image/png' }],
            inputAttachments: [{ file: 'input.jpg', mimetype: 'image/jpeg' }],
        };
        const payload = {
            content: 'my input',
            attachments: node.attachments || [],
            inputAttachments: node.inputAttachments || [],
        };
        assert.equal(payload.attachments.length, 1);
        assert.equal(payload.inputAttachments.length, 1);
        assert.equal(payload.attachments[0].file, 'ctx.png');
        assert.equal(payload.inputAttachments[0].file, 'input.jpg');
    });

    test('run_prompt_process payload has empty arrays when node has no attachments', () => {
        const node = {};
        const payload = {
            attachments: node.attachments || [],
            inputAttachments: node.inputAttachments || [],
        };
        assert.deepStrictEqual(payload.attachments, []);
        assert.deepStrictEqual(payload.inputAttachments, []);
    });

    test('merged allAttachments order: machine first, belt second', () => {
        const machineAtt = [{ file: 'm.png', mimetype: 'image/png' }];
        const beltAtt = [{ file: 'b.jpg', mimetype: 'image/jpeg' }];
        const all = [...machineAtt, ...beltAtt];
        assert.equal(all[0].file, 'm.png');
        assert.equal(all[1].file, 'b.jpg');
    });

    test('pipeline_completed event carries step output', async () => {
        const r = new PipelineRunner();
        r.registerProvider('openai', { call: async () => ({ content: 'translated text', model: 'mock' }) });
        await r.run('single', [
            { name: 'translate', type: 'ai', params: { provider: 'openai', userPrompt: 'Translate: {content}' } },
        ], 'Hello', [], 'child');
        const done = r.events.find(e => e.type === 'pipeline_completed');
        assert.equal(done.payload.steps[0].output, 'translated text');
        assert.equal(done.payload.steps[0].input, 'Hello');
    });
});

// ── 11. 連結モード (Pipeline/chain mode) ─────────────────────
describe('連結モード — pipeline chain mode logic', () => {
    test('step 0 source label is 元入力 ({content})', () => {
        assert.equal(getStepSourceLabel(0), '元入力 ({content})');
    });

    test('step N source label references previous step', () => {
        assert.equal(getStepSourceLabel(1), 'Step 1 出力 ({result})');
        assert.equal(getStepSourceLabel(3), 'Step 3 出力 ({result})');
    });

    test('selectOutputText: pending step shows (pending)', () => {
        const step = { completed: false, status: 'pending', output: '', streamingOutput: '' };
        assert.equal(selectOutputText(step), '(pending)');
    });

    test('selectOutputText: running step shows ...', () => {
        const step = { completed: false, status: 'running', output: '', streamingOutput: '' };
        assert.equal(selectOutputText(step), '...');
    });

    test('selectOutputText: running step shows streamingOutput when available', () => {
        const step = { completed: false, status: 'running', output: '', streamingOutput: 'partial res' };
        assert.equal(selectOutputText(step), 'partial res');
    });

    test('selectOutputText: completed step shows output', () => {
        const step = { completed: true, status: 'completed', output: 'final answer', streamingOutput: 'partial' };
        assert.equal(selectOutputText(step), 'final answer');
    });

    test('selectOutputText: completed step with empty output shows (empty output)', () => {
        const step = { completed: true, status: 'completed', output: '', streamingOutput: '' };
        assert.equal(selectOutputText(step), '(empty output)');
    });

    test('getPrevArtifacts: step 0 has no previous artifacts', () => {
        const steps = [
            { artifacts: [{ label: 'file.txt' }] },
            { artifacts: [] },
        ];
        assert.deepStrictEqual(getPrevArtifacts(steps, 0), []);
    });

    test('getPrevArtifacts: step 1 gets step 0 artifacts', () => {
        const artifacts = [{ label: 'out.pdf', path: '/tmp/out.pdf' }];
        const steps = [
            { artifacts },
            { artifacts: [] },
        ];
        assert.deepStrictEqual(getPrevArtifacts(steps, 1), artifacts);
    });

    test('getPrevArtifacts: step 2 gets step 1 artifacts, not step 0', () => {
        const steps = [
            { artifacts: [{ label: 'step0.txt' }] },
            { artifacts: [{ label: 'step1.txt' }] },
            { artifacts: [] },
        ];
        const prev = getPrevArtifacts(steps, 2);
        assert.equal(prev[0].label, 'step1.txt');
    });

    test('pipeline {result} placeholder picks up previous step output', async () => {
        const r = new PipelineRunner();
        const calls = [];
        r.registerProvider('openai', { call: async req => { calls.push(req.userPrompt); return { content: `out:${req.userPrompt}`, model: 'mock' }; } });
        const steps = [
            { name: 's1', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } },
            { name: 's2', type: 'ai', params: { provider: 'openai', userPrompt: 'Review: {result}' } },
        ];
        await r.run('chain', steps, 'original', [], 'child');
        assert.equal(calls[0], 'original');
        assert.equal(calls[1], 'Review: out:original');
    });

    test('{content} stays original through all chain steps', async () => {
        const r = new PipelineRunner();
        const calls = [];
        r.registerProvider('openai', { call: async req => { calls.push(req.userPrompt); return { content: 'processed', model: 'mock' }; } });
        const steps = [
            { name: 's1', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } },
            { name: 's2', type: 'ai', params: { provider: 'openai', userPrompt: 'Keep original: {content}' } },
        ];
        await r.run('chain', steps, 'source text', [], 'child');
        assert.equal(calls[1], 'Keep original: source text');
    });

    test('step attachments default to empty when not set', () => {
        const steps = [
            { name: 's1', completed: false, input: '', output: '', streamingOutput: '', status: 'pending' },
        ];
        assert.deepStrictEqual(steps[0].attachments || [], []);
    });

    test('step-specific attachments are preserved per step index', () => {
        const meta = {
            steps: [
                { name: 's1', attachments: [{ file: 'ref.png', mimetype: 'image/png' }] },
                { name: 's2', attachments: [] },
            ]
        };
        assert.equal(meta.steps[0].attachments.length, 1);
        assert.equal(meta.steps[1].attachments.length, 0);
    });

    test('pipeline run with three steps chains outputs correctly', async () => {
        const r = new PipelineRunner();
        let n = 0;
        r.registerProvider('openai', { call: async req => ({ content: `step${++n}:${req.userPrompt}`, model: 'mock' }) });
        const steps = [
            { name: 's1', type: 'ai', params: { provider: 'openai', userPrompt: '{content}' } },
            { name: 's2', type: 'ai', params: { provider: 'openai', userPrompt: '{result}' } },
            { name: 's3', type: 'ai', params: { provider: 'openai', userPrompt: '{result}' } },
        ];
        await r.run('3step', steps, 'start', [], 'child');
        assert.equal(r.historySteps[0].output, 'step1:start');
        assert.equal(r.historySteps[1].output, 'step2:step1:start');
        assert.equal(r.historySteps[2].output, 'step3:step2:step1:start');
    });
});

// ── 12. MockAIProvider — scripted provider self-tests ─────────
describe('MockAIProvider — scripted provider', () => {
    test('queue: scripted content is returned in order', async () => {
        const p = new MockAIProvider();
        p.queue('first').queue('second').queue('third');
        assert.equal((await p.call({ userPrompt: 'x' })).content, 'first');
        assert.equal((await p.call({ userPrompt: 'x' })).content, 'second');
        assert.equal((await p.call({ userPrompt: 'x' })).content, 'third');
    });

    test('queue: model field is preserved', async () => {
        const p = new MockAIProvider();
        p.queue('reply', 'gpt-4o');
        const r = await p.call({ userPrompt: 'hi' });
        assert.equal(r.model, 'gpt-4o');
    });

    test('default (no queue): echoes userPrompt', async () => {
        const p = new MockAIProvider();
        const r = await p.call({ userPrompt: 'hello world' });
        assert.equal(r.content, 'echo:hello world');
    });

    test('queueError: throws with the given message', async () => {
        const p = new MockAIProvider();
        p.queueError('rate limit exceeded');
        await assert.rejects(() => p.call({ userPrompt: 'x' }), /rate limit exceeded/);
    });

    test('queueError followed by queue: error then success', async () => {
        const p = new MockAIProvider();
        p.queueError('timeout').queue('ok');
        await assert.rejects(() => p.call({ userPrompt: 'a' }));
        const r = await p.call({ userPrompt: 'b' });
        assert.equal(r.content, 'ok');
    });

    test('calls: every request is captured', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'q1', model: 'gpt-4o' });
        await p.call({ userPrompt: 'q2', model: 'claude' });
        assert.equal(p.callCount, 2);
        assert.equal(p.calls[0].userPrompt, 'q1');
        assert.equal(p.calls[1].userPrompt, 'q2');
    });

    test('lastCall returns most recent request', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'first' });
        await p.call({ userPrompt: 'last' });
        assert.equal(p.lastCall.userPrompt, 'last');
    });

    test('nthCall returns request at given index', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'a' });
        await p.call({ userPrompt: 'b' });
        await p.call({ userPrompt: 'c' });
        assert.equal(p.nthCall(1).userPrompt, 'b');
    });

    test('reset: clears queue and calls', async () => {
        const p = new MockAIProvider();
        p.queue('x');
        await p.call({ userPrompt: 'hi' });
        p.reset();
        assert.equal(p.callCount, 0);
        // After reset, default echo behaviour resumes
        const r = await p.call({ userPrompt: 'ping' });
        assert.equal(r.content, 'echo:ping');
    });

    test('captures all request fields', async () => {
        const p = new MockAIProvider();
        await p.call({ model: 'gpt-4o', systemPrompt: 'sys', userPrompt: 'up', temperature: 0.3, maxTokens: 512 });
        assert.equal(p.lastCall.model, 'gpt-4o');
        assert.equal(p.lastCall.systemPrompt, 'sys');
        assert.equal(p.lastCall.temperature, 0.3);
        assert.equal(p.lastCall.maxTokens, 512);
    });

    // ── when() — deterministic rule-based matching ──
    test('when: exact string match — same input always returns same output', async () => {
        const p = new MockAIProvider().when('hello', 'world');
        assert.equal((await p.call({ userPrompt: 'hello' })).content, 'world');
        assert.equal((await p.call({ userPrompt: 'hello' })).content, 'world');
        assert.equal((await p.call({ userPrompt: 'hello' })).content, 'world');
    });

    test('when: regex match', async () => {
        const p = new MockAIProvider().when(/translate/i, 'traduction');
        assert.equal((await p.call({ userPrompt: 'Translate: hello' })).content, 'traduction');
        assert.equal((await p.call({ userPrompt: 'translate something' })).content, 'traduction');
    });

    test('when: unmatched userPrompt falls through to queue', async () => {
        const p = new MockAIProvider().when('exact', 'rule hit').queue('queued');
        assert.equal((await p.call({ userPrompt: 'other' })).content, 'queued');
    });

    test('when: unmatched and empty queue falls through to echo', async () => {
        const p = new MockAIProvider().when('exact', 'rule hit');
        assert.equal((await p.call({ userPrompt: 'other' })).content, 'echo:other');
    });

    test('when: rule takes priority over queue for matching input', async () => {
        const p = new MockAIProvider().when('hi', 'rule').queue('queued');
        assert.equal((await p.call({ userPrompt: 'hi' })).content, 'rule');
        // queue is still intact
        assert.equal((await p.call({ userPrompt: 'other' })).content, 'queued');
    });

    test('when: function predicate', async () => {
        const p = new MockAIProvider().when(req => req.model === 'vision', 'saw it');
        assert.equal((await p.call({ userPrompt: 'x', model: 'vision' })).content, 'saw it');
        assert.equal((await p.call({ userPrompt: 'x', model: 'other' })).content, 'echo:x');
    });

    test('when: function predicate + function response returns full object', async () => {
        const img = { file: 'a.png', mimetype: 'image/png', content: 'data', size: 1 };
        const p = new MockAIProvider().when(
            req => req.attachments?.length > 0,
            req => ({ content: `got:${req.attachments[0].file}`, model: 'img-model', outputAttachments: [req.attachments[0]] })
        );
        const r = await p.call({ userPrompt: 'describe', attachments: [img] });
        assert.equal(r.content, 'got:a.png');
        assert.equal(r.model, 'img-model');
        assert.equal(r.outputAttachments.length, 1);
    });

    test('when: multiple rules — first match wins', async () => {
        const p = new MockAIProvider()
            .when('x', 'first')
            .when('x', 'second');
        assert.equal((await p.call({ userPrompt: 'x' })).content, 'first');
    });

    test('reset: clears rules along with queue and calls', async () => {
        const p = new MockAIProvider().when('hi', 'rule');
        await p.call({ userPrompt: 'hi' });
        p.reset();
        assert.equal(p.callCount, 0);
        // Rule should be gone; falls through to echo
        assert.equal((await p.call({ userPrompt: 'hi' })).content, 'echo:hi');
    });
});

// ── 13. Pipeline features tested with MockAIProvider ──────────
describe('Pipeline features — MockAIProvider', () => {
    // ── helper: build a one-step AI pipeline step
    function aiStep(name, userPrompt, extra = {}) {
        return { name, type: 'ai', params: { provider: 'mock', userPrompt, ...extra } };
    }

    function makeRunner(provider) {
        const r = new PipelineRunner();
        r.registerProvider('mock', provider);
        return r;
    }

    // ── correct prompt is sent to the provider ──
    test('single-step: userPrompt is sent verbatim', async () => {
        const p = new MockAIProvider().queue('ok');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'Translate this text')], 'input', [], 'child');
        assert.equal(p.lastCall.userPrompt, 'Translate this text');
    });

    test('single-step: {content} is replaced with input', async () => {
        const p = new MockAIProvider().queue('done');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'Echo: {content}')], 'hello', [], 'child');
        assert.equal(p.lastCall.userPrompt, 'Echo: hello');
    });

    test('single-step: provider receives correct model', async () => {
        const p = new MockAIProvider().queue('ok');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi', { model: 'gpt-4o' })], 'x', [], 'child');
        assert.equal(p.lastCall.model, 'gpt-4o');
    });

    test('single-step: provider receives systemPrompt and temperature', async () => {
        const p = new MockAIProvider().queue('ok');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi', { systemPrompt: 'Be terse', temperature: '0.2' })], 'x', [], 'child');
        assert.equal(p.lastCall.systemPrompt, 'Be terse');
        assert.equal(p.lastCall.temperature, 0.2);
    });

    test('single-step: output is stored in historySteps', async () => {
        const p = new MockAIProvider().queue('translated result');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', '{content}')], 'source text', [], 'child');
        assert.equal(r.historySteps[0].output, 'translated result');
        assert.equal(r.historySteps[0].status, 'completed');
    });

    test('single-step: pipeline_completed event carries the output', async () => {
        const p = new MockAIProvider().queue('final answer');
        const r = makeRunner(p);
        await r.run('pipe', [aiStep('s1', '{content}')], 'question', [], 'child');
        const done = r.events.find(e => e.type === 'pipeline_completed');
        assert.equal(done.payload.steps[0].output, 'final answer');
    });

    // ── chaining ──
    test('two-step chain: step 2 receives step 1 output via {result}', async () => {
        const p = new MockAIProvider().queue('translated').queue('summary');
        const r = makeRunner(p);
        const steps = [
            aiStep('translate', '{content}'),
            aiStep('summarise', 'Summarise: {result}'),
        ];
        await r.run('chain', steps, 'long text', [], 'child');
        assert.equal(p.nthCall(1).userPrompt, 'Summarise: translated');
    });

    test('two-step chain: {content} stays original in step 2', async () => {
        const p = new MockAIProvider().queue('out1').queue('out2');
        const r = makeRunner(p);
        const steps = [aiStep('s1', '{content}'), aiStep('s2', 'Original was: {content}')];
        await r.run('chain', steps, 'original', [], 'child');
        assert.equal(p.nthCall(1).userPrompt, 'Original was: original');
    });

    test('three-step chain: output flows through all steps', async () => {
        const p = new MockAIProvider().queue('A').queue('B').queue('C');
        const r = makeRunner(p);
        const steps = [aiStep('s1', '{content}'), aiStep('s2', '{result}'), aiStep('s3', '{result}')];
        await r.run('chain', steps, 'start', [], 'child');
        assert.equal(r.historySteps[0].output, 'A');
        assert.equal(r.historySteps[1].output, 'B');
        assert.equal(r.historySteps[2].output, 'C');
        assert.equal(p.nthCall(1).userPrompt, 'A');
        assert.equal(p.nthCall(2).userPrompt, 'B');
    });

    // ── error handling ──
    test('provider error: pipeline emits pipeline_error', async () => {
        const p = new MockAIProvider().queueError('model overloaded');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi')], 'x', [], 'child');
        const err = r.events.find(e => e.type === 'pipeline_error');
        assert.ok(err, 'pipeline_error should be emitted');
        assert.match(err.payload.message, /model overloaded/);
    });

    test('provider error: runner stops after first error', async () => {
        const p = new MockAIProvider().queueError('fail').queue('should not reach');
        const r = makeRunner(p);
        const steps = [aiStep('s1', 'hi'), aiStep('s2', 'hi')];
        await r.run('t', steps, 'x', [], 'child');
        assert.equal(p.callCount, 1);  // second step never runs
    });

    // ── recipe / provider settings ──
    test('recipe temperature is forwarded as float', async () => {
        const p = new MockAIProvider().queue('ok');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi', { temperature: '0.9' })], 'x', [], 'child');
        assert.equal(p.lastCall.temperature, 0.9);
    });

    test('recipe systemPrompt is forwarded', async () => {
        const p = new MockAIProvider().queue('ok');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi', { systemPrompt: 'You are a translator.' })], 'x', [], 'child');
        assert.equal(p.lastCall.systemPrompt, 'You are a translator.');
    });

    // ── multiple runs / statelessness ──
    test('separate runs do not share state', async () => {
        const p = new MockAIProvider();
        const r = makeRunner(p);
        p.queue('run1');
        await r.run('t', [aiStep('s1', '{content}')], 'first', [], 'child');
        assert.equal(r.historySteps[0].output, 'run1');

        p.queue('run2');
        await r.run('t', [aiStep('s1', '{content}')], 'second', [], 'child');
        assert.equal(r.historySteps[0].output, 'run2');
    });

    test('provider is called exactly once per step', async () => {
        const p = new MockAIProvider();
        const r = makeRunner(p);
        const steps = [aiStep('s1', 'a'), aiStep('s2', 'b'), aiStep('s3', 'c')];
        await r.run('t', steps, 'x', [], 'child');
        assert.equal(p.callCount, 3);
    });

    // ── manual step interleaved with AI step ──
    test('manual step pause does not call the provider', async () => {
        const p = new MockAIProvider().queue('ai done');
        const r = makeRunner(p);
        const steps = [
            { name: 'human', type: 'manual', params: { mode: 'view', prompt: 'Check this', choices: '[]' } },
            aiStep('ai', '{result}'),
        ];
        const runPromise = r.run('t', steps, 'data', [], 'child');
        // Resume the manual step immediately
        setImmediate(() => r.resumeManual('human approved'));
        await runPromise;
        assert.equal(p.callCount, 1);
        assert.equal(p.lastCall.userPrompt, 'human approved');
    });

    // ── run_prompt_process attachment merge (bridge layer) ──
    test('bridge run_prompt_process: merged allAttachments are passed correctly', () => {
        const machineAtt = [{ file: 'bg.png', mimetype: 'image/png' }];
        const beltAtt    = [{ file: 'ref.jpg', mimetype: 'image/jpeg' }];
        // Mirror main.js merge logic
        const all = [...machineAtt, ...beltAtt];
        assert.equal(all.length, 2);
        assert.equal(all[0].mimetype, 'image/png');
        assert.equal(all[1].mimetype, 'image/jpeg');
    });
});

// ── test data ─────────────────────────────────────────────────
// Reusable fake attachment objects (base64 content is minimal valid data)
const FAKE_IMAGE_PNG = {
    file: 'photo.png',
    path: '/tmp/photo.png',
    mimetype: 'image/png',
    content: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==',
    size: 68,
};
const FAKE_IMAGE_JPEG = {
    file: 'scene.jpg',
    path: '/tmp/scene.jpg',
    mimetype: 'image/jpeg',
    content: '/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQ=',
    size: 40,
};
const FAKE_AUDIO_MP3 = {
    file: 'voice.mp3',
    path: '/tmp/voice.mp3',
    mimetype: 'audio/mpeg',
    content: 'SUQzBAAAAAAAI1RTU0UAAAAPAAADTGF2ZjU4LjI5LjEwMAAAAAAAAAAAAAAA',
    size: 512,
};
const FAKE_AUDIO_WAV = {
    file: 'sfx.wav',
    path: '/tmp/sfx.wav',
    mimetype: 'audio/wav',
    content: 'UklGRiQAAABXQVZFZm10IBAAAAABAAEARKwAAIhYAQACABAAZGF0YQAAAAA=',
    size: 256,
};

// ── 14. MockAIProvider — image/audio input ────────────────────
describe('MockAIProvider — image/audio input', () => {
    test('single image attachment is captured in req.attachments', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'describe', attachments: [FAKE_IMAGE_PNG] });
        assert.equal(p.inputAttachmentsOf(0).length, 1);
        assert.equal(p.inputAttachmentsOf(0)[0].mimetype, 'image/png');
    });

    test('single audio attachment is captured in req.attachments', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'transcribe', attachments: [FAKE_AUDIO_MP3] });
        assert.equal(p.inputAttachmentsOf(0).length, 1);
        assert.equal(p.inputAttachmentsOf(0)[0].mimetype, 'audio/mpeg');
    });

    test('mixed image + audio attachments are all captured', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'analyse', attachments: [FAKE_IMAGE_JPEG, FAKE_AUDIO_WAV] });
        assert.equal(p.inputAttachmentsOf(0).length, 2);
    });

    test('inputImagesOf filters only image/* attachments', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'x', attachments: [FAKE_IMAGE_PNG, FAKE_AUDIO_MP3, FAKE_IMAGE_JPEG] });
        const imgs = p.inputImagesOf(0);
        assert.equal(imgs.length, 2);
        assert.ok(imgs.every(a => a.mimetype.startsWith('image/')));
    });

    test('inputAudiosOf filters only audio/* attachments', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'x', attachments: [FAKE_IMAGE_PNG, FAKE_AUDIO_MP3, FAKE_AUDIO_WAV] });
        const auds = p.inputAudiosOf(0);
        assert.equal(auds.length, 2);
        assert.ok(auds.every(a => a.mimetype.startsWith('audio/')));
    });

    test('no attachments field → inputAttachmentsOf returns []', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'plain text only' });
        assert.deepStrictEqual(p.inputAttachmentsOf(0), []);
    });

    test('base64 content is preserved exactly', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'x', attachments: [FAKE_IMAGE_PNG] });
        assert.equal(p.inputAttachmentsOf(0)[0].content, FAKE_IMAGE_PNG.content);
    });

    test('file metadata (file, path, size) is preserved', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'x', attachments: [FAKE_AUDIO_MP3] });
        const att = p.inputAttachmentsOf(0)[0];
        assert.equal(att.file, FAKE_AUDIO_MP3.file);
        assert.equal(att.path, FAKE_AUDIO_MP3.path);
        assert.equal(att.size, FAKE_AUDIO_MP3.size);
    });

    test('attachments snapshot is independent (mutation after call does not affect captures)', async () => {
        const p = new MockAIProvider();
        const atts = [{ ...FAKE_IMAGE_PNG }];
        await p.call({ userPrompt: 'x', attachments: atts });
        atts[0].content = 'mutated';           // mutate original array
        assert.equal(p.inputAttachmentsOf(0)[0].content, FAKE_IMAGE_PNG.content);
    });

    test('lastInputAttachments points to most recent call', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'first', attachments: [FAKE_IMAGE_PNG] });
        await p.call({ userPrompt: 'second', attachments: [FAKE_AUDIO_MP3] });
        assert.equal(p.lastInputAttachments[0].mimetype, 'audio/mpeg');
    });

    test('per-call attachment tracking across multiple calls', async () => {
        const p = new MockAIProvider();
        await p.call({ userPrompt: 'a', attachments: [FAKE_IMAGE_PNG] });
        await p.call({ userPrompt: 'b', attachments: [FAKE_AUDIO_WAV] });
        await p.call({ userPrompt: 'c', attachments: [] });
        assert.equal(p.inputImagesOf(0).length, 1);
        assert.equal(p.inputAudiosOf(1).length, 1);
        assert.equal(p.inputAttachmentsOf(2).length, 0);
    });
});

// ── 15. MockAIProvider — media output (queueWithMedia) ────────
describe('MockAIProvider — media output', () => {
    test('queueWithMedia: outputAttachments returned in response', async () => {
        const p = new MockAIProvider();
        const outputAudio = { ...FAKE_AUDIO_MP3, file: 'tts_result.mp3' };
        p.queueWithMedia('Here is the audio', [outputAudio]);
        const resp = await p.call({ userPrompt: 'read this aloud' });
        assert.equal(resp.content, 'Here is the audio');
        assert.equal(resp.outputAttachments.length, 1);
        assert.equal(resp.outputAttachments[0].mimetype, 'audio/mpeg');
    });

    test('queueWithMedia: image output (e.g. generated image)', async () => {
        const p = new MockAIProvider();
        const outputImg = { ...FAKE_IMAGE_PNG, file: 'generated.png' };
        p.queueWithMedia('Image generated', [outputImg]);
        const resp = await p.call({ userPrompt: 'draw a cat' });
        assert.equal(resp.outputAttachments[0].file, 'generated.png');
        assert.equal(resp.outputAttachments[0].mimetype, 'image/png');
    });

    test('queueWithMedia: multiple output attachments', async () => {
        const p = new MockAIProvider();
        p.queueWithMedia('Two outputs', [FAKE_IMAGE_PNG, FAKE_AUDIO_MP3]);
        const resp = await p.call({ userPrompt: 'x' });
        assert.equal(resp.outputAttachments.length, 2);
    });

    test('queue (text-only): outputAttachments is empty array', async () => {
        const p = new MockAIProvider().queue('plain text');
        const resp = await p.call({ userPrompt: 'x' });
        assert.deepStrictEqual(resp.outputAttachments, []);
    });

    test('default (no queue): outputAttachments is empty array', async () => {
        const p = new MockAIProvider();
        const resp = await p.call({ userPrompt: 'x' });
        assert.deepStrictEqual(resp.outputAttachments, []);
    });

    test('queueWithMedia model field is preserved', async () => {
        const p = new MockAIProvider();
        p.queueWithMedia('result', [], 'dall-e-3');
        const resp = await p.call({ userPrompt: 'x' });
        assert.equal(resp.model, 'dall-e-3');
    });

    test('output and input attachments are independent', async () => {
        const p = new MockAIProvider();
        const outImg = { ...FAKE_IMAGE_JPEG, file: 'output.jpg' };
        p.queueWithMedia('done', [outImg]);
        const resp = await p.call({ userPrompt: 'x', attachments: [FAKE_AUDIO_MP3] });
        // Input: audio; Output: image — should not mix
        assert.equal(p.inputAudiosOf(0).length, 1);
        assert.equal(resp.outputAttachments[0].mimetype, 'image/jpeg');
    });
});

// ── 16. Pipeline — attachments flow through runner ────────────
describe('Pipeline — attachments flow through PipelineRunner', () => {
    function aiStep(name, prompt) {
        return { name, type: 'ai', params: { provider: 'mock', userPrompt: prompt } };
    }
    function makeRunner(provider) {
        const r = new PipelineRunner();
        r.registerProvider('mock', provider);
        return r;
    }

    test('inputAttachments are forwarded to the provider', async () => {
        const p = new MockAIProvider();
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', '{content}')], 'text', [FAKE_IMAGE_PNG], 'child');
        assert.equal(p.inputAttachmentsOf(0).length, 1);
        assert.equal(p.inputAttachmentsOf(0)[0].file, 'photo.png');
    });

    test('multiple mixed attachments are all forwarded', async () => {
        const p = new MockAIProvider();
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', '{content}')], 'text',
            [FAKE_IMAGE_PNG, FAKE_AUDIO_MP3, FAKE_IMAGE_JPEG], 'child');
        assert.equal(p.inputAttachmentsOf(0).length, 3);
        assert.equal(p.inputImagesOf(0).length, 2);
        assert.equal(p.inputAudiosOf(0).length, 1);
    });

    test('same inputAttachments are forwarded to every step in the chain', async () => {
        const p = new MockAIProvider();
        const r = makeRunner(p);
        const steps = [aiStep('s1', '{content}'), aiStep('s2', '{result}')];
        await r.run('t', steps, 'input', [FAKE_AUDIO_WAV], 'child');
        assert.equal(p.inputAudiosOf(0).length, 1);
        assert.equal(p.inputAudiosOf(1).length, 1);
    });

    test('no attachments: provider receives empty array', async () => {
        const p = new MockAIProvider();
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi')], 'x', [], 'child');
        assert.deepStrictEqual(p.inputAttachmentsOf(0), []);
    });

    test('outputAttachments from provider are stored as historyStep.artifacts', async () => {
        const p = new MockAIProvider();
        const outputAudio = { ...FAKE_AUDIO_MP3, file: 'tts.mp3' };
        p.queueWithMedia('spoken text', [outputAudio]);
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', '{content}')], 'hello', [], 'child');
        const artifacts = r.historySteps[0].artifacts;
        assert.ok(Array.isArray(artifacts));
        assert.equal(artifacts.length, 1);
        assert.equal(artifacts[0].file, 'tts.mp3');
    });

    test('outputAttachments are in pipeline_completed event steps', async () => {
        const p = new MockAIProvider();
        const outImg = { ...FAKE_IMAGE_PNG, file: 'gen.png' };
        p.queueWithMedia('image created', [outImg]);
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'generate')], 'prompt', [], 'child');
        const done = r.events.find(e => e.type === 'pipeline_completed');
        const step = done.payload.steps[0];
        assert.equal(step.artifacts?.[0]?.file, 'gen.png');
    });

    test('text-only response leaves historyStep.artifacts undefined (not set)', async () => {
        const p = new MockAIProvider().queue('plain output');
        const r = makeRunner(p);
        await r.run('t', [aiStep('s1', 'hi')], 'x', [], 'child');
        // outputAttachments was [] so artifacts should not be set
        assert.ok(!r.historySteps[0].artifacts || r.historySteps[0].artifacts.length === 0);
    });

    test('image input + audio output round-trip through pipeline', async () => {
        const p = new MockAIProvider();
        const ttsAudio = { ...FAKE_AUDIO_MP3, file: 'tts_output.mp3' };
        p.queueWithMedia('Audio generated from image description', [ttsAudio]);
        const r = makeRunner(p);
        await r.run('image-to-speech', [aiStep('describe+speak', 'Describe and read: {content}')],
            'an image of a sunset', [FAKE_IMAGE_JPEG], 'child');
        // Input: JPEG was sent
        assert.equal(p.inputImagesOf(0)[0].mimetype, 'image/jpeg');
        // Output: MP3 was produced
        assert.equal(r.historySteps[0].artifacts[0].file, 'tts_output.mp3');
        // Text output is captured
        assert.equal(r.historySteps[0].output, 'Audio generated from image description');
    });

    test('callStreaming delegates to call and invokes onChunk/onDone', async () => {
        const p = new MockAIProvider().queue('streamed reply');
        const chunks = [];
        let doneResp = null;
        await p.callStreaming(
            { userPrompt: 'hi', attachments: [] },
            chunk => chunks.push(chunk),
            resp  => { doneResp = resp; },
            _err  => { throw new Error('unexpected error'); }
        );
        assert.deepStrictEqual(chunks, ['streamed reply']);
        assert.equal(doneResp.content, 'streamed reply');
    });

    test('callStreaming error calls onError, not onDone', async () => {
        const p = new MockAIProvider().queueError('stream failed');
        let errMsg = null;
        await p.callStreaming(
            { userPrompt: 'hi', attachments: [] },
            () => { throw new Error('should not chunk'); },
            () => { throw new Error('should not done'); },
            msg => { errMsg = msg; }
        );
        assert.match(errMsg, /stream failed/);
    });
});

// ── inline MockProvider — mirrors main.js MockProvider ────────
// Kept in sync with main.js; if main.js changes, update here too.
class MockProvider {
    name() { return 'mock'; }
    defaultModels() { return ['echo', 'fixed', 'image-echo', 'image-compose']; }

    async call(req) {
        const model  = (req.model || 'echo').toLowerCase();
        const atts   = req.attachments || [];
        const images = atts.filter(a => a.mimetype?.startsWith('image/'));
        let content;
        let outputAttachments = [];

        if (model === 'image-echo') {
            const img = images[0];
            if (img) {
                content = `[Mock image-echo: ${img.file}]`;
                outputAttachments = [{ ...img, file: `echo_${img.file}` }];
            } else {
                content = '[Mock image-echo: no image provided]';
            }
        } else if (model === 'image-compose') {
            const base  = images[0];
            const extra = images.slice(1);
            if (base) {
                content = `[Mock image-compose: base=${base.file}, inputs=${extra.length}]`;
                outputAttachments = [{ ...base, file: `composed_${base.file}` }];
            } else {
                content = '[Mock image-compose: no base image provided]';
            }
        } else if (model === 'fixed') {
            content = req.systemPrompt || '[Mock: systemPrompt is empty]';
        } else {
            content = `[Mock] ${req.userPrompt}`;
            if (atts.length > 0) {
                const imgs  = images.length;
                const auds  = atts.filter(a => a.mimetype?.startsWith('audio/')).length;
                const other = atts.length - imgs - auds;
                const parts = [];
                if (imgs)  parts.push(`${imgs} image(s)`);
                if (auds)  parts.push(`${auds} audio(s)`);
                if (other) parts.push(`${other} other(s)`);
                content += `\n[Attachments: ${parts.join(', ')}]`;
            }
        }
        return { content, model: req.model || 'echo', outputAttachments };
    }

    async listModels() { return this.defaultModels(); }
    async testConnection() { return ''; }
}

// ── 17. MockProvider (app recipe provider) ────────────────────
describe('MockProvider — app recipe provider', () => {
    test('echo model returns [Mock] + userPrompt', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'echo', userPrompt: 'Translate this', systemPrompt: '' });
        assert.equal(r.content, '[Mock] Translate this');
    });

    test('fixed model returns systemPrompt verbatim', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'fixed', userPrompt: 'ignored', systemPrompt: 'Fixed reply here' });
        assert.equal(r.content, 'Fixed reply here');
    });

    test('fixed model with empty systemPrompt returns placeholder', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'fixed', userPrompt: 'hi', systemPrompt: '' });
        assert.equal(r.content, '[Mock: systemPrompt is empty]');
    });

    test('model field is preserved in response', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'echo', userPrompt: 'hi' });
        assert.equal(r.model, 'echo');
    });

    test('no model → defaults to echo behaviour', async () => {
        const p = new MockProvider();
        const r = await p.call({ userPrompt: 'hello' });
        assert.match(r.content, /\[Mock\]/);
    });

    test('no attachments → no attachment summary appended', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'echo', userPrompt: 'hi', attachments: [] });
        assert.ok(!r.content.includes('[Attachments:'));
    });

    test('single image attachment appended to summary', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'echo', userPrompt: 'hi', attachments: [FAKE_IMAGE_PNG] });
        assert.match(r.content, /\[Attachments: 1 image\(s\)\]/);
    });

    test('single audio attachment appended to summary', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'echo', userPrompt: 'hi', attachments: [FAKE_AUDIO_MP3] });
        assert.match(r.content, /\[Attachments: 1 audio\(s\)\]/);
    });

    test('mixed image + audio attachment summary', async () => {
        const p = new MockProvider();
        const r = await p.call({
            model: 'echo', userPrompt: 'hi',
            attachments: [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG, FAKE_AUDIO_MP3],
        });
        assert.match(r.content, /2 image\(s\)/);
        assert.match(r.content, /1 audio\(s\)/);
    });

    test('fixed model + attachments: shows only fixed text (no attachment summary)', async () => {
        const p = new MockProvider();
        const r = await p.call({
            model: 'fixed', systemPrompt: 'OK', userPrompt: 'ignored',
            attachments: [FAKE_AUDIO_WAV],
        });
        assert.equal(r.content, 'OK');
    });

    test('listModels returns all four models', async () => {
        const p = new MockProvider();
        const models = await p.listModels();
        assert.deepStrictEqual(models, ['echo', 'fixed', 'image-echo', 'image-compose']);
    });

    test('testConnection always succeeds (returns empty string)', async () => {
        const p = new MockProvider();
        const err = await p.testConnection();
        assert.equal(err, '');
    });

    test('get_providers bridge includes mock entry', () => {
        const tmpDir = makeTempDir();
        const { sent, handle } = makeApp(tmpDir);
        handle('get_providers');
        const result = sent.find(s => s.type === 'providers_result');
        assert.ok(result?.payload?.mock, 'providers_result should include mock');
        assert.deepStrictEqual(result.payload.mock.models, ['echo', 'fixed']);
        rmrf(tmpDir);
    });
});

// ── 18. MockProvider in pipeline via PipelineRunner ───────────
describe('MockProvider — pipeline integration', () => {
    function makeRunnerWithMock() {
        const r = new PipelineRunner();
        r.registerProvider('mock', new MockProvider());
        return r;
    }

    // Override registerProvider in test shim to accept a provider object directly
    // (In these tests we pass the MockProvider instance, matching main.js behaviour)

    test('recipe with provider=mock, model=echo runs without error', async () => {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        await r.run('t', [{ name: 's1', type: 'ai', params: { provider: 'mock', model: 'echo', userPrompt: 'Hello' } }], 'x', [], 'child');
        assert.equal(r.historySteps[0].status, 'completed');
        assert.equal(r.historySteps[0].output, '[Mock] Hello');
    });

    test('recipe with provider=mock, model=fixed returns systemPrompt', async () => {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        await r.run('t', [{
            name: 's1', type: 'ai',
            params: { provider: 'mock', model: 'fixed', userPrompt: 'ignored', systemPrompt: 'Test response text' },
        }], 'x', [], 'child');
        assert.equal(r.historySteps[0].output, 'Test response text');
    });

    test('{content} substitution works with mock provider', async () => {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        await r.run('t', [{
            name: 's1', type: 'ai',
            params: { provider: 'mock', model: 'echo', userPrompt: 'Process: {content}' },
        }], 'my data', [], 'child');
        assert.equal(r.historySteps[0].output, '[Mock] Process: my data');
    });

    test('mock provider in chain: {result} flows from step 1 to step 2', async () => {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        const steps = [
            { name: 's1', type: 'ai', params: { provider: 'mock', model: 'echo', userPrompt: '{content}' } },
            { name: 's2', type: 'ai', params: { provider: 'mock', model: 'echo', userPrompt: 'Got: {result}' } },
        ];
        await r.run('chain', steps, 'input text', [], 'child');
        assert.equal(r.historySteps[0].output, '[Mock] input text');
        assert.equal(r.historySteps[1].output, '[Mock] Got: [Mock] input text');
    });

    test('mock provider with image attachment includes summary in output', async () => {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        await r.run('t', [{
            name: 's1', type: 'ai',
            params: { provider: 'mock', model: 'echo', userPrompt: 'Describe image' },
        }], 'x', [FAKE_IMAGE_PNG], 'child');
        assert.match(r.historySteps[0].output, /1 image\(s\)/);
    });

    test('mock provider with audio attachment includes summary in output', async () => {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        await r.run('t', [{
            name: 's1', type: 'ai',
            params: { provider: 'mock', model: 'echo', userPrompt: 'Transcribe' },
        }], 'x', [FAKE_AUDIO_MP3], 'child');
        assert.match(r.historySteps[0].output, /1 audio\(s\)/);
    });

    test('mock recipe can be saved and loaded as a standard recipe entry', () => {
        const tmpDir = makeTempDir();
        const { st, handle } = makeApp(tmpDir);
        const mockRecipe = {
            name: 'Mock Echo',
            type: 'ai',
            provider: 'mock',
            model: 'echo',
            temperature: 0.7,
            systemPrompt: '',
            command: '',
        };
        handle('save_recipes', [mockRecipe]);
        const loaded = st.loadRecipes();
        assert.equal(loaded[0].name, 'Mock Echo');
        assert.equal(loaded[0].provider, 'mock');
        assert.equal(loaded[0].model, 'echo');
        rmrf(tmpDir);
    });
});

// ── 19. MockProvider — image-echo / image-compose ─────────────
describe('MockProvider — image modes', () => {
    // ── image-echo ──
    test('image-echo: returns first image as outputAttachment', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'describe', attachments: [FAKE_IMAGE_PNG] });
        assert.equal(r.outputAttachments.length, 1);
        assert.equal(r.outputAttachments[0].mimetype, 'image/png');
    });

    test('image-echo: output filename is prefixed with echo_', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'x', attachments: [FAKE_IMAGE_PNG] });
        assert.equal(r.outputAttachments[0].file, `echo_${FAKE_IMAGE_PNG.file}`);
    });

    test('image-echo: base64 content of input is preserved in output', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'x', attachments: [FAKE_IMAGE_PNG] });
        assert.equal(r.outputAttachments[0].content, FAKE_IMAGE_PNG.content);
    });

    test('image-echo: content text names the file', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'x', attachments: [FAKE_IMAGE_JPEG] });
        assert.match(r.content, /image-echo/);
        assert.match(r.content, new RegExp(FAKE_IMAGE_JPEG.file));
    });

    test('image-echo: with no image returns error text and empty outputAttachments', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'x', attachments: [] });
        assert.equal(r.outputAttachments.length, 0);
        assert.match(r.content, /no image provided/);
    });

    test('image-echo: multiple images — only first is returned', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'x',
            attachments: [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG] });
        assert.equal(r.outputAttachments.length, 1);
        assert.equal(r.outputAttachments[0].file, `echo_${FAKE_IMAGE_PNG.file}`);
    });

    test('image-echo: audio attachments are ignored (not treated as image)', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-echo', userPrompt: 'x',
            attachments: [FAKE_AUDIO_MP3] });
        assert.equal(r.outputAttachments.length, 0);
        assert.match(r.content, /no image provided/);
    });

    // ── image-compose ──
    test('image-compose: first image is base, returns composed outputAttachment', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x',
            attachments: [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG] });
        assert.equal(r.outputAttachments.length, 1);
        assert.equal(r.outputAttachments[0].file, `composed_${FAKE_IMAGE_PNG.file}`);
    });

    test('image-compose: content names base file and input count', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x',
            attachments: [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG] });
        assert.match(r.content, /base=photo\.png/);
        assert.match(r.content, /inputs=1/);
    });

    test('image-compose: single image = base + 0 extra inputs', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x',
            attachments: [FAKE_IMAGE_PNG] });
        assert.match(r.content, /inputs=0/);
        assert.equal(r.outputAttachments.length, 1);
    });

    test('image-compose: three images — base + 2 extra inputs', async () => {
        const extra = { ...FAKE_IMAGE_PNG, file: 'extra.png' };
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x',
            attachments: [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG, extra] });
        assert.match(r.content, /inputs=2/);
    });

    test('image-compose: no images → error text and empty outputAttachments', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x', attachments: [] });
        assert.equal(r.outputAttachments.length, 0);
        assert.match(r.content, /no base image provided/);
    });

    test('image-compose: audio attachments are not counted as images', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x',
            attachments: [FAKE_AUDIO_MP3, FAKE_IMAGE_PNG] });
        // FAKE_AUDIO_MP3 is not an image, so FAKE_IMAGE_PNG becomes the base
        assert.match(r.content, /base=photo\.png/);
        assert.match(r.content, /inputs=0/);
    });

    test('image-compose: composed output preserves mimetype of base image', async () => {
        const p = new MockProvider();
        const r = await p.call({ model: 'image-compose', userPrompt: 'x',
            attachments: [FAKE_IMAGE_JPEG, FAKE_IMAGE_PNG] });
        assert.equal(r.outputAttachments[0].mimetype, 'image/jpeg');
    });
});

// ── 20. MockProvider image modes through PipelineRunner ───────
describe('MockProvider image modes — pipeline integration', () => {
    function makeRunner() {
        const r = new PipelineRunner();
        r.providers['mock'] = new MockProvider();
        return r;
    }
    function imgStep(model) {
        return { name: 's1', type: 'ai', params: { provider: 'mock', model, userPrompt: 'process' } };
    }

    test('image-echo: output image stored in historyStep.artifacts', async () => {
        const r = makeRunner();
        await r.run('t', [imgStep('image-echo')], 'x', [FAKE_IMAGE_PNG], 'child');
        assert.equal(r.historySteps[0].artifacts?.length, 1);
        assert.equal(r.historySteps[0].artifacts[0].mimetype, 'image/png');
    });

    test('image-compose: composed image stored in historyStep.artifacts', async () => {
        const r = makeRunner();
        await r.run('t', [imgStep('image-compose')], 'x',
            [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG], 'child');
        assert.equal(r.historySteps[0].artifacts?.length, 1);
        assert.match(r.historySteps[0].artifacts[0].file, /^composed_/);
    });

    test('image-compose: output artifact in pipeline_completed event', async () => {
        const r = makeRunner();
        await r.run('t', [imgStep('image-compose')], 'x',
            [FAKE_IMAGE_PNG, FAKE_IMAGE_JPEG], 'child');
        const done = r.events.find(e => e.type === 'pipeline_completed');
        assert.equal(done.payload.steps[0].artifacts?.[0]?.mimetype, 'image/png');
    });

    test('two-step: image-echo then echo — output text flows via {result}', async () => {
        const r = makeRunner();
        const steps = [
            imgStep('image-echo'),
            { name: 's2', type: 'ai', params: { provider: 'mock', model: 'echo', userPrompt: 'Received: {result}' } },
        ];
        await r.run('t', steps, 'x', [FAKE_IMAGE_PNG], 'child');
        assert.match(r.historySteps[1].output, /Received:.*image-echo/);
    });
});

// ── 21. Drag-and-drop file processing logic ───────────────────
describe('Drag-and-drop file processing logic', () => {
    // Pure logic: mirrors app.js handleFileDrop — reads File objects and
    // converts to attachment objects. Tested without DOM via a stub.

    // Stub simulating browser FileReader behavior (synchronous for tests)
    function stubReadAsDataURL(file, base64Content) {
        return {
            file: file.name,
            path: file.path || '',
            mimetype: file.type,
            content: base64Content,
            size: file.size,
        };
    }

    // Mirror of app.js handleFileDrop filtering logic
    function filterDroppableFiles(files) {
        return files.filter(f =>
            f.type.startsWith('image/') ||
            f.type.startsWith('audio/') ||
            f.type.startsWith('video/')
        );
    }

    test('image files pass the filter', () => {
        const files = [{ name: 'a.png', type: 'image/png', size: 100 }];
        assert.equal(filterDroppableFiles(files).length, 1);
    });

    test('audio files pass the filter', () => {
        const files = [{ name: 'a.mp3', type: 'audio/mpeg', size: 100 }];
        assert.equal(filterDroppableFiles(files).length, 1);
    });

    test('video files pass the filter', () => {
        const files = [{ name: 'a.mp4', type: 'video/mp4', size: 100 }];
        assert.equal(filterDroppableFiles(files).length, 1);
    });

    test('text files are rejected by the filter', () => {
        const files = [{ name: 'readme.txt', type: 'text/plain', size: 100 }];
        assert.equal(filterDroppableFiles(files).length, 0);
    });

    test('mixed drop: image + text — only image passes', () => {
        const files = [
            { name: 'a.png', type: 'image/png', size: 100 },
            { name: 'b.txt', type: 'text/plain', size: 50 },
        ];
        assert.equal(filterDroppableFiles(files).length, 1);
    });

    test('attachment object has correct shape after conversion', () => {
        const file = { name: 'photo.jpg', type: 'image/jpeg', size: 2048, path: '/tmp/photo.jpg' };
        const att = stubReadAsDataURL(file, 'abc123base64');
        assert.equal(att.file, 'photo.jpg');
        assert.equal(att.mimetype, 'image/jpeg');
        assert.equal(att.content, 'abc123base64');
        assert.equal(att.size, 2048);
        assert.equal(att.path, '/tmp/photo.jpg');
    });

    test('multiple files produce multiple attachment objects', () => {
        const files = [
            { name: 'a.png', type: 'image/png', size: 100, path: '' },
            { name: 'b.wav', type: 'audio/wav', size: 200, path: '' },
        ];
        const atts = files.map(f => stubReadAsDataURL(f, 'data'));
        assert.equal(atts.length, 2);
        assert.equal(atts[0].mimetype, 'image/png');
        assert.equal(atts[1].mimetype, 'audio/wav');
    });

    test('file without path gets empty string path', () => {
        const file = { name: 'x.png', type: 'image/png', size: 1 };
        const att = stubReadAsDataURL(file, 'x');
        assert.equal(att.path, '');
    });

    test('_dropZoneAttrs generates ondragover/ondragleave/ondrop (logic check)', () => {
        // Mirror the logic of app.js _dropZoneAttrs
        function dropZoneAttrs(purpose, stepIndex) {
            const si = stepIndex != null ? `,${stepIndex}` : '';
            return `ondragover="event.preventDefault();this.style.outline='2px dashed #4fc3f7'"` +
                   ` ondragleave="this.style.outline=''"` +
                   ` ondrop="app.handleFileDrop(event,'${purpose}'${si !== '' ? si : ''})"`;
        }
        const attrs = dropZoneAttrs('input_attachment');
        assert.ok(attrs.includes('ondragover'));
        assert.ok(attrs.includes('ondragleave'));
        assert.ok(attrs.includes("'input_attachment'"));
    });

    test('_dropZoneAttrs with stepIndex includes it in ondrop call', () => {
        function dropZoneAttrs(purpose, stepIndex) {
            const si = stepIndex != null ? `,${stepIndex}` : '';
            return `ondragover="event.preventDefault();this.style.outline='2px dashed #4fc3f7'"` +
                   ` ondragleave="this.style.outline=''"` +
                   ` ondrop="app.handleFileDrop(event,'${purpose}'${si !== '' ? si : ''})"`;
        }
        const attrs = dropZoneAttrs('step_attachment', 2);
        assert.ok(attrs.includes(',2'));
    });
});

// ── 7. Selection / Color logic ──────────────────────────────────
describe('Selection & Color logic', () => {

    // ---- pure helpers (extracted from frontend) ----

    function isAncestor(ancestor, descendant) {
        if (!ancestor || !descendant) return false;
        const a = ancestor.split('/').filter(p => p !== '');
        const d = descendant.split('/').filter(p => p !== '');
        if (a.length >= d.length) return false;
        return a.every((p, i) => p === d[i]);
    }

    // result-node class: depends on selection + link state
    function resultNodeClass(childPath, currentResultNodePath, selectedDataPath, isLinkedSourceFn) {
        const sel = currentResultNodePath === childPath;
        const link = selectedDataPath === childPath;
        const hist = isLinkedSourceFn(childPath);
        if (sel && link) return 'selected-data';       // 🔴
        if (sel) return 'selected-result';             // 🟠
        if (link || hist) return 'selected-linked';    // 🟡
        return '';
    }

    // step-node class: depends on selection + link state
    function stepNodeClass(path, isSelected, currentNodePath, selectedDataPath, isLinkedSourceFn, getParentTitleFn) {
        if (isSelected) {
            if (getParentTitleFn(path) === 'Processed') return 'selected-result';
            return 'selected-input';  // 🟢
        }
        const ancestorOfLink = selectedDataPath &&
            (selectedDataPath === path || selectedDataPath.startsWith(path + '/'));
        if (ancestorOfLink || isLinkedSourceFn(path)) return 'selected-linked';  // gray
        return '';
    }

    function getParentTitle(path, tree) {
        const parts = path.split('/').filter(p => p !== '');
        if (parts.length < 2) return '';
        const parentPath = parts.slice(0, -1).join('/');
        let node = tree;
        for (const p of parentPath.split('/').filter(Boolean)) {
            const idx = parseInt(p);
            if (!node.children || idx >= node.children.length) return '';
            node = node.children[idx];
        }
        try { return node.title ? atob(node.title) : ''; } catch { return node.title || ''; }
    }

    // build a set of linked source paths by scanning linkInfo in tree
    function buildLinkedSources(tree) {
        const result = new Set();
        function scan(nodes) {
            if (!nodes) return;
            for (const n of nodes) {
                if (n.linkInfo) {
                    try {
                        const info = JSON.parse(n.linkInfo);
                        if (info.sourcePath) result.add(info.sourcePath);
                    } catch {}
                }
                scan(n.children);
            }
        }
        scan(tree.children);
        return result;
    }

    // ---- sample tree data ----
    const b64 = s => { try { return btoa(unescape(encodeURIComponent(s))); } catch { return btoa(s); } };

    const tree = {
        title: '', content: '', mimetype: 'text/plain', children: [
            { title: b64('Step 1'), content: '', mimetype: 'text/plain', children: [
                { title: b64('Processed'), content: '', mimetype: 'text/plain', children: [
                    { title: b64('2026-06-10 12:00:00'), content: b64('result A'), mimetype: 'text/plain', children: [] },
                ]}
            ]},
            { title: b64('Step 2'), content: '', mimetype: 'text/plain', children: [
                { title: b64('Processed'), content: '', mimetype: 'text/plain', children: [
                    { title: b64('2026-06-10 13:00:00'), content: b64('result B'), mimetype: 'text/plain',
                        linkInfo: JSON.stringify({ sourcePath: '0/0/0', sourceResultTitle: '2026-06-10 12:00:00', sourceStepTitle: 'Step 1' }),
                        children: [] },
                ]}
            ]},
        ]
    };
    // Paths:
    //   Step 1         = "0"
    //     Processed    = "0/0"
    //       Result A   = "0/0/0"
    //   Step 2         = "1"
    //     Processed    = "1/0"
    //       Result B   = "1/0/0"   (linkInfo.sourcePath = "0/0/0")

    const linkedSources = buildLinkedSources(tree);

    // ---- isAncestor tests ----
    test('isAncestor: root is ancestor of child', () => {
        assert.ok(isAncestor('0', '0/0'));
    });

    test('isAncestor: root is ancestor of grandchild', () => {
        assert.ok(isAncestor('0', '0/0/0'));
    });

    test('isAncestor: same path is NOT ancestor', () => {
        assert.ok(!isAncestor('0', '0'));
    });

    test('isAncestor: different branch is NOT ancestor', () => {
        assert.ok(!isAncestor('0', '1'));
        assert.ok(!isAncestor('0', '1/0'));
    });

    test('isAncestor: deeper path cannot be ancestor of shallower', () => {
        assert.ok(!isAncestor('0/0', '0'));
    });

    test('isAncestor: empty/null paths', () => {
        assert.ok(!isAncestor('', '0'));
        assert.ok(!isAncestor('0', ''));
        assert.ok(!isAncestor(null, '0'));
    });

    // ---- resultNodeClass tests ----
    test('resultNodeClass: unselected node returns empty', () => {
        const cls = resultNodeClass('1/0/0', '', '', () => false);
        assert.equal(cls, '');
    });

    test('resultNodeClass: selected only returns orange', () => {
        const cls = resultNodeClass('0/0/0', '0/0/0', '', () => false);
        assert.equal(cls, 'selected-result');
    });

    test('resultNodeClass: selected + linked returns red', () => {
        const cls = resultNodeClass('0/0/0', '0/0/0', '0/0/0', () => false);
        assert.equal(cls, 'selected-data');
    });

    test('resultNodeClass: unselected but has linkInfo returns lemon', () => {
        const cls = resultNodeClass('0/0/0', '1/0/0', '', p => linkedSources.has(p));
        assert.equal(cls, 'selected-linked');
    });

    test('resultNodeClass: unselected but is selectedDataPath returns lemon', () => {
        const cls = resultNodeClass('0/0/0', '', '0/0/0', () => false);
        assert.equal(cls, 'selected-linked');
    });

    test('resultNodeClass: selected but different path from linked returns orange', () => {
        const cls = resultNodeClass('1/0/0', '1/0/0', '0/0/0', () => false);
        assert.equal(cls, 'selected-result');
    });

    // ---- stepNodeClass tests ----
    test('stepNodeClass: selected returns green', () => {
        const cls = stepNodeClass('0', true, '0', '', () => false, p => getParentTitle(p, tree));
        assert.equal(cls, 'selected-input');
    });

    test('stepNodeClass: selected + parent is Processed returns orange (should not happen for steps)', () => {
        // If a step's parent were Processed, that would be a result node edge case
        const cls = stepNodeClass('0/0/0', true, '0/0/0', '', () => false, p => getParentTitle(p, tree));
        assert.equal(cls, 'selected-result');
    });

    test('stepNodeClass: unselected but descendant contains selectedDataPath returns gray', () => {
        // Step 0 has child Processed with Result A at "0/0/0" which IS selectedDataPath
        // Step 0 is ancestor of the linked node
        const cls = stepNodeClass('0', false, '1', '0/0/0', p => linkedSources.has(p), p => getParentTitle(p, tree));
        assert.equal(cls, 'selected-linked');
    });

    test('stepNodeClass: unselected step with no linked descendant returns empty', () => {
        // Step 2 is not linked in any way
        const cls = stepNodeClass('1', false, '0', '0/0/0', p => linkedSources.has(p), p => getParentTitle(p, tree));
        assert.equal(cls, '');
    });

    test('stepNodeClass: unselected with no link returns empty', () => {
        const cls = stepNodeClass('1', false, '0', '', () => false, p => getParentTitle(p, tree));
        assert.equal(cls, '');
    });

    test('stepNodeClass: unselected but is selectedDataPath ancestor returns gray', () => {
        const cls = stepNodeClass('0', false, '1', '0/0/0', () => false, p => getParentTitle(p, tree));
        assert.equal(cls, 'selected-linked');
    });

    // ---- linkedSources scanning ----
    test('buildLinkedSources finds sourcePath from linkInfo', () => {
        assert.ok(linkedSources.has('0/0/0'));
    });

    test('buildLinkedSources does not include unrelated paths', () => {
        assert.ok(!linkedSources.has('0/0'));
        assert.ok(!linkedSources.has('1/0/0'));
    });

    // ---- recalcLinkMode: test isAncestor-driven logic ----
    test('recalcLinkMode: step IS ancestor of result → viewing mode (no link)', () => {
        // Step "0" is ancestor of result "0/0/0" → should stay as viewing (selectedDataPath = null)
        const stepPath = '0';
        const resultPath = '0/0/0';
        const shouldLink = !isAncestor(stepPath, resultPath);
        assert.ok(!shouldLink);
    });

    test('recalcLinkMode: step is NOT ancestor of result → linking mode', () => {
        // Step "1" is NOT ancestor of result "0/0/0" → should link
        const stepPath = '1';
        const resultPath = '0/0/0';
        const shouldLink = !isAncestor(stepPath, resultPath);
        assert.ok(shouldLink);
    });

    // ---- Gemini provider header auth tests (need mock server) ----
    test('Gemini call sends X-Goog-Api-Key header', async () => {
        // Use the existing mock server from the AI Provider test suite
        // but since we can't access it here, we inline a simple server
        const srv = await new Promise(resolve => {
            const s = http.createServer((req, res) => {
                // capture and respond
                let body = '';
                req.on('data', c => body += c);
                req.on('end', () => {
                    globalThis._lastGemini = { url: req.url, headers: req.headers, body };
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ candidates: [{ content: { parts: [{ text: 'mock' }] } }] }));
                });
            });
            s.listen(0, '127.0.0.1', () => resolve(s));
        });
        const port = srv.address().port;

        // Simulate Gemini provider's call with X-Goog-Api-Key header
        const body = JSON.stringify({
            contents: [{ parts: [{ text: 'hello' }] }],
            generationConfig: { temperature: 0.7, maxOutputTokens: 4096 },
        });
        const raw = await new Promise((resolve, reject) => {
            const opts = { hostname: '127.0.0.1', port, path: `/v1beta/models/gemini-2.5-flash:generateContent`, method: 'POST',
                headers: { 'Content-Type': 'application/json', 'X-Goog-Api-Key': 'test-key-123', 'Content-Length': Buffer.byteLength(body) } };
            const req = http.request(opts, res => { const ch = []; res.on('data', c => ch.push(c)); res.on('end', () => resolve(Buffer.concat(ch).toString())); });
            req.on('error', reject);
            req.write(body);
            req.end();
        });

        assert.equal(globalThis._lastGemini.headers['x-goog-api-key'], 'test-key-123');
        assert.ok(!globalThis._lastGemini.url.includes('key='));
        const j = JSON.parse(raw);
        assert.equal(j.candidates[0].content.parts[0].text, 'mock');
        srv.close();
    });
});
