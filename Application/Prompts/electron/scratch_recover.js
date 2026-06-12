const fs = require('fs');
const path = require('path');
const brainDir = 'C:\\Users\\bluen\\.gemini\\antigravity-cli\\brain';
const subDirs = [
    'f15389a9-e786-4074-8e60-cc0b4e1bf287',
    '165f00dd-55d6-4551-ac9a-854883da3d89',
    'be87d5a5-6d90-475d-bd89-21ed737016fe'
];

for (const sub of subDirs) {
    const logDir = path.join(brainDir, sub, '.system_generated', 'logs');
    try {
        const files = fs.readdirSync(logDir);
        for (const file of files) {
            const stat = fs.statSync(path.join(logDir, file));
            console.log(`Sub: ${sub}, File: ${file}, Size: ${stat.size}`);
        }
    } catch(e) {
        console.error(`Error on sub ${sub}: ${e.message}`);
    }
}
