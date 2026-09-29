// Standalone WebGPU benchmark for the MLS-MPM solver kernels.
//
// Runs the *real* WGSL kernels from webgpu/compute/shaders on a synthetic, analytic terrain,
// with the same bindings, uniform layout and dispatch sequence as MpmSolverNode. No Qt, no
// tiles, no network: every run of a given configuration is bit-for-bit the same work, so the
// numbers are comparable across commits and machines. See 09-performance-analysis.md.
//
// Keep in sync with MpmSolverNode.{h,cpp}: the uniform packing (packSettings), the buffer
// sizes (allocate) and the per-substep dispatch list (encodeSubsteps). The kernel set is
// detected from the shader sources, so an older revision of the shaders (served next to the
// current one, see run_headless.mjs --baseline) runs through the schedule it was written for.

export const DEFAULTS = {
    // The Breite Ries scenario as the graph preset had it before 2026-09-27 (131k particles,
    // 16 layers, dt 0.01). Kept as the fixed reference so results stay comparable across
    // revisions; the current preset is particles=65536 gridLayers=12 dt=0.02.
    regionSize: 8000, // m, square
    heightTexels: 640, // ~12.5 m DEM, like zoom 15
    domainSize: 4000,
    gridRes: 320,
    gridLayers: 16,
    particles: 131072,
    releaseRadius: 100,
    slabThickness: 1.5,
    density: 400,
    dt: 0.01,
    substepsPerRun: 24,
    substepsPerSubmit: 24, // benchmark default: one submit per run, pure GPU throughput
    rasterRes: 1024,
    splatRadius: 8,
    // Cam Clay (Li et al. 2021 case V) by default: the snow-science model, and the most expensive
    // return mapping of the three. model=stomakhin / model=dp for the others.
    model: 'ccc',
    friction: 'coulomb',
    mu: 0.47,
    seed: 1,
    warmupRuns: 3,
    runs: 20,
    timing: 'stages', // 'stages' = one compute pass per stage with timestamps, 'pass' = one pass per chunk (as the app)
};

const MATERIALS = {
    stomakhin: { id: 0, E: 1.4e5, nu: 0.2 },
    dp: { id: 1, E: 3.0e6, nu: 0.3, phi: 13.3 },
    ccc: { id: 2, E: 3.0e6, nu: 0.3, M: 0.7, beta: 0.2, xi: 0.002, p0: 3000 },
};

const PARTICLE_BYTES = 128;
const GRID_NODE_BYTES = 20;
const SIM_STATE_BYTES = 56;
const TILE = 8; // columns per tile edge, must match TILE_SIZE in mpm_common.wgsl (v2 kernels)

// --------------------------------------------------------------------------------------
// Shader loading: resolves weBIGeo's `///use name` includes, include-once, like the app's
// ShaderPreprocessor for the subset the MPM kernels use.
// --------------------------------------------------------------------------------------

const USE_RE = /^\s*\/\/\/use\s+(?:([A-Za-z_][A-Za-z0-9_]*)::)?([\/\w .-]+?)\s*$/;

async function fetchText(url) {
    const response = await fetch(url, { cache: 'no-store' });
    if (!response.ok) return null;
    return response.text();
}

export async function loadKernelSources(shaderBase) {
    const cache = new Map();
    const read = async (name) => {
        if (!cache.has(name)) cache.set(name, await fetchText(`${shaderBase}${name}.wgsl`));
        return cache.get(name);
    };
    const resolve = async (name, seen) => {
        if (seen.has(name)) return '';
        seen.add(name);
        const src = await read(name);
        if (src === null) throw new Error(`shader ${name} not found under ${shaderBase}`);
        const out = [];
        for (const line of src.split('\n')) {
            const m = line.match(USE_RE);
            if (m) out.push(await resolve(m[2], seen));
            else if (!line.trimStart().startsWith('///')) out.push(line);
        }
        return out.join('\n');
    };

    const names = ['mpm_prepare', 'mpm_seed', 'mpm_clear_grid', 'mpm_p2g', 'mpm_grid_update', 'mpm_g2p', 'mpm_refresh_stress', 'mpm_splat', 'mpm_rasterize'];
    const kernels = {};
    for (const name of names) {
        if ((await read(name)) === null) continue; // a revision may not have every kernel
        kernels[name] = await resolve(name, new Set());
    }
    const common = await read('mpm_common');
    return { kernels, common };
}

// Which schedule the loaded kernels expect.
//   v1: clear -> P2G -> grid update (4x4x4 over all nodes) -> G2P, P2G computes the stress
//   v2: P2G (marks active tiles) -> grid update (one workgroup per tile, zeroes the
//       accumulators, writes grid_velocity) -> G2P (computes the next stress); no clear
function detectSchedule(common) {
    return /var<storage,\s*read_write>\s+tile_flags/.test(common) ? 'v2' : 'v1';
}

// Explicit layout from the binding declarations in mpm_common - every kernel includes it, so
// one layout serves all of them (as in the node).
function bindingTable(common) {
    const re = /@group\(0\)\s*@binding\((\d+)\)\s*var(<[^>]*>)?\s+(\w+)\s*:\s*([^;]+);/g;
    const table = [];
    let m;
    while ((m = re.exec(common)) !== null) {
        const [, binding, access = '', name, type] = m;
        let entry;
        if (access.includes('uniform')) entry = { buffer: { type: 'uniform' } };
        else if (access.includes('storage')) entry = { buffer: { type: access.includes('read_write') ? 'storage' : 'read-only-storage' } };
        else if (type.startsWith('texture_storage_2d')) entry = { storageTexture: { access: 'write-only', format: 'rgba8unorm', viewDimension: '2d' } };
        else if (type.startsWith('texture_2d')) entry = { texture: { sampleType: name === 'height_texture' ? 'unfilterable-float' : 'float', viewDimension: '2d' } };
        else throw new Error(`unknown binding type for ${name}: ${type}`);
        table.push({ binding: Number(binding), name, visibility: GPUShaderStage.COMPUTE, ...entry });
    }
    return table.sort((a, b) => a.binding - b.binding);
}

// --------------------------------------------------------------------------------------
// Synthetic terrain: a 35 degree release slope that bends into a flat runout, with a
// parabolic gully so the flow channelises. Deterministic and resolution independent.
// Region-relative metres, y = north. Returns absolute altitude in metres.
// --------------------------------------------------------------------------------------

export function terrainHeight(x, y, region) {
    const top = region * 0.5 + 1700; // 300 m inside the northern edge of the default 4 km domain
    const s = top - y; // distance downslope from the top (negative above it: keeps rising)
    const steep = Math.tan((35 * Math.PI) / 180);
    const bendStart = 900, bendEnd = 2200;
    // Integrated slope profile: constant steep, linear decrease to 0, then flat.
    let drop;
    if (s <= bendStart) drop = steep * s;
    else if (s <= bendEnd) {
        const t = s - bendStart, L = bendEnd - bendStart;
        drop = steep * bendStart + steep * (t - (t * t) / (2 * L));
    } else drop = steep * bendStart + (steep * (bendEnd - bendStart)) / 2;
    const gully = 0.004 * (x - region * 0.5) ** 2;
    return 2500 - drop + Math.min(gully, 300);
}

// --------------------------------------------------------------------------------------

export class MpmBench {
    constructor(device, sources, cfg) {
        this.device = device;
        this.cfg = { ...DEFAULTS, ...cfg };
        // run() loops until substepsPerRun are submitted in chunks of substepsPerSubmit: a zero,
        // negative or NaN chunk (e.g. from an empty form field) would never terminate.
        for (const k of ['substepsPerRun', 'substepsPerSubmit', 'particles', 'gridLayers', 'runs'])
            if (!Number.isInteger(this.cfg[k]) || this.cfg[k] < 1) throw new Error(`${k} must be a positive integer, got ${this.cfg[k]}`);
        this.kernels = sources.kernels;
        this.schedule = detectSchedule(sources.common);
        this.bindings = bindingTable(sources.common);
        this.hasTimestamps = device.features.has('timestamp-query');
        this.bytes = {};
    }

    async init() {
        const d = this.device;
        const c = this.cfg;
        d.pushErrorScope('validation');

        this.gridRes = [c.gridRes, c.gridRes, c.gridLayers];
        this.nodes = c.gridRes * c.gridRes * c.gridLayers;
        this.tiles = [Math.ceil(c.gridRes / TILE), Math.ceil(c.gridRes / TILE)];

        const layout = d.createBindGroupLayout({ entries: this.bindings.map(({ name, ...e }) => e) });
        this.pipelineLayout = d.createPipelineLayout({ bindGroupLayouts: [layout] });
        this.pipelines = {};
        const t0 = performance.now();
        await Promise.all(
            Object.entries(this.kernels).map(async ([name, code]) => {
                const module = d.createShaderModule({ code, label: name });
                this.pipelines[name] = await d.createComputePipelineAsync({
                    layout: this.pipelineLayout,
                    compute: { module, entryPoint: 'computeMain' },
                    label: name,
                });
            })
        );
        this.pipelineCompileMs = performance.now() - t0;

        this.resources = this.allocate();
        this.writeTerrain();
        this.packSettings();
        this.bindGroup = d.createBindGroup({
            layout,
            entries: this.bindings.map(({ binding, name }) => {
                const r = this.resources[name];
                if (!r) throw new Error(`no resource for binding ${binding} (${name})`);
                return { binding, resource: r instanceof GPUBuffer ? { buffer: r } : r.createView() };
            }),
        });

        if (this.hasTimestamps) {
            this.maxQueries = 4096;
            this.querySet = d.createQuerySet({ type: 'timestamp', count: this.maxQueries });
            this.queryResolve = d.createBuffer({ size: this.maxQueries * 8, usage: GPUBufferUsage.QUERY_RESOLVE | GPUBufferUsage.COPY_SRC });
            this.queryRead = d.createBuffer({ size: this.maxQueries * 8, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
        }
        this.stateRead = d.createBuffer({ size: SIM_STATE_BYTES, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });

        const error = await d.popErrorScope();
        if (error) throw new Error(`validation: ${error.message}`);
    }

    buffer(name, size, usage = GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC) {
        this.bytes[name] = size;
        return this.device.createBuffer({ size, usage, label: name });
    }

    allocate() {
        const d = this.device;
        const c = this.cfg;
        const r = {};
        r.settings = this.buffer('settings', 176, GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
        r.particles = this.buffer('particles', c.particles * PARTICLE_BYTES);
        r.grid = this.buffer('grid', this.nodes * GRID_NODE_BYTES);
        r.state = this.buffer('state', SIM_STATE_BYTES);
        r.density_raster = this.buffer('density_raster', c.rasterRes * c.rasterRes * 4);
        r.column_floor = this.buffer('column_floor', c.gridRes * c.gridRes * 4);
        if (this.schedule === 'v2') {
            r.grid_velocity = this.buffer('grid_velocity', this.nodes * 16);
            r.tile_flags = this.buffer('tile_flags', this.tiles[0] * this.tiles[1] * 4);
        }
        const texture = (name, w, h, format, usage) => {
            this.bytes[name] = w * h * 4;
            return d.createTexture({ size: [w, h], format, usage, label: name });
        };
        r.output_texture = texture('output_texture', c.rasterRes, c.rasterRes, 'rgba8unorm', GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING);
        r.height_texture = texture('height_texture', c.heightTexels, c.heightTexels, 'r32float', GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST);
        r.release_point_texture = texture('release_point_texture', c.heightTexels, c.heightTexels, 'rgba8unorm', GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST);
        return r;
    }

    // Solver-owned GPU memory, i.e. what MpmSolverNode allocates (inputs excluded).
    solverBytes() {
        const inputs = new Set(['height_texture', 'release_point_texture']);
        return Object.entries(this.bytes).reduce((sum, [k, v]) => sum + (inputs.has(k) ? 0 : v), 0);
    }

    writeTerrain() {
        const c = this.cfg;
        const n = c.heightTexels;
        const heights = new Float32Array(n * n);
        for (let row = 0; row < n; row++) {
            for (let col = 0; col < n; col++) {
                // texel centre -> region metres; row 0 is north (v grows with -y)
                const x = ((col + 0.5) / n) * c.regionSize;
                const y = (1 - (row + 0.5) / n) * c.regionSize;
                heights[row * n + col] = terrainHeight(x, y, c.regionSize);
            }
        }
        this.device.queue.writeTexture({ texture: this.resources.height_texture }, heights, { bytesPerRow: n * 4 }, [n, n]);
        const release = new Uint8Array(n * n * 4).fill(255);
        this.device.queue.writeTexture({ texture: this.resources.release_point_texture }, release, { bytesPerRow: n * 4 }, [n, n]);
    }

    // Port of MpmSolverNode::update_gpu_settings().
    packSettings() {
        const c = this.cfg;
        const m = MATERIALS[c.model];
        const buf = new ArrayBuffer(176);
        const f = new Float32Array(buf);
        const u = new Uint32Array(buf);
        const dx = c.domainSize / c.gridRes;
        const origin = (c.regionSize - c.domainSize) / 2;
        u[0] = c.gridRes; u[1] = c.gridRes; u[2] = c.gridLayers; u[3] = c.particles;
        f[4] = origin; f[5] = origin; f[6] = c.domainSize; f[7] = dx;
        f[8] = c.regionSize; f[9] = c.regionSize; u[10] = c.heightTexels; u[11] = c.heightTexels;
        f[12] = c.dt; f[13] = 9.81; f[14] = 1.0; f[15] = 1.0 / c.density;
        const nu = m.nu, E = m.E;
        f[16] = E / (2 * (1 + nu)); f[17] = (E * nu) / ((1 + nu) * (1 - 2 * nu));
        f[18] = 10.0; f[19] = 2.5e-2;
        f[20] = 7.5e-3; f[21] = c.mu; f[22] = c.slabThickness; u[23] = c.seed;
        u[24] = c.rasterRes; u[25] = c.rasterRes; f[26] = origin / c.regionSize; f[27] = origin / c.regionSize;
        f[28] = c.domainSize / c.regionSize; f[29] = c.domainSize / c.regionSize; f[30] = 1.0;
        const metresPerTexel = c.domainSize / c.rasterRes;
        const radiusTexels = Math.min(Math.max(c.splatRadius / metresPerTexel, 0.5), 8);
        f[31] = radiusTexels;
        const releaseRadius = Math.min(Math.max(c.releaseRadius, 1), c.domainSize * 0.4);
        const releaseTexels = Math.max(releaseRadius / metresPerTexel, 1);
        f[32] = Math.max((c.particles * Math.PI * radiusTexels ** 2) / (6 * Math.PI * releaseTexels ** 2), 1);
        // Release disc just below the top of the slope, on the gully axis - clamped into the
        // domain with a margin, like the node does.
        const margin = releaseRadius + 2 * dx;
        const clampToDomain = (v) => Math.min(Math.max(v, origin + margin), origin + c.domainSize - margin);
        f[33] = clampToDomain(c.regionSize * 0.5); f[34] = clampToDomain(c.regionSize * 0.5 + 1550); f[35] = releaseRadius;
        u[36] = m.id; u[37] = c.friction === 'voellmy' ? 1 : 0; f[38] = 4000;
        const sinPhi = Math.sin(((m.phi ?? 30) * Math.PI) / 180);
        f[39] = (Math.sqrt(2 / 3) * 2 * sinPhi) / (3 - sinPhi);
        f[40] = m.M ?? 0.7; f[41] = m.beta ?? 0.2; f[42] = m.xi ?? 0.002; f[43] = m.p0 ?? 3000;
        this.device.queue.writeBuffer(this.resources.settings, 0, buf);
        this.dx = dx;
    }

    writeInitialState() {
        const s = new Int32Array(SIM_STATE_BYTES / 4);
        s[0] = 0x7fffffff; s[1] = -0x80000000;
        this.device.queue.writeBuffer(this.resources.state, 0, s);
    }

    resetRunCounters() {
        this.device.queue.writeBuffer(this.resources.state, 12, new Uint32Array(11));
    }

    workgroups() {
        const c = this.cfg;
        const div = (a, b) => Math.ceil(a / b);
        return {
            particle: [div(c.particles, 256), 1, 1],
            gridV1: [div(c.gridRes, 4), div(c.gridRes, 4), div(c.gridLayers, 4)],
            gridV2: [this.tiles[0], this.tiles[1], 1],
            clear: [div(this.nodes, 256), 1, 1],
            prepare: [div(c.gridRes, 16), div(c.gridRes, 16), 1],
            raster: [div(c.rasterRes, 16), div(c.rasterRes, 16), 1],
        };
    }

    // One run = the node's run_impl + all chunks. Returns per-stage GPU ms and wall ms.
    async run(reset) {
        const d = this.device;
        const c = this.cfg;
        const wg = this.workgroups();
        const stages = []; // [label, pipeline, workgroups]
        const substep = () => {
            if (this.schedule === 'v1') {
                stages.push(['clear', 'mpm_clear_grid', wg.clear]);
                stages.push(['p2g', 'mpm_p2g', wg.particle]);
                stages.push(['grid_update', 'mpm_grid_update', wg.gridV1]);
                stages.push(['g2p', 'mpm_g2p', wg.particle]);
            } else {
                stages.push(['p2g', 'mpm_p2g', wg.particle]);
                stages.push(['grid_update', 'mpm_grid_update', wg.gridV2]);
                stages.push(['g2p', 'mpm_g2p', wg.particle]);
            }
        };

        if (reset) this.writeInitialState();
        else this.resetRunCounters();

        let queryIndex = 0;
        const labels = [];
        const t0 = performance.now();
        let left = c.substepsPerRun;
        let first = true;
        while (left > 0) {
            const count = Math.min(left, c.substepsPerSubmit);
            left -= count;
            stages.length = 0;
            if (first && reset) {
                stages.push(['prepare', 'mpm_prepare', wg.prepare]);
                stages.push(['seed', 'mpm_seed', wg.particle]);
            }
            for (let i = 0; i < count; i++) substep();
            if (left === 0) {
                stages.push(['splat', 'mpm_splat', wg.particle]);
                stages.push(['rasterize', 'mpm_rasterize', wg.raster]);
            }

            const encoder = d.createCommandEncoder();
            if (first) {
                encoder.clearBuffer(this.resources.density_raster);
                if (reset) {
                    encoder.clearBuffer(this.resources.grid);
                    if (this.resources.tile_flags) encoder.clearBuffer(this.resources.tile_flags);
                }
            }
            const perStage = c.timing === 'stages' && this.hasTimestamps;
            const timestamps = (begin) => ({ querySet: this.querySet, beginningOfPassWriteIndex: begin, endOfPassWriteIndex: begin + 1 });
            if (perStage) {
                for (const [label, pipeline, groups] of stages) {
                    const pass = encoder.beginComputePass({ timestampWrites: timestamps(queryIndex) });
                    labels.push(label);
                    queryIndex += 2;
                    pass.setBindGroup(0, this.bindGroup);
                    pass.setPipeline(this.pipelines[pipeline]);
                    pass.dispatchWorkgroups(...groups);
                    pass.end();
                }
            } else {
                const pass = encoder.beginComputePass(this.hasTimestamps ? { timestampWrites: timestamps(queryIndex) } : {});
                if (this.hasTimestamps) { labels.push('chunk'); queryIndex += 2; }
                pass.setBindGroup(0, this.bindGroup);
                for (const [, pipeline, groups] of stages) {
                    pass.setPipeline(this.pipelines[pipeline]);
                    pass.dispatchWorkgroups(...groups);
                }
                pass.end();
            }
            if (left === 0) encoder.copyBufferToBuffer(this.resources.state, 0, this.stateRead, 0, SIM_STATE_BYTES);
            d.queue.submit([encoder.finish()]);
            first = false;
        }

        await d.queue.onSubmittedWorkDone();
        const wallMs = performance.now() - t0;

        const gpu = {};
        if (this.hasTimestamps && queryIndex > 0) {
            const encoder = d.createCommandEncoder();
            encoder.resolveQuerySet(this.querySet, 0, queryIndex, this.queryResolve, 0);
            encoder.copyBufferToBuffer(this.queryResolve, 0, this.queryRead, 0, queryIndex * 8);
            d.queue.submit([encoder.finish()]);
            await this.queryRead.mapAsync(GPUMapMode.READ, 0, queryIndex * 8);
            const t = new BigInt64Array(this.queryRead.getMappedRange(0, queryIndex * 8).slice(0));
            this.queryRead.unmap();
            labels.forEach((label, i) => {
                const ms = Number(t[2 * i + 1] - t[2 * i]) / 1e6;
                gpu[label] = (gpu[label] ?? 0) + Math.max(ms, 0);
            });
        }
        return { wallMs, gpu, state: await this.readState() };
    }

    async readState() {
        await this.stateRead.mapAsync(GPUMapMode.READ);
        const raw = new Uint32Array(this.stateRead.getMappedRange().slice(0));
        this.stateRead.unmap();
        const i32 = new Int32Array(raw.buffer);
        const wide = (lo) => raw[lo + 1] * 4294967296 + raw[lo];
        const active = Math.max(raw[2], 1);
        return {
            activeParticles: raw[2],
            maxSpeed: raw[3] / 1000,
            plasticParticles: raw[4],
            minAltitude: i32[0] / 100,
            maxAltitude: i32[1] / 100,
            com: [wide(6) / 1e4 / active, wide(8) / 1e4 / active, wide(10) / 1e4 / active],
            meanSpeedSq: wide(12) / 1e5 / active,
        };
    }

    // Full benchmark: reset, warm-up, measured runs. Also records the physics fingerprint so a
    // speed-up can be shown not to have changed the result.
    async benchmark(onProgress = () => {}) {
        const c = this.cfg;
        const trace = [];
        let simTime = 0;
        const record = (r) => {
            simTime += c.dt * c.substepsPerRun;
            trace.push({ t: +simTime.toFixed(4), com: r.state.com.map((v) => +v.toFixed(3)), meanSpeed: +Math.sqrt(r.state.meanSpeedSq).toFixed(4), maxSpeed: r.state.maxSpeed });
        };
        record(await this.run(true));
        for (let i = 1; i < c.warmupRuns; i++) record(await this.run(false));
        const runs = [];
        for (let i = 0; i < c.runs; i++) {
            const r = await this.run(false);
            record(r);
            runs.push(r);
            onProgress(i + 1, c.runs);
        }
        const mean = (xs) => xs.reduce((a, b) => a + b, 0) / Math.max(xs.length, 1);
        const median = (xs) => { const s = [...xs].sort((a, b) => a - b); return s.length ? s[Math.floor(s.length / 2)] : 0; };
        const stageNames = [...new Set(runs.flatMap((r) => Object.keys(r.gpu)))];
        const stagesMs = Object.fromEntries(stageNames.map((s) => [s, mean(runs.map((r) => r.gpu[s] ?? 0))]));
        const gpuMs = Object.values(stagesMs).reduce((a, b) => a + b, 0);
        const wall = runs.map((r) => r.wallMs);
        const last = runs[runs.length - 1]?.state;
        return {
            config: c,
            schedule: this.schedule,
            pipelineCompileMs: +this.pipelineCompileMs.toFixed(1),
            perRun: {
                wallMsMedian: +median(wall).toFixed(2),
                wallMsMean: +mean(wall).toFixed(2),
                gpuMs: +gpuMs.toFixed(2),
                stagesMs: Object.fromEntries(Object.entries(stagesMs).map(([k, v]) => [k, +v.toFixed(3)])),
            },
            perSubstepGpuMs: +(gpuMs / c.substepsPerRun).toFixed(3),
            simSecondsPerWallSecond: +((c.dt * c.substepsPerRun) / (median(wall) / 1000)).toFixed(3),
            memory: {
                solverBytes: this.solverBytes(),
                buffers: { ...this.bytes },
                jsHeapBytes: performance.memory ? performance.memory.usedJSHeapSize : null,
            },
            physics: {
                activeParticles: last?.activeParticles,
                plasticRatio: last ? +(last.plasticParticles / Math.max(last.activeParticles, 1)).toFixed(4) : null,
                finalCom: last?.com.map((v) => +v.toFixed(3)),
                trace,
            },
        };
    }

    destroy() {
        for (const r of Object.values(this.resources ?? {})) r.destroy?.();
        this.querySet?.destroy();
        this.queryResolve?.destroy();
        this.queryRead?.destroy();
        this.stateRead?.destroy();
    }
}

export async function requestDevice() {
    if (!navigator.gpu) throw new Error('WebGPU not available in this browser');
    const adapter = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
    if (!adapter) throw new Error('no WebGPU adapter');
    const features = adapter.features.has('timestamp-query') ? ['timestamp-query'] : [];
    const device = await adapter.requestDevice({
        requiredFeatures: features,
        requiredLimits: {
            maxStorageBufferBindingSize: adapter.limits.maxStorageBufferBindingSize,
            maxBufferSize: adapter.limits.maxBufferSize,
            maxStorageBuffersPerShaderStage: Math.min(adapter.limits.maxStorageBuffersPerShaderStage, 10),
        },
    });
    const info = adapter.info ?? {};
    return { device, adapterInfo: { vendor: info.vendor, architecture: info.architecture, device: info.device, description: info.description } };
}
