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
        navFuture: []     // [{tabIndex, path}]
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
                if (this.state.embedded) {
                    const hb = document.getElementById('btn-hamburger');
                    if (hb) hb.style.display = '';
                }
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
            case 'providers_result':
                this.state.providers = msg.payload || {};
                this.onProvidersResult(this.state.providers);
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
                <button class="meta-reproduce-btn" onclick="app.reproducePipeline(${this.escapeHtml(JSON.stringify(meta.pipelineName))})">▶ 再実行</button>
            </div>
            <div class="meta-steps">${stepsHtml}</div>`;
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

    escapeHtml(s) {
        return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    },

    showConfig() {
        const modal = document.getElementById('config-modal');
        modal.classList.add('visible');
        this.onProvidersResult(this.state.providers || {});
        this.postMessage({ type: 'get_providers' });
    },

    closeConfig() {
        document.getElementById('config-modal').classList.remove('visible');
    },

    switchConfigTab(name, btn) {
        document.querySelectorAll('.config-tab-panel').forEach(p => p.style.display = 'none');
        document.querySelectorAll('.config-tab').forEach(b => b.classList.remove('active'));
        document.getElementById('config-tab-' + name).style.display = '';
        btn.classList.add('active');
    },

    onProvidersResult(providers) {
        const PROVIDERS = [
            { id: 'openai',    label: 'OpenAI',    defaultUrl: 'https://api.openai.com/v1' },
            { id: 'anthropic', label: 'Anthropic',  defaultUrl: 'https://api.anthropic.com' },
            { id: 'gemini',    label: 'Gemini',     defaultUrl: 'https://generativelanguage.googleapis.com' },
            { id: 'ollama',    label: 'Ollama',     defaultUrl: 'http://localhost:11434' },
        ];
        const list = document.getElementById('provider-list');
        if (!list) return;
        list.innerHTML = PROVIDERS.map(p => {
            const cfg = providers[p.id] || {};
            return `<div class="provider-item">
                <div class="provider-name">${p.label}</div>
                <label>API Key</label>
                <div class="api-key-row">
                    <input type="password" id="key-${p.id}" value="${this.escapeHtml(cfg.apiKey||'')}" placeholder="sk-...">
                    <button type="button" onclick="app.toggleKeyVisible('key-${p.id}',this)">👁</button>
                </div>
                <label>Base URL</label>
                <input type="text" id="url-${p.id}" value="${this.escapeHtml(cfg.baseUrl||p.defaultUrl)}" placeholder="${this.escapeHtml(p.defaultUrl)}">
            </div>`;
        }).join('');
    },

    toggleKeyVisible(id, btn) {
        const inp = document.getElementById(id);
        if (inp.type === 'password') { inp.type = 'text'; btn.textContent = '🙈'; }
        else { inp.type = 'password'; btn.textContent = '👁'; }
    },

    saveProviders() {
        const ids = ['openai','anthropic','gemini','ollama'];
        const providers = {};
        ids.forEach(id => {
            providers[id] = {
                apiKey:  document.getElementById('key-' + id)?.value || '',
                baseUrl: document.getElementById('url-' + id)?.value || '',
            };
        });
        this.postMessage({ type: 'save_providers', payload: providers });
        this.closeConfig();
        this.addLog('✅ Provider settings saved.');
    },

    testConnection() {
        this.addLog('🔌 Test connection not yet implemented.');
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
        this.pushNav();
        this.state.currentNodePath = path;
        this.renderTree();
        this.renderList();
        this.loadEditor(path);
    },

    // List rendering
    renderList() {
        const el = document.getElementById('list-content');
        if (!el) return;
        const node = this.getNodeByPath(this.state.currentNodePath);
        if (!node || !node.children) { el.innerHTML = '<div class="empty">Select a node</div>'; return; }
        el.innerHTML = node.children.map((child, i) => {
            const display = this.escapeHtml(child.title ? atob(child.title) : this.getTitleFallback(child));
            return `<div class="list-item" ondblclick="app.copyItemText(${i})">
                <span>${display}</span>
                <button class="copy-btn" onclick="app.copyItemText(${i})">📋</button>
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

    // ── Wizard ────────────────────────────────────────────────────
    WIZARD_STEPS: [
        {
            icon: '🤖',
            title: 'Prompts へようこそ',
            body: `<p><b>Prompts</b> は AI プロンプトとパイプラインを管理・実行するツールです。</p>
                   <div class="wizard-concept">
                     <div class="wizard-concept-item">📄 <b>素材</b><br><small>あなたのテキスト・画像</small></div>
                     <div class="wizard-concept-arrow">×</div>
                     <div class="wizard-concept-item">🔧 <b>レシピ</b><br><small>AIへの指示の連鎖</small></div>
                     <div class="wizard-concept-arrow">=</div>
                     <div class="wizard-concept-item">✨ <b>成果物</b><br><small>AI処理結果</small></div>
                   </div>
                   <p>成果物には<b>「どのAIで・どんな指示で作ったか」</b>が埋め込まれ、後から再現できます。</p>`
        },
        {
            icon: '⚙',
            title: 'APIキーを設定する',
            body: `<p>AIパイプラインを実行するには、使用するプロバイダのAPIキーが必要です。</p>
                   <ul class="wizard-list">
                     <li>🟢 <b>OpenAI</b> — GPT-4.1, o1 など</li>
                     <li>🟣 <b>Anthropic</b> — Claude シリーズ</li>
                     <li>🔵 <b>Gemini</b> — Google AI</li>
                     <li>⚫ <b>Ollama</b> — ローカルLLM（無料）</li>
                   </ul>
                   <p>右上の <b>⚙ Config</b> ボタンから設定できます。</p>
                   <button class="wizard-action-btn" onclick="app.closeWizard(); app.showConfig();">⚙ 今すぐ設定する</button>`
        },
        {
            icon: '📄',
            title: '素材ノードを作る',
            body: `<p>ツリーにノードを追加して、処理したいテキストや画像を入れます。</p>
                   <ul class="wizard-list">
                     <li>📄 <b>New</b> — 新しいタブを作成</li>
                     <li>右クリック → <b>子ノードを追加</b></li>
                     <li>タイトルと内容を入力 → <b>💾 更新</b></li>
                   </ul>
                   <p>ノードはツリー構造で管理されます。素材の下に成果物が子ノードとして蓄積されます。</p>`
        },
        {
            icon: '▶',
            title: 'パイプラインを実行する',
            body: `<p>ノードを選択して <b>▶ Run</b> ボタンを押すと、パイプラインが実行されます。</p>
                   <ul class="wizard-list">
                     <li>複数のAIを順番に適用できます</li>
                     <li><b>人間の確認ステップ</b>を挟めます</li>
                     <li>複数AIの結果を<b>比較して選択</b>できます</li>
                     <li>結果は子ノードとして自動保存されます</li>
                   </ul>
                   <p>パイプラインは <b>⚡ レシピ</b>（近日実装）から作成・管理できます。</p>
                   <div class="wizard-shortcut-box">
                     <span>Alt+← / Alt+→</span> ノード履歴を移動<br>
                     <span>Ctrl+F</span> 全文検索<br>
                     <span>F5</span> パイプライン実行<br>
                     <span>F1</span> このガイドを表示
                   </div>`
        }
    ],

    wizardStep_: 0,

    showWizard(forceStep) {
        this.wizardStep_ = forceStep || 0;
        document.getElementById('wizard-modal').classList.add('visible');
        this.renderWizardStep();
    },

    closeWizard() {
        document.getElementById('wizard-modal').classList.remove('visible');
        localStorage.setItem('prompts_wizard_done', '1');
    },

    renderWizardStep() {
        const steps = this.WIZARD_STEPS;
        const s = steps[this.wizardStep_];
        const total = steps.length;
        const cur = this.wizardStep_;

        // Progress dots
        document.getElementById('wizard-progress').innerHTML =
            steps.map((_, i) => `<span class="wizard-dot${i === cur ? ' active' : ''}"></span>`).join('');

        // Body
        document.getElementById('wizard-body').innerHTML = `
            <div class="wizard-icon">${s.icon}</div>
            <h2 class="wizard-title">${this.escapeHtml(s.title)}</h2>
            <div class="wizard-text">${s.body}</div>`;

        // Footer buttons
        document.getElementById('wizard-prev').style.visibility = cur === 0 ? 'hidden' : '';
        const nextBtn = document.getElementById('wizard-next');
        if (cur === total - 1) {
            nextBtn.textContent = '✓ 完了';
            nextBtn.onclick = () => this.closeWizard();
        } else {
            nextBtn.textContent = '次へ →';
            nextBtn.onclick = () => this.wizardNext();
        }
        document.getElementById('wizard-skip').style.display = cur === total - 1 ? 'none' : '';
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
            id: 'free',
            label: '🆓 自由形式',
            desc: 'テンプレートなしで自由に開始',
            sample: '',
            pipeline: ''
        }
    ],

    sw_: { step: 0, tabName: '', templateId: 'free', content: '', pipelineName: '' },

    showSetupWizard() {
        this.sw_ = { step: 0, tabName: 'プロジェクト ' + new Date().toLocaleDateString('ja'), templateId: 'free', content: '', pipelineName: '' };
        document.getElementById('setup-wizard-modal').classList.add('visible');
        this.swRender();
    },

    closeSetupWizard() {
        document.getElementById('setup-wizard-modal').classList.remove('visible');
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
                const tmpl = this.SW_TEMPLATES.find(t => t.id === this.sw_.templateId) || this.SW_TEMPLATES[3];
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
                <textarea id="sw-content" class="sw-textarea" placeholder="テキストをここに入力...">${this.escapeHtml(s.content)}</textarea>
                <div class="sw-hint">💡 画像・PDFなどは後からノードにドロップして追加できます</div>`;
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
            body.innerHTML = `
                <div class="wizard-icon">✅</div>
                <h2 class="wizard-title">準備完了！</h2>
                <div class="sw-summary">
                    <div class="sw-summary-row"><span class="sw-summary-label">プロジェクト名</span><span>${this.escapeHtml(s.tabName)}</span></div>
                    <div class="sw-summary-row"><span class="sw-summary-label">テンプレート</span><span>${tmpl ? tmpl.label : '自由形式'}</span></div>
                    <div class="sw-summary-row"><span class="sw-summary-label">コンテンツ</span><span class="sw-summary-preview">${this.escapeHtml(preview)}</span></div>
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
        const tmpl = this.SW_TEMPLATES.find(t => t.id === id) || this.SW_TEMPLATES[3];
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

    swCreate() {
        const s = this.sw_;
        const safeB64 = str => { try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str || ''); } };

        // Build root node with content
        const rootNode = {
            title: safeB64(s.tabName),
            content: safeB64(s.content),
            mimetype: 'text/plain',
            attachments: [],
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
    }
};

document.addEventListener('DOMContentLoaded', () => {
    app.init();
    document.addEventListener('keydown', (e) => app.handleKey(e));
});
