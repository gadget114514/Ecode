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
        searchTimeout: null
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
                this.loadLanguage(this.state.language);
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
    showConfig() { this.showModal('config-modal'); },

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
        const display = node.title ? atob(node.title) : this.getTitleFallback(node);
        const hasChildren = node.children && node.children.length > 0;
        const cls = 'tree-node' + (hasChildren ? ' branch' : ' leaf') + 
                    (this.state.currentNodePath === path ? ' selected' : '');
        html += `<div class="${cls}" onclick="app.selectNode('${path}')">${display}</div>`;
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

    selectNode(path) {
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
            const display = child.title ? atob(child.title) : this.getTitleFallback(child);
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
        if (title) node.title = btoa(title.value);
        if (content && node.mimetype === 'text/plain') {
            node.content = btoa(content.value);
        }
        this.renderTree();
        this.renderList();
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
    runPipeline() {
        if (this.state.pipelineRunning) return;
        window.chrome.webview.postMessage({ type: 'run_pipeline', pipelineName: '', nodeId: '' });
        this.state.pipelineRunning = true;
        this.addLog('▶ Pipeline started');
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
    }
};

document.addEventListener('DOMContentLoaded', () => {
    app.init();
    document.addEventListener('keydown', (e) => app.handleKey(e));
});
