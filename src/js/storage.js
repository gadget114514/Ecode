const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

class Storage {
    constructor() {
        this.basePath = '';
        this.globalPath = '';
        this.maxHistoryRuns = 50;
    }

    init(appDataPath) {
        this.basePath = path.resolve(appDataPath);
        if (!this.globalPath) {
            this.globalPath = this.basePath;
        }
        this.ensureDir(this.basePath);
        this.ensureDir(path.join(this.basePath, 'data'));
        this.ensureDir(path.join(this.basePath, 'blobs'));
        this.ensureDir(path.join(this.basePath, 'history'));
        return true;
    }

    ensureDir(dirPath) {
        if (!fs.existsSync(dirPath)) {
            fs.mkdirSync(dirPath, { recursive: true });
        }
    }

    writeFileAtomic(filePath, content) {
        const dir = path.dirname(filePath);
        this.ensureDir(dir);
        const tmpPath = filePath + '.tmp';
        fs.writeFileSync(tmpPath, content, 'utf8');
        fs.renameSync(tmpPath, filePath);
    }

    // --- Path helpers & isolation ---
    resolveProjectPath(relativePath) {
        const dataDir = path.join(this.basePath, 'data');
        const fullPath = path.resolve(dataDir, relativePath);
        if (!fullPath.startsWith(dataDir + path.sep) && fullPath !== dataDir) {
            return null; // traversal prevention
        }
        return fullPath;
    }

    // --- Session ---
    loadSession() {
        const sessionPath = path.join(this.basePath, 'session.json');
        if (!fs.existsSync(sessionPath)) {
            const def = {
                tabs: [{ name: 'General', file: 'general.json' }]
            };
            this.saveSession(def);
            return def;
        }
        try {
            return JSON.parse(fs.readFileSync(sessionPath, 'utf8'));
        } catch (e) {
            return { tabs: [] };
        }
    }

    saveSession(session) {
        const sessionPath = path.join(this.basePath, 'session.json');
        this.writeFileAtomic(sessionPath, JSON.stringify(session, null, 2));
    }

    // --- Tab Data (Node tree) ---
    loadTabData(filePath) {
        const resolved = this.resolveProjectPath(filePath);
        if (!resolved || !fs.existsSync(resolved)) {
            return { title: 'Root', content: '', mimetype: 'text/markdown', children: [], attachments: [] };
        }
        try {
            return JSON.parse(fs.readFileSync(resolved, 'utf8'));
        } catch (e) {
            return { title: 'Root', content: '', mimetype: 'text/markdown', children: [], attachments: [] };
        }
    }

    saveTabData(filePath, rootNode) {
        const resolved = this.resolveProjectPath(filePath);
        if (!resolved) return;
        this.writeFileAtomic(resolved, JSON.stringify(rootNode, null, 2));
    }

    // --- Blobs ---
    loadBlob(relativePath) {
        const blobPath = path.join(this.basePath, 'blobs', relativePath);
        if (!fs.existsSync(blobPath)) return '';
        return fs.readFileSync(blobPath, 'utf8');
    }

    saveBlob(data, ext) {
        const name = crypto.randomBytes(16).toString('hex') + (ext.startsWith('.') ? ext : '.' + ext);
        const blobPath = path.join(this.basePath, 'blobs', name);
        this.ensureDir(path.dirname(blobPath));
        fs.writeFileSync(blobPath, data);
        return name;
    }

    removeBlob(relativePath) {
        const blobPath = path.join(this.basePath, 'blobs', relativePath);
        if (fs.existsSync(blobPath)) {
            fs.unlinkSync(blobPath);
        }
    }

    garbageCollectBlobs(referencedPaths) {
        const blobDir = path.join(this.basePath, 'blobs');
        if (!fs.existsSync(blobDir)) return;
        const files = fs.readdirSync(blobDir);
        for (const file of files) {
            if (!referencedPaths.includes(file)) {
                fs.unlinkSync(path.join(blobDir, file));
            }
        }
    }

    getTabFiles() {
        const dataDir = path.join(this.basePath, 'data');
        if (!fs.existsSync(dataDir)) return [];
        return fs.readdirSync(dataDir).filter(f => f.endsWith('.json'));
    }

    getFileTreeJson() {
        const dataDir = path.join(this.basePath, 'data');
        if (!fs.existsSync(dataDir)) return '[]';

        const scan = (dir) => {
            const list = fs.readdirSync(dir);
            const children = [];
            for (const file of list) {
                const fullPath = path.join(dir, file);
                const stat = fs.statSync(fullPath);
                if (stat.isDirectory()) {
                    children.push({
                        name: file,
                        type: 'directory',
                        children: scan(fullPath)
                    });
                } else if (stat.isFile()) {
                    const rel = path.relative(dataDir, fullPath);
                    children.push({
                        name: file,
                        type: 'file',
                        path: rel.replace(/\\/g, '/')
                    });
                }
            }
            return children;
        };

        const result = [];
        const list = fs.readdirSync(dataDir);
        for (const file of list) {
            const fullPath = path.join(dataDir, file);
            const stat = fs.statSync(fullPath);
            if (stat.isDirectory()) {
                result.push({
                    name: file,
                    type: 'directory',
                    children: scan(fullPath)
                });
            } else if (stat.isFile()) {
                const rel = path.relative(dataDir, fullPath);
                result.push({
                    name: file,
                    type: 'file',
                    path: rel.replace(/\\/g, '/')
                });
            }
        }
        return JSON.stringify(result);
    }

    // --- History ---
    saveHistory(record) {
        const filename = record.id ? `run_${record.id}.json` : `run_${new Date().toISOString().replace(/[:.]/g, '')}.json`;
        const historyPath = path.join(this.basePath, 'history', filename);
        this.writeFileAtomic(historyPath, JSON.stringify(record, null, 2));

        // Enforce history retention
        this.enforceHistoryRetention();
    }

    enforceHistoryRetention() {
        const historyDir = path.join(this.basePath, 'history');
        if (!fs.existsSync(historyDir)) return;
        const files = fs.readdirSync(historyDir)
            .filter(f => f.startsWith('run_') && f.endsWith('.json'))
            .map(f => {
                const filePath = path.join(historyDir, f);
                return {
                    name: f,
                    path: filePath,
                    mtime: fs.statSync(filePath).mtimeMs
                };
            })
            .sort((a, b) => b.mtime - a.mtime); // newest first

        if (files.length > this.maxHistoryRuns) {
            for (let i = this.maxHistoryRuns; i < files.length; i++) {
                fs.unlinkSync(files[i].path);
            }
        }
    }

    listHistory() {
        const historyDir = path.join(this.basePath, 'history');
        if (!fs.existsSync(historyDir)) return [];
        return fs.readdirSync(historyDir)
            .filter(f => f.startsWith('run_') && f.endsWith('.json'))
            .map(f => {
                const filePath = path.join(historyDir, f);
                return {
                    name: f,
                    mtime: fs.statSync(filePath).mtimeMs
                };
            })
            .sort((a, b) => b.mtime - a.mtime) // newest first
            .map(x => x.name);
    }

    loadHistoryRecord(filename) {
        const recordPath = path.join(this.basePath, 'history', filename);
        if (!fs.existsSync(recordPath)) return '';
        return fs.readFileSync(recordPath, 'utf8');
    }

    updateHistoryEvaluation(filename, evaluation) {
        const recordPath = path.join(this.basePath, 'history', filename);
        if (!fs.existsSync(recordPath)) return;
        try {
            const rec = JSON.parse(fs.readFileSync(recordPath, 'utf8'));
            rec.evaluation = evaluation;
            this.writeFileAtomic(recordPath, JSON.stringify(rec, null, 2));
        } catch (e) {
            // Ignore parse issues
        }
    }

    // --- Providers ---
    loadProviders() {
        const providersPath = path.join(this.globalPath, 'providers.json');
        if (!fs.existsSync(providersPath)) return {};
        try {
            return JSON.parse(fs.readFileSync(providersPath, 'utf8'));
        } catch (e) {
            return {};
        }
    }

    saveProviders(providers) {
        const providersPath = path.join(this.globalPath, 'providers.json');
        this.writeFileAtomic(providersPath, JSON.stringify(providers, null, 2));
        return true;
    }

    // --- Pipelines ---
    loadPipelines() {
        const pipelinePath = path.join(this.basePath, 'pipeline.json');
        if (!fs.existsSync(pipelinePath)) return [];
        try {
            const root = JSON.parse(fs.readFileSync(pipelinePath, 'utf8'));
            return root.pipelines || [];
        } catch (e) {
            return [];
        }
    }

    savePipelines(pipelines) {
        const pipelinePath = path.join(this.basePath, 'pipeline.json');
        this.writeFileAtomic(pipelinePath, JSON.stringify({ pipelines }, null, 2));
    }

    // --- Recent Files ---
    loadRecentFiles() {
        const recentPath = path.join(this.basePath, 'recent_files.json');
        if (!fs.existsSync(recentPath)) return [];
        try {
            const root = JSON.parse(fs.readFileSync(recentPath, 'utf8'));
            return (root.files || []).map(f => 'data/' + f);
        } catch (e) {
            return [];
        }
    }

    saveRecentFiles(files) {
        const recentPath = path.join(this.basePath, 'recent_files.json');
        const relativeFiles = files.map(f => f.startsWith('data/') ? f.substring(5) : f);
        this.writeFileAtomic(recentPath, JSON.stringify({ files: relativeFiles }, null, 2));
    }

    // --- Checkpoints ---
    saveCheckpoint(runId, stepIndex, input, output, metaJson) {
        const dir = path.join(this.basePath, 'history', runId, `checkpoint_${stepIndex}`);
        this.ensureDir(dir);
        this.writeFileAtomic(path.join(dir, 'input.json'), input);
        this.writeFileAtomic(path.join(dir, 'output.json'), output);
        this.writeFileAtomic(path.join(dir, 'meta.json'), metaJson);
    }

    loadCheckpointInput(runId, stepIndex) {
        const file = path.join(this.basePath, 'history', runId, `checkpoint_${stepIndex}`, 'input.json');
        return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : '';
    }

    loadCheckpointOutput(runId, stepIndex) {
        const file = path.join(this.basePath, 'history', runId, `checkpoint_${stepIndex}`, 'output.json');
        return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : '';
    }

    loadCheckpointMeta(runId, stepIndex) {
        const file = path.join(this.basePath, 'history', runId, `checkpoint_${stepIndex}`, 'meta.json');
        return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : '';
    }

    // --- Run State ---
    saveRunState(runId, stateJson) {
        const dir = path.join(this.basePath, 'history', runId);
        this.ensureDir(dir);
        this.writeFileAtomic(path.join(dir, 'state.json'), stateJson);
    }

    loadRunState(runId) {
        const file = path.join(this.basePath, 'history', runId, 'state.json');
        return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : '';
    }

    // --- Incomplete Run Detection ---
    scanIncompleteRuns() {
        const result = [];
        const historyDir = path.join(this.basePath, 'history');
        if (!fs.existsSync(historyDir)) return result;
        const dirs = fs.readdirSync(historyDir).filter(f => {
            try {
                const stat = fs.statSync(path.join(historyDir, f));
                return stat.isDirectory();
            } catch (e) {
                return false;
            }
        });

        for (const runId of dirs) {
            const stateFile = path.join(historyDir, runId, 'state.json');
            if (fs.existsSync(stateFile)) {
                try {
                    const val = JSON.parse(fs.readFileSync(stateFile, 'utf8'));
                    if (val.status === 'running') {
                        result.push({
                            runId,
                            pipelineName: val.pipelineName || '',
                            lastCompletedStep: val.currentStep || 0,
                            totalSteps: val.totalSteps || 0,
                            startedAt: val.startedAt || '',
                            status: 'interrupted'
                        });
                    }
                } catch (e) {
                    // Ignore parse issues
                }
            }
        }
        return result;
    }

    closeRun(runId) {
        const stateFile = path.join(this.basePath, 'history', runId, 'state.json');
        if (fs.existsSync(stateFile)) {
            try {
                const val = JSON.parse(fs.readFileSync(stateFile, 'utf8'));
                val.status = 'completed';
                this.writeFileAtomic(stateFile, JSON.stringify(val, null, 2));
            } catch (e) {
                // Ignore
            }
        }
    }

    discardRun(runId) {
        const runDir = path.join(this.basePath, 'history', runId);
        if (fs.existsSync(runDir)) {
            fs.rmSync(runDir, { recursive: true, force: true });
        }
    }

    // --- Named Chests ---
    getChestRoot() {
        const parent = path.dirname(this.basePath);
        return path.join(parent, 'chests');
    }

    saveToNamedChest(chestName, content) {
        const chestDir = this.getChestRoot();
        this.ensureDir(chestDir);
        const chestPath = path.join(chestDir, `chest_${chestName}.json`);
        this.writeFileAtomic(chestPath, content);
    }

    loadFromNamedChest(chestName) {
        const chestPath = path.join(this.getChestRoot(), `chest_${chestName}.json`);
        if (!fs.existsSync(chestPath)) return '';
        return fs.readFileSync(chestPath, 'utf8');
    }

    chestExists(chestName) {
        const chestPath = path.join(this.getChestRoot(), `chest_${chestName}.json`);
        return fs.existsSync(chestPath);
    }

    listNamedChests() {
        const chestDir = this.getChestRoot();
        if (!fs.existsSync(chestDir)) return [];
        return fs.readdirSync(chestDir)
            .filter(f => f.startsWith('chest_') && f.endsWith('.json'))
            .map(f => f.substring(6, f.length - 5));
    }

    // --- Storage Chest ---
    getStorageChestDir() {
        const parent = path.dirname(this.basePath);
        return path.join(parent, 'storage_chest');
    }

    moveToStorageChest(sourcePath, storedName) {
        const chestDir = this.getStorageChestDir();
        this.ensureDir(chestDir);
        const destPath = path.join(chestDir, storedName);
        fs.renameSync(sourcePath, destPath);
    }

    listStorageChest() {
        const chestDir = this.getStorageChestDir();
        if (!fs.existsSync(chestDir)) return [];
        return fs.readdirSync(chestDir)
            .map(f => path.join(chestDir, f));
    }

    loadFromStorageChest(filename) {
        const filePath = path.join(this.getStorageChestDir(), filename);
        if (!fs.existsSync(filePath)) return '';
        return fs.readFileSync(filePath, 'utf8');
    }

    // --- General Config ---
    loadGeneralConfig() {
        const configPath = path.join(this.globalPath, 'config.json');
        if (!fs.existsSync(configPath)) {
            return { historyRetention: 50, defaultProvider: 'openai' };
        }
        try {
            return JSON.parse(fs.readFileSync(configPath, 'utf8'));
        } catch (e) {
            return { historyRetention: 50, defaultProvider: 'openai' };
        }
    }

    saveGeneralConfig(cfg) {
        const configPath = path.join(this.globalPath, 'config.json');
        this.writeFileAtomic(configPath, JSON.stringify(cfg, null, 2));
        return true;
    }

    // --- Recipes ---
    loadRecipes() {
        const filePath = path.join(this.globalPath, 'recipes.json');
        if (!fs.existsSync(filePath)) {
            const oldPath = path.join(this.globalPath, 'recipe.json');
            if (fs.existsSync(oldPath)) {
                try {
                    const content = fs.readFileSync(oldPath, 'utf8');
                    fs.writeFileSync(filePath, content, 'utf8');
                    fs.unlinkSync(oldPath);
                    return JSON.parse(content);
                } catch (e) {
                    return [];
                }
            }
            return [];
        }
        try {
            return JSON.parse(fs.readFileSync(filePath, 'utf8'));
        } catch (e) {
            return [];
        }
    }

    saveRecipes(recipes) {
        const filePath = path.join(this.globalPath, 'recipes.json');
        this.writeFileAtomic(filePath, JSON.stringify(recipes, null, 2));
        return true;
    }

    // --- Wizards ---
    loadWizardData(wizardName) {
        const userPath = path.join(this.basePath, 'wizards', `${wizardName}.json`);
        if (fs.existsSync(userPath)) {
            return fs.readFileSync(userPath, 'utf8');
        }
        const appPath = path.join(__dirname, '..', '..', 'frontend', 'wizards', `${wizardName}.json`);
        if (fs.existsSync(appPath)) {
            return fs.readFileSync(appPath, 'utf8');
        }
        return '';
    }

    // --- Optimizer Data ---
    saveOptimizerData(relativePath, json) {
        const optPath = path.join(this.basePath, relativePath);
        this.writeFileAtomic(optPath, json);
    }

    loadOptimizerData(relativePath) {
        const optPath = path.join(this.basePath, relativePath);
        if (!fs.existsSync(optPath)) return '';
        return fs.readFileSync(optPath, 'utf8');
    }
}

module.exports = new Storage();
