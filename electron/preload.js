'use strict';
const { contextBridge, ipcRenderer } = require('electron');

// Emulate window.chrome.webview so the existing frontend works unchanged
const listeners = [];

ipcRenderer.on('bridge-message', (_event, msg) => {
    const data = typeof msg === 'string' ? msg : JSON.stringify(msg);
    for (const fn of listeners) fn({ data });
});

contextBridge.exposeInMainWorld('chrome', {
    webview: {
        postMessage(obj) {
            ipcRenderer.send('bridge', obj);
        },
        addEventListener(_evt, fn) {
            listeners.push(fn);
        },
        removeEventListener(_evt, fn) {
            const i = listeners.indexOf(fn);
            if (i !== -1) listeners.splice(i, 1);
        }
    }
});
