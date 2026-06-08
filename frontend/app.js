// Prompts App - Vanilla JS frontend

const app = {
    state: {
        tabs: [],
        activeTab: 0,
        currentNode: null,
        currentNodePath: [],
        language: 'en',
        pipelineRunning: false,
        testMode: false,
        translations: {},
        searchTimeout: null,
        navHistory: [],   // [{tabIndex, path}]
        navFuture: [],    // [{tabIndex, path}]
        viewMode: 'node', // "node" | "pipeline"
        selectedStep: -1, // selected pipeline step index
        currentRunId: '',
        projects: [],
        activeProject: 'default',
        incompleteRuns: [],
        chestList: [],
        historyRetention: 50
    },

    init() {
        this.setupBridge();
        this.loadLanguage(this.state.language);
        this.setupHints();
    },

    setupBridge() {
        window.chrome.webview.addEventListener('message', (e) => {
            const msg = typeof e.data === 'string' ? JSON.parse(e.data) : e.data;
            this.handleBridge(msg);
        });
    },

    handleBridge(msg) {
        switch (msg.type) {
            case 'init':
                this.state.language = msg.payload.language || 'en';
                this.state.embedded = msg.payload.embedded || false;
                this.loadLanguage(this.state.language);
                if (msg.payload.tabs && msg.payload.tabs.length > 0) {
                    this.state.tabs = msg.payload.tabs.map(t => ({
                        name: t.name,
                        file: t.file,
                        root: (msg.payload.nodes && msg.payload.nodes[t.file])
                              || { title:'', content:'', mimetype:'text/plain', attachments:[], children:[] }
                    }));
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
            case 'pipeline_completed':
                this.onPipelineCompleted(msg.payload);
                break;
            case 'manual_step_pause':
                this.showManualStep(msg.payload);
                break;
            case 'wizard_step_pause':
                this.showPipelineWizardStep(msg.payload);
                break;
            case 'providers_result':
                this.state.providers = msg.payload || {};
                this.onProvidersResult(this.state.providers);
                break;
            case 'menu_command':
                this.handleMenuCommand(msg.payload);
                break;
            case 'open_file_result':
                this.onFileSelected(msg.payload.path);
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
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(obj);
        }
    },

    sendInitData() {
        window.chrome.webview.postMessage({
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
            el.onclick = () => this.switchTab(i);
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
        this.renderTabs();
        this.renderTree();
        this.addLog('Tab closed');
    },

    newTab() {
        this.state.tabs.push({ name: 'New', root: { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [] } });
        this.state.activeTab = this.state.tabs.length - 1;
        this.renderTabs();
        this.renderTree();
        this.addLog('📄 New tab created');
    },

    openFile() {
        window.chrome.webview.postMessage({ type: 'open_file_dialog', filter: 'JSON|*.json' });
    },

    onFileSelected(path) {
        if (path) {
            this.state.tabs.push({ name: path.split('/').pop().split('\\').pop(), file: path, root: { title: '', content: '', mimetype: 'text/plain', attachments: [], children: [] } });
            this.state.activeTab = this.state.tabs.length - 1;
            this.renderTabs();
            this.addLog('📂 Opened: ' + path);
        }
    },

    saveFile() { this.addLog('💾 Save requested'); },
    saveFileAs() { this.addLog('💾 Save As requested'); },
    onPipelineCompleted(meta) {
        this.state.pipelineRunning = false;
        this.addLog(`✅ Pipeline "${meta.pipelineName}" completed`);

        const safeB64 = str => {
            try { return btoa(unescape(encodeURIComponent(str))); }
            catch { return btoa(str); }
        };

        const outputNode = {
            title: safeB64(meta.pipelineName + ' — ' + (meta.executedAt || '').replace('T',' ').replace('Z','')),
            content: safeB64(meta.outputContent || ''),
            mimetype: 'text/plain',
            attachments: [],
            children: [],
            pipelineMeta: JSON.stringify(meta)
        };

        const tab = this.state.tabs[this.state.activeTab];
        const currentNode = this.getNodeByPath(this.state.currentNodePath);
        if (tab && currentNode) {
            if (!currentNode.children) currentNode.children = [];
            currentNode.children.push(outputNode);
            this.renderTree();
            this.renderList();
            if (tab.file && tab.root) {
                this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
            }
            this.addLog(`📦 Child node saved: "${meta.pipelineName}"`);
        }
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
        this.state.pipelineRunning = true;
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
            try { wizardData = JSON.parse(wizardData); } catch {}
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
            } catch {}
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

    showConfig() {
        const panel = document.getElementById('config-panel');
        if (!panel) return;
        panel.classList.add('visible');
        this.onProvidersResult(this.state.providers || {});
        this.initConfigDrag();
        this.addLog('⚙ Config opened');
    },

    closeConfig() {
        const panel = document.getElementById('config-panel');
        if (!panel) return;
        // Auto-save on close
        this.saveProviders();
        panel.classList.remove('visible');
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

    onProvidersResult(providers) {
        const DEFAULT_PROVIDERS = [
            { id: 'openai',    label: 'OpenAI',    defaultUrl: 'https://api.openai.com/v1' },
            { id: 'anthropic', label: 'Anthropic',  defaultUrl: 'https://api.anthropic.com' },
            { id: 'gemini',    label: 'Gemini',     defaultUrl: 'https://googleapis.com' },
            { id: 'ollama',    label: 'Ollama',     defaultUrl: 'http://localhost:11434' },
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
        list.innerHTML = allIds.map(id => {
            const def = DEFAULT_PROVIDERS.find(p => p.id === id);
            const cfg = (providers && providers[id]) || {};
            const label = def ? def.label : id.charAt(0).toUpperCase() + id.slice(1);
            const defaultUrl = def ? def.defaultUrl : 'https://api.openai.com/v1';
            const isCustom = !knownIds.includes(id);
            return `<div class="provider-item${isCustom ? ' provider-custom' : ''}">
                <div class="provider-name">${label}${isCustom ? ' <span class="provider-custom-badge">custom</span>' : ''}</div>
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
            <input type="text" id="new-custom-provider-id" placeholder="provider id (e.g. grok)" style="flex:1">
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
            const existing = (this.state.providers && this.state.providers[id]) || {};
            providers[id] = {
                apiKey:  keyInput.value || '',
                baseUrl: urlInput.value || '',
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
        const statusEl = document.getElementById('test-status-' + id);
        if (statusEl) {
            statusEl.textContent = '⏳ Testing...';
            statusEl.className = 'test-status';
        }
        this.addLog('🔌 Testing ' + id + ' connection...');
        this.postMessage({ type: 'test_provider_connection', payload: { provider: id, apiKey, baseUrl } });
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
            case 'toggle_pane':     this.togglePane(cmd.pane + '-pane'); break;
            case 'about':           this.showAbout(); break;
            case 'welcome_wizard':  this.showWizard(); break;
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

    // Tree rendering
    renderTree() {
        const el = document.getElementById('tree-content');
        if (!el) return;
        const tab = this.state.tabs[this.state.activeTab];
        if (!tab || !tab.root) { el.innerHTML = '<div class="empty">No data</div>'; return; }
        el.innerHTML = this.buildTreeHTML(tab.root, '');
    },

    buildTreeHTML(node, path) {
        let html = '';
        const display = this.escapeHtml(node.title ? atob(node.title) : this.getTitleFallback(node));
        const safePath = this.escapeHtml(path);
        const hasChildren = node.children && node.children.length > 0;
        const cls = 'tree-node' + (hasChildren ? ' branch' : ' leaf') +
                    (this.state.currentNodePath === path ? ' selected' : '');
        html += `<div class="${cls}" onclick="app.selectNode('${safePath}')">${display}</div>`;
        if (hasChildren) {
            html += '<div style="padding-left:16px">';
            node.children.forEach((child, i) => {
                html += this.buildTreeHTML(child, path + '/' + i);
            });
            html += '</div>';
        }
        return html;
    },

    getTitleFallback(node) {
        if (node.title) return atob(node.title);
        if (node.mimetype === 'text/plain' && node.content) {
            const text = atob(node.content);
            const words = text.split(/\s+/).slice(0, 4).join(' ');
            return words + (words.length < text.length ? '...' : '');
        }
        if (node.mimetype === 'application/rtf') return '[RTF ' + (node.content ? Math.round(atob(node.content).length / 1024) + 'KB' : '0B') + ']';
        if (node.mimetype.startsWith('image/')) return '[Image ' + (node.content ? Math.round(atob(node.content).length / 1024) + 'KB' : '0B') + ']';
        if (node.mimetype === 'text/html') return '[HTML ' + (node.content ? atob(node.content).length + ' chars' : '') + ']';
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
        if ('speechSynthesis' in window) {
            window.speechSynthesis.cancel();
            this.clearAllSpeakingStyles();
        }
        this.pushNav();
        this.state.currentNodePath = path;
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
        if (!el) return;
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.children) { el.innerHTML = '<div class="empty">Select a node</div>'; return; }
        const tab = this.state.tabs[this.state.activeTab];
        const tabFile = tab ? tab.file : '';
        el.innerHTML = node.children.map((child, i) => {
            const display = this.escapeHtml(child.title ? atob(child.title) : this.getTitleFallback(child));
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

    // Editor
    loadEditor(path) {
        const node = this.getNodeByPath(path);
        const titleEl = document.getElementById('node-title');
        const contentEl = document.getElementById('node-content');
        if (!node || !titleEl || !contentEl) return;
        titleEl.value = node.title ? atob(node.title) : '';
        if (node.mimetype === 'text/plain') {
            contentEl.value = node.content ? atob(node.content) : '';
        } else if (node.mimetype.startsWith('image/')) {
            contentEl.value = '[Image: ' + (node.content ? Math.round(atob(node.content).length / 1024) + 'KB' : 'empty') + ']';
        } else {
            contentEl.value = node.content ? atob(node.content) : '';
        }
        this.renderAttachments(node);
        this.renderPipelineMeta(node);
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
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;
        const title = document.getElementById('node-title');
        const content = document.getElementById('node-content');
        const safeB64 = str => { try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str); } };
        if (title) node.title = safeB64(title.value);
        if (content && node.mimetype === 'text/plain') node.content = safeB64(content.value);
        this.renderTree();
        this.renderList();
        const tab = this.state.tabs[this.state.activeTab];
        if (tab && tab.file && tab.root) {
            this.postMessage({ type: 'save_node', payload: { tabFile: tab.file, root: tab.root } });
        }
        this.addLog('💾 Node updated');
    },

    addChild() {
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) return;
        if (!node.children) node.children = [];
        node.children.push({ title: '', content: '', mimetype: 'text/plain', attachments: [], children: [] });
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
        if (this.state.pipelineRunning) { this.addLog('⚠ Pipeline already running'); return; }
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) { this.addLog('⚠ ノードを選択してください'); return; }
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
        this.state.pipelineRunning = true;
        this.addLog(`▶ Pipeline "${pipelineName}" started`);
    },

    cancelPipeline() {
        window.chrome.webview.postMessage({ type: 'cancel_pipeline' });
        this.state.pipelineRunning = false;
        this.addLog('✕ Pipeline canceled');
    },

    toggleTestMode() {
        this.state.testMode = !this.state.testMode;
        document.getElementById('btn-test-mode').classList.toggle('active');
        this.addLog(this.state.testMode ? '🧪 Test mode ON' : '🧪 Test mode OFF');
    },

    // Messages
    addLog(text) {
        const el = document.getElementById('messages-content');
        if (!el) return;
        const div = document.createElement('div');
        div.className = 'log-entry';
        div.textContent = '[' + new Date().toLocaleTimeString() + '] ' + text;
        el.appendChild(div);
        el.scrollTop = el.scrollHeight;
    },

    showError(msg) {
        this.addLog('❌ ' + msg);
        this.state.pipelineRunning = false;
    },

    // Log context menu and copy
    showLogContextMenu(event) {
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
        setTimeout(() => document.addEventListener('click', app.hideLogContextMenu, { once: true }), 0);
    },

    hideLogContextMenu() {
        const menu = document.getElementById('log-context-menu');
        if (menu) menu.style.display = 'none';
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
            window.chrome.webview.postMessage({ type: 'search', query, scope: 'all_tabs' });
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
    },

    onStepDone(payload) {
        this.addLog(`✅ Step ${payload.index} done` + (payload.tokens ? ` (${payload.tokens} tokens)` : ''));
        if (payload.status === 'completed') this.state.pipelineRunning = false;
    },

    highlightStep(payload) {
        this.addLog(`▶ Step ${payload.index}: ${payload.name || ''}`);
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
        const steps = this.state.pipelineSteps || [];
        if (steps.length === 0) {
            el.innerHTML = '<div class="empty">No pipeline steps</div>';
            return;
        }
        el.innerHTML = steps.map((s, i) => `
            <div class="tree-node ${s.completed ? 'completed' : ''} ${this.state.selectedStep === i ? 'selected' : ''}"
                 onclick="app.selectPipelineStep(${i})">
                ${s.completed ? '✔' : '○'} ${this.escapeHtml(s.name || s.type)}
            </div>
        `).join('');
    },

    selectPipelineStep(index) {
        this.state.viewMode = 'pipeline';
        this.state.selectedStep = index;
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

    // ── Wizard ────────────────────────────────────────────────────
    WIZARD_STEPS: [
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
    ],

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
                item(t('MenuSelectAll'),   'select_all',    'Ctrl+A')
            )}
            ${sep}
            ${section(t('MenuSettings'),
                item('⚙ ' + t('MenuSettingsItem'),  'settings') +
                item('🔑 ' + t('Config'),             'config')
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
            settings:       () => this.showSettings(),
            config:         () => this.showConfig(),
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

        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node) {
            inputEl.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const content = node.content ? (() => { try { return atob(node.content); } catch { return node.content; } })() : '';
        inputEl.innerHTML = `
            <textarea id="input-textarea" class="input-textarea" placeholder="${t('NoInput')}">${this.escapeHtml(content)}</textarea>
            <div class="input-source-bar">
                <span class="input-source-label">${t('Source')}</span>
                <span class="input-source-value">${t('PreviousStep')}</span>
                <button class="input-source-btn" onclick="app.showInputSourceDialog()">📂 ${t('Change')}</button>
            </div>`;
    },

    renderPipelineInput(el) {
        const si = this.state.selectedStep;
        const t = key => this.t(key);
        if (si < 0 || !this.state.pipelineSteps || si >= this.state.pipelineSteps.length) {
            el.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const step = this.state.pipelineSteps[si];
        const inputText = step.input || '(no input yet)';
        el.innerHTML = `
            <div class="input-header">
                <span class="input-step-badge">${t('Step')} ${si + 1}</span>
                <span class="input-step-name">${this.escapeHtml(step.name)}</span>
            </div>
            <div class="input-source-bar">
                <span class="input-source-value">${this.escapeHtml(step.source || t('PreviousStep'))}</span>
                <button class="input-source-btn" onclick="app.showInputSourceDialog()">📂 ${t('Change')}</button>
            </div>
            <pre class="input-display">${this.escapeHtml(inputText)}</pre>`;
    },

    renderPrompt() {
        const promptEl = document.getElementById('prompt-content');
        if (!promptEl) return;
        const t = key => this.t(key);

        if (this.state.viewMode === 'pipeline') {
            this.renderPipelinePrompt(promptEl);
            return;
        }

        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.pipelineMeta) {
            promptEl.innerHTML = `<div class="empty">${t('NoPrompt')}</div>`;
            return;
        }
        let meta;
        try { meta = JSON.parse(node.pipelineMeta); } catch {
            promptEl.innerHTML = `<div class="empty">${t('NoPrompt')}</div>`;
            return;
        }
        promptEl.innerHTML = this.buildPromptHtml(meta);
    },

    renderPipelinePrompt(el) {
        const si = this.state.selectedStep;
        const t = key => this.t(key);
        if (si < 0 || !this.state.pipelineSteps || si >= this.state.pipelineSteps.length) {
            el.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const step = this.state.pipelineSteps[si];
        const typeInfo = this.PM_STEP_TYPES[step.type] || { icon: '❓', label: step.type, fields: [] };
        let html = `<div class="prompt-header">${typeInfo.icon} ${this.escapeHtml(typeInfo.label)}</div>`;

        if (step.params) {
            for (const [key, value] of Object.entries(step.params)) {
                const displayVal = value.length > 200 ? value.substring(0, 200) + '...' : value;
                html += `<div class="param-row">
                    <span class="param-key">${this.escapeHtml(key)}</span>
                    <span class="param-value">${this.escapeHtml(displayVal)}</span>
                </div>`;
            }
        }
        el.innerHTML = html;
    },

    renderOutput() {
        const outputEl = document.getElementById('output-content');
        if (!outputEl) return;
        const t = key => this.t(key);

        if (this.state.viewMode === 'pipeline') {
            this.renderPipelineOutput(outputEl);
            return;
        }

        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.children || node.children.length === 0) {
            outputEl.innerHTML = `<div class="empty">${t('NoOutput')}</div>`;
            return;
        }
        const child = node.children[0];
        const outputContent = child.content ? (() => { try { return atob(child.content); } catch { return child.content; } })() : '';
        let html = `<div class="output-toolbar">
            <span class="output-label">${t('Output')} (${node.children.length})</span>
            <button class="output-save-btn" onclick="app.saveCurrentOutput()">${t('Save')}</button>
            <button class="output-discard-btn" onclick="app.discardCurrentOutput()">${t('Discard')}</button>
            <button class="output-chest-btn" onclick="app.sendToChestDialog()">${t('SendToChest')}</button>
        </div>`;

        // Show score if available
        if (child.evaluation) {
            html += `<div class="eval-badge">★ ${this.escapeHtml(child.evaluation)}</div>`;
        }

        html += `<pre class="output-display">${this.escapeHtml(outputContent)}</pre>`;
        outputEl.innerHTML = html;
    },

    renderPipelineOutput(el) {
        const si = this.state.selectedStep;
        const t = key => this.t(key);
        if (si < 0 || !this.state.pipelineSteps || si >= this.state.pipelineSteps.length) {
            el.innerHTML = `<div class="empty">${t('EmptyNode')}</div>`;
            return;
        }
        const step = this.state.pipelineSteps[si];
        if (!step.completed) {
            el.innerHTML = `<div class="empty">${t('NoOutput')}</div>`;
            return;
        }
        const outputText = step.output || '(empty output)';
        el.innerHTML = `
            <div class="output-toolbar">
                <span class="output-label">${t('Step')} ${si + 1} ${t('Output')}</span>
                <button class="output-save-btn" onclick="app.savePipelineOutput(${si})">${t('Save')}</button>
                <button class="output-chest-btn" onclick="app.sendToChestDialog()">${t('SendToChest')}</button>
            </div>
            <pre class="output-display">${this.escapeHtml(outputText)}</pre>`;
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
                this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.selectedStep, source: 'manual', content: input.value } });
            }
        } else if (source === 'checkpoint') {
            this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.selectedStep, source: 'checkpoint' } });
        } else {
            this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.selectedStep, source } });
        }
        document.getElementById('input-source-modal')?.classList.remove('visible');
    },

    confirmChestSource() {
        const name = document.getElementById('chest-name-input')?.value;
        if (!name) return;
        this.postMessage({ type: 'select_input_source', payload: { stepIndex: this.state.selectedStep, source: 'chest', chestName: name } });
        document.getElementById('input-source-modal')?.classList.remove('visible');
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
            children: []
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
        ai:         { icon: '🤖', label: 'AI Call', fields: ['provider','model','systemPrompt','userPrompt','temperature','maxTokens','attachMedia'] },
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
        step.name = document.getElementById('pms-name')?.value || step.name;
        step.type = document.getElementById('pms-type')?.value || step.type;
        const typeInfo = this.PM_STEP_TYPES[step.type] || { fields: [] };
        if (!step.params) step.params = {};
        typeInfo.fields.forEach(f => {
            const el = document.getElementById('pms-' + f);
            if (el) step.params[f] = el.value;
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
        this.state.pipelineRunning = true;
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
});
