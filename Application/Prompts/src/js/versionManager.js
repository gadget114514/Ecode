const storage = require('./storage');

class PipelineVersionManager {
    sanitizeName(name) {
        return name.replace(/[^a-zA-Z0-9-]/g, '_');
    }

    getTimestamp() {
        return new Date().toISOString();
    }

    loadCursor(pipelineName) {
        const safeName = this.sanitizeName(pipelineName);
        const json = storage.loadOptimizerData(`opt_cursor_${safeName}.json`);
        if (!json) {
            return {
                pipelineName,
                currentVersion: 0,
                headVersion: 0,
                entries: []
            };
        }
        try {
            return JSON.parse(json);
        } catch (e) {
            return {
                pipelineName,
                currentVersion: 0,
                headVersion: 0,
                entries: []
            };
        }
    }

    saveCursor(pipelineName, cursor) {
        const safeName = this.sanitizeName(pipelineName);
        storage.saveOptimizerData(`opt_cursor_${safeName}.json`, JSON.stringify(cursor, null, 2));
    }

    saveSnapshot(pipelineName, version, pipeline) {
        const safeName = this.sanitizeName(pipelineName);
        storage.saveOptimizerData(`opt_snapshot_${safeName}_${version}.json`, JSON.stringify({ pipeline }, null, 2));
    }

    loadSnapshot(pipelineName, version) {
        const safeName = this.sanitizeName(pipelineName);
        const json = storage.loadOptimizerData(`opt_snapshot_${safeName}_${version}.json`);
        if (!json) return null;
        try {
            const data = JSON.parse(json);
            return data.pipeline;
        } catch (e) {
            return null;
        }
    }

    createVersion(pipelineName, approvedProposals, pipeline) {
        const cursor = this.loadCursor(pipelineName);
        
        // If we were in the middle of undo history, prune forward history before adding new version
        if (cursor.currentVersion < cursor.headVersion) {
            cursor.entries = cursor.entries.filter(e => e.version <= cursor.currentVersion);
            cursor.headVersion = cursor.currentVersion;
        }

        const newVersion = cursor.headVersion + 1;
        cursor.headVersion = newVersion;
        cursor.currentVersion = newVersion;

        const entry = {
            version: newVersion,
            timestamp: this.getTimestamp(),
            sessionId: '', // can be populated or empty
            label: `Version ${newVersion}`,
            approvedProposals
        };
        cursor.entries.push(entry);

        this.saveCursor(pipelineName, cursor);
        this.saveSnapshot(pipelineName, newVersion, pipeline);

        return newVersion;
    }

    undo(pipelineName) {
        const cursor = this.loadCursor(pipelineName);
        if (cursor.currentVersion <= 0) return null; // already at version 0 (initial)
        
        cursor.currentVersion -= 1;
        this.saveCursor(pipelineName, cursor);

        if (cursor.currentVersion === 0) {
            // Revert to original pipeline (we don't have a snapshot for version 0, so load the original pipeline from project config)
            const pipelines = storage.loadPipelines();
            const original = pipelines.find(p => p.name === pipelineName);
            return original || null;
        }

        return this.loadSnapshot(pipelineName, cursor.currentVersion);
    }

    redo(pipelineName) {
        const cursor = this.loadCursor(pipelineName);
        if (cursor.currentVersion >= cursor.headVersion) return null; // already at head

        cursor.currentVersion += 1;
        this.saveCursor(pipelineName, cursor);

        return this.loadSnapshot(pipelineName, cursor.currentVersion);
    }

    checkout(pipelineName, version) {
        const cursor = this.loadCursor(pipelineName);
        if (version < 0 || version > cursor.headVersion) return null;

        cursor.currentVersion = version;
        this.saveCursor(pipelineName, cursor);

        if (version === 0) {
            const pipelines = storage.loadPipelines();
            return pipelines.find(p => p.name === pipelineName) || null;
        }

        return this.loadSnapshot(pipelineName, version);
    }

    reapply(pipelineName, approvedProposals, pipeline) {
        return this.createVersion(pipelineName, approvedProposals, pipeline);
    }

    getVersionList(pipelineName) {
        const cursor = this.loadCursor(pipelineName);
        return cursor.entries || [];
    }
}

module.exports = new PipelineVersionManager();
