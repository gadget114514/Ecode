const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('__promptsBridge', {
    postMessage: (message) => {
        ipcRenderer.send('webview-message', message);
    },
    addEventListener: (_type, callback) => {
        ipcRenderer.on('webview-message', (_event, data) => {
            callback({ data });
        });
    },
    removeEventListener: (_type, _callback) => {
        // not used
    }
});
