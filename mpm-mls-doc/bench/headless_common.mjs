// Shared by run_headless.mjs and check_substep.mjs: Playwright, a static file server over the
// repository root (plus exported shader revisions), and the Chromium flags for WebGPU.

import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

export const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
export const SHADER_DIR = 'webgpu/compute/shaders';

export async function loadPlaywright() {
    const candidates = ['playwright'];
    try {
        const globalRoot = execFileSync('npm', ['root', '-g'], { encoding: 'utf8' }).trim();
        candidates.push(path.join(globalRoot, 'playwright', 'index.js'));
    } catch {}
    const require = createRequire(import.meta.url);
    for (const c of candidates) {
        try { return require(c); } catch {}
    }
    throw new Error('Playwright not found - npm i -g playwright');
}

// Export the shader directory of a git revision into a temp dir, served under /__rev/<rev>/.
export function exportRevision(rev) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mpm-bench-'));
    const files = execFileSync('git', ['ls-tree', '--name-only', `${rev}:${SHADER_DIR}`], { cwd: ROOT, encoding: 'utf8' }).split('\n').filter((f) => f.endsWith('.wgsl'));
    for (const f of files) fs.writeFileSync(path.join(dir, f), execFileSync('git', ['show', `${rev}:${SHADER_DIR}/${f}`], { cwd: ROOT }));
    return dir;
}

export function serve(revDirs = {}) {
    const types = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wgsl': 'text/plain', '.json': 'application/json' };
    const server = http.createServer((req, res) => {
        const url = decodeURIComponent(new URL(req.url, 'http://x').pathname);
        let file;
        const rev = url.match(/^\/__rev\/(.+)\/([^/]+)$/); // refs may contain slashes (feature/foo)
        if (rev && revDirs[rev[1]]) file = path.join(revDirs[rev[1]], path.basename(rev[2]));
        else file = path.join(ROOT, path.normalize(url));
        if (!file.startsWith(ROOT) && !Object.values(revDirs).some((d) => file.startsWith(d))) { res.writeHead(403); res.end(); return; }
        if (fs.existsSync(file) && fs.statSync(file).isDirectory()) file = path.join(file, 'index.html');
        if (!fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
        res.writeHead(200, { 'content-type': types[path.extname(file)] ?? 'application/octet-stream', 'cache-control': 'no-store' });
        fs.createReadStream(file).pipe(res);
    });
    return new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(server)));
}

// gpu: the machine's GPU instead of SwiftShader; chrome: the installed Google Chrome (implies gpu).
export async function launchBrowser({ gpu = false, chrome = false } = {}) {
    const { chromium } = await loadPlaywright();
    const flags = ['--enable-unsafe-webgpu', '--enable-webgpu-developer-features', '--enable-dawn-features=allow_unsafe_apis'];
    if (!gpu && !chrome) flags.push('--use-webgpu-adapter=swiftshader', '--enable-features=Vulkan');
    return chromium.launch({ headless: true, args: flags, ...(chrome ? { channel: 'chrome' } : {}) });
}
