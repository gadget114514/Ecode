const storage = require('./storage');
const { AIProvider } = require('./ai');

class PipelineOptimizer {
    constructor() {
        this.systemPromptTemplate = `You are a pipeline prompt optimizer.
Analyze execution traces of an AI pipeline and propose minimal, targeted improvements
to the systemPrompt and userPrompt fields of pipeline steps.

Rules:
- Propose at most {maxEdits} operations per step.
- Operations: "replace" (change text), "add" (append instruction), "delete" (remove).
- Pinned content must NOT be modified.
- Do NOT re-propose previously rejected edits.
- Output a single JSON array only — no other text.

Output format (strict JSON array):
[
  {
    "op": "replace"|"add"|"delete",
    "stepName": "<step name>",
    "field": "systemPrompt"|"userPrompt",
    "oldValue": "<current text or empty for add>",
    "newValue": "<proposed text or empty for delete>",
    "rationale": "<one sentence>"
  }
]`;
    }

    sanitizeName(name) {
        return name.replace(/[^a-zA-Z0-9-]/g, '_');
    }

    getTimestamp() {
        return new Date().toISOString();
    }

    truncateUtf8(s, maxBytes) {
        if (!s) return '';
        const buf = Buffer.from(s, 'utf8');
        if (buf.length <= maxBytes) return s;
        return buf.subarray(0, maxBytes).toString('utf8') + '...';
    }

    // --- Rejected buffer persistence ---
    saveRejectedBuffer(pipelineName, buffer) {
        let entries = [...buffer];
        if (entries.length > 50) {
            entries = entries.slice(entries.length - 50);
        }
        const safeName = this.sanitizeName(pipelineName);
        storage.saveOptimizerData(`opt_rejected_${safeName}.json`, JSON.stringify({ entries }, null, 2));
    }

    loadRejectedBuffer(pipelineName) {
        const safeName = this.sanitizeName(pipelineName);
        const json = storage.loadOptimizerData(`opt_rejected_${safeName}.json`);
        if (!json) return [];
        try {
            const val = JSON.parse(json);
            return val.entries || [];
        } catch (e) {
            return [];
        }
    }

    // --- History loading ---
    loadEvaluatedHistory(pipelineName, evaluation, limit = 10) {
        const result = [];
        const files = storage.listHistory();
        let count = 0;

        for (const f of files) {
            if (count >= limit) break;
            const json = storage.loadHistoryRecord(f);
            if (!json) continue;

            try {
                const val = JSON.parse(json);
                if (val.pipelineName !== pipelineName) continue;
                
                const evalVal = val.evaluation || '';
                if (evaluation && evalVal !== evaluation) continue;

                const rec = {
                    id: val.id || '',
                    pipelineName: val.pipelineName || '',
                    startedAt: val.startedAt || '',
                    status: val.status || '',
                    evaluation: evalVal,
                    steps: (val.steps || []).map(s => ({
                        index: s.index || 0,
                        name: s.name || '',
                        type: s.type || '',
                        input: s.input || '',
                        output: s.output || '',
                        status: s.status || '',
                        evaluation: s.evaluation || '',
                        evaluationNote: s.evaluationNote || ''
                    }))
                };
                result.push(rec);
                count++;
            } catch (e) {
                // ignore
            }
        }
        return result;
    }

    collectPinnedContent(pipelineName) {
        const result = [];
        const files = storage.listHistory();
        for (const f of files) {
            const json = storage.loadHistoryRecord(f);
            if (!json) continue;
            try {
                const val = JSON.parse(json);
                if (val.pipelineName !== pipelineName) continue;
                if (!val.steps) continue;
                for (const s of val.steps) {
                    if (s.evaluation === 'pinned' && s.output) {
                        result.push(s.output);
                    }
                }
            } catch (e) {}
        }
        return result;
    }

    // --- Optimizer Prompt Builder ---
    buildOptimizerPrompt(pipeline, okSamples, rejectedSamples, pinnedContents, rejectedBuffer, maxEditsPerStep = 2) {
        let p = '## Current Pipeline Definition\n';
        p += `Name: ${pipeline.name}\n`;
        p += 'Steps:\n';
        for (const step of pipeline.steps) {
            p += `  ### Step: ${step.name} (type: ${step.type})\n`;
            if (step.systemPrompt) p += `  systemPrompt: ${this.truncateUtf8(step.systemPrompt, 1000)}\n`;
            if (step.userPrompt) p += `  userPrompt: ${this.truncateUtf8(step.userPrompt, 1000)}\n`;
        }

        p += '\n## Pinned Content (DO NOT modify)\n';
        if (pinnedContents.length === 0) {
            p += 'None\n';
        } else {
            pinnedContents.forEach((c, idx) => {
                p += `[pin ${idx + 1}] ${this.truncateUtf8(c, 300)}\n`;
            });
        }

        const dumpSamples = (samples, label) => {
            p += `\n## ${label} (${samples.length} runs)\n`;
            for (const rec of samples) {
                p += `Run: ${rec.id} | ${rec.startedAt}\n`;
                for (const step of rec.steps) {
                    p += `  Step "${step.name}" (${step.type})`;
                    if (step.evaluation) p += ` [${step.evaluation}]`;
                    p += '\n';
                    if (step.input) p += `    Input: ${this.truncateUtf8(step.input, 400)}\n`;
                    if (step.output) p += `    Output: ${this.truncateUtf8(step.output, 400)}\n`;
                }
            }
        };

        dumpSamples(okSamples, 'OK (success) samples');
        dumpSamples(rejectedSamples, 'Rejected (failure) samples');

        p += '\n## Previously Rejected Proposals (do NOT re-propose)\n';
        if (rejectedBuffer.length === 0) {
            p += 'None\n';
        } else {
            for (const entry of rejectedBuffer) {
                p += `- ${entry.op} | ${entry.stepName}.${entry.field} | ${entry.rationale}\n`;
            }
        }

        p += '\n## Task\n';
        p += 'Analyze the traces. Propose improvements for the REJECTED/failed samples.\n';
        p += 'Preserve patterns shown in the OK samples.\n';
        p += `Each step may have at most ${maxEditsPerStep} operations.\n`;

        return p;
    }

    parseProposals(aiResponse) {
        if (!aiResponse) return [];
        const start = aiResponse.indexOf('[');
        const end = aiResponse.lastIndexOf(']');
        if (start === -1 || end === -1) return [];
        const json = aiResponse.substring(start, end + 1);
        try {
            const arr = JSON.parse(json);
            if (!Array.isArray(arr)) return [];
            return arr.filter(e => e.op && e.stepName && e.field);
        } catch (e) {
            return [];
        }
    }

    applyApprovals(pipeline, approvedIndices, rejectedIndices, session) {
        const result = JSON.parse(JSON.stringify(pipeline));
        const approvedProposals = [];

        // Save rejected to rejected-edit buffer
        const rejectedBuffer = this.loadRejectedBuffer(pipeline.name);

        for (let i = 0; i < session.proposals.length; i++) {
            const prop = session.proposals[i];
            if (approvedIndices.includes(i)) {
                approvedProposals.push(prop);
                // Apply changes
                for (const step of result.steps) {
                    if (step.name === prop.stepName) {
                        if (prop.op === 'replace' || prop.op === 'add') {
                            step[prop.field] = prop.newValue;
                        } else if (prop.op === 'delete') {
                            step[prop.field] = '';
                        }
                    }
                }
            } else if (rejectedIndices.includes(i)) {
                rejectedBuffer.push(prop);
            }
        }

        if (rejectedIndices.length > 0) {
            this.saveRejectedBuffer(pipeline.name, rejectedBuffer);
        }

        session.approvedProposals = approvedProposals;
        return result;
    }

    async startSession(pipelineName, pipeline, historyLimit, maxEditsPerStep, providerType, apiKey, baseUrl, model, bridgeCb) {
        const postProgress = (msg) => {
            bridgeCb("optimize_progress", JSON.stringify({ message: msg }));
        };

        try {
            postProgress("Loading execution history...");
            let okSamples = this.loadEvaluatedHistory(pipelineName, "ok", historyLimit);
            let rejSamples = this.loadEvaluatedHistory(pipelineName, "rejected", historyLimit);

            if (okSamples.length === 0 && rejSamples.length === 0) {
                const allSamples = this.loadEvaluatedHistory(pipelineName, "", historyLimit);
                for (const rec of allSamples) {
                    const anyFail = rec.steps.some(s => s.status !== "completed" && s.status !== "ok");
                    if (anyFail) rejSamples.push(rec);
                    else okSamples.push(rec);
                }
            }

            if (okSamples.length === 0 && rejSamples.length === 0) {
                bridgeCb("optimize_error", JSON.stringify({ message: `No execution history found for pipeline: ${pipelineName}` }));
                return;
            }

            postProgress("Collecting pinned content...");
            const pinnedContents = this.collectPinnedContent(pipelineName);
            const rejectedBuffer = this.loadRejectedBuffer(pipelineName);

            postProgress("Building optimizer prompt...");
            const userPrompt = this.buildOptimizerPrompt(pipeline, okSamples, rejSamples, pinnedContents, rejectedBuffer, maxEditsPerStep);

            const sysPrompt = this.systemPromptTemplate.replace(/{maxEdits}/g, String(maxEditsPerStep));

            postProgress("Calling optimizer AI...");
            const provider = AIProvider.create(providerType, apiKey, baseUrl);
            if (!provider) {
                throw new Error(`Failed to create provider: ${providerType}`);
            }

            const req = {
                model,
                systemPrompt: sysPrompt,
                userPrompt,
                temperature: 0.3,
                maxTokens: 2048
            };

            const resp = await provider.call(req);
            const proposals = this.parseProposals(resp.content);

            if (proposals.length === 0) {
                bridgeCb("optimize_error", JSON.stringify({ message: "Optimizer returned no valid proposals." }));
                return;
            }

            // session ID
            const sessionId = "opt_" + new Date().toISOString().replace(/[-:T.Z]/g, "").substring(0, 15);
            
            const payload = {
                sessionId,
                pipelineName,
                proposals,
                evaluationSummary: {
                    okCount: okSamples.length,
                    rejectedCount: rejSamples.length,
                    pinnedCount: pinnedContents.length
                }
            };

            bridgeCb("optimize_proposals", JSON.stringify(payload));

        } catch (e) {
            bridgeCb("optimize_error", JSON.stringify({ message: `Optimizer error: ${e.message}` }));
        }
    }
}

module.exports = new PipelineOptimizer();

