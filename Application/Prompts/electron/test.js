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
            const resp = await provider.call({ model: step.params?.model || 'gpt-4.1', systemPrompt: step.params?.systemPrompt || '', userPrompt, temperature: parseFloat(step.params?.temperature || '0.7'), maxTokens: 4096 });
            if (idx < this.historySteps.length) { this.historySteps[idx].output = resp.content; this.historySteps[idx].status = 'completed'; }
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

    function makeApp(tmpDir) {
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
                case 'get_providers':
                    post('providers_result', st.loadProviders());
                    break;
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
});
