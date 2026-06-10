const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('chrome', {
    webview: {
        postMessage: (message) => {
            ipcRenderer.send('webview-message', message);
        },
        addEventListener: (type, callback) => {
            if (type === 'message') {
                ipcRenderer.on('webview-message', (event, data) => {
                    // Mimic the WebView2 message event structure (e.data)
                    callback({ data });
                });
            }
        },
        removeEventListener: (type, callback) => {
            // In case cleanup is needed, though not heavily used in app.js
        }
    }
});
