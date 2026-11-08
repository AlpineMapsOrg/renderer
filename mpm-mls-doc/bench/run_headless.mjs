#!/usr/bin/env node
// Headless driver for the MLS-MPM kernel benchmark (index.html / mpm_bench.js).
//
//   node mpm-mls-doc/bench/run_headless.mjs [--baseline <git-rev>] [--gpu] [--chrome] [--out results.json] [key=value ...]
//
// Serves the repository root, opens the benchmark page in Chromium and prints the result.
// --baseline runs the same configuration a second time on the shaders of <git-rev> (exported
// with `git show`), so a kernel change can be shown to be faster *and* to leave the physics
// alone - the fixed-point P2G makes identical kernels bit-for-bit reproducible.
// --gpu uses the machine's GPU; the default is SwiftShader (CPU Vulkan), which works on any CI
// box but whose absolute timings say nothing about a real GPU - use it for relative numbers
// and for the physics check only. Playwright's headless shell has no GPU on macOS and falls back
// to SwiftShader even with --gpu; --chrome launches the installed Google Chrome instead (implies
// --gpu). Check `adapter` in the result to see which device ran.
//
// key=value pairs override benchmark settings, e.g. particles=32768 runs=5.
// Needs Playwright: `npm i -g playwright` (or a local install) plus a Chromium it can launch.

import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import { ROOT, SHADER_DIR, exportRevision, launchBrowser, serve } from './headless_common.mjs';

function parseArgs(argv) {
    const args = { overrides: {}, baseline: null, gpu: false, chrome: false, out: null };
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a === '--baseline') args.baseline = argv[++i];
        else if (a === '--gpu') args.gpu = true;
        else if (a === '--chrome') args.chrome = args.gpu = true;
        else if (a === '--out') args.out = argv[++i];
        else if (a.includes('=')) { const [k, v] = a.split('='); args.overrides[k] = v; }
        else throw new Error(`unknown argument ${a}`);
    }
    return args;
}

async function runPage(browser, port, shaders, overrides) {
    const page = await browser.newPage();
    page.on('console', (msg) => process.stderr.write(`  [page] ${msg.text()}\n`));
    const q = new URLSearchParams({ ...overrides, autorun: '1' });
    if (shaders) q.set('shaders', shaders);
    await page.goto(`http://127.0.0.1:${port}/mpm-mls-doc/bench/index.html?${q}`);
    await page.waitForFunction(() => window.__result || window.__error, null, { timeout: 0, polling: 500 });
    const { result, error } = await page.evaluate(() => ({ result: window.__result, error: window.__error }));
    await page.close();
    if (error) throw new Error(error);
    return result;
}

function summary(r) {
    const stages = Object.entries(r.perRun.stagesMs).map(([k, v]) => `${k} ${v.toFixed(2)}`).join(', ');
    return `${r.adapter?.architecture ?? '?'} ${r.schedule}: GPU ${r.perRun.gpuMs.toFixed(2)} ms/run (${stages}); wall ${r.perRun.wallMsMedian.toFixed(1)} ms; ` +
        `${r.simSecondsPerWallSecond}x real time; solver ${(r.memory.solverBytes / 2 ** 20).toFixed(1)} MiB; COM ${r.physics.finalCom.join(', ')}`;
}

// Relative deviation of the centre-of-mass trace, normalised by how far the COM moved.
function physicsDeviation(a, b) {
    const n = Math.min(a.physics.trace.length, b.physics.trace.length);
    const start = a.physics.trace[0].com;
    let maxErr = 0, travel = 1e-9;
    for (let i = 0; i < n; i++) {
        const pa = a.physics.trace[i].com, pb = b.physics.trace[i].com;
        maxErr = Math.max(maxErr, Math.hypot(pa[0] - pb[0], pa[1] - pb[1], pa[2] - pb[2]));
        travel = Math.max(travel, Math.hypot(pa[0] - start[0], pa[1] - start[1], pa[2] - start[2]));
    }
    return { maxComDifference: +maxErr.toFixed(4), comTravel: +travel.toFixed(3), relative: +(maxErr / travel).toFixed(5) };
}

const args = parseArgs(process.argv.slice(2));
const revDirs = {};
if (args.baseline) revDirs[args.baseline] = exportRevision(args.baseline);
const server = await serve(revDirs);
const port = server.address().port;
const browser = await launchBrowser({ gpu: args.gpu, chrome: args.chrome });

const out = { date: new Date().toISOString(), commit: execFileSync('git', ['rev-parse', '--short', 'HEAD'], { cwd: ROOT, encoding: 'utf8' }).trim(), dirty: execFileSync('git', ['status', '--porcelain', SHADER_DIR], { cwd: ROOT, encoding: 'utf8' }).trim() !== '', runs: {} };
try {
    out.runs.current = await runPage(browser, port, null, args.overrides);
    console.log(`current   ${summary(out.runs.current)}`);
    if (args.baseline) {
        out.runs.baseline = await runPage(browser, port, `/__rev/${args.baseline}/`, args.overrides);
        console.log(`baseline  ${summary(out.runs.baseline)}`);
        const speedup = out.runs.baseline.perRun.gpuMs / out.runs.current.perRun.gpuMs;
        out.comparison = { gpuSpeedup: +speedup.toFixed(3), physics: physicsDeviation(out.runs.baseline, out.runs.current) };
        console.log(`speed-up  ${speedup.toFixed(2)}x GPU; physics ${JSON.stringify(out.comparison.physics)}`);
    }
} finally {
    await browser.close();
    server.close();
}
if (args.out) fs.writeFileSync(args.out, JSON.stringify(out, null, 2));
