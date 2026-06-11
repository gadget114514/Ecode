const fs = require('fs');
const path = require('path');
const os = require('os');
const { spawn } = require('child_process');
const crypto = require('crypto');
const storage = require('./storage');
const { AIProvider } = require('./ai');

class PipelineRunner {
    constructor() {
        this.running = false;
        this.cancelled = false;
        this.pendingSteps = [];
        this.originalSteps = [];
        this.inputContent = '';
        this.inputAttachments = [];
        this.outputMode = 'child';
        this.pipelineName = '';
        this.bridgeCb = null;

        this.historySteps = [];
        this.currentStepIndex = -1;
        this.waitingForManual = false;
        this.waitingForWizard = false;
        this.waitingForFilter = false;

        this.runId = '';
        this.startedAt = '';

        // Wizard state
        this.wizardValues = {};
        
        // Filter state
        this.filterApproved = [];
        this.filterRejected = [];

        // Input override
        this.inputSourceOverridden = false;
        this.inputSourceContent = '';
        this.inputSourceName = '';

        // Providers config
        this.providers = {};
    }

    setBridgeCallback(cb) {
        this.bridgeCb = cb;
    }

    registerProvider(type, apiKey, baseUrl) {
        const provider = AIProvider.create(type, apiKey, baseUrl);
        if (provider) {
            this.providers[type] = provider;
        }
    }

    postBridge(type, payload) {
        if (this.bridgeCb) {
            const payloadStr = typeof payload === 'string' ? payload : JSON.stringify(payload);
            this.bridgeCb(type, payloadStr);
        }
    }

    getRunId() {
        return this.runId;
    }

    run(pipelineName, steps, inputContent, inputAttachments = [], outputMode = 'child') {
        this.running = true;
        this.cancelled = false;
        this.pipelineName = pipelineName;
        this.originalSteps = JSON.parse(JSON.stringify(steps));
        this.pendingSteps = JSON.parse(JSON.stringify(steps));
        this.inputContent = inputContent;
        this.inputAttachments = inputAttachments;
        this.outputMode = outputMode;

        this.runId = crypto.randomUUID();
        this.startedAt = new Date().toISOString();
        this.currentStepIndex = -1;
        this.historySteps = [];
        this.waitingForManual = false;
        this.waitingForWizard = false;
        this.waitingForFilter = false;

        this.postBridge('pipeline_started', {
            runId: this.runId,
            pipelineName: this.pipelineName,
            startedAt: this.startedAt
        });

        // Initialize history steps list
        this.historySteps = steps.map((s, idx) => ({
            index: idx,
            name: s.name,
            type: s.type,
            input: '',
            output: '',
            parallelBranches: {},
            retries: 0,
            iterations: 0,
            status: 'pending',
            evaluation: '',
            evaluationNote: ''
        }));

        this.runNextStep();
    }

    runNextStep() {
        if (this.cancelled) {
            this.running = false;
            return;
        }

        this.currentStepIndex++;
        if (this.currentStepIndex >= this.pendingSteps.length) {
            // Pipeline completed
            this.running = false;
            const record = this.buildMetaRecord();
            this.postBridge('pipeline_completed', record);
            return;
        }

        const step = this.pendingSteps[this.currentStepIndex];
        const prevOutput = this.currentStepIndex > 0 ? this.historySteps[this.currentStepIndex - 1].output : this.inputContent;
        const currentInput = this.inputSourceOverridden ? this.inputSourceContent : prevOutput;
        
        // Reset override for next step
        this.inputSourceOverridden = false;

        this.historySteps[this.currentStepIndex].input = currentInput;
        this.historySteps[this.currentStepIndex].status = 'running';

        // Save progress to run state database
        const state = {
            runId: this.runId,
            pipelineName: this.pipelineName,
            currentStep: this.currentStepIndex,
            totalSteps: this.pendingSteps.length,
            startedAt: this.startedAt,
            status: 'running'
        };
        this.postBridge('save_run_state', state);

        this.postBridge('step_started', {
            index: this.currentStepIndex,
            name: step.name
        });

        this.executeStep(step, currentInput);
    }

    async executeStep(step, currentInput) {
        const type = step.type;

        if (type === 'ai') {
            const providerType = step.provider || step.params?.provider || 'openai';
            const provider = this.providers[providerType] || AIProvider.create(providerType, '', '');
            if (!provider) {
                this.handleError(`Provider not configured: ${providerType}`);
                return;
            }

            const model = step.model || step.params?.model || 'gpt-4o-mini';
            let systemPrompt = step.systemPrompt || step.params?.systemPrompt || '';
            let userPrompt = step.userPrompt || step.params?.userPrompt || '{content}';
            let temp = 0.7;
            if (step.temperature || step.params?.temperature) {
                temp = parseFloat(step.temperature || step.params?.temperature);
            }

            // Replace placeholders
            const replacePlaceholders = (s) => {
                if (!s) return '';
                let res = s;
                // replace {content}
                res = res.replace(/{content}/g, this.inputContent);
                // replace {result}
                res = res.replace(/{result}/g, currentInput);
                return res;
            };

            systemPrompt = replacePlaceholders(systemPrompt);
            userPrompt = replacePlaceholders(userPrompt);

            const req = {
                model,
                systemPrompt,
                userPrompt,
                temperature: temp,
                maxTokens: 4096,
                attachments: this.inputAttachments
            };

            const idx = this.currentStepIndex;
            
            // Replicate C++: use stream callback to stream to frontend
            try {
                await provider.callStreaming(req, 
                    (chunk) => {
                        this.postBridge('stream_chunk', {
                            stepIndex: idx,
                            text: chunk
                        });
                    },
                    (resp) => {
                        if (idx < this.historySteps.length) {
                            this.historySteps[idx].output = resp.content;
                            this.historySteps[idx].status = 'completed';
                        }
                        this.postBridge('step_done', {
                            index: idx,
                            tokens: 0 // Stub or extract if provider gives it
                        });
                        this.runNextStep();
                    },
                    (err) => {
                        this.handleError(`AI Call failed: ${err}`);
                    }
                );
            } catch (e) {
                this.handleError(`AI call failed: ${e.message}`);
            }

        } else if (type === 'manual') {
            const mode = step.mode || step.params?.mode || 'view';
            const prompt = step.prompt || step.params?.prompt || '';
            this.waitingForManual = true;

            if (mode === 'compare') {
                const branches = [];
                if (this.currentStepIndex > 0) {
                    const prevStep = this.historySteps[this.currentStepIndex - 1];
                    for (const [bName, bOutput] of Object.entries(prevStep.parallelBranches || {})) {
                        branches.push({ name: bName, content: bOutput });
                    }
                }
                this.postBridge('manual_step_pause', {
                    index: this.currentStepIndex,
                    mode: 'compare',
                    prompt,
                    branches
                });
            } else {
                let choices = [];
                try {
                    choices = JSON.parse(step.choices || step.params?.choices || '[]');
                } catch (e) {}

                this.postBridge('manual_step_pause', {
                    index: this.currentStepIndex,
                    mode,
                    prompt,
                    content: currentInput,
                    choices
                });
            }

        } else if (type === 'command') {
            const cmd = step.command || step.params?.command || '';
            let args = [];
            try {
                const argsStr = step.args || step.params?.args || '[]';
                args = JSON.parse(argsStr);
            } catch (e) {}
            const workDir = step.workingDir || step.params?.workingDir || '';
            const resultAs = step.resultAs || step.params?.resultAs || 'text';
            const timeoutSec = parseInt(step.timeout || step.params?.timeout || '60');

            const idx = this.currentStepIndex;

            // Write content to a temp file
            const tempDir = os.tmpdir();
            const tempFile = path.join(tempDir, `prom_${crypto.randomBytes(6).toString('hex')}.tmp`);
            fs.writeFileSync(tempFile, currentInput, 'utf8');

            const resolvedArgs = args.map(arg => {
                let res = arg;
                res = res.replace(/{content_file}/g, tempFile);
                res = res.replace(/{content}/g, currentInput);
                res = res.replace(/{result}/g, currentInput);
                return res;
            });

            const env = { ...process.env };
            if (workDir) {
                // Replicate C++ variable resolution
                env.APPDATA = process.env.APPDATA || '';
            }

            const proc = spawn(cmd, resolvedArgs, {
                cwd: workDir ? workDir.replace(/%APPDATA%/g, env.APPDATA) : undefined,
                env,
                shell: true
            });

            let output = '';
            proc.stdout.on('data', (data) => {
                const chunk = data.toString('utf8');
                output += chunk;
                this.postBridge('stream_chunk', {
                    stepIndex: idx,
                    text: chunk
                });
            });

            proc.stderr.on('data', (data) => {
                const chunk = data.toString('utf8');
                output += chunk;
                this.postBridge('stream_chunk', {
                    stepIndex: idx,
                    text: chunk
                });
            });

            const timer = setTimeout(() => {
                proc.kill();
                output += '\n[Process timed out]';
            }, timeoutSec * 1000);

            proc.on('close', (code) => {
                clearTimeout(timer);
                try {
                    fs.unlinkSync(tempFile);
                } catch (e) {}

                if (idx < this.historySteps.length) {
                    this.historySteps[idx].output = (resultAs === 'text') ? output : '';
                    this.historySteps[idx].status = 'completed';
                }

                this.postBridge('step_done', { index: idx });
                this.runNextStep();
            });

            proc.on('error', (err) => {
                clearTimeout(timer);
                try {
                    fs.unlinkSync(tempFile);
                } catch (e) {}

                output = `[command launch failed: ${cmd} - ${err.message}]`;
                this.postBridge('stream_chunk', {
                    stepIndex: idx,
                    text: output
                });

                if (idx < this.historySteps.length) {
                    this.historySteps[idx].output = output;
                    this.historySteps[idx].status = 'completed';
                }

                this.postBridge('step_done', { index: idx });
                this.runNextStep();
            });

        } else if (type === 'parallel') {
            let branches = [];
            try {
                const branchesStr = step.branches || step.params?.branches || '[]';
                branches = JSON.parse(branchesStr);
            } catch (e) {}

            this.parallelState = {
                branches,
                currentBranch: 0,
                results: {},
                inputContent: currentInput
            };

            this.executeNextParallelBranch();

        } else if (type === 'wizard') {
            const wizardName = step.wizard || step.params?.wizard || '';
            const wizardData = step.wizardData || step.params?.wizardData || '{}';
            this.waitingForWizard = true;

            this.postBridge('wizard_step_pause', {
                index: this.currentStepIndex,
                wizard: wizardName,
                wizardData: typeof wizardData === 'string' ? JSON.parse(wizardData) : wizardData,
                content: currentInput
            });

        } else if (type === 'filter') {
            const mode = step.mode || step.params?.mode || 'manual';
            const splitBy = step.splitBy || step.params?.splitBy || '';
            this.waitingForFilter = true;
            this.filterApproved = [];
            this.filterRejected = [];

            const outputs = [];
            if (splitBy && currentInput) {
                const blocks = currentInput.split(splitBy);
                blocks.forEach((blk, i) => {
                    outputs.push({ index: i, content: blk });
                });
            } else {
                outputs.push({ index: 0, content: currentInput });
            }

            if (mode === 'auto') {
                this.filterApproved.push(0);
                if (this.currentStepIndex < this.historySteps.length) {
                    this.historySteps[this.currentStepIndex].status = 'completed';
                }
                this.postBridge('step_done', { index: this.currentStepIndex });
                this.waitingForFilter = false;
                this.runNextStep();
            } else {
                this.postBridge('step_filter_pause', {
                    index: this.currentStepIndex,
                    mode,
                    outputs
                });
            }

        } else if (type === 'evaluate') {
            const criteria = step.criteria || step.params?.criteria || '';
            const rubric = step.rubric || step.params?.rubric || '1-10';

            this.postBridge('evaluate_result', {
                stepIndex: this.currentStepIndex,
                content: currentInput,
                criteria,
                rubric
            });

            if (this.currentStepIndex < this.historySteps.length) {
                this.historySteps[this.currentStepIndex].status = 'completed';
                this.historySteps[this.currentStepIndex].output = currentInput;
            }

            this.postBridge('step_done', { index: this.currentStepIndex });
            this.runNextStep();

        } else if (type === 'chest') {
            const chestName = step.chestName || step.params?.chestName || '';
            const mode = step.mode || step.params?.mode || 'put';

            if (!chestName) {
                this.handleError('Chest step missing chestName');
                return;
            }

            if (mode === 'put') {
                storage.saveToNamedChest(chestName, currentInput);
                this.postBridge('chest_put', { chestName, content: currentInput });
            } else if (mode === 'take') {
                const chestContent = storage.loadFromNamedChest(chestName);
                this.setExternalInput(chestContent);
            }

            if (this.currentStepIndex < this.historySteps.length) {
                this.historySteps[this.currentStepIndex].status = 'completed';
            }

            this.postBridge('step_done', { index: this.currentStepIndex });
            this.runNextStep();

        } else if (type === 'condition') {
            const expr = step.expression || step.params?.expression || '{result}';
            const op = step.operator || step.params?.operator || 'contains';
            const val = step.value || step.params?.value || '';

            const resolvedExpr = expr.replace(/{result}/g, currentInput);
            let matched = false;
            if (op === 'contains') {
                matched = resolvedExpr.includes(val);
            } else if (op === 'equals') {
                matched = (resolvedExpr === val);
            }

            this.postBridge('log', { message: 'Condition step evaluated: matching = ' + matched });

            if (this.currentStepIndex < this.historySteps.length) {
                this.historySteps[this.currentStepIndex].status = 'completed';
            }

            this.postBridge('step_done', { index: this.currentStepIndex });
            this.runNextStep();

        } else {
            // Unknown step type
            this.postBridge('log', { message: `⚠ Unknown step type: ${type} - skipped` });
            if (this.currentStepIndex < this.historySteps.length) {
                this.historySteps[this.currentStepIndex].status = 'skipped';
            }
            this.runNextStep();
        }
    }

    async executeNextParallelBranch() {
        if (!this.parallelState || this.cancelled) {
            this.parallelState = null;
            this.runNextStep();
            return;
        }

        const idx = this.parallelState.currentBranch;
        if (idx >= this.parallelState.branches.length) {
            // All parallel branches completed
            const hs = this.historySteps[this.currentStepIndex];
            hs.parallelBranches = this.parallelState.results;
            hs.status = 'completed';
            hs.output = JSON.stringify(this.parallelState.results, null, 2);

            this.postBridge('step_done', { index: this.currentStepIndex });
            this.parallelState = null;
            this.runNextStep();
            return;
        }

        const branch = this.parallelState.branches[idx];
        const branchName = branch.name || 'branch';

        this.postBridge('stream_chunk', {
            stepIndex: this.currentStepIndex,
            branch: branchName,
            text: ''
        });

        if (!branch.steps || branch.steps.length === 0) {
            this.parallelState.results[branchName] = this.parallelState.inputContent;
            this.parallelState.currentBranch++;
            this.executeNextParallelBranch();
            return;
        }

        const bStep = branch.steps[0];
        const providerType = bStep.provider || 'openai';
        const provider = this.providers[providerType] || AIProvider.create(providerType, '', '');
        if (!provider) {
            this.parallelState.results[branchName] = '[Provider not configured]';
            this.parallelState.currentBranch++;
            this.executeNextParallelBranch();
            return;
        }

        const req = {
            model: bStep.model || 'gpt-4o-mini',
            systemPrompt: (bStep.systemPrompt || '').replace(/{content}/g, this.parallelState.inputContent).replace(/{result}/g, this.parallelState.inputContent),
            userPrompt: (bStep.userPrompt || '{content}').replace(/{content}/g, this.parallelState.inputContent).replace(/{result}/g, this.parallelState.inputContent),
            temperature: parseFloat(bStep.temperature || '0.7'),
            maxTokens: 4096
        };

        const currentStepIdx = this.currentStepIndex;

        try {
            await provider.callStreaming(req,
                (chunk) => {
                    this.postBridge('stream_chunk', {
                        stepIndex: currentStepIdx,
                        branch: branchName,
                        text: chunk
                    });
                },
                (resp) => {
                    if (this.parallelState) {
                        this.parallelState.results[branchName] = resp.content;
                        this.parallelState.currentBranch++;
                    }
                    this.executeNextParallelBranch();
                },
                (err) => {
                    if (this.parallelState) {
                        this.parallelState.results[branchName] = `[Error: ${err}]`;
                        this.parallelState.currentBranch++;
                    }
                    this.executeNextParallelBranch();
                }
            );
        } catch (e) {
            if (this.parallelState) {
                this.parallelState.results[branchName] = `[Error: ${e.message}]`;
                this.parallelState.currentBranch++;
            }
            this.executeNextParallelBranch();
        }
    }

    handleError(message) {
        this.running = false;
        this.postBridge('pipeline_error', { message });
    }

    resumeManual(content) {
        if (!this.waitingForManual) return;
        this.waitingForManual = false;
        if (this.currentStepIndex < this.historySteps.length) {
            this.historySteps[this.currentStepIndex].output = content;
            this.historySteps[this.currentStepIndex].status = 'completed';
        }
        this.postBridge('step_done', { index: this.currentStepIndex });
        this.runNextStep();
    }

    cancelManual() {
        if (!this.waitingForManual) return;
        this.waitingForManual = false;
        this.cancel();
    }

    resumeWizard(valuesJson) {
        if (!this.waitingForWizard) return;
        this.waitingForWizard = false;

        try {
            const vals = JSON.parse(valuesJson);
            this.wizardValues = { ...this.wizardValues, ...vals };
        } catch (e) {}

        if (this.currentStepIndex < this.historySteps.length) {
            this.historySteps[this.currentStepIndex].output = valuesJson;
            this.historySteps[this.currentStepIndex].status = 'completed';
        }

        this.postBridge('step_done', { index: this.currentStepIndex });
        this.runNextStep();
    }

    resumeFilter(decisionJson) {
        if (!this.waitingForFilter) return;
        this.waitingForFilter = false;

        try {
            const val = JSON.parse(decisionJson);
            if (val.approved) this.filterApproved = val.approved;
            if (val.rejected) this.filterRejected = val.rejected;
        } catch (e) {}

        if (this.currentStepIndex < this.historySteps.length) {
            this.historySteps[this.currentStepIndex].status = 'completed';
        }

        this.postBridge('step_filter_result', {
            approved: this.filterApproved.length,
            rejected: this.filterRejected.length
        });
        this.postBridge('step_done', { index: this.currentStepIndex });
        this.runNextStep();
    }

    cancel() {
        this.cancelled = true;
        this.running = false;
        this.pendingSteps = [];
        this.postBridge('pipeline_cancelled', {});
    }

    setExternalInput(content) {
        this.inputSourceOverridden = true;
        this.inputSourceContent = content;
        this.inputSourceName = 'external';
    }

    buildMetaRecord() {
        const lastOutput = this.historySteps.length > 0 ? this.historySteps[this.historySteps.length - 1].output : '';
        return {
            id: this.runId,
            pipelineName: this.pipelineName,
            startedAt: this.startedAt,
            executedAt: new Date().toISOString(),
            status: 'completed',
            outputContent: lastOutput,
            steps: this.originalSteps.map((step, idx) => {
                const hist = this.historySteps[idx] || {};
                const res = {
                    name: step.name,
                    type: step.type,
                    input: hist.input || '',
                    output: hist.output || '',
                    artifacts: hist.artifacts || [],
                    tokens: hist.completionTokens || 0
                };
                for (const [k, v] of Object.entries(step.params || {})) {
                    res[k] = v;
                }
                return res;
            })
        };
    }
}

module.exports = new PipelineRunner();
