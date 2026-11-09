#!/usr/bin/env node
// Whole-kernel check: the real sort / P2G / grid update / G2P kernels on the GPU against the
// float64 reference in golden/substep_golden.json (scripts/make_golden_substep.py).
//
//   node mpm-mls-doc/bench/check_substep.mjs [--chrome | --gpu] [--shaders-rev <git-rev>]
//
// --chrome uses the installed Google Chrome (the GPU on macOS); the default is SwiftShader. A
// reference mismatch exits with status 1. Needs Playwright (npm i -g playwright).

import { ROOT, exportRevision, launchBrowser, serve } from './headless_common.mjs';

// float32 against float64, 3 substeps. The grid accumulators are fixed point at 1e-4 (momentum)
// and 2^-20 (mass): the f32 evaluation of a contribution can land on the other side of a rounding
// step, a 1e-4 change of a node's momentum that reaches a particle through its weight and 4/dx.
// Measured errors are 3-50x below these. Positions are 2000-4000 m, where float32 spacing is
// 1.2e-4 to 2.4e-4 m: 1e-3 is about four spacings (three roundings plus the velocity error).
const TOLERANCE = { position: 1e-3, velocity: 2e-3, plastic_state: 1e-4, C: 1e-3, tau: 5e-3, F: 1e-4, mass: 0 };
// Stress is compared relative to its largest component.
const RELATIVE = new Set(['tau']);

const args = process.argv.slice(2);
const chrome = args.includes('--chrome');
const gpu = args.includes('--gpu') || chrome;
const revIndex = args.indexOf('--shaders-rev');
const revDirs = {};
if (revIndex >= 0) revDirs[args[revIndex + 1]] = exportRevision(args[revIndex + 1]);
const server = await serve(revDirs);
const port = server.address().port;
const browser = await launchBrowser({ gpu, chrome });
let failed = false;
try {
    const page = await browser.newPage();
    page.on('console', (msg) => { if (/ERROR/.test(msg.text())) process.stderr.write(`  [page] ${msg.text()}\n`); });
    const query = revIndex >= 0 ? `?shaders=/__rev/${args[revIndex + 1]}/` : '';
    await page.goto(`http://127.0.0.1:${port}/mpm-mls-doc/bench/check_substep.html${query}`);
    await page.waitForFunction(() => window.__result || window.__error, null, { timeout: 120000, polling: 250 });
    const { result, error } = await page.evaluate(() => ({ result: window.__result, error: window.__error }));
    if (error) throw new Error(error);
    console.log(`${result.adapter.architecture} ${result.schedule}: ${result.particles} particles, GPU vs float64 reference`);
    for (const [scenario, materials] of Object.entries(result.scenarios)) {
        for (const [material, steps] of Object.entries(materials)) {
            for (const [step, fields] of Object.entries(steps)) {
                const cells = Object.entries(fields).map(([name, { maxError, scale }]) => {
                    const error = RELATIVE.has(name) ? maxError / scale : maxError;
                    const ok = error <= TOLERANCE[name];
                    if (!ok) failed = true;
                    return `${name} ${error.toExponential(1)}${ok ? '' : ' !!'}`;
                });
                console.log(`  ${scenario.padEnd(8)} ${material.padEnd(10)} ${step.padEnd(7)} ${cells.join('  ')}`);
            }
        }
    }
    // The grid velocity buffer: node mass in w, and the old part of the grid cleared once the flow has moved.
    const g = result.grid;
    const massOk = (m) => Math.abs(m - g.particles) <= 1e-3; // each particle's weights add up to 1; fixed point at 2^-20
    const gridOk = massOk(g.massBefore) && massOk(g.massAfter) && g.velocityWithoutMassBefore === 0 && g.velocityWithoutMassAfter === 0;
    if (!gridOk) failed = true;
    console.log(`  grid     node mass ${g.massBefore.toFixed(4)} -> ${g.massAfter.toFixed(4)} after the flow moved 500 m (expected ${g.particles}); ` +
        `velocity on nodes without mass ${g.velocityWithoutMassBefore} / ${g.velocityWithoutMassAfter}${gridOk ? '' : ' !!'}`);
    console.log(failed ? 'FAILED' : 'all fields within tolerance');
} finally {
    await browser.close();
    server.close();
}
process.exit(failed ? 1 : 0);
