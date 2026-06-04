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

    // Keyboard shortcuts
    handleKey(e) {
        if (e.ctrlKey && e.key === 's') { e.preventDefault(); this.saveFile(); }
        if (e.ctrlKey && e.key === 'f') { e.preventDefault(); document.getElementById('search-box')?.focus(); }
        if (e.altKey && e.key === 'ArrowLeft')  { e.preventDefault(); this.navBack(); }
        if (e.altKey && e.key === 'ArrowRight') { e.preventDefault(); this.navForward(); }
    }
};

document.addEventListener('DOMContentLoaded', () => {
    app.init();
    document.addEventListener('keydown', (e) => app.handleKey(e));
});
