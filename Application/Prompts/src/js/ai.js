const { EventEmitter } = require('events');

let httpLogCallback = null;

function setHttpLogCallback(cb) {
    httpLogCallback = cb;
}

function maskAuthHeader(headerValue) {
    if (!headerValue) return '';
    const match = headerValue.match(/Bearer\s+(.+)/i);
    if (match) {
        const token = match[1];
        if (token.length > 12) {
            return 'Bearer ' + token.substring(0, 4) + '...' + token.substring(token.length - 4);
        }
    }
    // For Anthropic (x-api-key)
    if (headerValue.length > 12) {
        return headerValue.substring(0, 4) + '...' + headerValue.substring(headerValue.length - 4);
    }
    return '***';
}

function logHttp(method, url, headers, requestBody, statusCode, responseBody) {
    if (!httpLogCallback) return;

    let logEntry = `<details><summary><span class="log-tag-http">HTTP LOG</span> 🌐 <b>${method} ${url}</b> → ${statusCode}</summary>`;
    logEntry += `<pre style="font-size:10px;margin:4px 0;padding:4px;background:#1a1a1a;border-radius:3px;max-height:200px;overflow-y:auto;white-space:pre-wrap">`;
    logEntry += `──── Request ────\n`;
    logEntry += `${method} ${url}\n`;
    
    // Format headers (mask keys)
    for (const [key, val] of Object.entries(headers)) {
        let displayVal = val;
        if (key.toLowerCase() === 'authorization' || key.toLowerCase() === 'x-api-key') {
            displayVal = maskAuthHeader(val);
        }
        logEntry += `${key}: ${displayVal}\n`;
    }

    if (requestBody) {
        const bodyStr = typeof requestBody === 'string' ? requestBody : JSON.stringify(requestBody);
        logEntry += `\n${bodyStr.substring(0, 2000)}${bodyStr.length > 2000 ? '\n... (truncated)' : ''}\n`;
    }

    logEntry += `\n──── Response ────\n`;
    logEntry += `Status: ${statusCode}\n`;

    if (responseBody) {
        const respStr = typeof responseBody === 'string' ? responseBody : JSON.stringify(responseBody);
        logEntry += `${respStr.substring(0, 2000)}${respStr.length > 2000 ? '\n... (truncated)' : ''}\n`;
    }

    logEntry += `</pre></details>`;
    httpLogCallback(logEntry);
}

class AIProvider {
    constructor(apiKey, baseUrl) {
        this.apiKey = apiKey;
        this.baseUrl = baseUrl;
    }

    static create(providerType, apiKey, baseUrl) {
        if (providerType === 'openai') return new OpenAIProvider(apiKey, baseUrl);
        if (providerType === 'anthropic') return new AnthropicProvider(apiKey, baseUrl);
        if (providerType === 'gemini') return new GeminiProvider(apiKey, baseUrl);
        if (providerType === 'ollama') return new OllamaProvider(apiKey, baseUrl);
        return null;
    }
}

class OpenAIProvider extends AIProvider {
    constructor(apiKey, baseUrl) {
        super(apiKey, baseUrl || 'https://api.openai.com');
    }

    name() { return 'openai'; }
    listModels() { return ['gpt-4.1', 'gpt-4o-mini']; }

    async testConnection() {
        const url = `${this.baseUrl}/v1/models`;
        const headers = {
            'Authorization': `Bearer ${this.apiKey}`
        };
        try {
            const resp = await fetch(url, { method: 'GET', headers });
            const bodyText = await resp.text();
            logHttp('GET', url, headers, '', resp.status, bodyText);
            
            if (resp.status === 200) return '';
            const data = JSON.parse(bodyText);
            if (data.error && data.error.message) return data.error.message;
            return `Connection failed: ${resp.status} ${resp.statusText}`;
        } catch (e) {
            logHttp('GET', url, headers, '', 0, e.message);
            return `Connection failed: ${e.message}`;
        }
    }

    buildBody(req, stream = false) {
        const messages = [];
        if (req.systemPrompt) {
            messages.push({ role: 'system', content: req.systemPrompt });
        }
        messages.push({ role: 'user', content: req.userPrompt });

        if (req.attachments) {
            for (const att of req.attachments) {
                if (att.mimetype.startsWith('image/') && att.content) {
                    messages.push({
                        role: 'user',
                        content: [
                            { type: 'text', text: 'Attached image' },
                            { type: 'image_url', image_url: { url: `data:${att.mimetype};base64,${att.content}` } }
                        ]
                    });
                }
            }
        }

        return {
            model: req.model,
            messages,
            temperature: req.temperature,
            max_tokens: req.maxTokens || 4096,
            stream
        };
    }

    async call(req) {
        const url = `${this.baseUrl}/v1/chat/completions`;
        const headers = {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${this.apiKey}`
        };
        const bodyObj = this.buildBody(req, false);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });
            const text = await resp.text();
            logHttp('POST', url, headers, bodyObj, resp.status, text);

            if (!resp.ok) {
                return { model: req.model, content: `[OpenAI Error: ${resp.status} ${resp.statusText}]` };
            }

            const data = JSON.parse(text);
            const content = data.choices && data.choices[0] && data.choices[0].message ? data.choices[0].message.content : '[OpenAI: empty response]';
            return { model: req.model, content };
        } catch (e) {
            logHttp('POST', url, headers, bodyObj, 0, e.message);
            return { model: req.model, content: `[OpenAI Error: ${e.message}]` };
        }
    }

    async callStreaming(req, onChunk, onDone, onError) {
        const url = `${this.baseUrl}/v1/chat/completions`;
        const headers = {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${this.apiKey}`
        };
        const bodyObj = this.buildBody(req, true);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });

            if (!resp.ok) {
                const errText = await resp.text();
                logHttp('POST', url, headers, bodyObj, resp.status, errText);
                onError(`HTTP error! status: ${resp.status}`);
                return;
            }

            logHttp('POST', url, headers, bodyObj, resp.status, '[Streaming response started]');

            const reader = resp.body.getReader();
            const decoder = new TextDecoder('utf-8');
            let buffer = '';
            let accumulatedContent = '';

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;

                buffer += decoder.decode(value, { stream: true });
                const lines = buffer.split('\n');
                buffer = lines.pop(); // keep last incomplete line

                for (const line of lines) {
                    const cleanLine = line.trim();
                    if (!cleanLine.startsWith('data: ')) continue;
                    const dataStr = cleanLine.substring(6).trim();
                    if (dataStr === '[DONE]') continue;

                    try {
                        const parsed = JSON.parse(dataStr);
                        const delta = parsed.choices && parsed.choices[0] && parsed.choices[0].delta ? parsed.choices[0].delta.content : '';
                        if (delta) {
                            onChunk(delta);
                            accumulatedContent += delta;
                        }
                    } catch (e) {
                        // ignore malformed lines
                    }
                }
            }
            onDone({ model: req.model, content: accumulatedContent });
        } catch (e) {
            onError(e.message);
        }
    }
}

class AnthropicProvider extends AIProvider {
    constructor(apiKey, baseUrl) {
        super(apiKey, baseUrl || 'https://api.anthropic.com');
    }

    name() { return 'anthropic'; }
    listModels() { return ['claude-sonnet-4-6', 'claude-haiku-4-5']; }

    async testConnection() {
        const url = `${this.baseUrl}/v1/models`;
        const headers = {
            'x-api-key': this.apiKey,
            'anthropic-version': '2023-06-01'
        };
        try {
            const resp = await fetch(url, { method: 'GET', headers });
            const bodyText = await resp.text();
            logHttp('GET', url, headers, '', resp.status, bodyText);

            if (resp.status === 200) return '';
            const data = JSON.parse(bodyText);
            if (data.error && data.error.message) return data.error.message;
            return `Connection failed: ${resp.status} ${resp.statusText}`;
        } catch (e) {
            logHttp('GET', url, headers, '', 0, e.message);
            return `Connection failed: ${e.message}`;
        }
    }

    buildBody(req, stream = false) {
        const content = [{ type: 'text', text: req.userPrompt }];
        if (req.attachments) {
            for (const att of req.attachments) {
                if (att.mimetype.startsWith('image/') && att.content) {
                    content.push({
                        type: 'image',
                        source: {
                            type: 'base64',
                            media_type: att.mimetype,
                            data: att.content
                        }
                    });
                }
            }
        }

        const body = {
            model: req.model,
            max_tokens: req.maxTokens || 4096,
            messages: [{ role: 'user', content }],
            stream
        };

        if (req.systemPrompt) {
            body.system = req.systemPrompt;
        }

        return body;
    }

    async call(req) {
        const url = `${this.baseUrl}/v1/messages`;
        const headers = {
            'Content-Type': 'application/json',
            'x-api-key': this.apiKey,
            'anthropic-version': '2023-06-01'
        };
        const bodyObj = this.buildBody(req, false);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });
            const text = await resp.text();
            logHttp('POST', url, headers, bodyObj, resp.status, text);

            if (!resp.ok) {
                return { model: req.model, content: `[Anthropic Error: ${resp.status} ${resp.statusText}]` };
            }

            const data = JSON.parse(text);
            const content = data.content && data.content[0] ? data.content[0].text : '[Anthropic: empty response]';
            return { model: req.model, content };
        } catch (e) {
            logHttp('POST', url, headers, bodyObj, 0, e.message);
            return { model: req.model, content: `[Anthropic Error: ${e.message}]` };
        }
    }

    async callStreaming(req, onChunk, onDone, onError) {
        const url = `${this.baseUrl}/v1/messages`;
        const headers = {
            'Content-Type': 'application/json',
            'x-api-key': this.apiKey,
            'anthropic-version': '2023-06-01'
        };
        const bodyObj = this.buildBody(req, true);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });

            if (!resp.ok) {
                const errText = await resp.text();
                logHttp('POST', url, headers, bodyObj, resp.status, errText);
                onError(`HTTP error! status: ${resp.status}`);
                return;
            }

            logHttp('POST', url, headers, bodyObj, resp.status, '[Streaming response started]');

            const reader = resp.body.getReader();
            const decoder = new TextDecoder('utf-8');
            let buffer = '';
            let accumulatedContent = '';
            let captureContent = false;

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;

                buffer += decoder.decode(value, { stream: true });
                const lines = buffer.split('\n');
                buffer = lines.pop();

                for (const line of lines) {
                    const cleanLine = line.trim();
                    if (cleanLine.startsWith('event: content_block_delta')) {
                        captureContent = true;
                    } else if (cleanLine.startsWith('data: ')) {
                        if (captureContent) {
                            try {
                                const parsed = JSON.parse(cleanLine.substring(6).trim());
                                const text = parsed.delta && parsed.delta.text ? parsed.delta.text : '';
                                if (text) {
                                    onChunk(text);
                                    accumulatedContent += text;
                                }
                            } catch (e) {}
                            captureContent = false;
                        }
                    }
                }
            }
            onDone({ model: req.model, content: accumulatedContent });
        } catch (e) {
            onError(e.message);
        }
    }
}

class GeminiProvider extends AIProvider {
    constructor(apiKey, baseUrl) {
        super(apiKey, baseUrl || 'https://generativelanguage.googleapis.com');
    }

    name() { return 'gemini'; }
    listModels() { return ['gemini-2.5-flash', 'gemini-2.5-pro']; }

    async testConnection() {
        const url = `${this.baseUrl}/v1beta/models?key=${this.apiKey}`;
        try {
            const resp = await fetch(url, { method: 'GET' });
            const bodyText = await resp.text();
            logHttp('GET', url, {}, '', resp.status, bodyText);

            if (resp.status === 200) return '';
            const data = JSON.parse(bodyText);
            if (data.error && data.error.message) return data.error.message;
            return `Connection failed: ${resp.status} ${resp.statusText}`;
        } catch (e) {
            logHttp('GET', url, {}, '', 0, e.message);
            return `Connection failed: ${e.message}`;
        }
    }

    buildBody(req) {
        const parts = [{ text: req.userPrompt }];
        if (req.attachments) {
            for (const att of req.attachments) {
                if (att.mimetype.startsWith('image/') && att.content) {
                    parts.push({
                        inlineData: {
                            mimeType: att.mimetype,
                            data: att.content
                        }
                    });
                }
            }
        }

        const body = {
            contents: [{ parts }],
            generationConfig: {
                temperature: req.temperature,
                maxOutputTokens: req.maxTokens || 4096
            }
        };

        if (req.systemPrompt) {
            body.systemInstruction = {
                parts: [{ text: req.systemPrompt }]
            };
        }

        return body;
    }

    async call(req) {
        const url = `${this.baseUrl}/v1beta/models/${req.model}:generateContent?key=${this.apiKey}`;
        const headers = { 'Content-Type': 'application/json' };
        const bodyObj = this.buildBody(req);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });
            const text = await resp.text();
            logHttp('POST', url, headers, bodyObj, resp.status, text);

            if (!resp.ok) {
                return { model: req.model, content: `[Gemini Error: ${resp.status} ${resp.statusText}]` };
            }

            const data = JSON.parse(text);
            const content = data.candidates && data.candidates[0] && data.candidates[0].content && data.candidates[0].content.parts && data.candidates[0].content.parts[0] ? data.candidates[0].content.parts[0].text : '[Gemini: empty response]';
            return { model: req.model, content };
        } catch (e) {
            logHttp('POST', url, headers, bodyObj, 0, e.message);
            return { model: req.model, content: `[Gemini Error: ${e.message}]` };
        }
    }

    async callStreaming(req, onChunk, onDone, onError) {
        const url = `${this.baseUrl}/v1beta/models/${req.model}:streamGenerateContent?key=${this.apiKey}&alt=sse`;
        const headers = { 'Content-Type': 'application/json' };
        const bodyObj = this.buildBody(req);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });

            if (!resp.ok) {
                const errText = await resp.text();
                logHttp('POST', url, headers, bodyObj, resp.status, errText);
                onError(`HTTP error! status: ${resp.status}`);
                return;
            }

            logHttp('POST', url, headers, bodyObj, resp.status, '[Streaming response started]');

            const reader = resp.body.getReader();
            const decoder = new TextDecoder('utf-8');
            let buffer = '';
            let accumulatedContent = '';

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;

                buffer += decoder.decode(value, { stream: true });
                const lines = buffer.split('\n');
                buffer = lines.pop();

                for (const line of lines) {
                    const cleanLine = line.trim();
                    if (!cleanLine.startsWith('data: ')) continue;
                    try {
                        const parsed = JSON.parse(cleanLine.substring(6).trim());
                        const text = parsed.candidates && parsed.candidates[0] && parsed.candidates[0].content && parsed.candidates[0].content.parts && parsed.candidates[0].content.parts[0] ? parsed.candidates[0].content.parts[0].text : '';
                        if (text) {
                            onChunk(text);
                            accumulatedContent += text;
                        }
                    } catch (e) {}
                }
            }
            onDone({ model: req.model, content: accumulatedContent });
        } catch (e) {
            onError(e.message);
        }
    }
}

class OllamaProvider extends AIProvider {
    constructor(apiKey, baseUrl) {
        super(apiKey, baseUrl || 'http://localhost:11434');
    }

    name() { return 'ollama'; }
    listModels() { return ['llama3.2', 'mistral']; }

    async testConnection() {
        const url = `${this.baseUrl}/api/tags`;
        try {
            const resp = await fetch(url, { method: 'GET' });
            const bodyText = await resp.text();
            logHttp('GET', url, {}, '', resp.status, bodyText);

            if (resp.status === 200) return '';
            const data = JSON.parse(bodyText);
            if (data.error) return data.error;
            return `Connection failed: ${resp.status} ${resp.statusText}`;
        } catch (e) {
            logHttp('GET', url, {}, '', 0, e.message);
            return `Connection failed: ${e.message}`;
        }
    }

    buildBody(req, stream = false) {
        // Ollama uses system/prompt or messages
        return {
            model: req.model,
            system: req.systemPrompt,
            prompt: req.userPrompt,
            options: {
                temperature: req.temperature
            },
            stream
        };
    }

    async call(req) {
        const url = `${this.baseUrl}/api/generate`;
        const headers = { 'Content-Type': 'application/json' };
        const bodyObj = this.buildBody(req, false);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });
            const text = await resp.text();
            logHttp('POST', url, headers, bodyObj, resp.status, text);

            if (!resp.ok) {
                return { model: req.model, content: `[Ollama Error: ${resp.status} ${resp.statusText}]` };
            }

            const data = JSON.parse(text);
            const content = data.response || '[Ollama: empty response]';
            return { model: req.model, content };
        } catch (e) {
            logHttp('POST', url, headers, bodyObj, 0, e.message);
            return { model: req.model, content: `[Ollama Error: ${e.message}]` };
        }
    }

    async callStreaming(req, onChunk, onDone, onError) {
        const url = `${this.baseUrl}/api/generate`;
        const headers = { 'Content-Type': 'application/json' };
        const bodyObj = this.buildBody(req, true);

        try {
            const resp = await fetch(url, {
                method: 'POST',
                headers,
                body: JSON.stringify(bodyObj)
            });

            if (!resp.ok) {
                const errText = await resp.text();
                logHttp('POST', url, headers, bodyObj, resp.status, errText);
                onError(`HTTP error! status: ${resp.status}`);
                return;
            }

            logHttp('POST', url, headers, bodyObj, resp.status, '[Streaming response started]');

            const reader = resp.body.getReader();
            const decoder = new TextDecoder('utf-8');
            let buffer = '';
            let accumulatedContent = '';

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;

                buffer += decoder.decode(value, { stream: true });
                const lines = buffer.split('\n');
                buffer = lines.pop();

                for (const line of lines) {
                    const cleanLine = line.trim();
                    if (!cleanLine) continue;
                    try {
                        const parsed = JSON.parse(cleanLine);
                        const text = parsed.response || '';
                        if (text) {
                            onChunk(text);
                            accumulatedContent += text;
                        }
                        if (parsed.done) break;
                    } catch (e) {}
                }
            }
            onDone({ model: req.model, content: accumulatedContent });
        } catch (e) {
            onError(e.message);
        }
    }
}

module.exports = {
    AIProvider,
    setHttpLogCallback
};
