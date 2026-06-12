// Prompts App - Vanilla JS frontend

const app = {
    state: {
        tabs: [],
        activeTab: 0,
        currentNode: null,
        currentNodePath: '',
        selectedOpPath: '',
        selectedDataPath: '',
        language: 'en',
        testMode: false,
        translations: {},
        searchTimeout: null,
        navHistory: [],   // [{tabIndex, path}]
        navFuture: [],    // [{tabIndex, path}]
        viewMode: 'node', // "node" | "pipeline"
        currentRunId: '',
        pipelineRun: {
            running: false,
            steps: [],       // [{index, name, type, completed, input, output, outputAttachments, artifacts}]
            selectedStep: -1
        },
        activeTreeTab: 'pipeline',
        fileTree: [],
        projects: [],
        activeProject: 'default',
        incompleteRuns: [],
        chestList: [],
        historyRetention: 50,
        defaultProvider: 'openai',
        defaultModel: '',
        recipes: [],
        selectedRecipe: '',
        editingRecipeIndex: -1,
        providerModels: {},
        collapsedPaths: new Set()
    },

    init() {
        this.setupBridge();
        this.loadLanguage(this.state.language);
        this.setupHints();
        this.initMessagesResizer();
        window.addEventListener('beforeunload', () => {
            this.updateNode();
        });
        document.addEventListener('click', () => this.hideTreeContextMenu());
    },

    setupBridge() {
        const bridge = window.__promptsBridge || window.chrome?.webview;
        if (!bridge) { console.error('No IPC bridge available'); return; }
        bridge.addEventListener('message', (e) => {
            const msg = typeof e.data === 'string' ? JSON.parse(e.data) : e.data;
            this.handleBridge(msg);
        });
    },

    handleBridge(msg) {
        switch (msg.type) {
            case 'init':
                this.state.language = msg.payload.language || 'en';
                this.state.embedded = msg.payload.embedded || false;
                this.state.appDataPath = msg.payload.appDataPath || '';
                this.loadLanguage(this.state.language);
                if (msg.payload.tabs && msg.payload.tabs.length > 0) {
                    this.state.tabs = msg.payload.tabs.map(t => ({
                        name: t.name,
                        file: t.file,
                        root: (msg.payload.nodes && msg.payload.nodes[t.file])
                              || { title:'', content:'', mimetype:'text/plain', attachments:[], children:[], nodeType: 'root' }
                    }));
                    this.state.tabs.forEach(t => this.patchNodeTypes(t.root, true));
                    this.renderTabs();
                    this.renderTree();
                    this.renderList();
                }
                if (msg.payload.pipelines) {
                    this.state.pipelines = msg.payload.pipelines;
                }
                if (msg.payload.providers) {
                    this.state.providers = msg.payload.providers;
                }
                if (msg.payload.config) {
                    if (msg.payload.config.historyRetention)
                        this.state.historyRetention = msg.payload.config.historyRetention;
                    if (msg.payload.config.chestList)
                        this.state.chestList = msg.payload.config.chestList;
                    if (msg.payload.config.defaultProvider)
                        this.state.defaultProvider = msg.payload.config.defaultProvider;
                    if (msg.payload.config.defaultModel !== undefined)
                        this.state.defaultModel = msg.payload.config.defaultModel;
                }
                if (msg.payload.recipes) {
                    this.state.recipes = msg.payload.recipes;
                }
                // Always show hamburger (embedded: replaces menubar; standalone: supplement)
                const hb = document.getElementById('btn-hamburger');
                if (hb) hb.style.display = '';
                this.addLog('✅ App ready');
                // Show wizard on first run
                if (!localStorage.getItem('prompts_wizard_done')) {
                    setTimeout(() => this.showWizard(), 400);
                }
                break;
            case 'node_updated':
                this.updateNodeUI(msg.payload);
                break;
            case 'stream_chunk':
                this.appendStreamOutput(msg.payload);
                break;
            case 'pipeline_init':
                this.onPipelineInit(msg.payload);
                break;
            case 'step_done':
                this.onStepDone(msg.payload);
                break;
            case 'step_started':
                this.highlightStep(msg.payload);
                break;
            case 'pipeline_error':
                this.showError(msg.payload.message);
                break;
            case 'rtf_position':
                this.onRtfPosition(msg.payload);
                break;
            case 'search_results':
                this.showSearchResults(msg.payload.results);
                break;
            case 'ready':
                this.sendInitData();
                break;
            case 'open_file_dialog_result':
                this.onFileSelected(msg.payload);
                break;
            case 'file_dialog_result':
                this.onMediaFileDialogResult(msg.payload);
                break;
            case 'pipeline_completed':
                this.onPipelineCompleted(msg.payload);
                break;
            case 'manual_step_pause':
                this.showManualStep(msg.payload);
                break;
            case 'wizard_step_pause':
                this.showPipelineWizardStep(msg.payload);
                break;
            case 'providers_result': {
                let payloadProviders = msg.payload;
                let customMetadata = null;
                if (msg.payload && typeof msg.payload === 'object' && msg.payload.hasOwnProperty('customMetadata') && msg.payload.hasOwnProperty('providers')) {
                    payloadProviders = msg.payload.providers;
                    customMetadata = msg.payload.customMetadata;
                }
                this.state.providers = payloadProviders || {};
                this.onProvidersResult(this.state.providers, customMetadata);
                // Re-render Recipe Manager if it's open
                if (document.getElementById('recipe-modal')?.classList.contains('visible')) {
                    this.renderRecipeManager();
                }
                break;
            }
            case 'model_list':
                if (msg.payload && msg.payload.models) {
                    if (!this.state.providerModels) this.state.providerModels = {};
                    this.state.providerModels[msg.payload.provider] = msg.payload.models;
                    // Update model inputs in Recipe Manager if open
                    if (document.getElementById('recipe-modal')?.classList.contains('visible')) {
                        this.updateRecipeManagerModels(msg.payload.provider);
                    }
                }
                break;
            case 'menu_command':
                this.handleMenuCommand(msg.payload);
                break;
            case 'open_file_result':
                this.onFileSelected(msg.payload.path);
                break;
            case 'file_tree_result':
                this.state.fileTree = msg.payload.tree || [];
                this.renderFileTree();
                break;
            case 'file_data_result':
                this.onFileDataResult(msg.payload.path, msg.payload.root);
                break;
            case 'rename_file_result':
                this.onRenameFileResult(msg.payload);
                break;
            case 'save_as_result':
                this.onSaveAsResult(msg.payload.path);
                break;
            case 'pipeline_list':
                this.state.pipelines = msg.payload.pipelines || [];
                // If pipeline manager is open, sync its local list
                if (this.pmState_) {
                    this.pmState_.pipelines = this.state.pipelines.slice();
                    this.pmRenderPipelineList();
                }
                this.addLog('📋 Pipelines updated');
                break;
            case 'history_list_result':
                this.onHistoryListResult(msg.payload);
                break;
            case 'history_detail_result':
                this.onHistoryDetailResult(msg.payload);
                break;
            case 'evaluation_saved':
                this.onEvaluationSaved(msg.payload);
                break;
            case 'optimize_proposals':
                this.onOptimizeProposals(msg.payload);
                break;
            case 'optimize_applied':
                this.onOptimizeApplied(msg.payload);
                break;
            case 'optimize_version_changed':
                this.onOptimizeVersionChanged(msg.payload);
                break;
            case 'optimize_version_list_result':
                this.onOptimizeVersionListResult(msg.payload);
                break;
            case 'optimize_error':
                this.onOptimizeError(msg.payload);
                break;
            case 'optimize_progress':
                this.onOptimizeProgress(msg.payload);
                break;
            case 'test_connection_result':
                this.onTestConnectionResult(msg.payload);
                break;
            case 'log':
                this.addLog('📋 ' + (msg.payload.message || ''));
                break;
            // ── Stream Model Extensions ──
            case 'step_filter_pause':
                this.showFilterStep(msg.payload);
                break;
            case 'step_filter_result':
                this.addLog(`🔍 ${this.t('FilterTitle')}: ${msg.payload.approved} ${this.t('Save')}, ${msg.payload.rejected} ${this.t('Discard')}`);
                break;
            case 'evaluate_result':
                this.showEvaluateResult(msg.payload);
                break;
            case 'chest_put':
                this.addLog(`📦 Sending to chest: ${msg.payload.chestName}`);
                this.postMessage({ type: 'send_to_chest', payload: msg.payload });
                break;
            case 'chest_take':
                this.addLog(`📦 Loading from chest: ${msg.payload.chestName}`);
                this.postMessage({ type: 'select_input_source', payload: { source: 'chest', chestName: msg.payload.chestName } });
                break;
            case 'save_run_state':
                this.postMessage({ type: 'save_run_state', payload: msg.payload });
                break;
            case 'incomplete_run_detected':
                this.state.incompleteRuns = msg.payload.runs || [];
                this.showIncompleteRuns();
                break;
            case 'project_changed':
                this.state.activeProject = msg.payload.projectName;
                if (msg.payload.tabs) this.state.tabs = msg.payload.tabs;
                if (msg.payload.pipelines) this.state.pipelines = msg.payload.pipelines;
                this.renderTabs();
                this.renderTree();
                this.addLog(`📁 Switched to project: ${msg.payload.projectName}`);
                break;
        }
    },

    postMessage(obj) {
        const bridge = window.__promptsBridge || window.chrome?.webview;
        if (bridge) bridge.postMessage(obj);
    },

    sendInitData() {
        this.postMessage({
            type: 'init_complete',
            language: this.state.language
        });
        this.addLog('✅ App initialized');
    },

    loadLanguage(lang) {
        this.state.language = lang;
        fetch(`lang/${lang}.json`)
            .then(r => r.json())
            .then(t => {
                this.state.translations = t;
                this.applyTranslations();
            })
            .catch(() => {
                if (lang !== 'en') this.loadLanguage('en');
            });
    },

    applyTranslations() {
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.dataset.i18n;
            if (this.state.translations[key]) {
                el.textContent = this.state.translations[key];
            }
        });
        document.title = this.state.translations.AppName || 'Prompts';
    },

    // Tab management
    renderTabs() {
        const bar = document.getElementById('tab-bar');
        if (!bar) return;
        bar.innerHTML = '';
        this.state.tabs.forEach((tab, i) => {
            const el = document.createElement('div');
            el.className = 'tab' + (i === this.state.activeTab ? ' active' : '');
            el.textContent = tab.name || 'Untitled';

            // Set tooltip (hover title) to full path
            const appData = this.state.appDataPath || '';
            let fullPath = tab.file || '';
            if (fullPath && !fullPath.includes('\\') && !fullPath.includes('/')) {
                if (appData) {
                    const sep = appData.includes('\\') ? '\\' : '/';
                    fullPath = appData + sep + 'data' + sep + fullPath;
                }
            }
            el.title = fullPath;

            el.onclick = () => this.switchTab(i);
            el.oncontextmenu = (e) => {
                e.stopPropagation();
                e.preventDefault();
                const newName = prompt('Enter new tab name:', tab.name);
                if (newName !== null && newName.trim() !== '') {
                    this.renameTab(i, newName.trim());
                }
            };
            const close = document.createElement('span');
            close.className = 'close';
            close.textContent = '×';
            close.onclick = (e) => { e.stopPropagation(); this.closeTab(i); };
            el.appendChild(close);
            bar.appendChild(el);
        });
        // Add tab button
        const addBtn = document.createElement('div');
        addBtn.className = 'tab';
        addBtn.textContent = '+';
        addBtn.onclick = () => this.newTab();
        bar.appendChild(addBtn);
    },

    switchTab(index) {
        if ('speechSynthesis' in window) {
            window.speechSynthesis.cancel();
            this.clearAllSpeakingStyles();
        }
        this.state.activeTab = index;
        this.renderTabs();
        this.renderTree();
        this.renderList();
    },

    closeTab(index) {
        if (this.state.tabs.length <= 1) return;
        this.state.tabs.splice(index, 1);
        if (this.state.activeTab >= this.state.tabs.length)
            this.state.activeTab = this.state.tabs.length - 1;
        this.postMessage({ type: 'save_session', payload: {
            tabs: this.state.tabs.map(t => ({ name: t.name, file: t.file }))
        }});
        this.renderTabs();
        this.renderTree();
        this.addLog('Tab closed');
    },

    renameTab(index, newName) {
        if (!this.state.tabs[index]) return;
        let targetName = newName.trim();
        if (targetName === '') return;
        if (!targetName.endsWith('.json')) {
            targetName += '.json';
        }
        if (!this.isValidFileName(targetName)) {
            alert('ファイル名に使用できない文字 (\\ / : * ? " < > |) が含まれているか、システム予約名です。');
            return;
        }
        const oldFile = this.state.tabs[index].file;
        let newFile = targetName;
        if (oldFile && (oldFile.includes('/') || oldFile.includes('\\'))) {
            const parts = oldFile.split(/[/\\]/);
            parts[parts.length - 1] = targetName;
            const sep = oldFile.includes('\\') ? '\\' : '/';
            newFile = parts.join(sep);
        }
        this.postMessage({ type: 'rename_file', payload: { oldFile, newFile } });
    },

    newTab() {
        const fileName = 'untitled_' + Date.now() + '.json';
        const rootNode = { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [], nodeType: 'root' };
        this.state.tabs.push({ name: fileName, file: fileName, root: rootNode });
        this.state.activeTab = this.state.tabs.length - 1;
        this.state.currentNodePath = '';

        this.postMessage({ type: 'save_node', payload: { tabFile: fileName, root: rootNode } });
        this.postMessage({ type: 'save_session', payload: {
            tabs: this.state.tabs.map(t => ({ name: t.name, file: t.file }))
        }});

        this.renderTabs();
        this.renderTree();
        this.renderList();
        this.addLog('📄 New tab created');
    },

    openFile() {
        this.postMessage({ type: 'open_file_dialog', filter: 'JSON|*.json' });
    },

    onFileSelected(path) {
        if (path) {
            const idx = this.state.tabs.findIndex(t => t.file === path);
            if (idx >= 0) {
                this.switchTab(idx);
                return;
            }
            this.state.tabs.push({ name: path.split('/').pop().split('\\').pop(), file: path, root: { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [], nodeType: 'root' } });
            this.state.activeTab = this.state.tabs.length - 1;
            this.renderTabs();
            this.addLog('📂 Opened: ' + path);
            this.postMessage({ type: 'load_file_data', payload: { path: path } });
        }
    },

    saveFile() {
        this.addLog('💾 Save requested');
        this.updateNode();
    },
    saveFileAs() { this.addLog('💾 Save As requested'); },
    onPipelineCompleted(meta) {
        this.state.pipelineRun.running = false;
        this.addLog(`✅ Pipeline "${meta.pipelineName}" completed`);

        const safeB64 = str => {
            try { return btoa(unescape(encodeURIComponent(str))); }
            catch { return btoa(str); }
        };

        const outputContent = meta.outputContent || '';
        const autoTitle = outputContent.replace(/\s+/g, ' ').trim().substring(0, 50) + (outputContent.length > 50 ? '...' : '');
        const tab = this.state.tabs[this.state.activeTab];
        let opNodePath = this.state.selectedOpPath || this.state.currentNodePath;
        let opNode = this.getNodeByPath(opNodePath);
        if (opNode && opNode.nodeType === 'data') {
            const parts = opNodePath.split('/');
            if (parts.length >= 3) {
                opNodePath = parts.slice(0, -2).join('/');
                opNode = this.getNodeByPath(opNodePath);
            }
        }
        const opNodeCopy = opNode ? JSON.parse(JSON.stringify(opNode)) : null;
        if (opNodeCopy && opNodeCopy.children) {
            opNodeCopy.children = [];
        }
        const inputAttachmentsCopy = opNode && opNode.inputAttachments ? JSON.parse(JSON.stringify(opNode.inputAttachments)) : [];

        const outputNode = {
            title: safeB64(autoTitle || meta.pipelineName),
            content: safeB64(outputContent),
            mimetype: 'text/plain',
            attachments: [],
            children: [],
            pipelineMeta: JSON.stringify(meta),
            nodeType: 'data',
            originalOpNode: opNodeCopy,
            inputAttachments: inputAttachmentsCopy
        };
        if (tab && opNode) {
            if (!opNode.children) opNode.children = [];
            // Find "Processed" (placeholder) child
            let processedNode = null;
            opNode.children.forEach(child => {
                if (child.nodeType === 'placeholder' || (!child.nodeType && child.title && this.safeAtob(child.title) === 'Processed')) {
                    processedNode = child;
                }
            });
            if (processedNode) {
                if (!processedNode.children) processedNode.children = [];
                processedNode.children.unshift(outputNode);
                this.addLog(`📦 Child node saved under Processed: "${meta.pipelineName}"`);
            } else {
                opNode.children.unshift(outputNode);
                this.addLog(`📦 Child node saved: "${meta.pipelineName}"`);
            }
            this.renderTree();
            this.renderList();
            if (tab.file && tab.root) {
                this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
            }
        }
        this.state.selectedOutputRunIndex = 0;
        this.renderOutput();
    },

    renderPipelineMeta(node) {
        const el = document.getElementById('pipeline-meta-panel');
        if (!el) return;
        if (!node || !node.pipelineMeta) { el.style.display = 'none'; return; }

        let meta;
        try { meta = JSON.parse(node.pipelineMeta); } catch { el.style.display = 'none'; return; }

        el.style.display = '';
        const stepsHtml = (meta.steps || []).map((s, i) => `
            <div class="meta-step">
                <span class="meta-step-num">${i + 1}</span>
                <span class="meta-step-name">${this.escapeHtml(s.name)}</span>
                <span class="meta-step-type">${this.escapeHtml(s.type)}</span>
                ${s.provider ? `<span class="meta-step-provider">${this.escapeHtml(s.provider)}</span>` : ''}
                ${s.model    ? `<span class="meta-step-model">${this.escapeHtml(s.model)}</span>` : ''}
                ${s.tokens   ? `<span class="meta-step-tokens">${s.tokens} tok</span>` : ''}
            </div>`).join('');

        el.innerHTML = `
            <div class="meta-header">
                <span class="meta-pipeline-name">📋 ${this.escapeHtml(meta.pipelineName)}</span>
                <span class="meta-date">${(meta.executedAt||'').replace('T',' ').replace('Z','')}</span>
                <button class="meta-reproduce-btn" data-pipeline="${this.escapeHtml(meta.pipelineName)}">▶ 再実行</button>
                <button class="meta-save-btn">💾 パイプラインとして保存</button>
            </div>
            <div class="meta-steps">${stepsHtml}</div>`;
        // Attach click handlers
        const reproduceBtn = el.querySelector('.meta-reproduce-btn');
        if (reproduceBtn) {
            reproduceBtn.onclick = () => {
                const name = reproduceBtn.dataset.pipeline;
                if (name) this.reproducePipeline(name);
            };
        }
        const saveBtn = el.querySelector('.meta-save-btn');
        if (saveBtn) {
            saveBtn.onclick = () => {
                try {
                    const pipeline = {
                        name: meta.pipelineName || 'pipeline',
                        mode: 'basic',
                        outputMode: 'child',
                        outputNaming: '{pipeline_name}_{timestamp}',
                        steps: (meta.steps || []).map(s => {
                            const step = { name: s.name, type: s.type };
                            if (s.provider) step.provider = s.provider;
                            if (s.model) step.model = s.model;
                            if (s.systemPrompt) step.systemPrompt = s.systemPrompt;
                            if (s.userPrompt) step.userPrompt = s.userPrompt;
                            if (s.temperature) step.temperature = s.temperature;
                            return step;
                        })
                    };
                    this.postMessage({ type: 'save_pipeline', payload: pipeline });
                    this.addLog(`💾 Pipeline "${pipeline.name}" saved from metadata`);
                } catch (e) {
                    this.addLog('⚠ Failed to save pipeline: ' + e.message);
                }
            };
        }
    },



    reproducePipeline(pipelineName) {
        if (!this.state.currentNode) { this.addLog('⚠ ノードを選択してください'); return; }
        const content = this.state.currentNode.content
            ? decodeURIComponent(escape(atob(this.state.currentNode.content))) : '';
        const tab = this.state.tabs[this.state.activeTab];
        this.postMessage({
            type: 'run_pipeline',
            payload: {
                pipelineName,
                nodeId: this.state.currentNodeId || '',
                tabFile: tab ? tab.file : '',
                content
            }
        });
        this.state.pipelineRun.running = true;
        this.addLog(`▶ Reproducing pipeline "${pipelineName}"...`);
    },

    showManualStep(payload) {
        const { index, mode, prompt, content, choices } = payload;
        const modal = document.getElementById('manual-modal');
        document.getElementById('manual-step-badge').textContent = `Step ${index + 1}`;
        document.getElementById('manual-prompt').textContent = prompt || '';
        document.getElementById('manual-prompt').style.display = prompt ? '' : 'none';

        const body = document.getElementById('manual-body');
        const actions = document.getElementById('manual-actions');

        if (mode === 'view') {
            document.getElementById('manual-title').textContent = '確認';
            body.innerHTML = `<div class="manual-view-content">${this.escapeHtml(content)}</div>`;
            actions.innerHTML = `
                <button class="btn-primary" onclick="app.resumeManual(null)">Continue</button>
                <button onclick="app.cancelManual()">Cancel</button>`;

        } else if (mode === 'edit') {
            document.getElementById('manual-title').textContent = '編集';
            body.innerHTML = `<textarea id="manual-edit-area" class="manual-textarea">${this.escapeHtml(content)}</textarea>`;
            actions.innerHTML = `
                <button class="btn-primary" onclick="app.resumeManual(document.getElementById('manual-edit-area').value)">Continue</button>
                <button onclick="app.cancelManual()">Cancel</button>`;

        } else if (mode === 'compare') {
            document.getElementById('manual-title').textContent = '比較選択';
            const branches = payload.branches || [];
            body.innerHTML = `<div class="compare-grid">${
                branches.map(b => `
                    <div class="compare-card">
                        <div class="compare-card-name">${this.escapeHtml(b.name)}</div>
                        <div class="compare-card-content">${this.escapeHtml(b.content)}</div>
                        <button class="btn-primary compare-select-btn"
                            onclick="app.resumeManual(${JSON.stringify(b.content)})">✓ これを選ぶ</button>
                    </div>`).join('')
            }</div>`;
            actions.innerHTML = `<button onclick="app.cancelManual()">Cancel</button>`;

        } else if (mode === 'select') {
            document.getElementById('manual-title').textContent = '選択';
            // Show content preview if any
            body.innerHTML = content
                ? `<div class="manual-view-content">${this.escapeHtml(content)}</div>`
                : '';
            // Build choice buttons
            const list = (choices && choices.length) ? choices : [
                { label: 'Continue', action: 'next_step' },
                { label: 'Cancel',   action: 'cancel' }
            ];
            actions.innerHTML = list.map(c =>
                `<button class="${c.action === 'cancel' ? '' : 'btn-primary'}"
                    onclick="app.resumeManualChoice(${JSON.stringify(c)})"
                >${this.escapeHtml(c.label)}</button>`
            ).join('');
        }

        modal.classList.add('visible');
        this.addLog(`⏸ Manual step ${index + 1} — waiting for user (${mode})`);
    },

    resumeManual(content) {
        document.getElementById('manual-modal').classList.remove('visible');
        this.postMessage({ type: 'manual_step_resume', payload: { content: content ?? '' } });
    },

    resumeManualChoice(choice) {
        document.getElementById('manual-modal').classList.remove('visible');
        if (choice.action === 'cancel') {
            this.postMessage({ type: 'manual_step_cancel' });
        } else {
            this.postMessage({ type: 'manual_step_resume', payload: { content: choice.label, action: choice.action, gotoStep: choice.index } });
        }
    },

    cancelManual() {
        document.getElementById('manual-modal').classList.remove('visible');
        this.postMessage({ type: 'manual_step_cancel' });
    },

    // ── Pipeline Wizard Step ──────────────────────────────────────
    pwState_: { step: 0, values: {}, wizardData: null, index: 0 },

    showPipelineWizardStep(payload) {
        const { index, wizard, content } = payload;
        let wizardData = payload.wizardData;
        if (!wizardData && wizard) {
            // Try to fetch from frontend/wizards/
            this.addLog(`📋 Loading wizard: ${wizard}`);
            fetch(`wizards/${wizard}.json`).then(r => r.json()).then(data => {
                this._renderPipelineWizard(index, data, content);
            }).catch(err => {
                this.addLog(`❌ Failed to load wizard "${wizard}": ${err.message}`);
                this.postMessage({ type: 'wizard_step_resume', payload: { values: {} } });
            });
            return;
        }
        if (typeof wizardData === 'string') {
            try { wizardData = JSON.parse(wizardData); } catch { this.addLog('⚠ Failed to parse wizard data'); }
        }
        this._renderPipelineWizard(index, wizardData, content);
    },

    _renderPipelineWizard(index, wizardData, content) {
        if (!wizardData || !wizardData.steps) {
            this.addLog('⚠ Invalid wizard definition — skipping');
            this.postMessage({ type: 'wizard_step_resume', payload: { values: {} } });
            return;
        }
        this.pwState_ = { step: 0, values: {}, wizardData, index };
        const modal = document.getElementById('wizard-modal');
        if (!modal) return;

        // Override wizard buttons for pipeline mode
        const skipBtn = document.getElementById('wizard-skip');
        const prevBtn = document.getElementById('wizard-prev');
        const nextBtn = document.getElementById('wizard-next');

        skipBtn.textContent = 'キャンセル';
        skipBtn.onclick = () => {
            modal.classList.remove('visible');
            this.postMessage({ type: 'wizard_step_resume', payload: { values: {} } });
        };

        prevBtn.onclick = () => this.pwPrev();
        nextBtn.onclick = () => this.pwNext();
        prevBtn.style.visibility = 'hidden';

        modal.classList.add('visible');
        this.pwRenderStep();
    },

    pwRenderStep() {
        const s = this.pwState_;
        const stepDef = s.wizardData.steps[s.step];
        if (!stepDef) {
            // All steps done — submit
            this._pwFinish();
            return;
        }

        const total = s.wizardData.steps.length;
        const cur = s.step;

        // Progress dots
        const progressEl = document.getElementById('wizard-progress');
        if (progressEl) {
            progressEl.innerHTML = Array.from({length: total}, (_, i) =>
                `<span class="wizard-dot${i === cur ? ' active' : i < cur ? ' done' : ''}"></span>`
            ).join('');
        }

        const bodyEl = document.getElementById('wizard-body');
        if (!bodyEl) return;

        const icon = s.wizardData.name ? '🚀' : '📋';
        const title = stepDef.prompt || `Step ${cur + 1}`;
        const currentVal = s.values[stepDef.id] || stepDef.default || '';

        let inputHtml = '';
        if (stepDef.type === 'choice') {
            const opts = stepDef.options || {};
            inputHtml = Object.entries(opts).map(([k, v]) =>
                `<label class="pw-choice${currentVal === k ? ' selected' : ''}"
                    onclick="app.pwSetValue('${stepDef.id}','${k}')">
                    <input type="radio" name="pw-${stepDef.id}" value="${k}"${currentVal === k ? ' checked' : ''}>
                    ${v}
                </label>`
            ).join('');
        } else if (stepDef.type === 'confirm') {
            inputHtml = `
                <label class="pw-choice${currentVal === 'y' || currentVal === '' ? ' selected' : ''}"
                    onclick="app.pwSetValue('${stepDef.id}','y')">
                    <input type="radio" name="pw-${stepDef.id}" value="y"${currentVal === 'y' || currentVal === '' ? ' checked' : ''}> Yes
                </label>
                <label class="pw-choice${currentVal === 'n' ? ' selected' : ''}"
                    onclick="app.pwSetValue('${stepDef.id}','n')">
                    <input type="radio" name="pw-${stepDef.id}" value="n"${currentVal === 'n' ? ' checked' : ''}> No
                </label>`;
        } else if (stepDef.type === 'password') {
            inputHtml = `<input type="password" id="pw-input" class="sw-input" value="${this.escapeHtml(currentVal)}"
                oninput="app.pwSetValue('${stepDef.id}', this.value)" placeholder="${stepDef.default || ''}">`;
        } else {
            inputHtml = `<input type="text" id="pw-input" class="sw-input" value="${this.escapeHtml(currentVal)}"
                oninput="app.pwSetValue('${stepDef.id}', this.value)" placeholder="${stepDef.default || ''}">`;
        }

        bodyEl.innerHTML = `
            <div class="wizard-icon">${icon}</div>
            <h2 class="wizard-title">${this.escapeHtml(title)}</h2>
            <div class="pw-input-area">${inputHtml}</div>`;

        const prevBtn = document.getElementById('wizard-prev');
        const nextBtn = document.getElementById('wizard-next');
        if (prevBtn) prevBtn.style.visibility = cur === 0 ? 'hidden' : '';
        if (nextBtn) {
            nextBtn.textContent = cur === total - 1 ? '✓ 完了' : '次へ →';
        }
    },

    pwSetValue(id, value) {
        this.pwState_.values[id] = value;
        // Re-render choice highlights
        const input = document.getElementById('pw-input');
        if (input && input.id === 'pw-input') {
            // text input handled via oninput
        }
        // Update radio highlights
        document.querySelectorAll('.pw-choice').forEach(el => {
            const radio = el.querySelector('input[type="radio"]');
            if (radio && radio.checked) {
                el.classList.add('selected');
            } else {
                el.classList.remove('selected');
            }
        });
    },

    pwNext() {
        // Validate current step
        const s = this.pwState_;
        const stepDef = s.wizardData.steps[s.step];
        const val = s.values[stepDef.id] !== undefined ? s.values[stepDef.id] : stepDef.default || '';
        if (stepDef.validate && val) {
            try {
                const re = new RegExp(stepDef.validate);
                if (!re.test(val)) {
                    this.addLog(`⚠ Invalid input for "${stepDef.id}"`);
                    return;
                }
            } catch { this.addLog('⚠ Failed to parse wizard step input'); }
        }
        // Apply action
        if (stepDef.action === 'setLanguage') {
            this.state.language = val;
        }
        s.values[stepDef.id] = val;
        s.step++;
        this.pwRenderStep();
    },

    pwPrev() {
        const s = this.pwState_;
        if (s.step > 0) {
            s.step--;
            this.pwRenderStep();
        }
    },

    _pwFinish() {
        const s = this.pwState_;
        document.getElementById('wizard-modal').classList.remove('visible');
        // Apply output mapping
        if (s.wizardData.outputMapping) {
            for (const [targetField, mapping] of Object.entries(s.wizardData.outputMapping)) {
                const sourceVal = s.values[mapping.source];
                if (sourceVal && mapping.map && mapping.map[sourceVal]) {
                    s.values[targetField] = mapping.map[sourceVal];
                }
            }
        }
        this.addLog(`✅ Wizard "${s.wizardData.name || 'pipeline'}" completed`);
        this.postMessage({ type: 'wizard_step_resume', payload: { values: s.values } });
    },

    escapeHtml(s) {
        return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    },

    safeAtob(str) {
        if (!str) return '';
        try {
            return decodeURIComponent(escape(atob(str)));
        } catch {
            try {
                return atob(str);
            } catch {
                return str;
            }
        }
    },

    patchNodeTypes(node, isRoot = false) {
        if (!node.nodeType) {
            if (isRoot) {
                node.nodeType = 'root';
            } else if (node.pipelineMeta !== undefined) {
                node.nodeType = 'data';
            } else if (node.title && this.safeAtob(node.title) === 'Processed') {
                node.nodeType = 'placeholder';
            } else {
                node.nodeType = 'assemble';
            }
        }
        if (node.children) {
            node.children.forEach(child => this.patchNodeTypes(child, false));
        }
    },

    isDataNodePath(path) {
        if (!path) return false;
        const node = this.getNodeByPath(path);
        if (node) {
            // pipelineMeta が存在すれば確実に data node
            if (node.pipelineMeta !== undefined) return true;
            if (node.nodeType) return node.nodeType === 'data';
        }
        const parts = path.split('/');
        for (let i = 1; i < parts.length; i++) {
            const ancestorPath = parts.slice(0, i).join('/');
            const ancestorNode = this.getNodeByPath(ancestorPath);
            if (ancestorNode && (ancestorNode.nodeType === 'placeholder' ||
                (!ancestorNode.nodeType && ancestorNode.title && this.safeAtob(ancestorNode.title) === 'Processed'))) {
                return true;
            }
        }
        return false;
    },

    getLogicalOpPath(path) {
        if (!path) return '';
        const node = this.getNodeByPath(path);
        if (node && (node.nodeType === 'data' || (!node.nodeType && node.pipelineMeta !== undefined))) {
            const lastSlash = path.lastIndexOf('/');
            if (lastSlash < 0) return '';
            const parentPath = lastSlash === 0 ? '' : path.substring(0, lastSlash);
            const parentNode = this.getNodeByPath(parentPath || null);
            if (parentNode && (parentNode.nodeType === 'placeholder' ||
                (!parentNode.nodeType && parentNode.title && this.safeAtob(parentNode.title) === 'Processed'))) {
                const lastSlash2 = parentPath.lastIndexOf('/');
                if (lastSlash2 < 0) return '';
                return lastSlash2 === 0 ? '' : parentPath.substring(0, lastSlash2);
            }
            return parentPath;
        }
        return path;
    },

    isAncestor(ancestor, descendant) {
        if (!ancestor || !descendant) return false;
        const a = ancestor.split('/').filter(p => p !== '');
        const d = descendant.split('/').filter(p => p !== '');
        if (a.length >= d.length) return false;
        return a.every((p, i) => p === d[i]);
    },

    getDOMElementForPath(path) {
        if (path === '') return null;
        const escapedPath = path.replace(/'/g, "\\'");
        const els = document.querySelectorAll('.tree-node');
        for (let i = 0; i < els.length; i++) {
            const attr = els[i].getAttribute('onclick') || '';
            if (attr.includes(`selectNode('${escapedPath}')`) || attr.includes(`selectNode("${escapedPath}")`)) {
                return els[i];
            }
        }
        return null;
    },

    showConfig() {
        const panel = document.getElementById('config-panel');
        if (!panel) return;
        panel.classList.add('visible');
        this.postMessage({ type: 'get_providers' });
        this.initConfigDrag();
        this.addLog('⚙ Config opened');
    },

    closeConfig() {
        const panel = document.getElementById('config-panel');
        if (!panel) return;
        // Auto-save on close
        this.saveProviders();
        this.saveRecipes();
        this.postMessage({ type: 'save_config', payload: {
            historyRetention: this.state.historyRetention,
            defaultProvider: this.state.defaultProvider,
            defaultModel: this.state.defaultModel
        }});
        panel.classList.remove('visible');
    },

    showRecipeManager() {
        const modal = document.getElementById('recipe-modal');
        if (!modal) return;
        modal.classList.add('visible');
        this.postMessage({ type: 'get_providers' });
        this.renderRecipeManager();
    },

    closeRecipeManager() {
        this.saveRecipes();
        document.getElementById('recipe-modal')?.classList.remove('visible');
    },

    renderRecipeManager() {
        const body = document.getElementById('recipe-modal-body');
        if (!body) return;
        const recipes = this.state.recipes || [];
        const editingIdx = this.state.editingRecipeIndex;
        const providers = Object.keys(this.state.providers || {});
        let html = recipes.map((r, i) => {
            if (i === editingIdx) return this._renderRecipeEditForm(r, i, providers);
            return this._renderRecipeCard(r, i);
        }).join('');
        html += this._renderRecipeAddForm(providers);
        body.innerHTML = html;
    },

    _renderRecipeCard(r, i) {
        const typeIcon = r.type === 'command' ? '⚙️' : '🤖';
        let detail = '';
        if (r.type === 'command') {
            detail = `<span class="recipe-mgr-item-detail-text">⚙️ ${this.escapeHtml(r.command || '')}</span>`;
        } else {
            detail = `<span class="recipe-mgr-item-detail-text">${this.escapeHtml(r.provider)}${r.model ? ' / ' + this.escapeHtml(r.model) : ''}</span>`;
        }
        return `
            <div class="recipe-mgr-item">
                <div class="recipe-mgr-item-header">
                    <span class="recipe-mgr-item-name">${typeIcon} ${this.escapeHtml(r.name)}</span>
                    <span class="recipe-mgr-item-type-badge ${r.type === 'command' ? 'type-command' : 'type-ai'}">${r.type === 'command' ? '⚙️ CMD' : '🤖 AI'}</span>
                </div>
                <div class="recipe-mgr-item-detail">${detail}</div>
                <div class="recipe-mgr-item-actions">
                    <button class="recipe-btn" onclick="app.editRecipe(${i})">✏️ Edit</button>
                    <button class="recipe-btn recipe-btn-danger" onclick="app.deleteRecipe(${i});app.renderRecipeManager()">🗑 Delete</button>
                    <span class="recipe-mgr-item-reorder">
                        <button class="recipe-btn recipe-btn-sm" onclick="app.moveRecipeUp(${i});app.renderRecipeManager()" ${i === 0 ? 'disabled' : ''}>▲</button>
                        <button class="recipe-btn recipe-btn-sm" onclick="app.moveRecipeDown(${i});app.renderRecipeManager()" ${i === (this.state.recipes || []).length - 1 ? 'disabled' : ''}>▼</button>
                    </span>
                </div>
            </div>`;
    },

    _renderRecipeEditForm(r, i, providers) {
        const isCommand = r.type === 'command';
        let fields = '';
        if (isCommand) {
            fields = `<input type="text" id="edit-command" value="${this.escapeHtml(r.command || '')}" placeholder="Command (e.g. echo hello)" class="recipe-input" style="flex:3">`;
        } else {
            fields = `
                <select id="edit-provider" class="recipe-select" style="flex:1" onchange="app.fetchModelsForProvider(this.value)">
                    ${providers.map(k => `<option value="${this.escapeHtml(k)}" ${k === r.provider ? 'selected' : ''}>${this.escapeHtml(k)}</option>`).join('')}
                </select>
                <input type="text" id="edit-model" value="${this.escapeHtml(r.model)}" placeholder="Model" class="recipe-input" style="flex:1">
                <button class="recipe-btn" onclick="app.fetchModelsForProvider(document.getElementById('edit-provider')?.value)" style="font-size:10px;padding:2px 6px">🔄</button>`;
        }
        return `
            <div class="recipe-mgr-item recipe-mgr-editing">
                <div class="recipe-edit-row">
                    <input type="text" id="edit-name" value="${this.escapeHtml(r.name)}" placeholder="Recipe name" class="recipe-input" style="flex:2">
                    ${fields}
                </div>
                ${!isCommand ? `
                <div class="recipe-edit-row">
                    <textarea id="edit-system-prompt" placeholder="System prompt (optional)" class="recipe-textarea" style="flex:3">${this.escapeHtml(r.systemPrompt || '')}</textarea>
                    <input type="number" id="edit-temperature" value="${r.temperature ?? 0.7}" min="0" max="2" step="0.1" placeholder="Temp" class="recipe-input" style="width:60px">
                </div>
                <div class="recipe-edit-row">
                    <textarea id="edit-custom-params" placeholder="Custom parameters (JSON, e.g. {&quot;negative_prompt&quot;: &quot;ugly&quot;})" class="recipe-textarea" style="flex:3">${this.escapeHtml(r.customParams ? JSON.stringify(r.customParams) : '')}</textarea>
                </div>` : ''}
                <div class="recipe-edit-actions">
                    <button class="recipe-btn recipe-btn-primary" onclick="app.saveEditRecipe(${i})">Save</button>
                    <button class="recipe-btn" onclick="app.cancelEditRecipe()">Cancel</button>
                </div>
            </div>`;
    },

    _renderRecipeAddForm(providers) {
        return `
            <div class="recipe-mgr-add">
                <div class="recipe-add-title">+ New Recipe</div>
                <div class="recipe-edit-row">
                    <input type="text" id="rm-name" placeholder="Recipe name" class="recipe-input" style="flex:2">
                    <select id="rm-type" class="recipe-select" style="flex:0 0 110px" onchange="app.onNewRecipeTypeChange()">
                        <option value="ai">🤖 AI</option>
                        <option value="command">⚙️ Command</option>
                    </select>
                </div>
                <div id="rm-ai-fields">
                    <div class="recipe-edit-row">
                        <select id="rm-provider" class="recipe-select" style="flex:1" onchange="app.fetchModelsForProvider(this.value)">
                            ${providers.map(k => `<option value="${this.escapeHtml(k)}">${this.escapeHtml(k)}</option>`).join('')}
                        </select>
                        <input type="text" id="rm-model" placeholder="Model" class="recipe-input" style="flex:1">
                        <button class="recipe-btn" onclick="app.fetchModelsForProvider(document.getElementById('rm-provider')?.value)" style="font-size:10px;padding:2px 6px">🔄</button>
                        <input type="number" id="rm-temperature" placeholder="Temp" value="0.7" min="0" max="2" step="0.1" class="recipe-input" style="width:70px">
                    </div>
                    <div class="recipe-edit-row">
                        <textarea id="rm-system-prompt" placeholder="System prompt (optional)" class="recipe-textarea"></textarea>
                    </div>
                    <div class="recipe-edit-row">
                        <textarea id="rm-custom-params" placeholder="Custom parameters (JSON, e.g. {&quot;negative_prompt&quot;: &quot;ugly&quot;})" class="recipe-textarea"></textarea>
                    </div>
                </div>
                <div id="rm-cmd-fields" style="display:none">
                    <div class="recipe-edit-row">
                        <input type="text" id="rm-command" placeholder="Command (e.g. echo Hello World)" class="recipe-input" style="flex:1">
                    </div>
                </div>
                <div class="recipe-edit-actions">
                    <button class="recipe-btn recipe-btn-primary" onclick="app.addRecipeFromManager()">+ Add</button>
                </div>
            </div>`;
    },

    onNewRecipeTypeChange() {
        const type = document.getElementById('rm-type')?.value;
        const aiFields = document.getElementById('rm-ai-fields');
        const cmdFields = document.getElementById('rm-cmd-fields');
        if (!aiFields || !cmdFields) return;
        aiFields.style.display = type === 'command' ? 'none' : '';
        cmdFields.style.display = type === 'command' ? '' : 'none';
    },

    fetchModelsForProvider(provider) {
        if (!provider) return;
        this.postMessage({ type: 'fetch_models', payload: { provider } });
    },

    updateRecipeManagerModels(provider) {
        if (!provider) return;
        const models = (this.state.providerModels || {})[provider] || [];
        if (models.length === 0) return;
        for (const id of ['rm-model', 'edit-model']) {
            const el = document.getElementById(id);
            if (!el || el.tagName === 'SELECT') continue;
            const currentVal = el.value;
            const opts = '<option value="">Select model...</option>' +
                models.map(m => `<option value="${this.escapeHtml(m)}" ${m === currentVal ? 'selected' : ''}>${this.escapeHtml(m)}</option>`).join('');
            const sel = document.createElement('select');
            sel.id = id;
            sel.className = el.className;
            sel.style.cssText = el.style.cssText;
            sel.innerHTML = opts;
            el.parentNode.replaceChild(sel, el);
        }
    },

    addRecipeFromManager() {
        const name = document.getElementById('rm-name')?.value?.trim();
        if (!name) return;
        const type = document.getElementById('rm-type')?.value || 'ai';
        if (!this.state.recipes) this.state.recipes = [];
        if (type === 'command') {
            const command = document.getElementById('rm-command')?.value?.trim() || '';
            this.state.recipes.push({ type, name, command, provider: '', model: '', temperature: 0.7, systemPrompt: '', customParams: {} });
        } else {
            const provider = document.getElementById('rm-provider')?.value || 'openai';
            const model = document.getElementById('rm-model')?.value?.trim() || '';
            const temp = parseFloat(document.getElementById('rm-temperature')?.value || '0.7');
            const systemPrompt = document.getElementById('rm-system-prompt')?.value?.trim() || '';
            const customParamsStr = document.getElementById('rm-custom-params')?.value?.trim() || '';
            let customParams = {};
            if (customParamsStr) {
                try {
                    customParams = JSON.parse(customParamsStr);
                } catch (e) {
                    alert('Invalid JSON in custom parameters');
                    return;
                }
            }
            this.state.recipes.push({ type, name, provider, model, temperature: temp, systemPrompt, command: '', customParams });
        }
        this.renderRecipeManager();
        this.saveRecipes();
        this.addLog(`📋 Recipe added: ${name}`);
    },

    editRecipe(index) {
        this.state.editingRecipeIndex = index;
        this.renderRecipeManager();
    },

    saveEditRecipe(index) {
        const recipes = this.state.recipes || [];
        if (index < 0 || index >= recipes.length) return;
        const name = document.getElementById('edit-name')?.value?.trim();
        if (!name) return;
        const recipe = recipes[index];
        recipe.name = name;
        if (recipe.type === 'command') {
            recipe.command = document.getElementById('edit-command')?.value?.trim() || '';
        } else {
            recipe.provider = document.getElementById('edit-provider')?.value || 'openai';
            recipe.model = document.getElementById('edit-model')?.value?.trim() || '';
            recipe.systemPrompt = document.getElementById('edit-system-prompt')?.value?.trim() || '';
            recipe.temperature = parseFloat(document.getElementById('edit-temperature')?.value || '0.7');
            const customParamsStr = document.getElementById('edit-custom-params')?.value?.trim() || '';
            let customParams = {};
            if (customParamsStr) {
                try {
                    customParams = JSON.parse(customParamsStr);
                } catch (e) {
                    alert('Invalid JSON in custom parameters');
                    return;
                }
            }
            recipe.customParams = customParams;
        }
        this.state.editingRecipeIndex = -1;
        this.renderRecipeManager();
        this.renderPrompt();
        this.saveRecipes();
        this.addLog(`✏️ Recipe saved: ${name}`);
    },

    cancelEditRecipe() {
        this.state.editingRecipeIndex = -1;
        this.renderRecipeManager();
    },

    initConfigDrag() {
        const panel = document.getElementById('config-panel');
        const handle = document.getElementById('config-drag-handle');
        if (!panel || !handle) return;
        // Remove old listeners
        const newHandle = handle.cloneNode(true);
        handle.parentNode.replaceChild(newHandle, handle);
        let dragging = false, startX, startY, origX, origY;
        newHandle.onmousedown = (e) => {
            if (e.target.tagName === 'BUTTON') return;
            dragging = true;
            const rect = panel.getBoundingClientRect();
            startX = e.clientX; startY = e.clientY;
            origX = rect.left; origY = rect.top;
            panel.style.left = origX + 'px';
            panel.style.top = origY + 'px';
            panel.style.right = 'auto';
            e.preventDefault();
        };
        document.onmousemove = (e) => {
            if (!dragging) return;
            panel.style.left = (origX + e.clientX - startX) + 'px';
            panel.style.top = (origY + e.clientY - startY) + 'px';
        };
        document.onmouseup = () => { dragging = false; };
    },

    switchConfigTab(name, btn) {
        document.querySelectorAll('.config-tab-panel').forEach(p => p.style.display = 'none');
        document.querySelectorAll('.config-tab').forEach(b => b.classList.remove('active'));
        document.getElementById('config-tab-' + name).style.display = '';
        btn.classList.add('active');
        if (name === 'general') this.renderGeneralConfig();
    },

    renderGeneralConfig() {
        // Default provider/model
        const provEl = document.getElementById('config-default-provider');
        if (provEl) {
            const providers = this.state.providers || {};
            provEl.innerHTML = Object.keys(providers).map(k =>
                `<option value="${this.escapeHtml(k)}" ${k === this.state.defaultProvider ? 'selected' : ''}>${this.escapeHtml(k)}</option>`
            ).join('');
        }
        const modelEl = document.getElementById('config-default-model');
        if (modelEl) modelEl.value = this.state.defaultModel || '';
        // History retention
        const retentionEl = document.getElementById('config-history-retention');
        if (retentionEl) retentionEl.value = this.state.historyRetention || 50;

        // List named chests
        const chestListEl = document.getElementById('config-chest-list');
        if (!chestListEl) return;
        const chestNames = this.state.chestList || [];
        if (chestNames.length === 0) {
            chestListEl.innerHTML = `<div class="empty" data-i18n="EmptyChests">No chests yet</div>`;
        } else {
            chestListEl.innerHTML = chestNames.map(n =>
                `<div class="chest-item"><span class="chest-name">📦 ${this.escapeHtml(n)}</span>
                <button class="chest-load-btn" onclick="app.loadFromChestConfig('${this.escapeHtml(n)}')">📂 Load</button>
                <button class="chest-delete-btn" onclick="app.deleteChest('${this.escapeHtml(n)}')">✕</button></div>`
            ).join('');
        }
    },

    // ── Recipes ──────────────────────────────────────────────
    renderRecipesConfig() {
        const el = document.getElementById('recipe-list');
        if (!el) return;
        const recipes = this.state.recipes || [];
        let html = recipes.map((r, i) => `
            <div class="recipe-item">
                <div class="recipe-item-header">
                    <span class="recipe-item-name">${r.type === 'command' ? '⚙️' : '🤖'} ${this.escapeHtml(r.name)}</span>
                    <span class="recipe-item-type-badge ${r.type === 'command' ? 'type-command' : 'type-ai'}">${r.type === 'command' ? 'CMD' : 'AI'}</span>
                </div>
                <div class="recipe-item-detail">
                    ${r.type === 'command'
                        ? '⚙️ ' + this.escapeHtml(r.command || '')
                        : this.escapeHtml(r.provider) + (r.model ? ' / ' + this.escapeHtml(r.model) : '')}
                </div>
                <div class="recipe-item-actions">
                    <button class="recipe-sm-btn" onclick="app.editRecipe(${i})">✏️</button>
                    <button class="recipe-sm-btn recipe-sm-btn-danger" onclick="app.deleteRecipe(${i})">🗑</button>
                    <button class="recipe-sm-btn" onclick="app.moveRecipeUp(${i});app.renderRecipesConfig()" ${i === 0 ? 'disabled' : ''}>▲</button>
                    <button class="recipe-sm-btn" onclick="app.moveRecipeDown(${i});app.renderRecipesConfig()" ${i === recipes.length - 1 ? 'disabled' : ''}>▼</button>
                </div>
            </div>
        `).join('');
        html += `
            <div class="recipe-add-row">
                <div class="recipe-add-title">+ New Recipe</div>
                <div class="recipe-config-row">
                    <input type="text" id="new-recipe-name" placeholder="Recipe name" class="recipe-config-input" style="flex:2">
                    <select id="new-recipe-type" class="recipe-config-select" style="flex:0 0 90px" onchange="app.onConfigRecipeTypeChange()">
                        <option value="ai">🤖 AI</option>
                        <option value="command">⚙️ CMD</option>
                    </select>
                </div>
                <div id="new-ai-fields">
                    <div class="recipe-config-row">
                        <select id="new-recipe-provider" class="recipe-config-select" style="flex:1">
                            ${Object.keys(this.state.providers || {}).map(k => `<option value="${this.escapeHtml(k)}">${this.escapeHtml(k)}</option>`).join('')}
                        </select>
                        <input type="text" id="new-recipe-model" placeholder="model" class="recipe-config-input" style="flex:1">
                        <input type="number" id="new-recipe-temperature" placeholder="Temp" value="0.7" min="0" max="2" step="0.1" class="recipe-config-input" style="width:60px">
                    </div>
                    <div class="recipe-config-row">
                        <input type="text" id="new-recipe-system-prompt" placeholder="System prompt (optional)" class="recipe-config-input" style="flex:3">
                    </div>
                </div>
                <div id="new-cmd-fields" style="display:none">
                    <div class="recipe-config-row">
                        <input type="text" id="new-recipe-command" placeholder="Command (e.g. echo Hello)" class="recipe-config-input" style="flex:1">
                    </div>
                </div>
                <div class="recipe-edit-actions" style="margin-top:4px">
                    <button class="recipe-btn recipe-btn-primary" style="font-size:11px;padding:2px 10px" onclick="app.addRecipe()">+ Add Recipe</button>
                </div>
            </div>`;
        el.innerHTML = html;
    },

    onConfigRecipeTypeChange() {
        const type = document.getElementById('new-recipe-type')?.value;
        const aiFields = document.getElementById('new-ai-fields');
        const cmdFields = document.getElementById('new-cmd-fields');
        if (type === 'command') {
            aiFields.style.display = 'none';
            cmdFields.style.display = '';
        } else {
            aiFields.style.display = '';
            cmdFields.style.display = 'none';
        }
    },

    addRecipe() {
        const name = document.getElementById('new-recipe-name')?.value?.trim();
        if (!name) return;
        const type = document.getElementById('new-recipe-type')?.value || 'ai';
        if (!this.state.recipes) this.state.recipes = [];
        if (type === 'command') {
            const command = document.getElementById('new-recipe-command')?.value?.trim() || '';
            this.state.recipes.push({ type, name, command, provider: '', model: '', temperature: 0.7, systemPrompt: '' });
        } else {
            const provider = document.getElementById('new-recipe-provider')?.value || 'openai';
            const model = document.getElementById('new-recipe-model')?.value?.trim() || '';
            const systemPrompt = document.getElementById('new-recipe-system-prompt')?.value?.trim() || '';
            const temp = parseFloat(document.getElementById('new-recipe-temperature')?.value || '0.7');
            this.state.recipes.push({ type, name, provider, model, temperature: temp, systemPrompt, command: '' });
        }
        this.renderRecipesConfig();
        this.saveRecipes();
        this.addLog(`📋 Recipe added: ${name}`);
    },

    deleteRecipe(index) {
        const recipes = this.state.recipes || [];
        if (index < 0 || index >= recipes.length) return;
        const name = recipes[index].name;
        if (!confirm(`Delete recipe "${name}"?`)) return;
        recipes.splice(index, 1);
        if (this.state.selectedRecipe === name) this.state.selectedRecipe = '';
        this.renderRecipesConfig();
        this.renderPrompt();
        this.updateRecipeBadge();
        this.saveRecipes();
        this.addLog(`🗑 Recipe deleted: ${name}`);
    },

    moveRecipeUp(index) {
        const recipes = this.state.recipes || [];
        if (index <= 0 || index >= recipes.length) return;
        [recipes[index - 1], recipes[index]] = [recipes[index], recipes[index - 1]];
        if (this.state.editingRecipeIndex === index) this.state.editingRecipeIndex = index - 1;
        else if (this.state.editingRecipeIndex === index - 1) this.state.editingRecipeIndex = index;
        this.saveRecipes();
    },

    moveRecipeDown(index) {
        const recipes = this.state.recipes || [];
        if (index < 0 || index >= recipes.length - 1) return;
        [recipes[index], recipes[index + 1]] = [recipes[index + 1], recipes[index]];
        if (this.state.editingRecipeIndex === index) this.state.editingRecipeIndex = index + 1;
        else if (this.state.editingRecipeIndex === index + 1) this.state.editingRecipeIndex = index;
        this.saveRecipes();
    },

    saveRecipes() {
        const recipes = this.state.recipes || [];
        this.postMessage({ type: 'save_recipes', payload: recipes });
    },

    setDefaultProvider(val) {
        this.state.defaultProvider = val;
        this.renderRecipesConfig();
    },

    setDefaultModel(val) {
        this.state.defaultModel = val;
    },

    getRecipeSettings() {
        const recipeName = this.state.selectedRecipe;
        if (recipeName) {
            const recipe = (this.state.recipes || []).find(r => r.name === recipeName);
            if (recipe) return recipe;
        }
        return {
            type: 'ai',
            provider: this.state.defaultProvider || 'openai',
            model: this.state.defaultModel || '',
            temperature: 0.7,
            systemPrompt: '',
            command: ''
        };
    },

    selectRecipe(index) {
        const recipes = this.state.recipes || [];
        if (index < 0 || index >= recipes.length) return;
        this.state.selectedRecipe = recipes[index].name;
        // Persist per-node recipe selection on logical parent op node
        let node = this.getNodeByPath(this.state.selectedOpPath || this.state.currentNodePath);
        if (node) {
            if (node.nodeType === 'data' && node.originalOpNode) {
                node = node.originalOpNode;
            }
            node.selectedRecipe = this.state.selectedRecipe;
            this.saveCurrentTab();
        }
        this.renderPrompt();
        this.updateRecipeBadge();
        this.addLog(`📋 Recipe selected: ${this.state.selectedRecipe}`);
    },

    chooseRecipe() {
        const recipes = this.state.recipes || [];
        if (recipes.length === 0) {
            this.addLog('⚠ No recipes defined. Create one in Config > Recipes.');
            return;
        }
        const current = this.state.selectedRecipe;
        const names = recipes.map(r => r.name);
        const idx = current ? names.indexOf(current) : -1;
        const nextIdx = (idx + 1) % names.length;
        this.state.selectedRecipe = names[nextIdx];
        let node = this.getNodeByPath(this.state.selectedOpPath || this.state.currentNodePath);
        if (node) {
            if (node.nodeType === 'data' && node.originalOpNode) {
                node = node.originalOpNode;
            }
            node.selectedRecipe = this.state.selectedRecipe;
            this.saveCurrentTab();
        }
        this.renderPrompt();
        this.updateRecipeBadge();
        this.addLog(`📋 Recipe: ${this.state.selectedRecipe}`);
    },

    updateRecipeBadge() {
        const badge = document.getElementById('recipe-badge');
        if (!badge) return;
        const name = this.state.selectedRecipe;
        if (name) {
            const recipe = (this.state.recipes || []).find(r => r.name === name);
            const icon = recipe?.type === 'command' ? '⚙️' : '🤖';
            badge.textContent = ` ${icon} ${name}`;
            badge.style.display = '';
        } else {
            badge.textContent = '';
            badge.style.display = 'none';
        }
    },

    adjustRetention(delta) {
        const el = document.getElementById('config-history-retention');
        if (!el) return;
        let val = parseInt(el.value) || 50;
        val = Math.max(10, Math.min(500, val + delta));
        el.value = val;
        this.setHistoryRetention(val);
    },

    loadFromChestConfig(name) {
        this.addLog(`📂 Loading from chest "${name}"...`);
        this.postMessage({ type: 'select_input_source', payload: { source: 'chest', chestName: name } });
    },

    deleteChest(name) {
        this.addLog(`🗑 Chest "${name}" will be deleted on next GC`);
    },

    clearStorageChest() {
        if (!confirm('Empty Storage Chest? This will permanently delete all discarded data.')) return;
        this.addLog('🧹 Storage chest cleared');
    },

    onProvidersResult(providers, customMetadata) {
        if (customMetadata) {
            this.state.customMetadata = customMetadata;
        } else {
            customMetadata = this.state.customMetadata || {};
        }

        const DEFAULT_PROVIDERS = [
            { id: 'openai',       label: 'OpenAI',             defaultUrl: 'https://api.openai.com/v1', defaultFormat: 'openai' },
            { id: 'anthropic',    label: 'Anthropic',          defaultUrl: 'https://api.anthropic.com',  defaultFormat: 'anthropic' },
            { id: 'gemini',       label: 'Gemini',             defaultUrl: 'https://googleapis.com',     defaultFormat: 'gemini' },
            { id: 'ollama',       label: 'Ollama',             defaultUrl: 'http://localhost:11434',     defaultFormat: 'ollama' },
            { id: 'openai-image', label: 'OpenAI Image (DALL-E)', defaultUrl: 'https://api.openai.com/v1', defaultFormat: 'openai-image' },
            { id: 'replicate',    label: 'Replicate',          defaultUrl: 'https://api.replicate.com',  defaultFormat: 'replicate' },
            { id: 'fal-ai',       label: 'Fal.ai',             defaultUrl: 'https://queue.fal.run',      defaultFormat: 'fal-ai' },
        ];
        // Collect all provider IDs: predefined + any custom ones from data
        const knownIds = DEFAULT_PROVIDERS.map(p => p.id);
        const allIds = [...knownIds];
        if (providers) {
            Object.keys(providers).forEach(id => {
                if (!allIds.includes(id)) allIds.push(id);
            });
        }
        const list = document.getElementById('provider-list');
        if (!list) return;

        const formats = [
            { id: 'openai',       label: 'OpenAI Chat' },
            { id: 'anthropic',    label: 'Anthropic Claude' },
            { id: 'gemini',       label: 'Google Gemini' },
            { id: 'ollama',       label: 'Ollama' },
            { id: 'openai-image', label: 'OpenAI Image (DALL-E)' },
            { id: 'replicate',    label: 'Replicate (Image/Video)' },
            { id: 'fal-ai',       label: 'Fal.ai (Image/Video)' }
        ];

        // Add custom formats dynamically
        Object.keys(customMetadata).forEach(id => {
            if (!formats.some(f => f.id === id)) {
                formats.push({ id: id, label: `${customMetadata[id].name || id} (Custom)` });
            }
        });

        // Initialize providerModels for custom formats so that models can be selected
        if (!this.state.providerModels) this.state.providerModels = {};
        Object.keys(customMetadata).forEach(id => {
            if (!this.state.providerModels[id]) {
                this.state.providerModels[id] = customMetadata[id].defaultModels || [];
            }
        });

        list.innerHTML = allIds.map(id => {
            const def = DEFAULT_PROVIDERS.find(p => p.id === id);
            const cfg = (providers && providers[id]) || {};
            const label = def ? def.label : id.charAt(0).toUpperCase() + id.slice(1);
            const defaultUrl = def ? def.defaultUrl : 'https://api.openai.com/v1';
            const defaultFormat = def ? def.defaultFormat : 'openai';
            const currentFormat = cfg.apiFormat || defaultFormat;
            const isCustom = !knownIds.includes(id);
            return `<div class="provider-item${isCustom ? ' provider-custom' : ''}">
                <div class="provider-name">${label}${isCustom ? ' <span class="provider-custom-badge">custom</span>' : ''}</div>
                
                <label>API Format</label>
                <select id="format-${id}" class="recipe-select" style="width:100%;margin-bottom:6px">
                    ${formats.map(f => `<option value="${f.id}" ${f.id === currentFormat ? 'selected' : ''}>${f.label}</option>`).join('')}
                </select>

                <label>API Key</label>
                <div class="api-key-row">
                    <input type="password" id="key-${id}" value="${this.escapeHtml(cfg.apiKey||'')}">
                    <button type="button" onclick="app.toggleKeyVisible('key-${id}',this)">👁</button>
                </div>
                <label>Base URL</label>
                <input type="text" id="url-${id}" value="${this.escapeHtml(cfg.baseUrl||defaultUrl)}" placeholder="${this.escapeHtml(defaultUrl)}">
                <div class="test-row">
                    <button type="button" class="btn-test" onclick="app.testProviderConnection('${id}')" data-i18n="Test">Test</button>
                    <span class="test-status" id="test-status-${id}"></span>
                </div>
                ${isCustom ? `<button class="provider-remove-btn" onclick="app.removeCustomProvider('${id}')">✕ remove</button>` : ''}
            </div>`;
        }).join('');
        // Add custom provider button at the bottom
        list.innerHTML += `<div class="provider-add-row">
            <input type="text" id="new-custom-provider-id" placeholder="provider id (e.g. gpt4all)" style="flex:1">
            <button onclick="app.addCustomProvider()">+ Add Custom</button>
        </div>`;
    },

    addCustomProvider() {
        const input = document.getElementById('new-custom-provider-id');
        if (!input || !input.value.trim()) return;
        const id = input.value.trim().toLowerCase();
        if (!this.state.providers) this.state.providers = {};
        this.state.providers[id] = { apiKey: '', baseUrl: '' };
        this.onProvidersResult(this.state.providers);
        input.value = '';
        this.addLog(`➕ Custom provider added: ${id}`);
    },

    removeCustomProvider(id) {
        if (!this.state.providers || !this.state.providers[id]) return;
        delete this.state.providers[id];
        this.onProvidersResult(this.state.providers);
        this.addLog(`🗑 Custom provider removed: ${id}`);
    },

    toggleKeyVisible(id, btn) {
        const inp = document.getElementById(id);
        if (inp.type === 'password') { inp.type = 'text'; btn.textContent = '🙈'; }
        else { inp.type = 'password'; btn.textContent = '👁'; }
    },

    saveProviders() {
        const list = document.getElementById('provider-list');
        if (!list) return;
        const providers = {};
        list.querySelectorAll('.provider-item').forEach(item => {
            let keyInput = null;
            let urlInput = null;
            item.querySelectorAll('input').forEach(inp => {
                if (inp.id.startsWith('key-')) keyInput = inp;
                else if (inp.id.startsWith('url-')) urlInput = inp;
            });
            if (!keyInput || !urlInput) return;
            const id = keyInput.id.replace('key-', '');
            const formatSelect = item.querySelector('#format-' + id);
            const existing = (this.state.providers && this.state.providers[id]) || {};
            providers[id] = {
                apiKey:  keyInput.value || '',
                baseUrl: urlInput.value || '',
                apiFormat: formatSelect?.value || 'openai',
                models:  existing.models || []
            };
        });
        if (Object.keys(providers).length === 0) return;
        this.postMessage({ type: 'save_providers', payload: providers });
        this.state.providers = providers;
    },

    testProviderConnection(id) {
        const apiKey = document.getElementById('key-' + id)?.value || '';
        const urlEl = document.getElementById('url-' + id);
        const baseUrl = urlEl?.value || '';
        const formatSelect = document.getElementById('format-' + id);
        const apiFormat = formatSelect?.value || 'openai';
        const statusEl = document.getElementById('test-status-' + id);
        if (statusEl) {
            statusEl.textContent = '⏳ Testing...';
            statusEl.className = 'test-status';
        }
        this.addLog('🔌 Testing ' + id + ' connection...');
        this.postMessage({ type: 'test_provider_connection', payload: { provider: id, apiFormat, apiKey, baseUrl } });
    },

    onTestConnectionResult(result) {
        const statusEl = document.getElementById('test-status-' + result.provider);
        if (!statusEl) return;
        if (result.success) {
            statusEl.textContent = '✅ ' + result.message;
            statusEl.className = 'test-status success';
            this.addLog('✅ ' + result.provider + ' connection OK');
        } else {
            statusEl.textContent = '❌ ' + result.message;
            statusEl.className = 'test-status error';
            this.addLog('❌ ' + result.provider + ' connection failed: ' + result.message);
        }
    },

    handleMenuCommand(cmd) {
        switch (cmd.action) {
            case 'new_tab':         this.newTab(); break;
            case 'save':            this.saveFile(); break;
            case 'save_as':         this.saveFileAs(); break;
            case 'import_zip':      this.addLog('📦 Import ZIP — coming soon'); break;
            case 'export_node':     this.addLog('📤 Export Node — coming soon'); break;
            case 'run_pipeline':    this.runPipeline(); break;
            case 'pipeline_manager': this.showPipelineManager(); break;
            case 'pipeline_history': this.showHistory(); break;
            case 'config':          this.showConfig(); break;
            case 'test_connection': this.testConnection(); break;
            case 'recipe_manager':  this.showRecipeManager(); break;
            case 'toggle_pane':     this.togglePane(cmd.pane + '-pane'); break;
            case 'about':           this.showAbout(); break;
            case 'welcome_wizard':  this.showWizard(); break;
            case 'reset_wizard':    this.resetWizard(); break;
            case 'setup_wizard':    this.showSetupWizard(); break;
            default: this.addLog('⚠ Unknown menu command: ' + cmd.action);
        }
    },

    onSaveAsResult(path) {
        const tab = this.state.tabs[this.state.activeTab];
        if (tab && path) {
            tab.file = path.split('/').pop().split('\\').pop();
            this.addLog('💾 Saved as: ' + path);
        }
    },

    switchTreeTab(tab) {
        this.state.activeTreeTab = tab;
        const nodeBtn = document.getElementById('btn-tree-tab-pipeline');
        const fileBtn = document.getElementById('btn-tree-tab-file');
        const nodeContent = document.getElementById('tree-content');
        const fileContent = document.getElementById('file-tree-content');

        if (tab === 'pipeline' || tab === 'node') {
            nodeBtn?.classList.add('active');
            fileBtn?.classList.remove('active');
            if (nodeContent) nodeContent.style.display = '';
            if (fileContent) fileContent.style.display = 'none';
            this.renderTree();
        } else {
            nodeBtn?.classList.remove('active');
            fileBtn?.classList.add('active');
            if (nodeContent) nodeContent.style.display = 'none';
            if (fileContent) fileContent.style.display = '';
            this.requestFileTree();
        }
    },

    requestFileTree() {
        this.postMessage({ type: 'get_file_tree' });
    },

    renderFileTree() {
        const el = document.getElementById('file-tree-content');
        if (!el) return;
        if (!this.state.fileTree || this.state.fileTree.length === 0) {
            el.innerHTML = '<div class="empty">No files</div>';
            return;
        }
        el.innerHTML = this.buildFileTreeHTML(this.state.fileTree, 0);
    },

    buildFileTreeHTML(items, indent) {
        let html = '';
        const activeTab = this.state.tabs[this.state.activeTab];
        const activeFile = activeTab ? activeTab.file : '';
        
        items.forEach(item => {
            if (item.type === 'directory') {
                html += `<div class="file-tree-node directory" style="padding-left:${indent}px">${this.escapeHtml(item.name)}</div>`;
                if (item.children && item.children.length > 0) {
                    html += this.buildFileTreeHTML(item.children, indent + 16);
                }
            } else if (item.type === 'file') {
                const isSelected = item.path === activeFile;
                const cls = 'file-tree-node file' + (isSelected ? ' selected' : '');
                const escPath = this.escapeHtml(item.path);
                html += `<div class="${cls}" style="padding-left:${indent}px" onclick="app.selectFileTreeItem('${escPath}')">${this.escapeHtml(item.name)}</div>`;
            }
        });
        return html;
    },

    selectFileTreeItem(path) {
        const tabIndex = this.state.tabs.findIndex(t => t.file === path);
        if (tabIndex >= 0) {
            this.switchTab(tabIndex);
        } else {
            this.state.tabs.push({ name: path.split('/').pop().split('\\').pop(), file: path, root: { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [], nodeType: 'root' } });
            this.state.activeTab = this.state.tabs.length - 1;
            this.renderTabs();
            this.postMessage({ type: 'load_file_data', payload: { path: path } });
        }
    },

    onFileDataResult(path, root) {
        const idx = this.state.tabs.findIndex(t => t.file === path);
        if (idx >= 0) {
            this.patchNodeTypes(root, true);
            this.state.tabs[idx].root = root;
            if (idx === this.state.activeTab) {
                this.renderTree();
                this.renderList();
                if (root && root.children && root.children.length > 0) {
                    this.selectNode(''); // Select root node by default
                }
            }
        }
    },

    onRenameFileResult(payload) {
        if (!payload || !payload.success) {
            this.addLog(`❌ Failed to rename file: ${payload ? payload.error : 'unknown'}`);
            return;
        }
        const { oldFile, newFile } = payload;
        const index = this.state.tabs.findIndex(t => t.file === oldFile);
        if (index >= 0) {
            const newName = newFile.split('/').pop().split('\\').pop();
            this.state.tabs[index].name = newName;
            this.state.tabs[index].file = newFile;
            this.postMessage({ type: 'save_session', payload: {
                tabs: this.state.tabs.map(t => ({ name: t.name, file: t.file }))
            }});
            this.renderTabs();
            this.postMessage({ type: 'get_file_tree' });
            this.addLog(`✏️ Tab and file renamed to "${newName}"`);
        }
    },

    isValidFileName(name) {
        if (!name || name.trim() === '') return false;
        const forbiddenChars = /[\\/:*?"<>|]/;
        if (forbiddenChars.test(name)) return false;
        const reservedNames = /^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$/i;
        if (reservedNames.test(name)) return false;
        return true;
    },

    checkNodeColorInvariants() {
        const tab = this.state.tabs[this.state.activeTab];
        if (!tab || !tab.root) return;

        const checkNode = (node, path) => {
            const isRoot = node.nodeType === 'root' || (!node.nodeType && path === '');
            const isProcessed = node.nodeType === 'placeholder' || (!node.nodeType && node.title && this.safeAtob(node.title) === 'Processed');

            // 1. Virtual state logic check
            let colorCls = '';
            const selectedOpPath = this.state.selectedOpPath;
            const selectedDataPath = this.state.selectedDataPath;

            if (!isRoot && !isProcessed) {
                if (selectedOpPath !== '' && path === selectedOpPath) {
                    colorCls = 'selected';
                } else if (selectedDataPath !== '' && path === selectedDataPath) {
                    colorCls = 'selected-data';
                }
            }

            if (isRoot || isProcessed) {
                if (colorCls) {
                    console.error(`Runtime node color invariant violation (logic): Root/Processed node at path "${path}" has color class "${colorCls}"`);
                }
            } else if (this.isDataNodePath(path)) {
                if (colorCls === 'selected' || colorCls === 'selected-input') {
                    console.error(`Runtime node color invariant violation (logic): Data node at path "${path}" has op-node color class "${colorCls}"`);
                }
            } else {
                if (colorCls === 'selected-data' || colorCls === 'selected-result') {
                    console.error(`Runtime node color invariant violation (logic): Op-node at path "${path}" has data node color class "${colorCls}"`);
                }
            }
            // selected-linked is valid on both op and data nodes

            // 2. Real DOM element check
            if (path !== '') {
                const el = this.getDOMElementForPath(path);

                if (el) {
                    const classes = el.className.split(' ');
                    if (isProcessed) {
                        if (classes.includes('selected') || classes.includes('selected-input') ||
                            classes.includes('selected-data') || classes.includes('selected-result') ||
                            classes.includes('selected-linked')) {
                            console.error(`Runtime node color invariant violation (DOM): Processed node at "${path}" has color classes in DOM: ${el.className}`);
                        }
                    } else if (this.isDataNodePath(path)) {
                        if (classes.includes('selected') || classes.includes('selected-input')) {
                            console.error(`Runtime node color invariant violation (DOM): Data node at "${path}" has op-node color classes in DOM: ${el.className}`);
                        }
                        // selected-linked is valid on data nodes
                    } else {
                        if (classes.includes('selected-data') || classes.includes('selected-result')) {
                            console.error(`Runtime node color invariant violation (DOM): Op-node at "${path}" has data node color classes in DOM: ${el.className}`);
                        }
                        // selected-linked is valid on op nodes
                    }
                }
            }

            if (node.children) {
                node.children.forEach((child, i) => {
                    checkNode(child, path + '/' + i);
                });
            }
        };

        checkNode(tab.root, '');


    },

    checkNodeTypeInvariants() {
        const tab = this.state.tabs[this.state.activeTab];
        if (!tab || !tab.root) return;

        const errors = [];

        const checkNode = (node, path, isRootLevel) => {
            const nt = node.nodeType;

            // nodeType が未設定は許容しない（patchNodeTypes が呼ばれていれば必ず設定済み）
            if (!nt) {
                errors.push(`path="${path}": nodeType が未設定`);
                if (node.children) node.children.forEach((c, i) => checkNode(c, path === '' ? String(i) : path + '/' + i, false));
                return;
            }

            // 有効な nodeType かチェック
            if (!['root', 'assemble', 'data', 'placeholder'].includes(nt)) {
                errors.push(`path="${path}": 不明な nodeType="${nt}"`);
            }

            // root はルートレベルのみ
            if (nt === 'root' && !isRootLevel) {
                errors.push(`path="${path}": root nodeType が非ルートノードに設定されている`);
            }
            if (isRootLevel && nt !== 'root') {
                errors.push(`path="${path}": ルートノードの nodeType が "${nt}" (expected "root")`);
            }

            // data ノードは pipelineMeta を持つべき
            if (nt === 'data' && node.pipelineMeta === undefined) {
                errors.push(`path="${path}": nodeType="data" だが pipelineMeta がない`);
            }

            // assemble ノードは pipelineMeta を持ってはいけない
            if (nt === 'assemble' && node.pipelineMeta !== undefined) {
                errors.push(`path="${path}": nodeType="assemble" だが pipelineMeta が設定されている（data nodeが誤設定）`);
            }

            // placeholder ノードのタイトルは "Processed" であるべき
            if (nt === 'placeholder') {
                const title = node.title ? this.safeAtob(node.title) : '';
                if (title !== 'Processed') {
                    errors.push(`path="${path}": nodeType="placeholder" だがタイトルが "${title}" (expected "Processed")`);
                }
            }

            if (node.children) node.children.forEach((c, i) => checkNode(c, path === '' ? String(i) : path + '/' + i, false));
        };

        checkNode(tab.root, '', true);

        if (errors.length > 0) {
            const msg = `ノード型不整合 (${errors.length}件):\n` + errors.map(e => '  • ' + e).join('\n');
            this.addLog('❌ [RC-01] ' + msg);
            console.error('[RC-01] checkNodeTypeInvariants:', msg);
        }
    },

    // Tree rendering
    renderTree() {
        if (this.state.viewMode === 'pipeline') {
            this.renderPipelineSteps();
            return;
        }
        const el = document.getElementById('tree-content');
        if (!el) return;
        const tab = this.state.tabs[this.state.activeTab];
        if (!tab || !tab.root) { el.innerHTML = '<div class="empty">No data</div>'; return; }
        // Pre-compute whether the currently selected node is a leaf (data node)
        const selNode = this.getNodeByPath(this.state.currentNodePath);
        this._selectedIsLeaf = selNode ? (!selNode.children || selNode.children.length === 0) : false;
        el.innerHTML = this.buildTreeHTML(tab.root, '');
        
        // Also sync file tree selections if visible
        if (this.state.activeTreeTab === 'file') {
            this.renderFileTree();
        }

        // Run runtime invariants validation
        this.checkNodeTypeInvariants();
        this.checkNodeColorInvariants();
    },

    buildTreeHTML(node, path) {
        let html = '';
        const display = this.escapeHtml(node.title ? this.safeAtob(node.title) : this.getTitleFallback(node));
        const safePath = this.escapeHtml(path);
        const hasChildren = node.children && node.children.length > 0;
        const collapsed = this.state.collapsedPaths.has(path);

        // Compute color class based on relationship to selected node
        const selectedOpPath = this.state.selectedOpPath;
        const selectedDataPath = this.state.selectedDataPath;
        let colorCls = '';

        const isRoot = node.nodeType === 'root' || (!node.nodeType && path === '');
        const isProcessed = node.nodeType === 'placeholder' || (!node.nodeType && node.title && this.safeAtob(node.title) === 'Processed');

        if (!isRoot && !isProcessed) {
            if (path === selectedOpPath) colorCls = ' selected';
            else if (path === selectedDataPath) colorCls = ' selected-data';
        }

        let extraCls = '';
        if (selectedOpPath !== '' && path === selectedOpPath) {
            extraCls += ' current-op';
        }
        if (selectedDataPath !== '' && path === selectedDataPath) {
            extraCls += ' current-data';
        }

        const cls = 'tree-node' + (hasChildren ? ' branch' : ' leaf') + colorCls + extraCls +
                    (collapsed ? ' collapsed' : '');
        const collapseBtn = hasChildren
            ? `<span class="tree-collapse-btn" onclick="event.stopPropagation();app.treeToggleCollapse('${safePath}')">${collapsed ? '▶' : '▼'}</span>`
            : '<span class="tree-collapse-btn-spacer"></span>';
        html += `<div class="${cls}" onclick="app.selectNode('${safePath}')" oncontextmenu="event.preventDefault();event.stopPropagation();app.showTreeContextMenu(event,'${safePath}')">${collapseBtn}${display}</div>`;
        if (hasChildren && !collapsed) {
            html += '<div class="tree-children">';
            node.children.forEach((child, i) => {
                html += this.buildTreeHTML(child, path + '/' + i);
            });
            html += '</div>';
        }
        return html;
    },

    treeToggleCollapse(path) {
        if (this.state.collapsedPaths.has(path)) {
            this.state.collapsedPaths.delete(path);
        } else {
            this.state.collapsedPaths.add(path);
        }
        this.renderTree();
    },

    showTreeContextMenu(event, path) {
        this.selectNode(path);
        const node = this.getNodeByPath(path);
        const parts = path.split('/').filter(p => p !== '');
        const isRoot = parts.length === 0;
        const hasChildren = node && node.children && node.children.length > 0;
        const collapsed = this.state.collapsedPaths.has(path);

        let idx = -1, siblingCount = 0;
        if (!isRoot) {
            idx = parseInt(parts[parts.length - 1]);
            const parentPath = parts.slice(0, -1).join('/');
            const parent = this.getNodeByPath(parentPath ? '/' + parentPath : '');
            siblingCount = parent && parent.children ? parent.children.length : 0;
        }

        const menu = document.getElementById('tree-context-menu');
        let items = '';

        if (hasChildren) {
            const label = collapsed ? '▶ 展開' : '▼ 折りたたむ';
            items += `<div class="ctx-item" onclick="app.treeToggleCollapse('${path}');app.hideTreeContextMenu()">${label}</div>`;
            items += '<div class="ctx-sep"></div>';
        }

        items += `<div class="ctx-item" onclick="app.treeCtxAddChild('${path}');app.hideTreeContextMenu()">➕ 子ノードを追加</div>`;
        if (!isRoot) {
            items += `<div class="ctx-item" onclick="app.treeCtxAddSibling('${path}');app.hideTreeContextMenu()">➕ 兄弟ノードを追加</div>`;
            items += '<div class="ctx-sep"></div>';
            items += `<div class="ctx-item" onclick="app.treeCtxRename('${path}');app.hideTreeContextMenu()">✏️ 名前変更</div>`;
            items += '<div class="ctx-sep"></div>';
            if (idx > 0) {
                items += `<div class="ctx-item" onclick="app.treeCtxMoveUp('${path}');app.hideTreeContextMenu()">⬆ 上に移動</div>`;
            }
            if (idx < siblingCount - 1) {
                items += `<div class="ctx-item" onclick="app.treeCtxMoveDown('${path}');app.hideTreeContextMenu()">⬇ 下に移動</div>`;
            }
            items += '<div class="ctx-sep"></div>';
            items += `<div class="ctx-item ctx-danger" onclick="app.treeCtxDelete('${path}');app.hideTreeContextMenu()">🗑 削除</div>`;
        }

        menu.innerHTML = items;
        menu.style.display = 'block';
        menu.style.left = Math.min(event.clientX, window.innerWidth - 180) + 'px';
        menu.style.top = Math.min(event.clientY, window.innerHeight - menu.offsetHeight - 10) + 'px';
    },

    hideTreeContextMenu() {
        const menu = document.getElementById('tree-context-menu');
        if (menu) menu.style.display = 'none';
    },

    treeCtxAddChild(path) {
        const node = this.getNodeByPath(path);
        if (!node) return;
        if (!node.children) node.children = [];
        node.children.push({ title: '', content: '', mimetype: 'text/plain', attachments: [], children: [], nodeType: 'assemble' });
        this.state.collapsedPaths.delete(path);
        this.state.currentNodePath = path + '/' + (node.children.length - 1);
        this.renderTree();
        this.renderList();
        this.loadEditor(this.state.currentNodePath);
        this.saveCurrentTab();
        this.addLog('➕ 子ノードを追加しました');
    },

    treeCtxAddSibling(path) {
        const parts = path.split('/').filter(p => p !== '');
        if (parts.length === 0) return;
        const idx = parseInt(parts[parts.length - 1]);
        const parentPath = parts.slice(0, -1).join('/');
        const parent = this.getNodeByPath(parentPath ? '/' + parentPath : '');
        if (!parent || !parent.children) return;
        parent.children.splice(idx + 1, 0, { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [], nodeType: 'assemble' });
        const newPath = (parentPath ? '/' + parentPath : '') + '/' + (idx + 1);
        this.state.currentNodePath = newPath;
        this.renderTree();
        this.renderList();
        this.loadEditor(this.state.currentNodePath);
        this.saveCurrentTab();
        this.addLog('➕ 兄弟ノードを追加しました');
    },

    treeCtxDelete(path) {
        const parts = path.split('/').filter(p => p !== '');
        if (parts.length === 0) return;
        const idx = parseInt(parts[parts.length - 1]);
        const parentPath = parts.slice(0, -1).join('/');
        const parent = this.getNodeByPath(parentPath ? '/' + parentPath : '');
        if (!parent || !parent.children || idx >= parent.children.length) return;
        if (!confirm('このノードを削除しますか？')) return;
        parent.children.splice(idx, 1);
        this.state.currentNodePath = parentPath ? '/' + parentPath : '';
        this.renderTree();
        this.renderList();
        this.loadEditor(this.state.currentNodePath);
        this.saveCurrentTab();
        this.addLog('🗑 ノードを削除しました');
    },

    treeCtxMoveUp(path) {
        const parts = path.split('/').filter(p => p !== '');
        if (parts.length === 0) return;
        const idx = parseInt(parts[parts.length - 1]);
        if (idx === 0) return;
        const parentPath = parts.slice(0, -1).join('/');
        const parent = this.getNodeByPath(parentPath ? '/' + parentPath : '');
        if (!parent || !parent.children) return;
        [parent.children[idx - 1], parent.children[idx]] = [parent.children[idx], parent.children[idx - 1]];
        const newPath = (parentPath ? '/' + parentPath : '') + '/' + (idx - 1);
        this.state.currentNodePath = newPath;
        this.renderTree();
        this.renderList();
        this.saveCurrentTab();
        this.addLog('⬆ ノードを上に移動しました');
    },

    treeCtxMoveDown(path) {
        const parts = path.split('/').filter(p => p !== '');
        if (parts.length === 0) return;
        const idx = parseInt(parts[parts.length - 1]);
        const parentPath = parts.slice(0, -1).join('/');
        const parent = this.getNodeByPath(parentPath ? '/' + parentPath : '');
        if (!parent || !parent.children || idx >= parent.children.length - 1) return;
        [parent.children[idx], parent.children[idx + 1]] = [parent.children[idx + 1], parent.children[idx]];
        const newPath = (parentPath ? '/' + parentPath : '') + '/' + (idx + 1);
        this.state.currentNodePath = newPath;
        this.renderTree();
        this.renderList();
        this.saveCurrentTab();
        this.addLog('⬇ ノードを下に移動しました');
    },

    treeCtxRename(path) {
        const node = this.getNodeByPath(path);
        if (!node) return;
        const current = node.title ? atob(node.title) : '';
        const newName = prompt('ノード名を入力してください:', current);
        if (newName === null) return;
        const safeB64 = str => { try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str); } };
        node.title = safeB64(newName);
        this.renderTree();
        this.renderList();
        this.loadEditor(path);
        this.saveCurrentTab();
        this.addLog('✏️ ノード名を変更しました');
    },

    saveCurrentTab() {
        const tab = this.state.tabs[this.state.activeTab];
        if (tab && tab.file && tab.root) {
            this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
        }
    },

    getTitleFallback(node) {
        if (node.title) return this.safeAtob(node.title);
        if (node.mimetype === 'text/plain' && node.content) {
            const text = this.safeAtob(node.content);
            const words = text.split(/\s+/).slice(0, 4).join(' ');
            return words + (words.length < text.length ? '...' : '');
        }
        if (node.mimetype === 'application/rtf') return '[RTF ' + (node.content ? Math.round(this.safeAtob(node.content).length / 1024) + 'KB' : '0B') + ']';
        if (node.mimetype.startsWith('image/')) return '[Image ' + (node.content ? Math.round(this.safeAtob(node.content).length / 1024) + 'KB' : '0B') + ']';
        if (node.mimetype === 'text/html') return '[HTML ' + (node.content ? this.safeAtob(node.content).length + ' chars' : '') + ']';
        return '(empty)';
    },

    // --- Navigation history ---
    pushNav() {
        const cur = { tabIndex: this.state.activeTab, path: this.state.currentNodePath };
        // Don't push duplicate of last entry
        const last = this.state.navHistory[this.state.navHistory.length - 1];
        if (last && last.tabIndex === cur.tabIndex && last.path === cur.path) return;
        this.state.navHistory.push(cur);
        if (this.state.navHistory.length > 100) this.state.navHistory.shift();
        this.state.navFuture = [];
        this.updateNavButtons();
    },

    navBack() {
        if (this.state.navHistory.length === 0) return;
        this.state.navFuture.push({ tabIndex: this.state.activeTab, path: this.state.currentNodePath });
        const entry = this.state.navHistory.pop();
        this.updateNavButtons();
        this.gotoNavEntry(entry);
    },

    navForward() {
        if (this.state.navFuture.length === 0) return;
        this.state.navHistory.push({ tabIndex: this.state.activeTab, path: this.state.currentNodePath });
        const entry = this.state.navFuture.pop();
        this.updateNavButtons();
        this.gotoNavEntry(entry);
    },

    gotoNavEntry(entry) {
        if (entry.tabIndex !== this.state.activeTab) {
            this.state.activeTab = entry.tabIndex;
            this.renderTabs();
            this.renderTree();
        }
        this.state.currentNodePath = entry.path;
        this.renderTree();
        this.renderList();
        this.loadEditor(entry.path);
    },

    updateNavButtons() {
        const back = document.getElementById('btn-nav-back');
        const fwd  = document.getElementById('btn-nav-fwd');
        if (back) back.disabled = this.state.navHistory.length === 0;
        if (fwd)  fwd.disabled  = this.state.navFuture.length  === 0;
    },
    // --- end navigation history ---

    selectNode(path) {
       this.addLog("Select Node" + path);
        if ('speechSynthesis' in window) {
            window.speechSynthesis.cancel();
            this.clearAllSpeakingStyles();
        }
        // Case B: switching nodes while pipeline has completed steps → warn user
        if (
            this.state.viewMode === 'pipeline' &&
            this.state.currentNodePath !== path &&
            this.state.pipelineRun.steps &&
            this.state.pipelineRun.steps.some(s => s.completed)
        ) {
            if (!confirm('連結データを変更します。\n\n現在の実行状態・ステップ入出力データは破棄されます。続けますか？')) {
                return;
            }
            // Reset pipeline runtime state completely
            this.state.pipelineRun = { running: false, steps: [], selectedStep: -1 };
        }
        this.pushNav();
        this.state.currentNodePath = path;
        this.state.selectedOutputRunIndex = 0;

        const node = this.getNodeByPath(path);
        if (node) {
            const isRoot = node.nodeType === 'root' || (!node.nodeType && path === '');
            const isProcessed = node.nodeType === 'placeholder' || (!node.nodeType && node.title && this.safeAtob(node.title) === 'Processed');

            if (isRoot || isProcessed) {
                this.state.selectedOpPath = '';
                this.state.selectedDataPath = '';
            } else if (this.isDataNodePath(path)) {
                if (this.state.selectedDataPath === path) {
                    this.state.selectedDataPath = '';
                } else {
                    this.state.selectedDataPath = path;
                }
            } else {
                if (this.state.selectedOpPath === path) {
                    this.state.selectedOpPath = '';
                } else {
                    this.state.selectedOpPath = path;
                }
            }
        } else {
            this.state.selectedOpPath = '';
            this.state.selectedDataPath = '';
        }


        this.renderTree();
        this.renderList();
        this.loadEditor(path);
    },

    // Evaluation badge HTML helper
    evalBadgeHtml(evaluation) {
        if (!evaluation) return '';
        const map = { ok: '👍', rejected: '👎', pinned: '📌' };
        const icon = map[evaluation] || '';
        return icon ? `<span class="eval-badge eval-badge-${evaluation}" title="${evaluation}">${icon}</span>` : '';
    },

    // Evaluation buttons for a node (inline in list/editor)
    evalButtonsHtml(nodePathOrId, type, currentEval) {
        const ev = e => JSON.stringify(e);
        return `<span class="eval-btns" onclick="event.stopPropagation()">
            <button class="eval-btn ${currentEval==='ok'?'active':''}" title="OK" onclick="app.evaluateItem(${ev(type)},${ev(nodePathOrId)},'ok'${currentEval==='ok'?',true':''})">👍</button>
            <button class="eval-btn ${currentEval==='rejected'?'active':''}" title="却下" onclick="app.evaluateItem(${ev(type)},${ev(nodePathOrId)},'rejected'${currentEval==='rejected'?',true':''})">👎</button>
            <button class="eval-btn ${currentEval==='pinned'?'active':''}" title="ピン止め" onclick="app.evaluateItem(${ev(type)},${ev(nodePathOrId)},'pinned'${currentEval==='pinned'?',true':''})">📌</button>
        </span>`;
    },

    // Toggle evaluation (click active → clear)
    evaluateItem(type, id, evaluation, isActive) {
        const newEval = isActive ? '' : evaluation;
        if (type === 'node') {
            // id is "tabFile|nodePath"
            const [tabFile, nodePath] = id.split('|');
            // Update in-memory node
            const node = this.getNodeByPath(nodePath);
            if (node) {
                node.evaluation = newEval;
                node.evaluatedAt = new Date().toISOString();
                this.renderList();
                this.loadEditor(this.state.currentNodePath);
            }
            this.postMessage({ type: 'evaluate_node', payload: { nodeId: nodePath, tabFile, evaluation: newEval, note: '' } });
        } else if (type === 'step') {
            // id is "runId|stepIndex"
            const [runId, stepIdx] = id.split('|');
            this.postMessage({ type: 'evaluate_history_step', payload: { runId, stepIndex: parseInt(stepIdx), evaluation: newEval, note: '' } });
            // Update rendered detail view step badge
            const badge = document.querySelector(`[data-step-eval="${runId}-${stepIdx}"]`);
            if (badge) badge.className = `eval-badge eval-badge-${newEval}`;
        } else if (type === 'run') {
            this.postMessage({ type: 'evaluate_history_run', payload: { runId: id, evaluation: newEval } });
        }
    },

    onEvaluationSaved(payload) {
        const evalLabels = { ok: '👍 OK', rejected: '👎 却下', pinned: '📌 ピン止め', '': '評価クリア' };
        this.addLog(`✅ 評価保存: ${evalLabels[payload.evaluation] || payload.evaluation}`);
        this.renderList();
    },

    // List rendering
    renderList() {
        const el = document.getElementById('list-content');
        if (!el) {
            // New 5-pane layout: delegate to renderMainContent
            this.renderMainContent();
            return;
        }
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.children) { el.innerHTML = '<div class="empty">Select a node</div>'; return; }
        const tab = this.state.tabs[this.state.activeTab];
        const tabFile = tab ? tab.file : '';
        el.innerHTML = node.children.map((child, i) => {
            const display = this.escapeHtml(child.title ? this.safeAtob(child.title) : this.getTitleFallback(child));
            const childPath = (this.state.currentNodePath ? this.state.currentNodePath + '/' : '/') + i;
            const evalBadge = this.evalBadgeHtml(child.evaluation);
            const evalBtns = this.evalButtonsHtml(`${tabFile}|${childPath}`, 'node', child.evaluation || '');
            return `<div class="list-item ${child.evaluation ? 'has-eval eval-' + child.evaluation : ''}" ondblclick="app.copyItemText(${i})">
                <span class="list-item-title">${evalBadge}${display}</span>
                <span class="list-item-actions">
                    ${evalBtns}
                    <button class="copy-btn" onclick="app.copyItemText(${i})">📋</button>
                </span>
            </div>`;
        }).join('');
    },

    getNodeByPath(path) {
        const tab = this.state.tabs[this.state.activeTab];
        if (!tab || !tab.root) return null;
        if (!path) return tab.root;
        const parts = path.split('/').filter(p => p !== '');
        let node = tab.root;
        for (const p of parts) {
            const idx = parseInt(p);
            if (isNaN(idx) || !node.children || idx >= node.children.length) return null;
            node = node.children[idx];
        }
        return node;
    },

    copyItemText(index) {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.children || index >= node.children.length) return;
        const child = node.children[index];
        if (!child.content) return;
        const text = atob(child.content);
        navigator.clipboard.writeText(text).then(() => {
            this.addLog('📋 Copied!');
        });
    },

    loadEditor(path) {
        let node = this.getNodeByPath(path);
        if (node) {
            if (node.nodeType === 'data' && node.originalOpNode) {
                node = node.originalOpNode;
            }
            this.state.selectedRecipe = node.selectedRecipe || '';
        } else {
            this.state.selectedRecipe = '';
        }
        this.updateRecipeBadge();

        this.renderPrompt();
        this.renderInput();
        this.renderOutput();
    },

    renderAttachments(node) {
        const el = document.getElementById('attachments-area');
        if (!el) return;
        if (!node.attachments || node.attachments.length === 0) {
            el.innerHTML = '<div class="empty">No attachments</div>';
            return;
        }
        el.innerHTML = node.attachments.map(a => {
            const name = a.file || a.id || 'attachment';
            return `<div class="list-item">
                <span>${a.mimetype}: ${name}${a.size ? ' (' + Math.round(a.size/1024) + 'KB)' : ''}</span>
                <button class="copy-btn" onclick="app.log('Preview')">👁</button>
            </div>`;
        }).join('');
    },

    updateNode() {
        const nodePath = this.state.selectedDataPath || this.state.selectedOpPath || this.state.currentNodePath;
        const node = this.getNodeByPath(nodePath);
        if (!node) return;
        const title = document.getElementById('node-title');
        const content = document.getElementById('node-content');
        const safeB64 = str => { try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str); } };
        if (title) node.title = safeB64(title.value);
        
        let targetNode = node;
        if (node.nodeType === 'data' && node.originalOpNode) {
            targetNode = node.originalOpNode;
        }
        if (content && targetNode.mimetype === 'text/plain') targetNode.content = safeB64(content.value);

        const inputTextArea = document.getElementById('input-textarea');
        if (inputTextArea && node.nodeType === 'data') {
            node.input = inputTextArea.value;
        }

        this.renderTree();
        this.renderList();
        const tab = this.state.tabs[this.state.activeTab];
        if (tab && tab.file && tab.root) {
            this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
        }
        this.addLog('💾 Node updated');
    },

    processPrompt() {
        this.updateNode();

        if (this.state.viewMode === 'node') {
            const node = this.getNodeByPath(this.state.currentNodePath);
            if (!node) {
                this.addLog('⚠ ノードを選択してください');
                return;
            }

            const prompt = document.getElementById('node-content')?.value || '';
            const input = document.getElementById('input-textarea')?.value || '';
            const recipe = this.getRecipeSettings();

            const tab = this.state.tabs[this.state.activeTab];
            const sentText = prompt.includes('{content}') ? prompt.replace('{content}', input) : (prompt + '\n\n' + input);

            this.state.streamedOutput = '';
            const outputEl = document.getElementById('output-content');
            if (outputEl) {
                outputEl.innerHTML = `
                    <div class="output-history-container" style="display: flex; flex-direction: column; gap: 10px; padding: 8px; height: calc(100% - 35px); overflow-y: auto;">
                        <details class="output-history-sent-details" style="margin-bottom: 8px;">
                            <summary style="font-size: 10px; font-weight: bold; color: #858585; cursor: pointer; outline: none; user-select: none; border-bottom: 1px solid #333; padding-bottom: 2px;">📥 送信データ (Sent Input)</summary>
                            <pre class="sent-display" style="margin: 4px 0 0 0; background: #1e1e1e; border: 1px solid #2d2d2d; padding: 6px; font-family: monospace; white-space: pre-wrap; font-size: 11px; overflow-y: auto; max-height: 150px;">${this.escapeHtml(sentText)}</pre>
                        </details>
                        <details class="output-history-received-details" style="margin-bottom: 8px;">
                            <summary style="font-size: 10px; font-weight: bold; color: #858585; cursor: pointer; outline: none; user-select: none; border-bottom: 1px solid #333; padding-bottom: 2px;">📤 受信データ (Received Output)</summary>
                            <pre class="output-display" style="margin: 4px 0 0 0; background: #1e1e1e; border: 1px solid #2d2d2d; padding: 6px; font-family: monospace; white-space: pre-wrap; font-size: 11px; overflow-y: auto; max-height: 250px;">Connecting to AI...</pre>
                        </details>
                    </div>
                `;
            }

            let targetNode = node;
            if (node.nodeType === 'data' && node.originalOpNode) {
                targetNode = node.originalOpNode;
            }
            this.postMessage({
                type: 'run_prompt_process',
                payload: {
                    nodeId: this.state.currentNodePath || '',
                    tabFile: tab ? tab.file : '',
                    content: input,
                    userPrompt: prompt,
                    provider: recipe.provider,
                    model: recipe.model,
                    systemPrompt: recipe.systemPrompt,
                    temperature: recipe.temperature,
                    attachments: targetNode.attachments || [],        // machine-level (演算ペイン)
                    inputAttachments: node.inputAttachments || [], // belt-level (入力ペイン)
                    customParams: recipe.customParams || {},
                }
            });
            this.state.pipelineRun.running = true;
            this.addLog(`▶ Processing prompt using ${recipe.provider}/${recipe.model || '(default)'}`);
        } else {
            this.runPipeline();
        }
    },



    addChild() {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;
        if (!node.children) node.children = [];
        node.children.push({ title: '', content: '', mimetype: 'text/plain', attachments: [], children: [], nodeType: 'assemble' });
        this.renderTree();
        this.renderList();
        this.addLog('➕ Child added');
    },

    removeNode() {
        const path = this.state.currentNodePath;
        if (!path) return;
        const parts = path.split('/').filter(p => p !== '');
        const parentPath = parts.slice(0, -1).join('/');
        const idx = parseInt(parts[parts.length - 1]);
        if (isNaN(idx)) return;
        const parent = this.getNodeByPath('/' + parentPath);
        if (!parent || !parent.children || idx >= parent.children.length) return;
        parent.children.splice(idx, 1);
        this.state.currentNodePath = '/' + parentPath;
        this.renderTree();
        this.renderList();
        this.loadEditor(this.state.currentNodePath);
        this.addLog('🗑 Node removed');
    },

    navRoot() {
        this.state.currentNodePath = '';
        this.renderTree();
        this.renderList();
        this.loadEditor('');
    },

    navUp() {
        const path = this.state.currentNodePath;
        if (!path || path === '/') return;
        const parts = path.split('/').filter(p => p !== '');
        parts.pop();
        this.state.currentNodePath = '/' + parts.join('/');
        this.renderTree();
        this.renderList();
        this.loadEditor(this.state.currentNodePath);
    },

    navDown() {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.children || node.children.length === 0) return;
        this.state.currentNodePath = this.state.currentNodePath + '/' + 0;
        this.renderTree();
        this.renderList();
        this.loadEditor(this.state.currentNodePath);
    },

    // Pipeline
    runPipeline(pipelineName) {
        if (this.state.pipelineRun.running) { this.addLog('⚠ Pipeline already running'); return; }
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) { this.addLog('⚠ ノードを選択してください'); return; }
        if (!pipelineName && node.pipelineMeta) {
            try {
                const meta = JSON.parse(node.pipelineMeta);
                if (meta && meta.pipelineName) {
                    pipelineName = meta.pipelineName;
                }
            } catch (e) { this.addLog('⚠ Failed to parse pipelineMeta: ' + (e.message || '')); }
        }
        if (!pipelineName && this.state.pipelines && this.state.pipelines.length > 0) {
            pipelineName = this.state.pipelines[0].name;
        }
        if (!pipelineName) { this.addLog('⚠ パイプラインが未定義です'); return; }
        const content = node.content ? (() => { try { return decodeURIComponent(escape(atob(node.content))); } catch { return atob(node.content); } })() : '';
        const tab = this.state.tabs[this.state.activeTab];
        this.postMessage({ type: 'run_pipeline', payload: {
            pipelineName,
            nodeId:   this.state.currentNodePath || '',
            tabFile:  tab ? tab.file : '',
            content
        }});
        this.state.pipelineRun.running = true;
        this.addLog(`▶ Pipeline "${pipelineName}" started`);
    },

    cancelPipeline() {
        this.postMessage({ type: 'cancel_pipeline' });
        this.state.pipelineRun.running = false;
        this.addLog('✕ Pipeline canceled');
    },

    toggleTestMode() {
        this.state.testMode = !this.state.testMode;
        document.getElementById('btn-test-mode').classList.toggle('active');
        this.addLog(this.state.testMode ? '🧪 Test mode ON' : '🧪 Test mode OFF');
    },

    // Messages
    addLog(text) {
        console.log('[Prompts Log] ' + text);
        const el = document.getElementById('messages-content');
        if (!el) return;
        const div = document.createElement('div');
        div.className = 'log-entry';
        if (text.includes('<details')) {
            text = text.replace('<details', '<details open');
            div.innerHTML = '[' + new Date().toLocaleTimeString() + '] ' + text;
        } else {
            div.textContent = '[' + new Date().toLocaleTimeString() + '] ' + text;
        }
        el.appendChild(div);
        el.scrollTop = el.scrollHeight;
    },

    showError(msg) {
        this.addLog('❌ ' + msg);
        this.state.pipelineRun.running = false;
    },

    // Log context menu and copy
    showLogContextMenu(event) {
        event.preventDefault();
        event.stopPropagation();
        const menu = document.getElementById('log-context-menu');
        if (!menu) return;
        const el = event.target.closest('.log-entry');
        const entryText = el ? el.textContent : '';
        const allText = this.getAllLogText();
        menu.innerHTML = `
            <div class="ctx-item" onclick="app.copyLogEntry(this.dataset.text);app.hideLogContextMenu()" data-text="${this.escapeHtml(entryText)}">📋 Copy Line</div>
            <div class="ctx-item" onclick="app.copyAllLogs();app.hideLogContextMenu()">📋 Copy All</div>
            <div class="ctx-sep"></div>
            <div class="ctx-item" onclick="app.clearLogs();app.hideLogContextMenu()">✕ Clear</div>
        `;
        menu.style.display = 'block';
        menu.style.left = Math.min(event.clientX, window.innerWidth - 160) + 'px';
        menu.style.top = Math.min(event.clientY, window.innerHeight - 120) + 'px';
    },

    hideLogContextMenu() {
        const menu = document.getElementById('log-context-menu');
        if (menu) menu.style.display = 'none';
    },

    showTreeContextMenu(event, path) {
        event.preventDefault();
        event.stopPropagation();
        
        const menu = document.getElementById('tree-context-menu');
        if (!menu) return;
        
        const node = this.getNodeByPath(path);
        if (!node) return;
        const currentTitle = node.title ? this.safeAtob(node.title) : this.getTitleFallback(node);
        
        menu.style.left = event.clientX + 'px';
        menu.style.top = event.clientY + 'px';
        menu.style.display = 'block';
        
        const escTitle = this.escapeHtml(currentTitle);
        menu.innerHTML = `
            <div class="ctx-item" onclick="app.renameNode('${path}', '${escTitle}'); app.hideTreeContextMenu()">✏️ 名前の変更...</div>
            <div class="ctx-item" onclick="app.selectNode('${path}'); app.addChild(); app.hideTreeContextMenu()">➕ 子ノードを追加</div>
            <div class="ctx-item" onclick="app.selectNode('${path}'); app.removeNode(); app.hideTreeContextMenu()" style="color: #ff4a4a;">🗑️ 削除</div>
        `;
    },
    
    hideTreeContextMenu() {
        const menu = document.getElementById('tree-context-menu');
        if (menu) menu.style.display = 'none';
    },
    
    renameNode(path, oldTitle) {
        const node = this.getNodeByPath(path);
        if (!node) return;
        const newTitle = prompt('ノードの名前を変更:', oldTitle);
        if (newTitle === null) return; // Cancelled
        const trimmed = newTitle.trim();
        if (trimmed === '') return;
        
        const safeB64 = str => { try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str); } };
        node.title = safeB64(trimmed);
        
        this.renderTree();
        this.renderList();
        
        // Also refresh the prompt editor if the renamed node is the currently active one
        if (this.state.currentNodePath === path) {
            this.loadEditor(path);
        }
        
        const tab = this.state.tabs[this.state.activeTab];
        if (tab && tab.file && tab.root) {
            this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
        }
        this.addLog(`✏️ Node renamed to: "${trimmed}"`);
    },

    copyLogEntry(text) {
        if (!text) return;
        navigator.clipboard.writeText(text).then(() => {
            this.addLog('📋 Line copied');
        });
    },

    copyAllLogs() {
        const text = this.getAllLogText();
        if (!text) return;
        navigator.clipboard.writeText(text).then(() => {
            this.addLog('📋 All logs copied');
        });
    },

    getAllLogText() {
        const el = document.getElementById('messages-content');
        if (!el) return '';
        return Array.from(el.querySelectorAll('.log-entry'))
            .map(div => div.textContent)
            .join('\n');
    },

    clearLogs() {
        const el = document.getElementById('messages-content');
        if (el) el.innerHTML = '';
    },

    // Search
    search(query) {
        clearTimeout(this.state.searchTimeout);
        if (query.length < 2) return;
        this.state.searchTimeout = setTimeout(() => {
            this.postMessage({ type: 'search', query, scope: 'all_tabs' });
        }, 300);
    },

    showSearchResults(results) {
        if (!results || results.length === 0) {
            this.addLog('🔍 No matches found');
            return;
        }
        this.addLog(`🔍 Found ${results.length} match(es)`);
        (results || []).slice(0, 10).forEach(r => {
            this.addLog(`  ${r.title || r.excerpt || '(match)'}`);
        });
    },

    // Pipeline streaming
    appendStreamOutput(payload) {
        this.addLog(`[Step ${payload.stepIndex}] ${payload.text || '...'}`);
        this.state.streamedOutput = (this.state.streamedOutput || '') + (payload.text || '');

        const outputEl = document.getElementById('output-content');
        if (outputEl) {
            let display = outputEl.querySelector('.output-display');
            if (!display) {
                outputEl.innerHTML = `
                    <div class="output-toolbar">
                        <span class="output-label">Processing Output...</span>
                    </div>
                    <details class="output-history-received-details" style="margin: 8px;">
                        <summary style="font-size: 10px; font-weight: bold; color: #858585; cursor: pointer; outline: none; user-select: none; border-bottom: 1px solid #333; padding-bottom: 2px;">📤 受信データ (Received Output)</summary>
                        <pre class="output-display" style="margin: 4px 0 0 0; background: #1e1e1e; border: 1px solid #2d2d2d; padding: 6px; font-family: monospace; white-space: pre-wrap; font-size: 11px; overflow-y: auto; max-height: 250px;"></pre>
                    </details>
                `;
                display = outputEl.querySelector('.output-display');
            }
            if (display) {
                const details = display.closest('details');
                if (details && !details.open) {
                    details.open = true;
                }
                display.textContent = this.state.streamedOutput;
                display.scrollTop = display.scrollHeight;
            }
        }
    },

    onPipelineInit(payload) {
        if (!payload || !Array.isArray(payload.steps)) return;
        this.state.pipelineRun.steps = payload.steps.map(s => ({
            ...s, completed: false, input: '', output: '', streamingOutput: '', status: 'pending', outputAttachments: [], artifacts: []
        }));
        this.state.pipelineRun.selectedStep = 0;
        if (this.state.viewMode === 'pipeline') {
            this.renderPipelineSteps();
            this.renderInput();
        }
    },

    onStepDone(payload) {
        this.addLog(`✅ Step ${payload.index} done` + (payload.tokens ? ` (${payload.tokens} tokens)` : ''));
        if (payload.status === 'completed') this.state.pipelineRun.running = false;
        // Store outputAttachments so next step's input pane can show them
        if (this.state.pipelineRun.steps.length > 0) {
            const step = this.state.pipelineRun.steps[payload.index];
            if (step) {
                step.completed = true;
                step.outputAttachments = Array.isArray(payload.outputAttachments) ? payload.outputAttachments : [];
                if (this.state.viewMode === 'pipeline') {
                    this.renderPipelineSteps();
                    this.renderInput();
                }
            }
        }
    },

    highlightStep(payload) {
        this.addLog(`▶ Step ${payload.index}: ${payload.name || ''}`);
        this.state.pipelineRun.selectedStep = payload.index;
        if (payload.index === 0) {
            this.state.streamedOutput = '';
            const outputEl = document.getElementById('output-content');
            if (outputEl) {
                outputEl.innerHTML = `
                    <div class="output-toolbar">
                        <span class="output-label">Processing Output...</span>
                    </div>
                    <pre class="output-display">Connecting to AI...</pre>
                `;
            }
        }
        if (this.state.viewMode === 'pipeline') {
            this.renderPipelineSteps();
            this.renderInput();
        }
    },

    onRtfPosition(pos) {},

    // Modal
    showModal(id) {
        let modal = document.getElementById(id);
        if (!modal) {
            modal = document.createElement('div');
            modal.id = id;
            modal.className = 'modal';
            modal.innerHTML = `<div class="modal-content">
                <span class="modal-close" onclick="this.parentElement.parentElement.classList.remove('visible')">&times;</span>
                <div class="modal-body"></div>
            </div>`;
            document.body.appendChild(modal);
        }
        modal.classList.add('visible');
    },

    log(msg) { this.addLog(msg); },

    closeModal(id) {
        const modal = document.getElementById(id);
        if (modal) modal.classList.remove('visible');
    },

    showModeMenu(event) {
        const t = key => this.t(key);
        const existing = document.getElementById('mode-menu');
        if (existing) { existing.remove(); return; }
        event.stopPropagation();

        const menu = document.createElement('div');
        menu.id = 'mode-menu';
        menu.className = 'mode-menu';
        menu.innerHTML = `
            <div class="mode-menu-item ${this.state.viewMode === 'node' ? 'active' : ''}" onclick="app.switchViewMode('node');this.closest('.mode-menu').remove()">
                📄 ${t('NodeView')}
            </div>
            <div class="mode-menu-item ${this.state.viewMode === 'pipeline' ? 'active' : ''}" onclick="app.switchViewMode('pipeline');this.closest('.mode-menu').remove()">
                🔧 ${t('PipelineView')}
            </div>`;

        const rect = event.target.getBoundingClientRect();
        menu.style.top = (rect.bottom + 2) + 'px';
        menu.style.left = rect.left + 'px';
        document.body.appendChild(menu);

        setTimeout(() => document.addEventListener('click', function close() {
            menu.remove();
            document.removeEventListener('click', close);
        }, { once: true }), 0);
    },

    switchViewMode(mode) {
        this.state.viewMode = mode;
        if (mode === 'pipeline') {
            // Switch tree to show pipeline steps
            this.renderPipelineSteps();
        } else {
            this.renderTree();
        }
        this.renderMainContent();
        this.addLog(`👁 View mode: ${mode}`);
    },

    renderPipelineSteps() {
        const el = document.getElementById('tree-content');
        if (!el) return;
        const steps = this.state.pipelineRun.steps || [];
        if (steps.length === 0) {
            el.innerHTML = '<div class="empty">No pipeline steps</div>';
            return;
        }
        el.innerHTML = steps.map((s, i) => `
            <div class="tree-node ${s.completed ? 'completed' : ''} ${this.state.pipelineRun.selectedStep === i ? 'selected' : ''}"
                 onclick="app.selectPipelineStep(${i})">
                ${s.completed ? '✔' : '○'} ${this.escapeHtml(s.name || s.type)}
            </div>
        `).join('');
    },

    selectPipelineStep(index) {
        this.state.viewMode = 'pipeline';
        this.state.pipelineRun.selectedStep = index;
        this.renderPipelineSteps();
        this.renderMainContent();
        document.getElementById('view-mode-selector').value = 'pipeline';
    },

    togglePane(id) {
        const el = document.getElementById(id);
        if (el) {
            el.classList.toggle('collapsed');
            // Save pane states
            const states = JSON.parse(localStorage.getItem('prompts_panes') || '{}');
            states[id] = el.classList.contains('collapsed');
            localStorage.setItem('prompts_panes', JSON.stringify(states));
        }
    },

    initMessagesResizer() {
        const handle = document.getElementById('messages-resize-handle');
        const pane = document.getElementById('messages-pane');
        if (!handle || !pane) return;

        // Restore saved height
        const saved = localStorage.getItem('prompts_messages_height');
        if (saved) pane.style.height = saved + 'px';

        let isResizing = false;
        let startY, startHeight;

        handle.addEventListener('mousedown', (e) => {
            isResizing = true;
            startY = e.clientY;
            startHeight = pane.offsetHeight;
            handle.classList.add('active');
            document.body.style.cursor = 'ns-resize';
            document.body.style.userSelect = 'none';
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => {
            if (!isResizing) return;
            const newHeight = startHeight - (e.clientY - startY);
            const clamped = Math.max(80, Math.min(600, newHeight));
            pane.style.height = clamped + 'px';
        });

        document.addEventListener('mouseup', () => {
            if (!isResizing) return;
            isResizing = false;
            handle.classList.remove('active');
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
            // Save height
            localStorage.setItem('prompts_messages_height', pane.offsetHeight);
        });
    },

    // ── Wizard ────────────────────────────────────────────────────
    get WIZARD_STEPS() {
        return [
        {
            icon: '🤖',
            title: 'Prompts へようこそ',
            body: '<p><b>Prompts</b> は AI プロンプトとパイプラインを管理・実行するツールです。</p><p>データはローカルの JSON ファイルに保存され、git で管理できます。</p>',
            tips: [
                { icon: '🔁', text: '同じパイプラインを別の素材に繰り返し適用できます。' },
                { icon: '📂', text: 'データはローカルの JSON ファイルに保存されます。' }
            ]
        },
        {
            icon: '🔲',
            title: this.t('LangCode') === 'ja' ? '4ペインレイアウトの見方' : '4-Pane Layout Guide',
            body: this.t('LangCode') === 'ja'
                ? '<p>画面は <b>Tree | 入力(src) | 演算(op) | 出力(dst)</b> の4ペイン構成です。</p>' +
                  '<p style="font-size:11px;color:#888;margin-top:4px">GAS (GNU Assembler) のオペランド順 <b>op src, dst</b> に準拠。</p>' +
                  '<ul>' +
                  '<p>📥 <b>入力 (src)</b>: 処理するデータ。</p>' +
                  '<p>🔧 <b>演算 (op)</b>: 処理テンプレートとレシピ。</p>' +
                  '<p>📤 <b>出力 (dst)</b>: 生成された結果。</p>'
                : '<p>The screen has 4 panes: <b>Tree | Input(src) | Operation(op) | Output(dst)</b>.</p>' +
                  '<p style="font-size:11px;color:#888;margin-top:4px">Follows GAS (GNU Assembler) operand order: <b>op src, dst</b>.</p>' +
                  '<ul>' +
                  '<p>📥 <b>Input (src)</b>: The data to process.</p>' +
                  '<p>🔧 <b>Operation (op)</b>: Processing template and recipe.</p>' +
                  '<p>📤 <b>Output (dst)</b>: Generated results.</p>',
            tips: [
                { icon: '🔄', text: this.t('LangCode') === 'ja'
                    ? '各ペインのヘッダーにあるモード表示（📄 Node / 🔧 Step）をクリックすると、Node View と Pipeline View を切り替えられます。'
                    : 'Click the mode badge (📄 Node / 🔧 Step) in any pane header to switch between views.' },
                { icon: '✏️', text: this.t('LangCode') === 'ja'
                    ? 'Prompt/Params ペインのテキストボックスは直接編集可能。変更後は 💾 Apply Changes を押してください。'
                    : 'Edit prompts directly in the Prompt/Params pane. Click 💾 Apply Changes to save.' }
            ]
        },
        {
            icon: '⚙',
            title: 'APIキーを設定する',
            body: '<p>AIパイプラインを実行するには、使用するプロバイダのAPIキーが必要です。</p><p>右上の ⚙ Config ボタンから設定できます。</p>',
            tips: [
                { icon: '🆓', text: 'Ollama はローカルで動くためAPIキー不要です。' }
            ]
        },
        {
            icon: '📄',
            title: '素材ノードを作る',
            body: '<p>ツリーにノードを追加して、処理したいテキストや画像を入れます。</p>',
            tips: [
                { icon: '🖼', text: 'テキスト以外にも画像・PDF・RTFを格納できます。' },
                { icon: '🎙️', text: '入力欄では音声での入力（🎙️）やテキストの読み上げ（🔊）も可能です。' }
            ]
        },
        {
            icon: '▶',
            title: 'パイプラインを実行する',
            body: '<p>ノードを選択して ▶ Run ボタンを押すと、パイプラインが実行されます。</p>',
            tips: [
                { icon: '⚖', text: '同じ素材を複数のAIに同時投げて結果を比較できます。' },
                { icon: '💾', text: '成果物ノードにはパイプライン情報が自動記録されます。' }
            ]
        }
        ];
    },

    wizardStep_: 0,

    showWizard(forceStep) {
        try {
            this.wizardStep_ = forceStep || 0;
            const modal = document.getElementById('wizard-modal');
            if (!modal) { this.addLog('⚠ wizard-modal not found in DOM'); return; }
            modal.classList.add('visible');
            this.renderWizardStep();
        } catch (e) {
            this.addLog('❌ Welcome Guide error: ' + (e.message || e));
        }
    },

    closeWizard() {
        document.getElementById('wizard-modal').classList.remove('visible');
        localStorage.setItem('prompts_wizard_done', '1');
    },

    resetWizard() {
        localStorage.removeItem('prompts_wizard_done');
        this.addLog('🔄 Welcome Wizard reset — will show on next launch');
    },

    renderWizardStep() {
        try {
            const steps = this.WIZARD_STEPS;
            if (!steps || steps.length === 0) { this.addLog('⚠ Wizard steps empty'); return; }
            const s = steps[this.wizardStep_];
            if (!s) { this.addLog('⚠ Invalid wizard step: ' + this.wizardStep_); return; }
            const total = steps.length;
            const cur = this.wizardStep_;

            const progressEl = document.getElementById('wizard-progress');
            if (progressEl) progressEl.innerHTML =
                steps.map((_, i) => `<span class="wizard-dot${i === cur ? ' active' : ''}"></span>`).join('');

            const tipsHtml = s.tips && s.tips.length ? `
                <div class="wizard-tips">
                    <div class="wizard-tips-label">💡 Tips</div>
                    ${s.tips.map(t => `
                        <div class="wizard-tip">
                            <span class="wizard-tip-icon">${t.icon}</span>
                            <span class="wizard-tip-text">${t.text}</span>
                        </div>`).join('')}
                </div>` : '';
            const bodyEl = document.getElementById('wizard-body');
            if (bodyEl) bodyEl.innerHTML = `
                <div class="wizard-icon">${s.icon}</div>
                <h2 class="wizard-title">${this.escapeHtml(s.title)}</h2>
                <div class="wizard-text">${s.body}</div>
                ${tipsHtml}`;

            const prevBtn = document.getElementById('wizard-prev');
            if (prevBtn) prevBtn.style.visibility = cur === 0 ? 'hidden' : '';
            const nextBtn = document.getElementById('wizard-next');
            if (nextBtn) {
                if (cur === total - 1) {
                    nextBtn.textContent = '✓ 完了';
                    nextBtn.onclick = () => this.closeWizard();
                } else {
                    nextBtn.textContent = '次へ →';
                    nextBtn.onclick = () => this.wizardNext();
                }
            }
            const skipBtn = document.getElementById('wizard-skip');
            if (skipBtn) skipBtn.style.display = cur === total - 1 ? 'none' : '';
        } catch (e) {
            this.addLog('❌ renderWizardStep error: ' + (e.message || e));
        }
    },

    wizardNext() {
        if (this.wizardStep_ < this.WIZARD_STEPS.length - 1) {
            this.wizardStep_++;
            this.renderWizardStep();
        }
    },

    wizardPrev() {
        if (this.wizardStep_ > 0) {
            this.wizardStep_--;
            this.renderWizardStep();
        }
    },

    // ── Hamburger menu ────────────────────────────────────────────
    showHamburger(event) {
        event.stopPropagation();
        const existing = document.getElementById('hamburger-dropdown');
        if (existing) { existing.remove(); return; }

        const t = key => this.t(key);
        const sep = '<div class="hmenu-sep"></div>';
        const item = (label, action, shortcut='') =>
            `<div class="hmenu-item" onclick="app.hmenuAction('${action}')">${label}${shortcut ? `<span class="hmenu-shortcut">${shortcut}</span>` : ''}</div>`;
        const section = (title, items) =>
            `<div class="hmenu-section">${title}</div>${items}`;

        const html = `
            ${section(t('MenuFile'),
                item(t('MenuNewTab'),      'new_tab',       'Ctrl+T') +
                item(t('MenuOpen'),        'open',          'Ctrl+O') +
                item(t('MenuSave'),        'save',          'Ctrl+S') +
                item(t('MenuSaveAs'),      'save_as',       'Ctrl+Shift+S') +
                sep +
                item('📦 ' + t('MenuImportZip'),  'import_zip') +
                item('📤 ' + t('MenuExportNode'), 'export_node') +
                sep +
                item('🤖 ' + t('MenuWelcomeWizard'), 'welcome_wizard', 'F1') +
                item('🔄 Reset Welcome Wizard',       'reset_wizard') +
                item('🚀 ' + t('MenuSetupWizard'),   'setup_wizard')
            )}
            ${sep}
            ${section(t('MenuEdit'),
                item(t('MenuUndo'),        'undo',          'Ctrl+Z') +
                item(t('MenuRedo'),        'redo',          'Ctrl+Y') +
                sep +
                item(t('MenuCut'),         'cut',           'Ctrl+X') +
                item(t('MenuCopyText'),    'copy',          'Ctrl+C') +
                item(t('MenuPaste'),       'paste',         'Ctrl+V') +
                item(t('MenuSelectAll'),   'select_all',    'Ctrl+A') +
                sep +
                item(t('AddChild'),        'add_child',     'Alt+Ins') +
                item(t('Remove'),          'remove_node',   'Del')
            )}
            ${sep}
            ${section(t('MenuSettings'),
                item('⚙ ' + t('MenuSettingsItem'),  'settings') +
                item('🔑 ' + t('Config'),             'config')
            )}
            ${sep}
            ${section('Recipe',
                item('📋 Recipe Manager',  'recipe_manager', 'Ctrl+R')
            )}
            ${sep}
            ${section(t('MenuHelp'),
                item(t('MenuKeyboardShortcuts'), 'shortcuts', 'F1') +
                item(t('MenuAbout'),             'about') +
                item(t('MenuCopyright'),         'copyright')
            )}`;

        const dropdown = document.createElement('div');
        dropdown.id = 'hamburger-dropdown';
        dropdown.className = 'hamburger-dropdown';
        dropdown.innerHTML = html;

        const btn = document.getElementById('btn-hamburger');
        const r = btn.getBoundingClientRect();
        dropdown.style.top = (r.bottom + 4) + 'px';
        dropdown.style.left = r.left + 'px';
        document.body.appendChild(dropdown);

        setTimeout(() => document.addEventListener('click', function close() {
            dropdown.remove();
            document.removeEventListener('click', close);
        }), 0);
    },

    hmenuAction(action) {
        document.getElementById('hamburger-dropdown')?.remove();
        const map = {
            new_tab:        () => this.newTab(),
            open:           () => this.openFile(),
            save:           () => this.saveFile(),
            save_as:        () => this.saveFileAs(),
            import_zip:     () => this.addLog('📦 Import ZIP — coming soon'),
            export_node:    () => this.addLog('📤 Export Node — coming soon'),
            welcome_wizard: () => this.showWizard(),
            setup_wizard:   () => this.showSetupWizard(),
            undo:           () => document.execCommand('undo'),
            redo:           () => document.execCommand('redo'),
            cut:            () => document.execCommand('cut'),
            copy:           () => document.execCommand('copy'),
            paste:          () => document.execCommand('paste'),
            select_all:     () => document.execCommand('selectAll'),
            add_child:      () => this.addChild(),
            remove_node:    () => this.removeNode(),
            settings:       () => this.showSettings(),
            config:         () => this.showConfig(),
            recipe_manager: () => this.showRecipeManager(),
            shortcuts:      () => this.showWizard(3),
            about:          () => this.showAbout(),
            copyright:      () => this.showCopyright(),
        };
        if (map[action]) map[action]();
    },

    // ── Settings dialog ────────────────────────────────────────────
    showSettings() {
        const modal = document.getElementById('settings-modal');
        const sel = document.getElementById('settings-lang');
        if (sel) sel.value = this.state.language;
        modal.classList.add('visible');
    },

    closeSettings() {
        document.getElementById('settings-modal').classList.remove('visible');
    },

    saveSettings() {
        const sel = document.getElementById('settings-lang');
        if (!sel) return;
        const lang = sel.value;
        this.loadLanguage(lang);
        this.postMessage({ type: 'set_language', payload: { language: lang } });
        this.closeSettings();
        this.addLog(`🌐 Language set to: ${sel.options[sel.selectedIndex].text}`);
    },

    // ── Execution History ─────────────────────────────────────────
    showHistory() {
        const modal = document.getElementById('history-modal');
        if (!modal) return;
        document.getElementById('history-list-view').innerHTML =
            '<div class="history-loading">読み込み中...</div>';
        document.getElementById('history-list-view').style.display = '';
        document.getElementById('history-detail-view').style.display = 'none';
        modal.classList.add('visible');
        this.postMessage({ type: 'history_list' });
    },

    closeHistory() {
        document.getElementById('history-modal').classList.remove('visible');
    },

    onHistoryListResult(payload) {
        const items = (payload && payload.items) ? payload.items : [];
        const listView = document.getElementById('history-list-view');
        if (!listView) return;
        if (items.length === 0) {
            listView.innerHTML = '<div class="history-empty">実行履歴がありません</div>';
            return;
        }
        listView.innerHTML = items.map(item => {
            const evalBadge = this.evalBadgeHtml(item.evaluation || '');
            return `<div class="history-item ${item.evaluation ? 'has-eval eval-' + item.evaluation : ''}" onclick="app.showHistoryDetail(${JSON.stringify(item.id)})">
                <div class="history-item-name">${evalBadge}${this.escapeHtml(item.pipelineName || '')}</div>
                <div class="history-item-meta">
                    <span class="history-item-date">${this.escapeHtml((item.executedAt || item.startedAt || '').replace('T',' ').replace('Z',''))}</span>
                    <span class="history-item-steps">${item.stepCount} step${item.stepCount !== 1 ? 's' : ''}</span>
                    <span class="history-item-status ${this.escapeHtml(item.status || 'completed')}">${this.escapeHtml(item.status || 'completed')}</span>
                </div>
            </div>`;
        }).join('');
    },

    showHistoryDetail(id) {
        document.getElementById('history-list-view').style.display = 'none';
        const detailView = document.getElementById('history-detail-view');
        detailView.style.display = '';
        detailView.innerHTML = '<div class="history-loading">読み込み中...</div>';
        this.postMessage({ type: 'history_detail', payload: { id } });
    },

    onHistoryDetailResult(record) {
        const detailView = document.getElementById('history-detail-view');
        if (!detailView) return;
        if (!record || !record.pipelineName) {
            detailView.innerHTML = '<div class="history-empty">データが見つかりません</div>';
            return;
        }
        const runId = record.id || '';
        const runEval = record.evaluation || '';

        const stepsHtml = (record.steps || []).map((step, i) => {
            const stepEval = step.evaluation || '';
            const stepEvalBtns = this.evalButtonsHtml(`${runId}|${i}`, 'step', stepEval);
            const evalBadge = this.evalBadgeHtml(stepEval);
            return `
            <div class="history-step ${stepEval ? 'has-eval eval-' + stepEval : ''}">
                <div class="history-step-header" onclick="this.parentElement.classList.toggle('expanded')">
                    <span class="history-step-num">${i + 1}</span>
                    <span class="history-step-name">${evalBadge}${this.escapeHtml(step.name || '')}</span>
                    <span class="history-step-type">${this.escapeHtml(step.type || '')}</span>
                    ${step.promptTokens || step.completionTokens ? `<span class="history-step-tokens">${(step.promptTokens||0)+(step.completionTokens||0)} tok</span>` : ''}
                    <span class="history-step-eval-btns" onclick="event.stopPropagation()">${stepEvalBtns}</span>
                    <span class="history-step-toggle">▶</span>
                </div>
                <div class="history-step-body">
                    <div class="history-step-section">
                        <div class="history-step-label">Input</div>
                        <pre class="history-step-content">${this.escapeHtml(step.input || '')}</pre>
                    </div>
                    <div class="history-step-section">
                        <div class="history-step-label">Output</div>
                        <pre class="history-step-content">${this.escapeHtml(step.output || '')}</pre>
                    </div>
                </div>
            </div>`;
        }).join('');

        const runEvalBtns = this.evalButtonsHtml(runId, 'run', runEval);
        detailView.innerHTML = `
            <div class="history-detail-nav">
                <button class="btn-back" onclick="app.backToHistoryList()">← 一覧に戻る</button>
            </div>
            <div class="history-detail-header">
                <div class="history-detail-name">${this.escapeHtml(record.pipelineName || '')}</div>
                <div class="history-detail-meta">
                    <span class="history-detail-date">${this.escapeHtml((record.startedAt || record.executedAt || '').replace('T',' ').replace('Z',''))}</span>
                    <span class="history-detail-run-eval">${runEvalBtns}</span>
                </div>
            </div>
            ${record.outputContent ? `<div class="history-detail-output">
                <div class="history-step-label">最終出力</div>
                <pre class="history-step-content">${this.escapeHtml(record.outputContent)}</pre>
            </div>` : ''}
            <div class="history-steps">${stepsHtml}</div>`;
    },

    backToHistoryList() {
        document.getElementById('history-detail-view').style.display = 'none';
        document.getElementById('history-list-view').style.display = '';
        this.postMessage({ type: 'history_list' });
    },

    // ── Optimize Modal ─────────────────────────────────────────────

    showOptimize() {
        const modal = document.getElementById('optimize-modal');
        if (!modal) return;
        this._optimizeSession = null;

        // Populate pipeline selector
        const pipelineSel = document.getElementById('opt-pipeline-select');
        const pipelines = this.state.pipelines || [];
        if (pipelines.length === 0) {
            this.addLog('⚠ パイプラインがありません');
            return;
        }
        pipelineSel.innerHTML = pipelines.map(p =>
            `<option value="${this.escapeHtml(p.name)}">${this.escapeHtml(p.name)}</option>`
        ).join('');

        // Populate provider/model selectors
        this._populateOptProviders();

        // Show config view by default
        this.switchOptTab('config', document.querySelector('.opt-tab'));

        // Hide loading/proposals
        this._showOptView('config');

        // Load version info for currently selected pipeline
        const name = pipelineSel.value;
        if (name) this.postMessage({ type: 'optimize_version_list', payload: { pipelineName: name } });

        modal.classList.add('visible');
    },

    closeOptimize() {
        document.getElementById('optimize-modal').classList.remove('visible');
        this._optimizeSession = null;
    },

    discardOptimize() {
        this._optimizeSession = null;
        this._showOptView('config');
    },

    _populateOptProviders() {
        const providers = this.state.providers || {};
        const provSel = document.getElementById('opt-provider-select');
        const modelSel = document.getElementById('opt-model-select');
        const keys = Object.keys(providers);
        provSel.innerHTML = keys.map(k => `<option value="${this.escapeHtml(k)}">${this.escapeHtml(k)}</option>`).join('');
        provSel.onchange = () => this._updateOptModels();
        this._updateOptModels();
    },

    _updateOptModels() {
        const providers = this.state.providers || {};
        const provSel = document.getElementById('opt-provider-select');
        const modelSel = document.getElementById('opt-model-select');
        if (!provSel || !modelSel) return;
        const prov = providers[provSel.value];
        const models = (prov && prov.models) ? prov.models : [];
        modelSel.innerHTML = models.map(m => `<option value="${this.escapeHtml(m)}">${this.escapeHtml(m)}</option>`).join('');
    },

    switchOptTab(tabName, btn) {
        document.querySelectorAll('.opt-tab').forEach(t => t.classList.remove('active'));
        if (btn) btn.classList.add('active');
        if (tabName === 'config') {
            this._showOptView('config');
        } else if (tabName === 'versions') {
            this._showOptView('versions');
            const name = document.getElementById('opt-pipeline-select')?.value;
            if (name) this.postMessage({ type: 'optimize_version_list', payload: { pipelineName: name } });
        }
    },

    _showOptView(viewName) {
        ['config', 'loading', 'proposals', 'versions'].forEach(v => {
            const el = document.getElementById(`optimize-${v}-view`);
            if (el) el.style.display = v === viewName ? '' : 'none';
        });
    },

    runOptimize() {
        const pipelineName = document.getElementById('opt-pipeline-select')?.value;
        const historyLimit = parseInt(document.getElementById('opt-history-limit')?.value) || 10;
        const maxEditsPerStep = parseInt(document.getElementById('opt-max-edits')?.value) || 3;
        const provider = document.getElementById('opt-provider-select')?.value;
        const model = document.getElementById('opt-model-select')?.value;

        if (!pipelineName || !provider || !model) {
            this.addLog('⚠ パイプライン・プロバイダ・モデルを選択してください');
            return;
        }

        this._showOptView('loading');
        document.getElementById('optimize-progress-text').textContent = '準備中...';

        this.postMessage({ type: 'optimize_pipeline', payload: { pipelineName, historyLimit, maxEditsPerStep, provider, model } });
    },

    applyOptimize() {
        if (!this._optimizeSession) return;
        const proposals = this._optimizeSession.proposals || [];
        const approved = [], rejected = [];
        proposals.forEach((_, i) => {
            const card = document.getElementById(`opt-proposal-${i}`);
            if (!card) return;
            if (card.dataset.decision === 'rejected') rejected.push(i);
            else approved.push(i);
        });
        this.postMessage({ type: 'optimize_apply', payload: {
            sessionId: this._optimizeSession.sessionId,
            pipelineName: this._optimizeSession.pipelineName,
            approved,
            rejected
        }});
    },

    onOptimizeProposals(payload) {
        this._optimizeSession = {
            sessionId: payload.sessionId,
            pipelineName: payload.pipelineName,
            proposals: payload.proposals || []
        };
        const summary = payload.evaluationSummary || {};

        // Show eval summary
        const summaryEl = document.getElementById('optimize-eval-summary');
        if (summaryEl) {
            summaryEl.style.display = '';
            summaryEl.innerHTML = `<span class="opt-eval-count ok">👍 OK: ${summary.okCount||0}</span>
                <span class="opt-eval-count rejected">👎 却下: ${summary.rejectedCount||0}</span>
                <span class="opt-eval-count pinned">📌 ピン止め: ${summary.pinnedCount||0}</span>
                <span class="opt-eval-hint">（これらのシグナルを使って最適化しました）</span>`;
        }

        const listEl = document.getElementById('optimize-proposals-list');
        if (!listEl) return;
        if (this._optimizeSession.proposals.length === 0) {
            listEl.innerHTML = '<div class="opt-no-proposals">提案がありませんでした。実行履歴に評価（OK/却下）を付けると精度が上がります。</div>';
        } else {
            listEl.innerHTML = this._optimizeSession.proposals.map((p, i) => {
                const opClass = { replace: 'op-replace', add: 'op-add', delete: 'op-delete' }[p.op] || '';
                const opLabel = { replace: '置換', add: '追加', delete: '削除' }[p.op] || p.op;
                return `<div class="opt-proposal-card" id="opt-proposal-${i}" data-decision="approved">
                    <div class="opt-proposal-header">
                        <span class="opt-op-badge ${opClass}">${opLabel}</span>
                        <span class="opt-proposal-target">${this.escapeHtml(p.stepName)} › ${this.escapeHtml(p.field)}</span>
                        <span class="opt-proposal-decision-btns">
                            <button class="opt-dec-btn approve active" onclick="app.setProposalDecision(${i},'approved')">✓ 承認</button>
                            <button class="opt-dec-btn reject" onclick="app.setProposalDecision(${i},'rejected')">✗ 却下</button>
                        </span>
                    </div>
                    ${p.op !== 'add' && p.oldValue ? `<div class="opt-diff-row old"><span class="opt-diff-label">現在</span><pre class="opt-diff-text">${this.escapeHtml(p.oldValue)}</pre></div>` : ''}
                    ${p.op !== 'delete' && p.newValue ? `<div class="opt-diff-row new"><span class="opt-diff-label">${p.op === 'add' ? '追加' : '変更後'}</span><pre class="opt-diff-text">${this.escapeHtml(p.newValue)}</pre></div>` : ''}
                    <div class="opt-rationale">${this.escapeHtml(p.rationale || '')}</div>
                </div>`;
            }).join('');
        }
        this._showOptView('proposals');
    },

    setProposalDecision(index, decision) {
        const card = document.getElementById(`opt-proposal-${index}`);
        if (!card) return;
        card.dataset.decision = decision;
        card.querySelectorAll('.opt-dec-btn').forEach(b => b.classList.remove('active'));
        const btn = card.querySelector(`.opt-dec-btn.${decision === 'approved' ? 'approve' : 'reject'}`);
        if (btn) btn.classList.add('active');
        card.classList.toggle('opt-rejected', decision === 'rejected');
    },

    onOptimizeApplied(payload) {
        this.addLog(`✅ 最適化適用: v${payload.version} (承認 ${payload.approvedCount}, 却下 ${payload.rejectedCount})`);
        this._optimizeSession = null;
        // Refresh version list and switch to it
        const name = payload.pipelineName;
        if (name) {
            this.postMessage({ type: 'optimize_version_list', payload: { pipelineName: name } });
        }
        this._showOptView('versions');
        // Switch tab button
        document.querySelectorAll('.opt-tab').forEach(t => t.classList.remove('active'));
        const vTab = document.querySelector('.opt-tab[onclick*="versions"]');
        if (vTab) vTab.classList.add('active');
    },

    onOptimizeVersionChanged(payload) {
        const undoBtn = document.getElementById('btn-undo-opt');
        const redoBtn = document.getElementById('btn-redo-opt');
        if (undoBtn) undoBtn.disabled = !payload.canUndo;
        if (redoBtn) redoBtn.disabled = !payload.canRedo;
        this.addLog(`🔄 ${payload.pipelineName} → v${payload.version}`);
        // Refresh version list if modal is open
        const modal = document.getElementById('optimize-modal');
        if (modal && modal.classList.contains('visible')) {
            this.postMessage({ type: 'optimize_version_list', payload: { pipelineName: payload.pipelineName } });
        }
    },

    onOptimizeVersionListResult(payload) {
        const cursor = payload.cursor || {};
        const entries = cursor.entries || [];
        const listEl = document.getElementById('optimize-versions-list');
        if (!listEl) return;
        if (entries.length === 0) {
            listEl.innerHTML = '<div class="opt-no-proposals">バージョン履歴がありません。最適化を実行するとここに記録されます。</div>';
            return;
        }
        const current = cursor.currentVersion || 0;
        listEl.innerHTML = [...entries].reverse().map(e => {
            const isCurrent = e.version === current;
            const ts = (e.timestamp || '').replace('T',' ').replace('Z','');
            return `<div class="opt-version-item ${isCurrent ? 'current' : ''}">
                <span class="opt-ver-num">v${e.version}</span>
                ${isCurrent ? '<span class="opt-ver-current-badge">現在</span>' : ''}
                <span class="opt-ver-label">${this.escapeHtml(e.label || '')}</span>
                <span class="opt-ver-date">${this.escapeHtml(ts)}</span>
                <span class="opt-ver-actions">
                    ${!isCurrent ? `<button class="opt-ver-btn" onclick="app.checkoutVersion(${JSON.stringify(payload.pipelineName)},${e.version})">Checkout</button>` : ''}
                    ${e.version > 1 ? `<button class="opt-ver-btn" onclick="app.reapplyVersion(${JSON.stringify(payload.pipelineName)},${e.version})">Re-apply</button>` : ''}
                </span>
            </div>`;
        }).join('');
    },

    optimizeUndo() {
        const pipelines = this.state.pipelines || [];
        if (pipelines.length === 0) return;
        // Use the pipeline currently selected in toolbar, or first
        const name = this._lastOptPipeline || (pipelines[0] && pipelines[0].name) || '';
        if (name) this.postMessage({ type: 'optimize_undo', payload: { pipelineName: name } });
    },

    optimizeRedo() {
        const pipelines = this.state.pipelines || [];
        const name = this._lastOptPipeline || (pipelines[0] && pipelines[0].name) || '';
        if (name) this.postMessage({ type: 'optimize_redo', payload: { pipelineName: name } });
    },

    checkoutVersion(pipelineName, version) {
        if (!confirm(`v${version} に切り替えますか？`)) return;
        this._lastOptPipeline = pipelineName;
        this.postMessage({ type: 'optimize_checkout', payload: { pipelineName, version } });
    },

    reapplyVersion(pipelineName, version) {
        if (!confirm(`v${version} の承認済み提案を現在のパイプラインに再適用しますか？`)) return;
        this._lastOptPipeline = pipelineName;
        this.postMessage({ type: 'optimize_reapply', payload: { pipelineName, version } });
    },

    onOptimizeError(payload) {
        this._showOptView('config');
        this.addLog(`❌ 最適化エラー: ${payload.message || ''}`);
        this.showError(payload.message || '最適化に失敗しました');
    },

    onOptimizeProgress(payload) {
        const el = document.getElementById('optimize-progress-text');
        if (el) el.textContent = payload.message || '';
    },

    // ── About / Copyright ──────────────────────────────────────────
    showAbout() {
        const modal = document.getElementById('about-modal');
        modal.classList.add('visible');
        this.applyTranslations();
    },

    closeAbout() {
        document.getElementById('about-modal').classList.remove('visible');
    },

    showCopyright() {
        const modal = document.getElementById('copyright-modal');
        const body = document.getElementById('copyright-body');
        if (body) body.textContent = this.t('CopyrightBody') ||
            'Prompts — Part of the Ecode project.\n\nThird-party libraries:\n' +
            '• marked.js — MIT License\n• mark.js — MIT License\n' +
            '• mermaid.js — MIT License\n• cytoscape.js — MIT License\n' +
            '• Microsoft WebView2 SDK — BSD 3-Clause\n• Mbed TLS — Apache 2.0 / GPL 2.0+';
        modal.classList.add('visible');
    },

    closeCopyright() {
        document.getElementById('copyright-modal').classList.remove('visible');
    },

    t(key) {
        return (this.state.translations && this.state.translations[key]) || key;
    },

    // ── Filter Step UI ────────────────────────────────────────────
    filterState_: { outputs: [], stepIndex: 0 },

    showFilterStep(payload) {
        const { index, mode, outputs } = payload;
        const t = key => this.t(key);
        this.filterState_ = { outputs: outputs || [], stepIndex: index };

        const modal = document.getElementById('filter-modal');
        if (!modal) return;
        const body = document.getElementById('filter-body');
        if (!body) return;

        let html = `<h3>${t('FilterTitle')} — ${t('Step')} ${index + 1}</h3>`;
        (outputs || []).forEach((out, i) => {
            html += `<div class="filter-card" id="filter-card-${i}">
                <div class="filter-content">${this.escapeHtml(out.content)}</div>
                <div class="filter-actions">
                    <button class="btn-primary filter-approve" onclick="app.filterDecision(${i}, 'approved')">${t('Save')}</button>
                    <button class="filter-reject" onclick="app.filterDecision(${i}, 'rejected')">${t('Discard')}</button>
                </div>
            </div>`;
        });

        body.innerHTML = html;
        modal.classList.add('visible');
    },

    filterDecision(index, decision) {
        const card = document.getElementById(`filter-card-${index}`);
        if (!card) return;
        card.classList.add(decision === 'approved' ? 'approved' : 'rejected');
        card.querySelectorAll('button').forEach(b => b.disabled = true);
    },

    closeFilter() {
        const modal = document.getElementById('filter-modal');
        if (modal) modal.classList.remove('visible');
        const approved = [], rejected = [];
        (this.filterState_.outputs || []).forEach((_, i) => {
            const card = document.getElementById(`filter-card-${i}`);
            if (!card) return;
            if (card.classList.contains('approved')) approved.push(i);
            else if (card.classList.contains('rejected')) rejected.push(i);
            else approved.push(i);
        });
        this.postMessage({ type: 'step_filter_resume', payload: { stepIndex: this.filterState_.stepIndex, approved, rejected } });
    },

    filterDecision(index, decision) {
        const card = document.getElementById(`filter-card-${index}`);
        if (!card) return;
        card.classList.add(decision === 'approved' ? 'approved' : 'rejected');
        // Disable buttons after decision
        card.querySelectorAll('button').forEach(b => b.disabled = true);
    },

    closeFilter() {
        const modal = document.getElementById('filter-modal');
        if (modal) modal.classList.remove('visible');
        // Gather decisions
        const approved = [], rejected = [];
        (this.filterState_.outputs || []).forEach((_, i) => {
            const card = document.getElementById(`filter-card-${i}`);
            if (!card) return;
            if (card.classList.contains('approved')) approved.push(i);
            else if (card.classList.contains('rejected')) rejected.push(i);
            else approved.push(i); // default: approve
        });
        this.postMessage({ type: 'step_filter_resume', payload: { stepIndex: this.filterState_.stepIndex, approved, rejected } });
    },

    // ── Evaluate UI ────────────────────────────────────────────────
    showEvaluateResult(payload) {
        const { stepIndex, content, criteria, rubric } = payload;
        this.addLog(`★ Step ${stepIndex + 1} evaluation: "${criteria}" (${rubric})`);
        // Show in Output pane
        const outputEl = document.getElementById('output-content');
        if (!outputEl) return;
        outputEl.innerHTML += `<div class="eval-badge">★ Evaluating... <span class="eval-criteria">${this.escapeHtml(criteria)}</span></div>`;
    },

    // ── Incomplete Runs UI ─────────────────────────────────────────
    showIncompleteRuns() {
        const runs = this.state.incompleteRuns;
        const t = key => this.t(key);
        if (!runs || runs.length === 0) return;
        const modal = document.getElementById('recovery-modal');
        if (!modal) return;
        const body = document.getElementById('recovery-body');
        if (!body) return;

        body.innerHTML = runs.map(r => `
            <div class="recovery-item">
                <div class="recovery-name">📋 ${this.escapeHtml(r.pipelineName)}</div>
                <div class="recovery-meta">${t('RecoveryDesc').replace('{last}', r.lastCompletedStep).replace('{total}', r.totalSteps).replace('{started}', r.startedAt)}</div>
                <div class="recovery-actions">
                    <button class="btn-primary" onclick="app.resumeRun('${this.escapeHtml(r.runId)}', 'continue')">▶ ${t('Resume')}</button>
                    <button onclick="app.resumeRun('${this.escapeHtml(r.runId)}', 'keep')">📝 ${t('KeepOnly')}</button>
                    <button onclick="app.resumeRun('${this.escapeHtml(r.runId)}', 'discard')">🗑 ${t('Discard')}</button>
                </div>
            </div>
        `).join('');
        modal.classList.add('visible');
    },

    resumeRun(runId, action) {
        this.postMessage({ type: 'resume_run', payload: { runId, action } });
        document.getElementById('recovery-modal')?.classList.remove('visible');
    },

    // ── 5-Pane Rendering ───────────────────────────────────────────
    renderMainContent() {
        const t = key => this.t(key);
        const modeText = (this.state.viewMode === 'pipeline' && this.state.pipelineRun.selectedStep >= 0)
            ? `🔧 ${t('Step')} ${this.state.pipelineRun.selectedStep + 1} ▾`
            : `📄 ${t('NodeView')} ▾`;

        ['input-meta', 'prompt-meta', 'output-meta'].forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.textContent = modeText;
                el.style.cursor = 'pointer';
                el.title = t('ViewMode');
                el.onclick = (e) => {
                    e.stopPropagation();
                    this.showModeMenu(e);
                };
            }
        });

        this.renderInput();
        this.renderPrompt();
        this.renderOutput();
    },

    renderInput() {
        const inputEl = document.getElementById('input-content');
        if (!inputEl) return;
        const t = key => this.t(key);

        if (this.state.viewMode === 'pipeline') {
            this.renderPipelineInput(inputEl);
            return;
        }

        const currentPath = this.state.currentNodePath;
        const node = this.getNodeByPath(currentPath);
        if (!node) {
            inputEl.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }

        const getRunResults = (opNode) => {
            if (!opNode || !opNode.children) return [];
            const proc = opNode.children.find(c => c.nodeType === 'placeholder' || (!c.nodeType && c.title && this.safeAtob(c.title) === 'Processed'));
            return proc ? (proc.children || []) : opNode.children;
        };

        let inputData = '';
        const selectedDataPath = this.state.selectedDataPath;
        const selectedOpPath = this.state.selectedOpPath;
        const isCombined = selectedOpPath !== '' && selectedDataPath !== '';

        if (isCombined) {
            // 連結モード: 実行履歴グリッドを入力ペインに表示（出力アイコンが入力ペインへ移る）
            const opNode = this.getNodeByPath(selectedOpPath);
            const runs = getRunResults(opNode);
            inputEl.innerHTML = this.renderLinkedRunHistory(runs, true);
            return;
        } else if (selectedDataPath !== '') {
            // 閲覧モード（データノードのみ選択）: 送信に使ったテキストを表示
            const dataNode = this.getNodeByPath(selectedDataPath);
            if (dataNode && dataNode.input !== undefined) {
                inputData = dataNode.input;
            } else if (dataNode && dataNode.pipelineMeta) {
                try {
                    const meta = JSON.parse(dataNode.pipelineMeta);
                    if (meta && meta.steps && meta.steps.length > 0) {
                        inputData = meta.steps[0].input || '';
                    }
                } catch(e) {}
            }
            if (!inputData && dataNode && dataNode.content) {
                try { inputData = atob(dataNode.content); } catch { inputData = dataNode.content; }
            }
        } else {
            // 通常モード: 演算ノードのテンプレートを表示
            if (node.content) {
                try { inputData = atob(node.content); } catch { inputData = node.content; }
            }
        }
        // Belt-level media attachments (inputAttachments, separate from machine-level node.attachments)
        const inputAttachments = node.inputAttachments || [];
        const attachHtml = inputAttachments.length > 0
            ? inputAttachments.map((a, i) => {
                const name = a.file || a.id || 'attachment';
                return `<div class="list-item" style="display:flex;align-items:center;gap:4px;font-size:11px;padding:3px 4px">
                    <span style="flex:1">${this.escapeHtml(a.mimetype || '')}: ${this.escapeHtml(name)}${a.size ? ' (' + Math.round(a.size/1024) + 'KB)' : ''}</span>
                    <button class="copy-btn" onclick="app.removeInputAttachment(${i})" title="削除">✕</button>
                </div>`;
            }).join('')
            : `<div style="font-size:11px;color:#666;padding:4px">(なし)</div>`;
        inputEl.innerHTML = `
            <div style="margin-bottom:6px">
                <div style="font-size:10px;color:#888;margin-bottom:2px">入力テキスト ({content}):</div>
                <textarea id="input-textarea" class="input-textarea" placeholder="${t('NoInput')}">${this.escapeHtml(inputData)}</textarea>
            </div>
            <div>
                <div style="font-size:10px;color:#888;margin-bottom:3px;border-bottom:1px solid #333;padding-bottom:2px;display:flex;align-items:center;justify-content:space-between">
                    <span>追加メディア入力 (Belt attachments)</span>
                    <button class="copy-btn" onclick="app.addInputAttachment()" style="font-size:10px;padding:1px 6px">＋</button>
                </div>
                <div id="input-attachments-list" ${this._dropZoneAttrs('input_attachment')}
                     style="min-height:32px;border:1px dashed #3c3c3c;border-radius:3px;padding:2px">${attachHtml}</div>
            </div>`;
    },

    // ── Drag-and-drop file handling ──────────────────────────────
    // Called from ondrop attributes on attachment drop zones.
    // purpose: 'machine_attachment' | 'input_attachment' | 'step_attachment'
    handleFileDrop(event, purpose, stepIndex) {
        event.preventDefault();
        event.stopPropagation();
        const el = event.currentTarget;
        el.style.outline = '';
        const files = Array.from(event.dataTransfer.files)
            .filter(f => f.type.startsWith('image/') || f.type.startsWith('audio/') || f.type.startsWith('video/'));
        if (files.length === 0) return;
        Promise.all(files.map(f => new Promise(resolve => {
            const reader = new FileReader();
            reader.onload = e => resolve({
                file: f.name,
                path: f.path || '',
                mimetype: f.type,
                content: e.target.result.split(',')[1],
                size: f.size,
            });
            reader.readAsDataURL(f);
        }))).then(attachments => {
            this.onMediaFileDialogResult({ purpose, stepIndex, attachments });
        });
    },

    _dropZoneAttrs(purpose, stepIndex) {
        const si = stepIndex != null ? `,${stepIndex}` : '';
        return `ondragover="event.preventDefault();this.style.outline='2px dashed #4fc3f7'"` +
               ` ondragleave="this.style.outline=''"` +
               ` ondrop="app.handleFileDrop(event,'${purpose}'${si !== '' ? si : ''})"`;
    },

    addInputAttachment() {
        this.postMessage({ type: 'open_file_dialog', payload: { filter: 'media', purpose: 'input_attachment' } });
    },

    removeInputAttachment(index) {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;
        if (!node.inputAttachments) node.inputAttachments = [];
        node.inputAttachments.splice(index, 1);
        this.saveCurrentTab();
        this.renderInput();
    },

    renderPipelineInput(el) {
        const si = this.state.pipelineRun.selectedStep;
        const t = key => this.t(key);
        if (si < 0 || this.state.pipelineRun.steps.length === 0 || si >= this.state.pipelineRun.steps.length) {
            el.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const step = this.state.pipelineRun.steps[si];
        const inputText = step.input || '(pending)';
        const sourceLabel = si === 0 ? `元入力 ({content})` : `Step ${si} 出力 ({result})`;
        // Previous step output media (outputAttachments) → show as media grid
        const prevStep = si > 0 ? this.state.pipelineRun.steps[si - 1] : null;
        const prevOutputAttachments = (prevStep && prevStep.outputAttachments) || [];
        const prevArtifacts = (prevStep && prevStep.artifacts) || [];
        const prevMediaHtml = (prevOutputAttachments.length > 0 || prevArtifacts.length > 0)
            ? `<div style="margin-top:6px;font-size:10px;color:#888;margin-bottom:3px">前ステップ出力メディア:</div>
               ${this.renderOutputGrid('', prevOutputAttachments, prevArtifacts)}`
            : '';
        const artifactsHtml = '';
        // Step-specific attachments from pipelineMeta
        const stepAttachments = step.attachments || [];
        const stepAttachHtml = stepAttachments.length > 0
            ? stepAttachments.map((a, i) => `<div class="list-item" style="display:flex;align-items:center;gap:4px;font-size:11px;padding:3px 4px">
                <span style="flex:1">${this.escapeHtml(a.mimetype || '')}: ${this.escapeHtml(a.file || a.id || '')}</span>
                <button class="copy-btn" onclick="app.removeStepAttachment(${si},${i})" title="削除">✕</button>
              </div>`).join('')
            : `<div style="font-size:11px;color:#666;padding:4px">(添付なし)</div>`;
        el.innerHTML = `
            <div style="margin-bottom:6px">
                <div style="font-size:10px;color:#888;margin-bottom:2px;display:flex;align-items:center;justify-content:space-between">
                    <span>${sourceLabel}</span>
                    <button class="input-source-btn" onclick="app.showInputSourceDialog()">📂 ${t('Change')}</button>
                </div>
                <pre class="input-display" style="margin:0;background:#1a1a1a;border:1px solid #2d2d2d;padding:6px;white-space:pre-wrap;font-size:11px;max-height:120px;overflow-y:auto">${this.escapeHtml(inputText)}</pre>
                ${prevMediaHtml}
            </div>
            <div>
                <div style="font-size:10px;color:#888;margin-bottom:3px;border-bottom:1px solid #333;padding-bottom:2px;display:flex;align-items:center;justify-content:space-between">
                    <span>固有追加入力 (Attachments)</span>
                    <button class="copy-btn" onclick="app.addStepAttachment(${si})" style="font-size:10px;padding:1px 6px">＋</button>
                </div>
                <div id="step-attachments-${si}" ${this._dropZoneAttrs('step_attachment', si)}
                     style="min-height:32px;border:1px dashed #3c3c3c;border-radius:3px;padding:2px">${stepAttachHtml}</div>
            </div>`;
    },

    addStepAttachment(stepIndex) {
        this.postMessage({ type: 'open_file_dialog', payload: { filter: 'media', purpose: 'step_attachment', stepIndex } });
    },

    removeStepAttachment(stepIndex, attachIndex) {
        const step = this.state.pipelineRun.steps && this.state.pipelineRun.steps[stepIndex];
        if (!step || !step.attachments) return;
        step.attachments.splice(attachIndex, 1);
        // Persist to pipelineMeta
        this._savePipelineStepAttachments(stepIndex);
        this.renderInput();
    },

    _savePipelineStepAttachments(stepIndex) {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.pipelineMeta) return;
        try {
            const meta = JSON.parse(node.pipelineMeta);
            if (meta && meta.steps && meta.steps[stepIndex]) {
                meta.steps[stepIndex].attachments = (this.state.pipelineRun.steps[stepIndex] || {}).attachments || [];
                node.pipelineMeta = JSON.stringify(meta);
                this.saveCurrentTab();
            }
        } catch (e) {}
    },

    renderPrompt() {
        const promptEl = document.getElementById('prompt-content');
        if (!promptEl) return;
        const t = key => this.t(key);

        if (this.state.viewMode === 'pipeline') {
            this.renderPipelinePrompt(promptEl);
            return;
        }

        // If the selected node is a data/leaf node, show the parent operation node's prompt
        const promptNodePath = this.state.selectedOpPath || this.state.currentNodePath;
        let node = this.getNodeByPath(promptNodePath);
        if (node && node.nodeType === 'data' && node.originalOpNode) {
            node = node.originalOpNode;
        }
        if (!node) {
            promptEl.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }

        // Prompt text
        const promptText = node.content ? (() => { try { return atob(node.content); } catch { return node.content; } })() : '';

        let meta = {};
        if (node.pipelineMeta) {
            try { meta = JSON.parse(node.pipelineMeta) || {}; } catch (e) {}
        }

        // Recipe selector
        const recipes = this.state.recipes || [];
        let recipeHtml = '';
        if (recipes.length > 0) {
            recipeHtml += `<div style="margin-bottom:6px">`;
            recipeHtml += `<div style="font-size:10px;color:#888;margin-bottom:3px">レシピ:</div>`;
            recipes.forEach((r, i) => {
                const sel = r.name === this.state.selectedRecipe ? 'background:#094771;color:#fff' : '';
                const icon = r.type === 'command' ? '⚙️' : '🤖';
                let detail = '';
                if (r.type === 'command') {
                    detail = this.escapeHtml(r.command || '');
                } else {
                    detail = this.escapeHtml(r.provider) + (r.model ? '/' + this.escapeHtml(r.model) : '');
                }
                recipeHtml += `<div class="recipe-option" style="cursor:pointer;padding:3px 6px;border-radius:3px;font-size:11px;margin-bottom:2px;${sel}" onclick="app.selectRecipe(${i})">
                    ${icon} ${this.escapeHtml(r.name)}
                    <span style="font-size:9px;color:#888">${detail}</span>
                </div>`;
            });
            recipeHtml += `</div>`;
        } else {
            recipeHtml += `<div style="font-size:10px;color:#888;margin-bottom:6px">Config > Recipes でレシピを追加</div>`;
        }

        // Machine-level attachments (node.attachments = images/audio/video for prompt context)
        const machineAttachments = node.attachments || [];
        const machineAttachHtml = machineAttachments.length > 0
            ? machineAttachments.map((a, i) => {
                const name = a.file || a.id || 'attachment';
                const icon = (a.mimetype || '').startsWith('image/') ? '🖼' : (a.mimetype || '').startsWith('audio/') ? '🎵' : (a.mimetype || '').startsWith('video/') ? '🎬' : '📎';
                return `<div class="list-item" style="display:flex;align-items:center;gap:4px;font-size:11px;padding:3px 4px">
                    <span style="flex:1">${icon} ${this.escapeHtml(name)}${a.size ? ' (' + Math.round(a.size/1024) + 'KB)' : ''}</span>
                    <button class="copy-btn" onclick="app.removeMachineAttachment(${i})" title="削除">✕</button>
                </div>`;
            }).join('')
            : `<div style="font-size:11px;color:#666;padding:4px">(なし)</div>`;

        promptEl.innerHTML = `
            <button class="btn-primary prompt-editor-process-btn" onclick="app.processPrompt()" style="width:100%;padding:4px;font-size:11px;margin-bottom:6px">▶ 処理実行</button>
            <div style="margin-bottom:6px">
                <div style="font-size:10px;color:#888;margin-bottom:2px">プロンプト:</div>
                <textarea id="node-content" class="input-textarea" placeholder="{content} で入力を参照" style="min-height:100px">${this.escapeHtml(promptText)}</textarea>
            </div>
            ${recipeHtml}
            <div style="margin-top:6px">
                <div style="font-size:10px;color:#888;margin-bottom:3px;border-bottom:1px solid #333;padding-bottom:2px;display:flex;align-items:center;justify-content:space-between">
                    <span>演算添付 (Machine attachments)</span>
                    <button class="copy-btn" onclick="app.addMachineAttachment()" style="font-size:10px;padding:1px 6px">＋</button>
                </div>
                <div id="machine-attachments-list" ${this._dropZoneAttrs('machine_attachment')}
                     style="min-height:32px;border:1px dashed #3c3c3c;border-radius:3px;padding:2px">${machineAttachHtml}</div>
            </div>`;

        // Render pipeline meta if available
        this.renderPipelineMeta(node);
    },

    addMachineAttachment() {
        this.postMessage({ type: 'open_file_dialog', payload: { filter: 'media', purpose: 'machine_attachment' } });
    },

    removeMachineAttachment(index) {
        let node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;
        if (node.nodeType === 'data' && node.originalOpNode) {
            node = node.originalOpNode;
        }
        if (!node.attachments) node.attachments = [];
        node.attachments.splice(index, 1);
        this.saveCurrentTab();
        this.renderPrompt();
    },

    editNodePipelineMeta() {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;
        this.showPipelineManager();
    },

    saveNodePipelineMeta() {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.pipelineMeta) return;
        let meta;
        try { meta = JSON.parse(node.pipelineMeta); } catch { return; }
        const el = document.getElementById('prompt-content');
        if (!el) return;

        // Read inputs back into meta
        el.querySelectorAll('.param-input').forEach(inp => {
            const stepIdx = parseInt(inp.dataset.step);
            const key = inp.dataset.key;
            if (meta.steps && meta.steps[stepIdx]) {
                meta.steps[stepIdx][key] = inp.value;
            }
        });

        node.pipelineMeta = JSON.stringify(meta);
        const tab = this.state.tabs[this.state.activeTab];
        if (tab && tab.file) {
            this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
        }
        document.querySelector('.prompt-apply-btn').style.display = 'none';
        this.addLog('💾 Node pipeline meta updated');
    },

    renderPipelinePrompt(el) {
        const si = this.state.pipelineRun.selectedStep;
        const t = key => this.t(key);
        if (si < 0 || this.state.pipelineRun.steps.length === 0 || si >= this.state.pipelineRun.steps.length) {
            el.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const step = this.state.pipelineRun.steps[si];
        const typeInfo = this.PM_STEP_TYPES[step.type] || { icon: '❓', label: step.type, fields: [] };
        let html = `<div class="prompt-header">
            ${typeInfo.icon} ${this.escapeHtml(typeInfo.label)}
            <button class="prompt-edit-btn" onclick="app.pmEditStep(${si})" data-hint="${t('EditStep')}">✏ ${t('EditStep')}</button>
        </div>`;

        if (step.params) {
            for (const [key, value] of Object.entries(step.params)) {
                const isLong = value.length > 80;
                html += `<div class="param-row">
                    <span class="param-key">${this.escapeHtml(key)}</span>
                    ${isLong
                        ? `<textarea class="param-textarea" data-param="${this.escapeHtml(key)}" data-step="${si}">${this.escapeHtml(value)}</textarea>`
                        : `<input class="param-input" data-param="${this.escapeHtml(key)}" data-step="${si}" value="${this.escapeHtml(value)}">`
                    }
                </div>`;
            }
        }
        html += `<button class="prompt-apply-btn" onclick="app.applyPromptEdits(${si})" style="display:none">💾 ${t('ApplyChanges')}</button>`;
        el.innerHTML = html;

        // Show apply button when any textarea changes
        el.querySelectorAll('.param-textarea').forEach(ta => {
            ta.oninput = () => {
                document.querySelector('.prompt-apply-btn').style.display = '';
            };
        });
    },

    applyPromptEdits(stepIndex) {
        const el = document.getElementById('prompt-content');
        if (!el || this.state.pipelineRun.steps.length === 0 || !this.state.pipelineRun.steps[stepIndex]) return;
        const step = this.state.pipelineRun.steps[stepIndex];
        // Collect from both textareas and inputs
        el.querySelectorAll('.param-textarea, .param-input').forEach(field => {
            const key = field.dataset.param;
            if (key) step.params[key] = field.value;
        });
        document.querySelector('.prompt-apply-btn').style.display = 'none';
        this.addLog(`✏ Step ${stepIndex + 1} params updated`);
        this.postMessage({ type: 'save_pipeline', payload: { name: this.state.pipelines?.[0]?.name || '', steps: this.state.pipelineRun.steps } });
    },

    renderOutput() {
        const outputEl = document.getElementById('output-content');
        if (!outputEl) return;
        const t = key => this.t(key);

        if (this.state.viewMode === 'pipeline') {
            this.renderPipelineOutput(outputEl);
            return;
        }

        const getRunResults = (opNode) => {
            if (!opNode || !opNode.children) return [];
            const proc = opNode.children.find(c => c.nodeType === 'placeholder' || (!c.nodeType && c.title && this.safeAtob(c.title) === 'Processed'));
            return proc ? (proc.children || []) : opNode.children;
        };

        const currentPath = this.state.currentNodePath;
        const selectedDataPath = this.state.selectedDataPath;
        const selectedOpPath = this.state.selectedOpPath;
        const isCombined = selectedOpPath !== '' && selectedDataPath !== '';

        let runs = [];
        let selectedIdx = 0;

        if (selectedOpPath !== '' && selectedDataPath === '') {
            // 演算ノードのみ選択: 実行履歴グリッドを出力ペインに表示
            const opNode = this.getNodeByPath(selectedOpPath);
            const linkedRuns = getRunResults(opNode);
            outputEl.innerHTML = this.renderLinkedRunHistory(linkedRuns, false);
            return;
        } else if (selectedDataPath !== '') {
            // 連結モード or 閲覧モード: 選択されたデータノードの出力を表示
            // 閲覧モード: データノードを1件だけ表示
            const dataNode = this.getNodeByPath(selectedDataPath);
            if (dataNode) runs = [dataNode];
            selectedIdx = 0;
        } else {
            const opNode = this.getNodeByPath(currentPath);
            runs = getRunResults(opNode);
            selectedIdx = this.state.selectedOutputRunIndex !== undefined ? this.state.selectedOutputRunIndex : 0;
            if (selectedIdx >= runs.length) {
                selectedIdx = 0;
                this.state.selectedOutputRunIndex = 0;
            }
        }

        if (runs.length === 0) {
            outputEl.innerHTML = `<div class="empty">${t('NoOutput')}</div>`;
            return;
        }

        const child = runs[selectedIdx];
        let receivedText = child.content ? (() => { try { return atob(child.content); } catch { return child.content; } })() : '';
        let artifacts = [];

        let outputAttachments = child.attachments || [];
        if (child.pipelineMeta) {
            try {
                const meta = JSON.parse(child.pipelineMeta);
                if (meta && meta.steps && meta.steps.length > 0) {
                    const lastStep = meta.steps[meta.steps.length - 1];
                    receivedText = lastStep.output || receivedText;
                    artifacts = lastStep.artifacts || [];
                    outputAttachments = lastStep.outputAttachments || outputAttachments;
                }
            } catch(e) {}
        }

        let html;
        {
            const runOptions = runs.map((c, idx) => {
                const title = c.title ? this.safeAtob(c.title) : `Run ${idx + 1}`;
                return `<option value="${idx}" ${idx === selectedIdx ? 'selected' : ''}>${this.escapeHtml(title)}</option>`;
            }).join('');
            html = `
                <div class="output-toolbar">
                    <span class="output-label">${t('Output')} (${runs.length})</span>
                    <button class="output-save-btn" onclick="app.saveCurrentOutput()">${t('Save')}</button>
                    <button class="output-discard-btn" onclick="app.discardCurrentOutput()">${t('Discard')}</button>
                    <button class="output-chest-btn" onclick="app.sendToChestDialog()">${t('SendToChest')}</button>
                </div>
                <div class="output-run-selector-row" style="margin: 8px; display: flex; align-items: center; gap: 8px; font-size: 11px;">
                    <label for="output-run-selector" style="font-weight: bold; color: #858585;">実行履歴 (History):</label>
                    <select id="output-run-selector" onchange="app.onOutputRunSelected(this.value)" style="background: #252526; color: #ccc; border: 1px solid #3c3c3c; padding: 2px; font-size: 11px; flex: 1;">
                        ${runOptions}
                    </select>
                </div>`;
        }

        if (child.evaluation) {
            html += `<div class="eval-badge" style="margin: 0 8px 8px 8px;">★ ${this.escapeHtml(child.evaluation)}</div>`;
        }

        const contentHtml = this.renderOutputGrid(receivedText, outputAttachments, artifacts);

        html += `<div style="padding:8px;height:calc(100% - 75px);overflow-y:auto;">${contentHtml}</div>`;
        outputEl.innerHTML = html;
    },

    onOutputRunSelected(value) {
        const idx = parseInt(value);
        this.state.selectedOutputRunIndex = idx;

        if (this.state.selectedDataPath !== '') {
            const opNode = this.getNodeByPath(this.state.selectedOpPath);
            if (opNode && opNode.children) {
                const processedIdx = opNode.children.findIndex(c => c.nodeType === 'placeholder' || (!c.nodeType && c.title && this.safeAtob(c.title) === 'Processed'));
                if (processedIdx !== -1) {
                    const newPath = this.state.selectedOpPath + '/' + processedIdx + '/' + idx;
                    this.selectNode(newPath);
                    return;
                }
            }
        }

        this.renderInput();
        this.renderOutput();
    },

    // ── 連結モード 実行履歴カードグリッド ──────────────────────────────
    renderLinkedRunHistory(runs, isCombined = false) {
        const historyHidden = localStorage.getItem('prompts.historyHidden') === '1';
        const toggleLabel = historyHidden ? '履歴 ▶' : '履歴 ▼';
        const label = isCombined ? '🔴 連結モード' : '出力';

        if (runs.length === 0) {
            return `<div class="output-toolbar">
                        <span class="output-label">${label}</span>
                        <button class="output-save-btn" onclick="app.toggleRunHistory()">${toggleLabel}</button>
                    </div>
                    <div class="empty">実行履歴なし</div>`;
        }

        const selectedIdx = this.state.selectedOutputRunIndex ?? -1;

        const makeDetail = (child) => {
            let inputText = '', outputText = '';
            let inputAttachments = child.attachments || [];
            let outputAttachments = [], artifacts = [];
            if (child.pipelineMeta) {
                try {
                    const meta = JSON.parse(child.pipelineMeta);
                    if (meta.steps?.length > 0) {
                        inputText = meta.steps[0].input || '';
                        const last = meta.steps[meta.steps.length - 1];
                        outputText = last.output || '';
                        outputAttachments = last.outputAttachments || [];
                        artifacts = last.artifacts || [];
                    }
                } catch(e) {}
            }
            if (!outputText && child.content) {
                try { outputText = atob(child.content); } catch { outputText = child.content; }
            }
            return `<div class="linked-run-detail">
                <details open>
                    <summary style="font-size:11px;font-weight:bold;color:#888;cursor:pointer;padding:4px 0">📤 送信データ</summary>
                    <pre class="output-display" style="max-height:120px;overflow-y:auto;font-size:11px">${this.escapeHtml(inputText)}</pre>
                    ${this.renderOutputGrid('', inputAttachments, [])}
                </details>
                <details open>
                    <summary style="font-size:11px;font-weight:bold;color:#888;cursor:pointer;padding:4px 0">📥 受信データ</summary>
                    <pre class="output-display" style="max-height:120px;overflow-y:auto;font-size:11px">${this.escapeHtml(outputText)}</pre>
                    ${this.renderOutputGrid('', outputAttachments, artifacts)}
                </details>
            </div>`;
        };

        const items = runs.map((child, idx) => {
            let icon = '📄';
            if (child.pipelineMeta) {
                try {
                    const meta = JSON.parse(child.pipelineMeta);
                    const last = meta.steps?.[meta.steps.length - 1];
                    const att = last?.outputAttachments?.[0];
                    if (att?.mimetype?.startsWith('image/')) icon = '🖼';
                    else if (att?.mimetype?.startsWith('video/')) icon = '🎬';
                    else if (att?.mimetype?.startsWith('audio/')) icon = '🎵';
                } catch(e) {}
            }
            const title = this.escapeHtml(child.title ? this.safeAtob(child.title) : `Run ${idx + 1}`);
            const evalBadge = child.evaluation ? `<div class="linked-run-eval">★ ${this.escapeHtml(child.evaluation)}</div>` : '';
            const isSelected = idx === selectedIdx;
            const detail = isSelected ? makeDetail(child) : '';
            return `<div class="linked-run-item">
                <div class="linked-run-card${isSelected ? ' selected' : ''}" onclick="app.selectLinkedRun(${idx})">
                    <div class="linked-run-icon">${icon}</div>
                    <div class="linked-run-title">${title}</div>
                    ${evalBadge}
                </div>
                ${detail}
            </div>`;
        }).join('');

        const gridHtml = historyHidden ? '' : `<div class="linked-run-grid">${items}</div>`;

        return `<div class="output-toolbar">
                    <span class="output-label">${label} — 実行履歴 (${runs.length}件)</span>
                    <button class="output-save-btn" onclick="app.toggleRunHistory()">${toggleLabel}</button>
                </div>
                ${gridHtml}`;
    },

    selectLinkedRun(idx) {
        // 同じカードを再クリックで折りたたみ
        this.state.selectedOutputRunIndex = (this.state.selectedOutputRunIndex === idx) ? -1 : idx;
        this.renderInput();
        this.renderOutput();
    },

    toggleRunHistory() {
        const current = localStorage.getItem('prompts.historyHidden') === '1';
        localStorage.setItem('prompts.historyHidden', current ? '0' : '1');
        this.renderOutput();
    },

    // ── Output media grid ───────────────────────────────────────────
    renderOutputGrid(text, attachments, artifacts) {
        const cards = [];

        // Text card
        if (text && text.trim()) {
            const preview = this.escapeHtml(text.trim().substring(0, 120).replace(/\n/g, ' '));
            const encoded = encodeURIComponent(text);
            cards.push(`
                <div class="output-card" onclick="app.showMediaViewer('text',decodeURIComponent('${encoded}'),'テキスト出力')">
                    <div class="output-card-icon">📄</div>
                    <div class="output-card-preview">${preview}</div>
                    <div class="output-card-label">テキスト出力</div>
                </div>`);
        }

        // Attachment cards (outputAttachments from AI response)
        (attachments || []).forEach((a, i) => {
            const mime = a.mimetype || '';
            const label = this.escapeHtml(a.file || `attachment-${i}`);
            if (mime.startsWith('image/')) {
                const src = `data:${mime};base64,${a.content || ''}`;
                cards.push(`
                    <div class="output-card" onclick="app.showMediaViewer('image','${src}','${label}')">
                        <img class="output-thumb" src="${src}" onerror="this.src=''">
                        <div class="output-card-label">${label}</div>
                    </div>`);
            } else if (mime.startsWith('video/')) {
                const src = `data:${mime};base64,${a.content || ''}`;
                cards.push(`
                    <div class="output-card" onclick="app.showMediaViewer('video','${src}','${label}')">
                        <div class="output-card-icon">🎬</div>
                        <div class="output-card-label">${label}</div>
                    </div>`);
            } else if (mime.startsWith('audio/')) {
                const src = `data:${mime};base64,${a.content || ''}`;
                cards.push(`
                    <div class="output-card" onclick="app.showMediaViewer('audio','${src}','${label}')">
                        <div class="output-card-icon">🎵</div>
                        <div class="output-card-label">${label}</div>
                    </div>`);
            } else {
                const content = a.content ? atob(a.content) : '';
                const encoded = encodeURIComponent(content);
                cards.push(`
                    <div class="output-card" onclick="app.showMediaViewer('text',decodeURIComponent('${encoded}'),'${label}')">
                        <div class="output-card-icon">📎</div>
                        <div class="output-card-label">${label}</div>
                    </div>`);
            }
        });

        // Artifact cards (file paths)
        (artifacts || []).forEach(a => {
            const label = this.escapeHtml(a.label || a.path || '');
            const ext = (a.path || '').split('.').pop().toLowerCase();
            const imgExts = ['png','jpg','jpeg','gif','webp','bmp','svg'];
            const vidExts = ['mp4','webm','mov','avi','mkv'];
            const audExts = ['mp3','wav','ogg','flac','m4a'];
            let icon = '🔗';
            let viewer = `app.openArtifact(${JSON.stringify(a)})`;
            if (imgExts.includes(ext)) icon = '🖼';
            else if (vidExts.includes(ext)) icon = '🎬';
            else if (audExts.includes(ext)) icon = '🎵';
            cards.push(`
                <div class="output-card" onclick="${viewer}">
                    <div class="output-card-icon">${icon}</div>
                    <div class="output-card-label">${label}</div>
                </div>`);
        });

        if (cards.length === 0) return `<div class="empty" style="font-size:12px;color:#555">出力なし</div>`;
        return `<div class="output-grid">${cards.join('')}</div>`;
    },

    showMediaViewer(type, src, label) {
        document.getElementById('media-viewer-overlay')?.remove();
        let body = '';
        if (type === 'text') {
            body = `<pre class="media-viewer-text">${this.escapeHtml(src)}</pre>
                    <button class="media-viewer-copy" onclick="navigator.clipboard.writeText(decodeURIComponent(encodeURIComponent(document.querySelector('.media-viewer-text').textContent))).then(()=>app.addLog('📋 Copied'))">📋 コピー</button>`;
        } else if (type === 'image') {
            body = `<img src="${src}" class="media-viewer-img" alt="${this.escapeHtml(label)}">`;
        } else if (type === 'video') {
            body = `<video src="${src}" controls class="media-viewer-video"></video>`;
        } else if (type === 'audio') {
            body = `<audio src="${src}" controls class="media-viewer-audio"></audio>`;
        }

        const overlay = document.createElement('div');
        overlay.id = 'media-viewer-overlay';
        overlay.className = 'media-viewer-overlay';
        overlay.innerHTML = `
            <div class="media-viewer-box">
                <div class="media-viewer-header">
                    <span>${this.escapeHtml(label)}</span>
                    <button class="media-viewer-close" onclick="app.closeMediaViewer()">✕</button>
                </div>
                <div class="media-viewer-body">${body}</div>
            </div>`;
        overlay.addEventListener('click', e => { if (e.target === overlay) this.closeMediaViewer(); });
        document.body.appendChild(overlay);
    },

    closeMediaViewer() {
        document.getElementById('media-viewer-overlay')?.remove();
    },

    renderPipelineOutput(el) {
        const si = this.state.pipelineRun.selectedStep;
        const t = key => this.t(key);
        if (si < 0 || this.state.pipelineRun.steps.length === 0 || si >= this.state.pipelineRun.steps.length) {
            el.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const step = this.state.pipelineRun.steps[si];
        // Show streaming output while running, completed output when done
        const outputText = step.completed
            ? (step.output || '(empty output)')
            : (step.streamingOutput || (step.status === 'running' ? '...' : '(pending)'));
        const artifacts = step.artifacts || [];
        const artifactsHtml = artifacts.length > 0
            ? `<div style="font-size:10px;color:#888;margin:8px 8px 3px;border-bottom:1px solid #333;padding-bottom:2px">生産物 (Artifacts)</div>` +
              artifacts.map(a => `<div style="font-size:11px;padding:2px 8px">🔗 <a style="color:#4fc3f7" href="#" onclick="app.openArtifact(${JSON.stringify(a)});return false">${this.escapeHtml(a.label || a.path || '')}</a></div>`).join('')
            : '';
        const outputAttachments = step.outputAttachments || [];
        el.innerHTML = `
            <div class="output-toolbar">
                <span class="output-label">${t('Step')} ${si + 1} ${t('Output')}${step.completed ? '' : ' ⏳'}</span>
                ${step.completed ? `<button class="output-save-btn" onclick="app.savePipelineOutput(${si})">${t('Save')}</button>
                <button class="output-chest-btn" onclick="app.sendToChestDialog()">${t('SendToChest')}</button>` : ''}
            </div>
            <pre class="output-display" id="pipeline-output-${si}">${this.escapeHtml(outputText)}</pre>
            <div style="padding:4px 8px">${this.renderOutputGrid('', outputAttachments, artifacts)}</div>
            <div id="pipeline-artifacts-${si}"></div>`;
    },

    // ── Input Source Dialog ────────────────────────────────────────
    showInputSourceDialog() {
        const modal = document.getElementById('input-source-modal');
        if (!modal) return;
        const body = document.getElementById('input-source-body');
        if (!body) return;
        const t = key => this.t(key);

        body.innerHTML = `
            <h3>${t('Source')}</h3>
            <div class="source-option" onclick="app.selectInputSource('previous_step')">
                📦 ${t('PreviousStep')}
            </div>
            <div class="source-option" onclick="app.selectInputSource('manual')">
                ✏️ ${t('ManualInput')}
            </div>
            <div class="source-option" onclick="app.selectInputSource('chest')">
                📁 ${t('NamedChest')}
            </div>
            <div class="source-option" onclick="app.selectInputSource('file')">
                📂 ${t('ExternalFile')}
            </div>
            <div class="source-option" onclick="app.selectInputSource('checkpoint')">
                📜 ${t('PastCheckpoint')}
            </div>
            <div class="source-chest-name" style="display:none" id="source-chest-input">
                <input type="text" id="chest-name-input" placeholder="${t('EnterChestName')}">
                <button onclick="app.confirmChestSource()">${t('Confirm')}</button>
            </div>`;
        modal.classList.add('visible');
    },

    selectInputSource(source) {
        if (source === 'chest') {
            document.getElementById('source-chest-input').style.display = '';
            return;
        }
        if (source === 'manual') {
            const input = document.getElementById('input-textarea');
            if (input) {
                this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.pipelineRun.selectedStep, source: 'manual', content: input.value } });
            }
        } else if (source === 'checkpoint') {
            this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.pipelineRun.selectedStep, source: 'checkpoint' } });
        } else {
            this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.pipelineRun.selectedStep, source } });
        }
        document.getElementById('input-source-modal')?.classList.remove('visible');
    },

    confirmChestSource() {
        const name = document.getElementById('chest-name-input')?.value;
        if (!name) return;
        this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.pipelineRun.selectedStep, source: 'chest', chestName: name } });
        document.getElementById('input-source-modal')?.classList.remove('visible');
    },

    onMediaFileDialogResult(payload) {
        if (!payload || !payload.attachments || payload.attachments.length === 0) return;
        const purpose = payload.purpose;
        const attachments = payload.attachments;
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;

        if (purpose === 'machine_attachment') {
            let targetNode = node;
            if (node.nodeType === 'data' && node.originalOpNode) {
                targetNode = node.originalOpNode;
            }
            if (!targetNode.attachments) targetNode.attachments = [];
            targetNode.attachments.push(...attachments);
            this.saveCurrentTab();
            this.renderPrompt();
        } else if (purpose === 'input_attachment') {
            if (!node.inputAttachments) node.inputAttachments = [];
            node.inputAttachments.push(...attachments);
            this.saveCurrentTab();
            this.renderInput();
        } else if (purpose === 'step_attachment') {
            const si = payload.stepIndex;
            if (si == null || this.state.pipelineRun.steps.length === 0 || !this.state.pipelineRun.steps[si]) return;
            const step = this.state.pipelineRun.steps[si];
            if (!step.attachments) step.attachments = [];
            step.attachments.push(...attachments);
            this._savePipelineStepAttachments(si);
            this.renderInput();
        }
    },

    openArtifact(artifact) {
        if (!artifact) return;
        if (artifact.path) {
            this.postMessage({ type: 'open_artifact', payload: artifact });
        }
    },

    // ── Chest Operations ───────────────────────────────────────────
    sendToChestDialog() {
        const t = key => this.t(key);
        const chestName = prompt(t('EnterChestName'), '');
        if (!chestName) return;
        const outputEl = document.getElementById('output-content');
        const content = outputEl ? outputEl.textContent : '';
        this.postMessage({ type: 'send_to_chest', payload: { content, chestName } });
        this.addLog(`📦 ${t('SendToChest')} "${chestName}"`);
    },

    saveCurrentOutput() {
        this.addLog(`✔ ${this.t('Save')}`);
    },

    discardCurrentOutput() {
        this.addLog(`✕ ${this.t('Discard')}`);
    },

    savePipelineOutput(stepIndex) {
        this.addLog(`✔ ${this.t('Step')} ${stepIndex + 1} ${this.t('Save')}`);
    },

    buildPromptHtml(meta) {
        let html = '<div class="prompt-steps">';
        if (meta.pipelineName) {
            html += `<div class="prompt-title">📋 ${this.escapeHtml(meta.pipelineName)}</div>`;
        }
        if (meta.steps) {
            (meta.steps || []).forEach((s, i) => {
                html += `<div class="prompt-step">
                    <div class="prompt-step-header">Step ${i + 1}: ${this.escapeHtml(s.name || s.type)}</div>`;
                for (const [key, value] of Object.entries(s)) {
                    if (key === 'name' || key === 'type') continue;
                    const displayVal = String(value).length > 200 ? String(value).substring(0, 200) + '...' : String(value);
                    html += `<div class="param-row"><span class="param-key">${this.escapeHtml(key)}</span><span class="param-value">${this.escapeHtml(displayVal)}</span></div>`;
                }
                html += '</div>';
            });
        }
        html += '</div>';
        return html;
    },

    // ── Project Switcher ───────────────────────────────────────────
    switchProject(name) {
        this.postMessage({ type: 'select_project', payload: { projectName: name } });
    },

    createProject() {
        const name = prompt('New project name:');
        if (!name) return;
        this.postMessage({ type: 'create_project', payload: { projectName: name } });
    },

    setHistoryRetention(val) {
        const n = parseInt(val);
        if (isNaN(n)) return;
        this.state.historyRetention = Math.max(10, Math.min(500, n));
        this.postMessage({ type: 'set_history_retention', payload: { maxRuns: this.state.historyRetention } });
    },

    // ── Setup Wizard ──────────────────────────────────────────────
    SW_TEMPLATES: [
        {
            id: 'translate',
            label: '🌐 翻訳プロジェクト',
            desc: 'テキストを複数言語に翻訳・比較',
            sample: '翻訳したいテキストをここに入力してください。\n\nHello, world! This is a sample text.',
            pipeline: '翻訳→比較選択'
        },
        {
            id: 'summarize',
            label: '📝 文章要約',
            desc: '長文を要約・ポイント抽出',
            sample: '要約したい文章をここに貼り付けてください。\n\n長い記事や文書のテキストを入力すると、AIが重要なポイントを抽出します。',
            pipeline: '要約'
        },
        {
            id: 'review',
            label: '✏️ 文章レビュー',
            desc: '文章を校正・改善提案',
            sample: 'レビューしたい文章をここに入力してください。\n\nAIが文法・表現・構成の改善点を提案します。',
            pipeline: 'レビュー'
        },
        {
            id: 'image',
            label: '🖼️ 画像分析・タグ付け',
            desc: '画像の説明、検出、タグの自動生成',
            sample: 'この画像を分析して、詳細な説明、写っているオブジェクトの一覧、関連するメタデータやタグを生成してください。',
            pipeline: '画像分析'
        },
        {
            id: 'video',
            label: '🎥 動画要約・構成分析',
            desc: '動画のハイライト、シーンとタイムライン要約',
            sample: 'この動画のコンテンツを分析し、主要なシーンの要約と、タイムラインに沿った構成表を作成してください。',
            pipeline: '動画要約'
        },
        {
            id: 'music',
            label: '🎵 楽曲・音声分析',
            desc: 'テンポ（BPM）やキー、音声書き起こし',
            sample: 'この楽曲または音声データを分析し、構成要素、BPM・キーなどの特徴、もしくは書き起こしを抽出してください。',
            pipeline: '楽曲音声分析'
        },
        {
            id: 'free',
            label: '🆓 自由形式',
            desc: 'テンプレートなしで自由に開始',
            sample: '',
            pipeline: ''
        }
    ],

    sw_: { step: 0, tabName: '', templateId: 'free', content: '', pipelineName: '' },

    showSetupWizard() {
        this.sw_ = { step: 0, tabName: 'プロジェクト ' + new Date().toLocaleDateString('ja'), templateId: 'free', content: '', pipelineName: '', files: [] };
        document.getElementById('setup-wizard-modal').classList.add('visible');
        this.swRender();
    },

    closeSetupWizard() {
        document.getElementById('setup-wizard-modal').classList.remove('visible');
        if ('speechSynthesis' in window) {
            window.speechSynthesis.cancel();
            this.clearAllSpeakingStyles();
        }
    },

    swRender() {
        const s = this.sw_;
        const total = 4;
        const cur = s.step;

        // Progress dots
        document.getElementById('sw-progress').innerHTML =
            Array.from({length: total}, (_, i) =>
                `<span class="wizard-dot${i === cur ? ' active' : i < cur ? ' done' : ''}"></span>`
            ).join('');

        const body = document.getElementById('sw-body');
        const nextBtn = document.getElementById('sw-next');

        if (cur === 0) {
            // Step 1: テンプレート選択 + プロジェクト名
            body.innerHTML = `
                <div class="wizard-icon">🚀</div>
                <h2 class="wizard-title">何を始めますか？</h2>
                <div class="sw-field">
                    <label class="sw-label">プロジェクト名</label>
                    <input type="text" id="sw-tab-name" class="sw-input" value="${this.escapeHtml(s.tabName)}" placeholder="例: 翻訳プロジェクト">
                </div>
                <div class="sw-label" style="margin:14px 0 8px">テンプレートを選択</div>
                <div class="sw-templates">${
                    this.SW_TEMPLATES.map(t => `
                        <div class="sw-template${s.templateId === t.id ? ' selected' : ''}" onclick="app.swSelectTemplate('${t.id}')">
                            <div class="sw-template-label">${t.label}</div>
                            <div class="sw-template-desc">${t.desc}</div>
                        </div>`).join('')
                }</div>`;
            nextBtn.textContent = '次へ →';
            nextBtn.onclick = () => {
                const nameEl = document.getElementById('sw-tab-name');
                this.sw_.tabName = nameEl ? nameEl.value.trim() || 'プロジェクト' : 'プロジェクト';
                const tmpl = this.SW_TEMPLATES.find(t => t.id === this.sw_.templateId) || this.SW_TEMPLATES.find(t => t.id === 'free');
                if (!this.sw_.content) this.sw_.content = tmpl.sample;
                this.sw_.pipelineName = tmpl.pipeline;
                this.swNext();
            };

        } else if (cur === 1) {
            // Step 2: コンテンツ入力
            body.innerHTML = `
                <div class="wizard-icon">📄</div>
                <h2 class="wizard-title">最初のコンテンツを入力</h2>
                <p class="wizard-text" style="margin-bottom:10px">処理したいテキストを入力・貼り付けてください。後から変更できます。</p>
                <div class="textarea-container">
                    <textarea id="sw-content" class="sw-textarea" placeholder="テキストをここに入力するか、ファイルをドロップ...">${this.escapeHtml(s.content)}</textarea>
                    <button class="speak-btn" id="sw-speak-btn" onclick="app.toggleSpeak('sw-content', 'sw-speak-btn')" title="音声読み上げ">🔊</button>
                    <button class="voice-btn" id="sw-voice-btn" onclick="app.toggleVoiceInput('sw-content', 'sw-voice-btn')" title="音声入力">🎙️</button>
                </div>
                <div class="sw-files-list" id="sw-files-list"></div>
                <div class="sw-hint">💡 テキスト入力に加え、画像・動画・音楽などのメディアファイルをここにドラッグ＆ドロップして追加できます。</div>`;
            
            const textarea = document.getElementById('sw-content');
            if (textarea) {
                ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
                    textarea.addEventListener(eventName, (e) => {
                        e.preventDefault();
                        e.stopPropagation();
                    }, false);
                });

                ['dragenter', 'dragover'].forEach(eventName => {
                    textarea.addEventListener(eventName, () => {
                        textarea.classList.add('dragover');
                    }, false);
                });

                ['dragleave', 'drop'].forEach(eventName => {
                    textarea.addEventListener(eventName, () => {
                        textarea.classList.remove('dragover');
                    }, false);
                });

                textarea.addEventListener('drop', (e) => {
                    const dt = e.dataTransfer;
                    const files = dt.files;
                    this.swHandleFiles(files);
                }, false);
            }

            this.swRenderFilesList();

            nextBtn.textContent = '次へ →';
            nextBtn.onclick = () => {
                const el = document.getElementById('sw-content');
                this.sw_.content = el ? el.value : '';
                this.swNext();
            };

        } else if (cur === 2) {
            // Step 3: パイプライン選択
            const pipelines = (this.state.pipelines || []).map(p => p.name);
            const hasPipelines = pipelines.length > 0;
            body.innerHTML = `
                <div class="wizard-icon">🔧</div>
                <h2 class="wizard-title">パイプラインを選択（任意）</h2>
                <p class="wizard-text" style="margin-bottom:12px">作成後にすぐ実行するパイプラインを選べます。</p>
                <div class="sw-pipeline-list">
                    <div class="sw-pipeline-item${!s.pipelineName ? ' selected' : ''}" onclick="app.swSelectPipeline('')">
                        <span>⏭ スキップ（後で実行）</span>
                    </div>
                    ${hasPipelines ? pipelines.map(name => `
                        <div class="sw-pipeline-item${s.pipelineName === name ? ' selected' : ''}" onclick="app.swSelectPipeline('${this.escapeHtml(name)}')">
                            <span>🔧 ${this.escapeHtml(name)}</span>
                        </div>`).join('') : `<div class="sw-hint" style="margin-top:8px">⚠ パイプラインがまだありません。<br>スキップしてノード作成後に設定できます。</div>`}
                </div>`;
            nextBtn.textContent = '確認 →';
            nextBtn.onclick = () => this.swNext();

        } else if (cur === 3) {
            // Step 4: 確認
            const tmpl = this.SW_TEMPLATES.find(t => t.id === s.templateId);
            const preview = s.content ? s.content.slice(0, 80) + (s.content.length > 80 ? '…' : '') : '（空）';
            const filesCount = s.files ? s.files.length : 0;
            const filesSummary = filesCount > 0 ? `${filesCount} 個のファイル添付` : 'なし';
            body.innerHTML = `
                <div class="wizard-icon">✅</div>
                <h2 class="wizard-title">準備完了！</h2>
                <div class="sw-summary">
                    <div class="sw-summary-row"><span class="sw-summary-label">プロジェクト名</span><span>${this.escapeHtml(s.tabName)}</span></div>
                    <div class="sw-summary-row"><span class="sw-summary-label">テンプレート</span><span>${tmpl ? tmpl.label : '自由形式'}</span></div>
                    <div class="sw-summary-row"><span class="sw-summary-label">コンテンツ</span><span class="sw-summary-preview">${this.escapeHtml(preview)}</span></div>
                    <div class="sw-summary-row"><span class="sw-summary-label">添付ファイル</span><span>${filesSummary}</span></div>
                    <div class="sw-summary-row"><span class="sw-summary-label">パイプライン</span><span>${s.pipelineName ? '🔧 ' + this.escapeHtml(s.pipelineName) : 'スキップ'}</span></div>
                </div>`;
            nextBtn.textContent = '🚀 作成する';
            nextBtn.onclick = () => this.swCreate();
        }

        document.getElementById('sw-prev').style.visibility = cur === 0 ? 'hidden' : '';
        document.getElementById('sw-cancel').style.display = cur === 3 ? 'none' : '';
    },

    swSelectTemplate(id) {
        this.sw_.templateId = id;
        const tmpl = this.SW_TEMPLATES.find(t => t.id === id) || this.SW_TEMPLATES.find(t => t.id === 'free');
        this.sw_.content = tmpl.sample;
        this.sw_.pipelineName = tmpl.pipeline;
        this.swRender();
    },

    swSelectPipeline(name) {
        this.sw_.pipelineName = name;
        this.swRender();
    },

    swNext() {
        if (this.sw_.step < 3) { this.sw_.step++; this.swRender(); }
    },

    swPrev() {
        if (this.sw_.step > 0) { this.sw_.step--; this.swRender(); }
    },

    swHandleFiles(files) {
        if (!files || files.length === 0) return;
        for (let i = 0; i < files.length; i++) {
            const f = files[i];
            const reader = new FileReader();
            
            const isText = f.type.startsWith('text/') || f.name.endsWith('.txt') || f.name.endsWith('.json') || f.name.endsWith('.md');
            
            reader.onload = (e) => {
                const res = e.target.result;
                if (isText) {
                    const ta = document.getElementById('sw-content');
                    if (ta && !ta.value.trim()) {
                        ta.value = res;
                        this.sw_.content = res;
                    }
                }
                
                let base64Data = '';
                if (isText) {
                    try {
                        base64Data = btoa(unescape(encodeURIComponent(res)));
                    } catch {
                        base64Data = btoa(res);
                    }
                } else {
                    const parts = res.split(',');
                    base64Data = parts.length > 1 ? parts[1] : res;
                }
                
                if (!this.sw_.files) this.sw_.files = [];
                if (!this.sw_.files.some(existing => existing.name === f.name)) {
                    this.sw_.files.push({
                        name: f.name,
                        size: f.size,
                        mimetype: f.type || 'application/octet-stream',
                        content: base64Data
                    });
                    this.swRenderFilesList();
                }
            };
            
            if (isText) {
                reader.readAsText(f);
            } else {
                reader.readAsDataURL(f);
            }
        }
    },

    swRenderFilesList() {
        const el = document.getElementById('sw-files-list');
        if (!el) return;
        const files = this.sw_.files || [];
        if (files.length === 0) {
            el.innerHTML = '';
            return;
        }
        el.innerHTML = files.map((f, i) => `
            <div class="sw-file-item">
                <div style="display:flex; flex-direction:column; gap:2px">
                    <span class="sw-file-name" title="${this.escapeHtml(f.name)}">${this.escapeHtml(f.name)}</span>
                    <span class="sw-file-info">${f.mimetype} (${Math.round(f.size/1024)} KB)</span>
                </div>
                <button class="sw-file-remove" onclick="app.swRemoveFile(${i})">×</button>
            </div>
        `).join('');
    },

    swRemoveFile(idx) {
        if (this.sw_.files) {
            this.sw_.files.splice(idx, 1);
            this.swRenderFilesList();
        }
    },

    swCreate() {
        const s = this.sw_;
        const safeB64 = str => { try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str || ''); } };

        const attachments = (s.files || []).map(f => ({
            id: 'att_' + Math.random().toString(36).substring(2, 11),
            mimetype: f.mimetype,
            inline: true,
            content: f.content,
            file: f.name,
            size: f.size
        }));

        // Build root node with content
        const rootNode = {
            title: safeB64(s.tabName),
            content: safeB64(s.content),
            mimetype: 'text/plain',
            attachments: attachments,
            children: [],
            nodeType: 'root'
        };

        // Create new tab in state
        const fileName = 'setup_' + Date.now() + '.json';
        const tab = { name: s.tabName, file: fileName, root: rootNode };
        this.state.tabs.push(tab);
        this.state.activeTab = this.state.tabs.length - 1;
        this.state.currentNodePath = '';

        // Save via bridge
        this.postMessage({ type: 'save_node', payload: { tabFile: fileName, root: rootNode } });

        // Also save session
        this.postMessage({ type: 'save_session', payload: {
            tabs: this.state.tabs.map(t => ({ name: t.name, file: t.file }))
        }});

        this.renderTabs();
        this.renderTree();
        this.renderList();
        this.loadEditor('');
        this.closeSetupWizard();
        this.addLog(`🚀 プロジェクト "${s.tabName}" を作成しました`);

        // Run pipeline if selected
        if (s.pipelineName) {
            setTimeout(() => this.runPipeline(s.pipelineName), 300);
        }
    },

    // ── Pipeline Manager ──────────────────────────────────────────

    PM_STEP_TYPES: {
        ai:         { icon: '🤖', label: 'AI Call', fields: ['provider','model','systemPrompt','userPrompt','temperature','maxTokens','customParams','attachMedia'] },
        wizard:     { icon: '🚀', label: 'Wizard', fields: ['wizard','wizardData'] },
        manual:     { icon: '📝', label: 'Manual Review', fields: ['mode','prompt','choices'] },
        command:    { icon: '⚙️', label: 'CLI Command', fields: ['command','args','workingDir','timeout','resultAs'] },
        tool:       { icon: '🔧', label: 'External Tool', fields: ['command','args','waitForExit','resultAs','resultFile','confirm'] },
        fetch:      { icon: '🌐', label: 'HTTP Fetch', fields: ['url','method','auth','resultAs'] },
        condition:  { icon: '🔀', label: 'Condition', fields: ['expression','operator','value','onTrue','onFalse'] },
        transform:  { icon: '🔄', label: 'Transform', fields: ['engine','expression','input'] },
        call_pipeline: { icon: '📦', label: 'Call Pipeline', fields: ['pipelineName','input','inheritAttachments'] },
        foreach:    { icon: '🔁', label: 'Foreach', fields: ['input','itemVariable','concurrency'] },
        parallel:   { icon: '⚡', label: 'Parallel', fields: ['branches','outputMode'] },
        wait:       { icon: '⏱️', label: 'Wait', fields: ['durationMs','until','pollIntervalMs','timeoutMs'] },
        history:    { icon: '📜', label: 'History', fields: ['runId','stepIndex','field'] }
    },

    pmState_: { pipelines: [], selectedIndex: -1, dirty: false, stepEditIndex: -1 },

    showPipelineManager() {
        this.pmState_.pipelines = (this.state.pipelines || []).slice();
        this.pmState_.selectedIndex = -1;
        this.pmState_.dirty = false;
        document.getElementById('pipeline-manager-modal').classList.add('visible');
        this.pmRenderPipelineList();
        document.getElementById('pm-editor').style.display = 'none';
        document.getElementById('pm-empty').style.display = '';
        document.getElementById('pm-mermaid').innerHTML = '';
    },

    closePipelineManager() {
        document.getElementById('pipeline-manager-modal').classList.remove('visible');
    },

    pmRenderPipelineList() {
        const el = document.getElementById('pm-pipeline-list');
        if (!el) return;
        const list = this.pmState_.pipelines;
        el.innerHTML = list.map((p, i) => `
            <div class="pm-pipeline-item${i === this.pmState_.selectedIndex ? ' active' : ''}"
                 onclick="app.pmSelectPipeline(${i})">
                ${this.escapeHtml(p.name || 'Unnamed')}
            </div>`).join('');
    },

    pmSelectPipeline(index) {
        this.pmState_.selectedIndex = index;
        this.pmState_.dirty = false;
        this.pmRenderPipelineList();
        this.pmLoadEditor();
    },

    pmNewPipeline() {
        const list = this.pmState_.pipelines;
        const name = 'New Pipeline ' + (list.length + 1);
        list.push({ name, mode: 'basic', outputMode: 'child', outputNaming: '{pipeline_name}_{timestamp}', retryCount: 3, retryDelayMs: 2000, steps: [] });
        this.pmState_.selectedIndex = list.length - 1;
        this.pmState_.dirty = true;
        this.pmRenderPipelineList();
        this.pmLoadEditor();
        this.addLog('➕ New pipeline created');
    },

    pmSwitchMode(mode, btn) {
        document.querySelectorAll('.pm-mode-tab').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById('pm-body-basic').style.display = mode === 'basic' ? '' : 'none';
        document.getElementById('pm-body-expert').style.display = mode === 'expert' ? '' : 'none';
        this.pmDirty();
    },

    pmLoadEditor() {
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) {
            document.getElementById('pm-editor').style.display = 'none';
            document.getElementById('pm-empty').style.display = '';
            return;
        }
        document.getElementById('pm-editor').style.display = '';
        document.getElementById('pm-empty').style.display = 'none';
        const p = list[i];
        document.getElementById('pm-name').value = p.name || '';
        document.getElementById('pm-output-mode').value = p.outputMode || 'child';
        document.getElementById('pm-output-naming').value = p.outputNaming || '{pipeline_name}_{timestamp}';
        document.getElementById('pm-retry-count').value = p.retryCount || 3;
        document.getElementById('pm-retry-delay').value = p.retryDelayMs || 2000;
        this.pmRenderSteps();
    },

    pmRenderSteps() {
        const el = document.getElementById('pm-step-list');
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (!el || i < 0 || i >= list.length) return;
        const steps = list[i].steps || [];
        const info = this.PM_STEP_TYPES;
        el.innerHTML = steps.map((s, si) => {
            const typeInfo = info[s.type] || { icon: '❓', label: s.type };
            return `<div class="pm-step-item">
                <span class="pm-step-drag" title="Drag to reorder">⠿</span>
                <span class="pm-step-icon">${typeInfo.icon}</span>
                <span class="pm-step-name" onclick="app.pmEditStep(${si})">${this.escapeHtml(s.name || typeInfo.label)}</span>
                <span class="pm-step-type-badge">${this.escapeHtml(s.type)}</span>
                <button class="pm-step-edit-btn" onclick="app.pmEditStep(${si})">✏</button>
                <button class="pm-step-del-btn" onclick="app.pmDeleteStep(${si})">✕</button>
            </div>`;
        }).join('');
        this.pmRenderMermaid();
    },

    pmAddStep() {
        const sel = document.getElementById('pm-step-type-select');
        const type = sel.value;
        const typeInfo = this.PM_STEP_TYPES[type] || { icon: '❓', label: type };
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) return;
        if (!list[i].steps) list[i].steps = [];
        list[i].steps.push({ name: typeInfo.label, type, params: {} });
        this.pmState_.dirty = true;
        this.pmRenderSteps();
        this.addLog(`➕ Step added: ${typeInfo.label}`);
    },

    pmEditStep(index) {
        this.pmState_.stepEditIndex = index;
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) return;
        const step = list[i].steps[index];
        if (!step) return;
        const typeInfo = this.PM_STEP_TYPES[step.type] || { icon: '❓', label: step.type, fields: [] };
        document.getElementById('pm-step-edit-title').textContent = `✏ ${typeInfo.icon} ${typeInfo.label}`;
        const form = document.getElementById('pm-step-edit-form');
        form.innerHTML = `
            <div class="field-row">
                <label>Name</label>
                <input type="text" id="pms-name" value="${this.escapeHtml(step.name || '')}">
            </div>
            <div class="field-row">
                <label>Type</label>
                <select id="pms-type" onchange="app.pmStepEditTypeChanged()">
                    ${Object.entries(this.PM_STEP_TYPES).map(([k, v]) =>
                        `<option value="${k}"${k === step.type ? ' selected' : ''}>${v.icon} ${v.label}</option>`
                    ).join('')}
                </select>
            </div>
            <div class="pm-step-edit-fields" id="pms-fields">${this.pmBuildFieldInputs(step)}</div>`;
        document.getElementById('pm-step-edit-modal').classList.add('visible');
    },

    pmStepEditTypeChanged() {
        const stepType = document.getElementById('pms-type').value;
        const i = this.pmState_.selectedIndex;
        const si = this.pmState_.stepEditIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length || si < 0) return;
        const step = list[i].steps[si];
        if (!step) return;
        step.type = stepType;
        const typeInfo = this.PM_STEP_TYPES[stepType] || { icon: '❓', label: stepType, fields: [] };
        document.getElementById('pm-step-edit-title').textContent = `✏ ${typeInfo.icon} ${typeInfo.label}`;
        // Keep name if exists, otherwise use type label
        if (!step.name || !step.name.trim()) step.name = typeInfo.label;
        document.getElementById('pms-name').value = step.name;
        document.getElementById('pms-fields').innerHTML = this.pmBuildFieldInputs(step);
    },

    pmBuildFieldInputs(step) {
        const typeInfo = this.PM_STEP_TYPES[step.type] || { fields: [] };
        return typeInfo.fields.map(f => {
            const val = step.params && step.params[f] ? step.params[f] : '';
            if (f === 'customParams') {
                const jsonStr = val && typeof val === 'object' ? JSON.stringify(val, null, 2) : (val || '');
                return `<div class="field-row" style="flex-direction:column;align-items:stretch">
                    <label>Custom Params (JSON)</label>
                    <textarea id="pms-${f}" style="height:60px;font-family:monospace;font-size:11px" placeholder='{"aspect_ratio": "16:9"}'>${this.escapeHtml(jsonStr)}</textarea>
                </div>`;
            }
            if (f === 'provider') {
                return `<div class="field-row">
                    <label>Provider</label>
                    <select id="pms-${f}">
                        <option value="openai"${val === 'openai' ? ' selected' : ''}>OpenAI</option>
                        <option value="anthropic"${val === 'anthropic' ? ' selected' : ''}>Anthropic</option>
                        <option value="gemini"${val === 'gemini' ? ' selected' : ''}>Gemini</option>
                        <option value="ollama"${val === 'ollama' ? ' selected' : ''}>Ollama</option>
                    </select>
                </div>`;
            }
            if (f === 'mode') {
                return `<div class="field-row">
                    <label>Mode</label>
                    <select id="pms-${f}">
                        <option value="view"${val === 'view' ? ' selected' : ''}>View</option>
                        <option value="edit"${val === 'edit' ? ' selected' : ''}>Edit</option>
                        <option value="select"${val === 'select' ? ' selected' : ''}>Select</option>
                    </select>
                </div>`;
            }
            if (f === 'method') {
                return `<div class="field-row">
                    <label>Method</label>
                    <select id="pms-${f}">
                        <option value="GET"${val === 'GET' ? ' selected' : ''}>GET</option>
                        <option value="POST"${val === 'POST' ? ' selected' : ''}>POST</option>
                    </select>
                </div>`;
            }
            if (f === 'operator') {
                return `<div class="field-row">
                    <label>Operator</label>
                    <select id="pms-${f}">
                        <option value="contains"${val === 'contains' ? ' selected' : ''}>contains</option>
                        <option value="equals"${val === 'equals' ? ' selected' : ''}>equals</option>
                        <option value="startsWith"${val === 'startsWith' ? ' selected' : ''}>startsWith</option>
                        <option value="regex"${val === 'regex' ? ' selected' : ''}>regex</option>
                    </select>
                </div>`;
            }
            if (f === 'resultAs') {
                return `<div class="field-row">
                    <label>Result As</label>
                    <select id="pms-${f}">
                        <option value="text"${val === 'text' ? ' selected' : ''}>text</option>
                        <option value="exitcode"${val === 'exitcode' ? ' selected' : ''}>exitcode</option>
                        <option value="file"${val === 'file' ? ' selected' : ''}>file</option>
                        <option value="attachment"${val === 'attachment' ? ' selected' : ''}>attachment</option>
                        <option value="json"${val === 'json' ? ' selected' : ''}>json</option>
                    </select>
                </div>`;
            }
            if (f === 'engine') {
                return `<div class="field-row">
                    <label>Engine</label>
                    <select id="pms-${f}">
                        <option value="regex"${val === 'regex' ? ' selected' : ''}>regex</option>
                        <option value="json_path"${val === 'json_path' ? ' selected' : ''}>json_path</option>
                        <option value="template"${val === 'template' ? ' selected' : ''}>template</option>
                    </select>
                </div>`;
            }
            if (f === 'waitForExit' || f === 'confirm' || f === 'inheritAttachments') {
                return `<div class="field-row">
                    <label>${f}</label>
                    <select id="pms-${f}">
                        <option value="true"${val === 'true' ? ' selected' : ''}>true</option>
                        <option value="false"${val !== 'true' ? ' selected' : ''}>false</option>
                    </select>
                </div>`;
            }
            if (f === 'temperature' || f === 'retryCount' || f === 'retryDelayMs' || f === 'timeout' || f === 'timeoutMs' || f === 'durationMs' || f === 'concurrency' || f === 'maxTokens' || f === 'pollIntervalMs') {
                return `<div class="field-row">
                    <label>${f}</label>
                    <input type="number" step="any" id="pms-${f}" value="${this.escapeHtml(val || '')}">
                </div>`;
            }
            if (f === 'systemPrompt' || f === 'userPrompt' || f === 'prompt' || f === 'choices') {
                return `<div class="field-row">
                    <label>${f}</label>
                    <textarea id="pms-${f}">${this.escapeHtml(val || '')}</textarea>
                </div>`;
            }
            return `<div class="field-row">
                <label>${f}</label>
                <input type="text" id="pms-${f}" value="${this.escapeHtml(val || '')}">
            </div>`;
        }).join('');
    },

    pmSaveStepEdit() {
        const i = this.pmState_.selectedIndex;
        const si = this.pmState_.stepEditIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length || si < 0) return;
        const step = list[i].steps[si];
        if (!step) return;

        let customParamsObj = {};
        const customParamsEl = document.getElementById('pms-customParams');
        if (customParamsEl) {
            const rawVal = customParamsEl.value.trim();
            if (rawVal !== '') {
                try {
                    customParamsObj = JSON.parse(rawVal);
                } catch (err) {
                    alert('Custom Params に指定された JSON のパースに失敗しました。正しい JSON 形式で入力してください。');
                    return;
                }
            }
        }

        step.name = document.getElementById('pms-name')?.value || step.name;
        step.type = document.getElementById('pms-type')?.value || step.type;
        const typeInfo = this.PM_STEP_TYPES[step.type] || { fields: [] };
        if (!step.params) step.params = {};
        typeInfo.fields.forEach(f => {
            if (f === 'customParams') {
                step.params[f] = customParamsObj;
            } else {
                const el = document.getElementById('pms-' + f);
                if (el) step.params[f] = el.value;
            }
        });
        this.pmState_.dirty = true;
        this.pmCloseStepEdit();
        this.pmRenderSteps();
        this.addLog(`✏ Step "${step.name}" updated`);
    },

    pmCloseStepEdit() {
        document.getElementById('pm-step-edit-modal').classList.remove('visible');
        this.pmState_.stepEditIndex = -1;
    },

    pmDeleteStep(index) {
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) return;
        list[i].steps.splice(index, 1);
        this.pmState_.dirty = true;
        this.pmRenderSteps();
        this.addLog('🗑 Step removed');
    },

    pmMoveStep(index, dir) {
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) return;
        const steps = list[i].steps;
        const newIdx = index + dir;
        if (newIdx < 0 || newIdx >= steps.length) return;
        [steps[index], steps[newIdx]] = [steps[newIdx], steps[index]];
        this.pmState_.dirty = true;
        this.pmRenderSteps();
    },

    pmRenderMermaid() {
        const el = document.getElementById('pm-mermaid');
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (!el || i < 0 || i >= list.length) { if (el) el.innerHTML = ''; return; }
        const steps = list[i].steps || [];
        if (steps.length === 0) { el.innerHTML = '<div style="color:#666;font-size:12px">No steps</div>'; return; }
        // Build mermaid flowchart
        let mermaidDef = 'graph LR\n';
        mermaidDef += '    Input[Input]\n';
        steps.forEach((s, si) => {
            const safeName = (s.name || 'step' + si).replace(/[^a-zA-Z0-9]/g, '_');
            const displayName = (s.name || s.type).replace(/"/g, "'");
            mermaidDef += `    ${safeName}["${si+1}. ${displayName}"]\n`;
            if (si === 0) mermaidDef += `    Input --> ${safeName}\n`;
            else {
                const prev = (steps[si-1].name || 'step' + (si-1)).replace(/[^a-zA-Z0-9]/g, '_');
                mermaidDef += `    ${prev} --> ${safeName}\n`;
            }
        });
        const last = (steps[steps.length-1].name || 'step' + (steps.length-1)).replace(/[^a-zA-Z0-9]/g, '_');
        mermaidDef += `    ${last} --> Output[Output]\n`;
        el.innerHTML = `<div class="mermaid">${mermaidDef}</div>`;
        // Render mermaid
        if (window.mermaid) {
            try {
                mermaid.run({ nodes: [el.querySelector('.mermaid')] });
            } catch(e) {
                // mermaid may already have rendered
            }
        }
    },

    pmGetCurrentPipeline() {
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) return null;
        const p = list[i];
        return {
            name: document.getElementById('pm-name')?.value || p.name,
            mode: 'basic',
            outputMode: document.getElementById('pm-output-mode')?.value || 'child',
            outputNaming: document.getElementById('pm-output-naming')?.value || '{pipeline_name}_{timestamp}',
            retryCount: parseInt(document.getElementById('pm-retry-count')?.value) || 3,
            retryDelayMs: parseInt(document.getElementById('pm-retry-delay')?.value) || 2000,
            steps: p.steps || []
        };
    },

    pmSave() {
        const pipeline = this.pmGetCurrentPipeline();
        if (!pipeline) return;
        // Update state
        const i = this.pmState_.selectedIndex;
        this.pmState_.pipelines[i] = pipeline;
        this.pmState_.dirty = false;
        // Send to C++
        this.postMessage({ type: 'save_pipeline', payload: pipeline });
        this.addLog(`💾 Pipeline "${pipeline.name}" saved`);
    },

    pmDelete() {
        const i = this.pmState_.selectedIndex;
        const list = this.pmState_.pipelines;
        if (i < 0 || i >= list.length) return;
        const name = list[i].name;
        if (!confirm(`Delete pipeline "${name}"?`)) return;
        this.postMessage({ type: 'delete_pipeline', payload: { name } });
        list.splice(i, 1);
        this.pmState_.selectedIndex = Math.min(i, list.length - 1);
        this.pmState_.dirty = false;
        this.pmRenderPipelineList();
        this.pmLoadEditor();
        this.addLog(`🗑 Pipeline "${name}" deleted`);
    },

    pmRunNow() {
        this.pmSave();
        const pipeline = this.pmGetCurrentPipeline();
        if (!pipeline || !pipeline.steps || pipeline.steps.length === 0) {
            this.addLog('⚠ No steps in pipeline');
            return;
        }
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) { this.addLog('⚠ Select a node first'); return; }
        const content = node.content ? (() => { try { return decodeURIComponent(escape(atob(node.content))); } catch { return atob(node.content); } })() : '';
        const tab = this.state.tabs[this.state.activeTab];
        this.postMessage({ type: 'run_pipeline', payload: {
            pipelineName: pipeline.name,
            nodeId: this.state.currentNodePath || '',
            tabFile: tab ? tab.file : '',
            content
        }});
        this.state.pipelineRun.running = true;
        this.closePipelineManager();
        this.addLog(`▶ Pipeline "${pipeline.name}" started`);
    },

    pmDirty() {
        this.pmState_.dirty = true;
    },

    // ── Hint tooltips ──────────────────────────────────────────────
    setupHints() {
        const tooltip = document.getElementById('hint-tooltip');
        if (!tooltip) return;
        let hintTimer = null;

        document.addEventListener('mouseover', (e) => {
            const el = e.target.closest('[data-hint]');
            if (!el) return;
            clearTimeout(hintTimer);
            hintTimer = setTimeout(() => {
                const hint = el.getAttribute('data-hint');
                if (!hint) return;
                tooltip.textContent = hint;
                tooltip.style.display = 'block';
                const r = el.getBoundingClientRect();
                let left = r.left;
                let top = r.bottom + 6;
                // Keep within viewport
                tooltip.style.left = '0';
                tooltip.style.top = '0';
                tooltip.style.display = 'block';
                const tw = tooltip.offsetWidth;
                if (left + tw > window.innerWidth - 8) left = window.innerWidth - tw - 8;
                if (left < 4) left = 4;
                tooltip.style.left = left + 'px';
                tooltip.style.top = top + 'px';
            }, 500);
        });

        document.addEventListener('mouseout', (e) => {
            const el = e.target.closest('[data-hint]');
            if (!el) return;
            clearTimeout(hintTimer);
            tooltip.style.display = 'none';
        });

        document.addEventListener('click', () => {
            clearTimeout(hintTimer);
            tooltip.style.display = 'none';
        });
    },

    // Keyboard shortcuts
    handleKey(e) {
        if (e.ctrlKey && e.key === 's') { e.preventDefault(); this.saveFile(); }
        if (e.ctrlKey && e.key === 'f') { e.preventDefault(); document.getElementById('search-box')?.focus(); }
        if (e.ctrlKey && e.key === 'r') { e.preventDefault(); this.showRecipeManager(); }
        if (e.altKey && e.key === 'ArrowLeft')  { e.preventDefault(); this.navBack(); }
        if (e.altKey && e.key === 'ArrowRight') { e.preventDefault(); this.navForward(); }
        if (e.key === 'F1') { e.preventDefault(); this.showWizard(); }
    },

    toggleVoiceInput(textareaId, buttonId) {
        if (!('webkitSpeechRecognition' in window) && !('SpeechRecognition' in window)) {
            this.addLog('❌ 音声入力（SpeechRecognition）はこのブラウザ/環境でサポートされていません。');
            alert('音声入力はお使いの環境でサポートされていません。');
            return;
        }

        const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
        
        if (!this.voiceRecognitions_) {
            this.voiceRecognitions_ = {};
        }

        const textarea = document.getElementById(textareaId);
        const button = document.getElementById(buttonId);
        if (!textarea || !button) return;

        let rec = this.voiceRecognitions_[textareaId];

        if (rec) {
            rec.stop();
            return;
        }

        rec = new SpeechRecognition();
        rec.continuous = true;
        rec.interimResults = false;
        rec.lang = this.state.language === 'ja' ? 'ja-JP' : 'en-US';

        rec.onstart = () => {
            button.classList.add('recording');
            button.title = '音声入力停止';
            this.addLog('🎙️ 音声入力開始... お話しください。');
        };

        rec.onresult = (event) => {
            let resultText = '';
            for (let i = event.resultIndex; i < event.results.length; ++i) {
                if (event.results[i].isFinal) {
                    resultText += event.results[i][0].transcript;
                }
            }
            if (resultText) {
                const start = textarea.selectionStart;
                const end = textarea.selectionEnd;
                const val = textarea.value;
                textarea.value = val.substring(0, start) + resultText + val.substring(end);
                
                const newCursorPos = start + resultText.length;
                textarea.setSelectionRange(newCursorPos, newCursorPos);
                textarea.focus();

                textarea.dispatchEvent(new Event('input', { bubbles: true }));
                
                if (textareaId === 'node-content') {
                    this.updateNode();
                } else if (textareaId === 'sw-content') {
                    this.sw_.content = textarea.value;
                }
            }
        };

        rec.onerror = (event) => {
            this.addLog('❌ 音声認識エラー: ' + event.error);
            console.error('Speech recognition error:', event.error);
            cleanup();
        };

        rec.onend = () => {
            this.addLog('🎙️ 音声入力終了。');
            cleanup();
        };

        const cleanup = () => {
            button.classList.remove('recording');
            button.title = '音声入力';
            if (this.voiceRecognitions_[textareaId] === rec) {
                delete this.voiceRecognitions_[textareaId];
            }
        };

        this.voiceRecognitions_[textareaId] = rec;
        rec.start();
    },

    toggleSpeak(textareaId, buttonId) {
        if (!('speechSynthesis' in window)) {
            this.addLog('❌ 音声読み上げ（SpeechSynthesis）はこのブラウザ/環境でサポートされていません。');
            alert('音声読み上げはお使いの環境でサポートされていません。');
            return;
        }

        const textarea = document.getElementById(textareaId);
        const button = document.getElementById(buttonId);
        if (!textarea || !button) return;

        if (window.speechSynthesis.speaking) {
            window.speechSynthesis.cancel();
            this.clearAllSpeakingStyles();
            this.addLog('🔊 読み上げを停止しました。');
            return;
        }

        const text = textarea.value.trim();
        if (!text) {
            this.addLog('⚠ 読み上げるテキストがありません。');
            return;
        }

        const utterance = new SpeechSynthesisUtterance(text);
        utterance.lang = this.state.language === 'ja' ? 'ja-JP' : 'en-US';

        utterance.onstart = () => {
            button.classList.add('speaking');
            button.title = '読み上げ停止';
            this.addLog('🔊 テキストの読み上げを開始します...');
        };

        utterance.onend = () => {
            button.classList.remove('speaking');
            button.title = '音声読み上げ';
            this.addLog('🔊 読み上げが完了しました。');
        };

        utterance.onerror = (event) => {
            this.addLog('❌ 読み上げエラー: ' + event.error);
            console.error('Speech synthesis error:', event.error);
            button.classList.remove('speaking');
            button.title = '音声読み上げ';
        };

        window.speechSynthesis.speak(utterance);
    },

    clearAllSpeakingStyles() {
        document.querySelectorAll('.speak-btn').forEach(btn => {
            btn.classList.remove('speaking');
            btn.title = '音声読み上げ';
        });
    }
};

document.addEventListener('DOMContentLoaded', () => {
    app.init();
    document.addEventListener('keydown', (e) => app.handleKey(e));
    
    // Global listener to close context menus when clicking outside
    document.addEventListener('mousedown', (e) => {
        const treeMenu = document.getElementById('tree-context-menu');
        if (treeMenu && treeMenu.style.display !== 'none' && !treeMenu.contains(e.target)) {
            app.hideTreeContextMenu();
        }
        const logMenu = document.getElementById('log-context-menu');
        if (logMenu && logMenu.style.display !== 'none' && !logMenu.contains(e.target)) {
            app.hideLogContextMenu();
        }
    });
});
