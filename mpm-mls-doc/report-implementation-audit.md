# MPM implementation audit

**Scope.** This report answers issue [#3](https://github.com/Emanum/webigeo/issues/3):
compare the MPM documentation and implementation with the proposal and the cited papers,
identify material deviations, and separate verified behaviour from remaining work. The
audit was performed against the WGSL and C++ sources on the current `main` branch and the
offline checks in [`scripts/`](scripts/). It is an implementation audit, not a claim that
the solver is a validated avalanche forecast model.

**AI assistance.** Prepared with GitHub Copilot CLI in **Auto mode**. The interface
displayed **GPT-5.6 Luna** for this session; Auto mode may select the underlying model
dynamically.

## Executive result

The core solver is a coherent MLS-MPM/APIC implementation. The quadratic B-spline stencil,
`4/dx²` MLS stress factor, fixed-point P2G accumulation, grid update, G2P affine velocity
gradient, deformation update, and advection are wired in the expected order. The
Stomakhin model is a close implementation of the 2013 snow model. Drucker–Prager and
Cohesive Cam Clay are useful, clearly isolated alternatives, but they are not equivalent
to every aspect of the cited papers:

| Area | Finding | Assessment |
|---|---|---|
| MLS-MPM/APIC | `mpm_p2g.wgsl` and `mpm_g2p.wgsl` match the Hu et al. transfer structure and keep world/grid-unit offsets consistent. | **Matches** |
| Stomakhin snow | Fixed-corotated stress, singular-value plastic clamp, and exponential `Jp` hardening are present. | **Matches, with numerical clamps** |
| Drucker–Prager | Hencky elasticity and the three return cases are present; friction-angle hardening from Klár §5.4 is intentionally absent. | **Partial** |
| Cohesive Cam Clay | Gaume’s yield surface and hardening variables are present, but return mapping is Wolper’s explicit fixed-`p` non-associated projection. | **Partial, explicitly documented** |
| Basal friction | Coulomb contact is separate from constitutive stress; Voellmy adds a grid-level quadratic drag. | **Matches the selected boundary model, not an MPM material law** |
| Li et al. regimes | Five CCC parameter sets and a Drucker–Prager fallback are exposed as presets. | **Parameter mapping, not calibration** |
| Avalanche validation | No measured avalanche or independent com1DFA/Flow-Py comparison is included. | **Open** |

## 1. Solver and transfer formulation

The implementation follows the intended four-stage MPM loop, with grid clearing folded
into grid update:

1. `mpm_p2g.wgsl` scatters particle mass and momentum to the 3×3×3 quadratic
   B-spline stencil.
2. `mpm_grid_update.wgsl` converts momentum to velocity, applies gravity and terrain/domain
   boundary conditions, and clears the fixed-point accumulators.
3. `mpm_g2p.wgsl` gathers velocity and constructs the APIC affine matrix.
4. The same G2P pass updates `F`, applies the selected return mapping, and advects particles.

The P2G stress term is

```text
-dt * V0 * (4/dx^2) * (P F^T)
```

and G2P uses `4/dx` with the offset in grid units. This is the important MLS-MPM unit
asymmetry from Hu et al.; the source does not accidentally use world units in both places.
The fixed-point atomic representation is an engineering adaptation required by WGSL, not a
change to the continuum equations. The production-scale regression in
[`06-verification.md`](06-verification.md) is important: truncation previously injected
energy through the ratio of quantised momentum to mass, and rounding plus the 64-bit mass
pair fixed that failure.

**Conclusion:** the transfer implementation is consistent with Hu et al. and the
documented `mls-mpm88` reference. The remaining numerical risk is resolution: the default
12–16 m cells can make a 1.5 m slab sub-cell, so a visually fluid result is not evidence
that the material law is wrong.

### 1.1 Particle-to-grid interpolation

Hu et al. formulate MLS-MPM using a quadratic B-spline kernel. For one coordinate, with
`x_p / dx = base + fx`, the three weights are

```text
w0(fx) = 0.5 (1.5 - fx)^2
w1(fx) = 0.75 - (fx - 1)^2
w2(fx) = 0.5 (fx - 0.5)^2
```

The implementation constructs the same three-dimensional tensor-product kernel in
`mpm_common.wgsl`. `compute_kernel()` computes `base = floor(grid_pos - 0.5)` and
`fx = grid_pos - base`; `kernel_weight()` evaluates the one-dimensional weights and
multiplies the three axes (`webgpu/compute/shaders/mpm_common.wgsl:338-348`). This is
not merely an approximate nearest-node transfer: every active particle contributes to up
to 27 nodes, and the same weights are used by P2G and G2P.

The P2G code then evaluates, for each stencil node `i`,

```text
m_i       += w_ip m_p
p_i       += w_ip [m_p v_p + (m_p C_p - dt V_p (4/dx^2) tau_p) dpos_ip]
dpos_ip   = (x_i - x_p) in world units
tau_p     = P_p F_p^T
```

The corresponding source is `mpm_p2g.wgsl:28-64`, specifically:

```wgsl
let stress_term = -settings.dt * settings.particle_volume
    * (4.0 * inv_dx * inv_dx) * particle_kirchhoff(p);
let affine = stress_term + p.mass * particle_c(p);
...
let dpos = (vec3f(offset) - k.fx) * settings.dx;
let momentum = weight * (p.mass * p.velocity + affine * dpos);
```

The first term in `affine` is the MLS internal-force contribution and the second is the
APIC affine momentum contribution. The sign and `dt V` factor are important: omitting the
negative sign would turn the internal stress into an energy source. The source paper is
Hu et al. 2018, §4 and the paper’s MLS-MPM update equations ([3] in
[`refs.md`](refs.md)); the repository also identifies the `mls-mpm88` implementation as
the practical reference.

G2P uses

```text
v_p^{n+1} = sum_i w_ip v_i
C_p^{n+1} = (4/dx) sum_i w_ip (v_i outer dpos_ip)
```

where `dpos_ip` is now in grid units. This is implemented at
`mpm_g2p.wgsl:43-47`:

```wgsl
let dpos = vec3f(offset) - k.fx; // in grid units
new_velocity += weight * node_velocity;
new_c += 4.0 * inv_dx * weight * outer_product(node_velocity, dpos);
```

The apparent mismatch between world-unit `dpos` in P2G and grid-unit `dpos` in G2P is
intentional. The `dx` in the P2G stress term and `1/dx` in G2P make the two expressions
dimensionally consistent. This is one of the easiest places to accidentally implement a
PIC-like method that appears to work but has the wrong stiffness and angular-momentum
response.

### 1.2 Deformation update and time integration

After G2P, the APIC velocity gradient is used as the incremental deformation map:

```text
F_trial = (I + dt C_p^{n+1}) F_p^n
```

The exact implementation is `mpm_g2p.wgsl:51-57`:

```wgsl
let f_trial = (identity3() + settings.dt * new_c) * particle_f(p);
let plastic = material_plasticity(f_trial, p.plastic_state);
store_f(&p, plastic.f_elastic);
p.plastic_state = plastic.plastic_state;
store_kirchhoff(&p, plastic.kirchhoff);
```

This is an explicit first-order update. It is not a Newton solve for the full momentum
equations, nor is it a multiplicative exponential update such as `F = exp(dt C) F`.
Therefore the CFL warning and the fixed-point tests are part of the method’s validity
envelope, not optional UI concerns. The particle is then advected explicitly,
`x^{n+1} = x^n + dt v^{n+1}` (`mpm_g2p.wgsl:60-61`).

The four-stage interpretation is consistent with Sulsky et al. [2], while the particular
MLS/APIC transfer is Hu et al. [3]. The implementation fuses G2P, deformation, plasticity,
and advection into one dispatch; fusion changes scheduling, not the mathematical sequence.

### 1.3 Normalised mass and fixed-point arithmetic

The continuum equations are invariant under a common mass/volume scale if the particle
mass and volume are changed consistently. This implementation sets particle mass to one
and uses `particle_volume = 1 / density`, as documented in
`mpm_common.wgsl:29-31` and `MpmSolverNode::update_gpu_settings()`. This is an
engineering normalisation; it means the simulation’s absolute total mass is not directly
the physical mass of the released slab unless the scale is restored externally.

WGSL does not provide atomic floating-point addition, so P2G uses fixed-point atomics:

* momentum uses `MOMENTUM_SCALE = 10^4` in an `i32`;
* mass uses `MASS_SCALE = 2^20` and a software 64-bit `(lo, hi)` pair;
* `to_fixed()` rounds and clamps momentum;
* `add_node_mass()` detects carry from `atomicAdd()`.

These details are implemented at `mpm_common.wgsl:161-201`. The design is not in Hu’s
continuum equations and must therefore be assessed as a discretisation error source. The
earlier truncation bug demonstrated why: quantising mass and momentum differently biased
`momentum / mass` upward at low-weight nodes, and APIC fed that bias back into `C`. The
device-scale sheet regression in `scripts/test_sheet_fixed_point.py` is consequently more
relevant than a float-only toy test.

## 1.4 Numerical and geometric conventions

The solver uses a right-handed world convention with `x/y` as region-relative horizontal
metres and `z` as absolute altitude. The DEM is sampled as a non-filterable `R32Float`
texture using four `textureLoad()` calls and bilinear interpolation
(`mpm_common.wgsl:218-239`). The upward terrain normal is

```text
n = normalize(-dh/dx, -dh/dy, 1)
```

from central differences at `h = max(dx, 1 m)`, exactly represented by
`terrain_normal()` (`mpm_common.wgsl:243-247`).

This terrain treatment is an application-specific coupling, not part of the MPM equations
in Hu, Stomakhin, Klár, or Gaume. The papers assume a prescribed boundary or rigid
collision model; this repository adds a sampled geographic surface and a terrain-following
band grid. That distinction matters when attributing observed runout or energy loss to a
paper model.

## 2. Constitutive models

### 2.1 Stomakhin et al. (2013)

`mpm_material_stomakhin.wgsl` implements:

```text
F = U Sigma V^T
Sigma_hat_i = clamp(Sigma_i, 1-theta_c, 1+theta_s)
Jp_new = Jp * product(Sigma_i / Sigma_hat_i)
h = exp(xi * (1 - Jp_new))
P F^T = 2 mu h (F - R) F^T + lambda h J (J - 1) I
```

This matches the fixed-corotated elasticity, singular-value box projection, and
compaction hardening described by Stomakhin et al. The implementation correctly computes
stress from the returned elastic state rather than the unprojected trial state.

There are two deliberate numerical deviations:

* `Jp` is clamped to `[0.05, 20]`, and hardening is clamped to `[0.05, 20]`; these bounds
  prevent overflow/degenerate states but are not part of the ideal equations.
* The SVD path clamps very small/negative singular values before taking logarithms in the
  Hencky models. Inverted or badly degenerate elements are therefore stabilised as extreme
  compression rather than given a physically complete inversion treatment.

The offline drop test demonstrates compaction and settling, but it does not validate a
fracture pattern or a snow-mechanics measurement. The paper parameters were authored for
metre-scale film snow and should not be treated as calibrated at the current geographic
cell size.

#### 2.1.1 Explicit correspondence with Stomakhin’s equations

For the singular-value decomposition `F = U Sigma V^T`, Stomakhin et al. define an elastic
fixed-corotated response and project plastic deformation by clamping singular values. The
code’s return map is directly readable at `mpm_material_stomakhin.wgsl:52-69`:

```wgsl
let svd = svd3(f_trial);
let lo = 1.0 - settings.critical_compression;
let hi = 1.0 + settings.critical_stretch;
let clamped = clamp(svd.sigma, vec3f(lo), vec3f(hi));
let ratio = (svd.sigma.x / clamped.x)
          * (svd.sigma.y / clamped.y)
          * (svd.sigma.z / clamped.z);
result.plastic_state = clamp(jp * ratio, 0.05, 20.0);
result.f_elastic = svd.u * diag3(clamped) * transpose(svd.v);
```

The corresponding stress code at `mpm_material_stomakhin.wgsl:38-49` is the principal-frame
form of

```text
R = U V^T
J = product(sigma_hat_i)
tau = P F^T
    = U diag(2 mu (sigma_hat_i - 1) sigma_hat_i
             + lambda J (J - 1)) U^T
```

The source implements the same expression as:

```wgsl
let h = clamp(exp(settings.hardening * (1.0 - jp)), 0.05, 20.0);
let j = sigma.x * sigma.y * sigma.z;
let deviatoric = 2.0 * mu * (sigma - vec3f(1.0)) * sigma;
let volumetric = lambda * j * (j - 1.0);
return from_principal(u, deviatoric + vec3f(volumetric));
```

This is a useful source-level check because P2G requires `P F^T`, not `P` alone. The
implementation stores `tau = P F^T` in the particle and reuses it in the next P2G pass;
it does not recompute a differently rotated stress there.

The Lamé conversion used by the CPU settings path is the standard isotropic relation:

```text
mu     = E / [2(1 + nu)]
lambda = E nu / [(1 + nu)(1 - 2 nu)]
```

The implementation shares these parameters between models. This is physically meaningful
only when `E` and `nu` are interpreted as the selected model’s elastic parameters; the
Li presets change them because their recommended Cam-Clay stiffness is not Stomakhin’s
film-snow stiffness.

### 2.2 Klár et al. (2016) Drucker–Prager

`mpm_material_drucker_prager.wgsl` uses Hencky strain

```text
epsilon = log(Sigma)
tau = 2 mu epsilon + lambda tr(epsilon) I
||dev(tau)|| + alpha tr(tau) <= 0
```

The three branches have the expected interpretation: tensile expansion returns to the
apex, an admissible state remains elastic, and a yielded compressive/shear state projects
in the deviatoric direction. The dedicated test checks the yield residual, idempotence,
rotation covariance, and hydrostatic compression.

This is not a complete reproduction of Klár’s model because the friction-angle hardening
law is omitted. `plastic_state` is a diagnostic accumulator only and does not feed back into
`alpha` or the yield surface. Consequently this model should be described as a
cohesionless, fixed-friction Drucker–Prager fallback, not as the full hardening model.
It is also not a substitute for cohesive snow: it cannot represent tensile slab failure,
cohesion, or the Cam-Clay consolidation cap.

#### 2.2.1 Explicit return-map derivation

The code computes the principal Hencky strain and its volumetric/deviatoric split in
`mpm_material_drucker_prager.wgsl:55-64`:

```wgsl
let eps = dp_hencky_strain(svd.sigma);
let trace = eps.x + eps.y + eps.z;
let dev = eps - vec3f(trace / 3.0);
let dev_norm = length(dev);
```

For Hencky elasticity,

```text
tau_i = 2 mu epsilon_i + lambda tr(epsilon)
```

and Klár’s Drucker–Prager surface is represented in the source comments and implementation
as

```text
f(tau) = ||dev(tau)|| + alpha tr(tau) <= 0.
```

The coefficient is calculated from the friction angle using

```text
alpha = sqrt(2/3) * 2 sin(phi) / (3 - sin(phi)),
```

with `dp_alpha` precomputed by the C++ settings code and uploaded as a uniform. This is
the cone convention used by Klár et al. §5.3; it should not be confused with Cam-Clay’s
critical-state slope `M`, although the UI documentation supplies the conversion
`sin(phi) = 3M/(6+M)` when a Li regime is used as a DP fallback.

For `tr(epsilon) > 0`, the code executes the cohesionless tension branch:

```wgsl
new_eps = vec3f(0.0);
delta_gamma = length(eps);
```

This is the apex return. For compression, it evaluates the closed-form projection distance:

```wgsl
delta_gamma = dev_norm
    + (3.0 * settings.lambda_0 + 2.0 * settings.mu_0)
      / (2.0 * settings.mu_0) * trace * settings.dp_alpha;
```

If `delta_gamma <= 0`, the trial point is inside the cone. Otherwise:

```wgsl
new_eps = eps - delta_gamma * dev / dev_norm;
```

This is the deviatoric return of Klár §5.3. The implementation’s guard is deliberately on
the sign of `trace`, not on `dev_norm == 0`: pure hydrostatic compression has zero
deviatoric strain and must remain elastic. This exact regression is covered by
`scripts/test_material_dp.py:104-110`.

What the implementation does **not** contain is Klár §5.4’s evolution of the friction
angle. `plastic_state` is updated at `mpm_material_drucker_prager.wgsl:84`, but the value
is not used to update `dp_alpha`; it is a yield diagnostic. Therefore the implementation
is mathematically the fixed-cone return map, not the complete hardening model.

### 2.3 Gaume et al. (2018) Cohesive Cam Clay

The code computes the documented ellipse:

```text
p = -K tr(epsilon)
q = sqrt(3/2) * 2 mu ||dev(epsilon)||
y = (1 + 2 beta) q^2 + M^2 (p + beta p0)(p - p0)
p0 = K sinh(xi max(-alpha, 0))
```

The initial `alpha` is back-calculated so that the configured `p0` is reproduced exactly.
The three return branches are:

* compressive cap: `(p, q) -> (p0, 0)` and compaction hardening;
* tensile tip: `(p, q) -> (-beta p0, 0)` and softening;
* shear: reduce `q` at fixed `p`, with no volumetric hardening.

The key qualification is that Gaume et al. describe an associative flow rule, while this
implementation deliberately uses the explicit three-case, fixed-pressure projection from
Wolper et al. (2019). That choice is computationally appropriate for a real-time shader,
but it changes the plastic strain direction and therefore the evolution of `alpha`, flow
dilation, and post-yield response. The report and UI should continue to call this
**Cohesive Cam Clay with Wolper/NACC return mapping**, rather than implying an exact
associative Gaume implementation.

The `p0` argument is clamped before `sinh`; this is a safe finite-precision guard, not a
literature constitutive rule. The fixed-`p` shear branch also does not harden, by design.
If associative Cam Clay becomes a research requirement, the return mapping needs a
per-particle nonlinear solve or a separately validated approximation.

#### 2.3.1 Explicit Cam-Clay correspondence

The source’s `ccc_p0()` is a direct implementation of the consolidation-pressure law:

```wgsl
fn ccc_p0(alpha: f32) -> f32 {
    let arg = clamp(settings.ccc_xi * max(-alpha, 0.0), 0.0, 20.0);
    return ccc_bulk_modulus() * sinh(arg);
}
```

At `mpm_material_ccc.wgsl:60-64`, this corresponds to

```text
K       = lambda + 2 mu / 3
p0(alpha) = K sinh(xi max(-alpha, 0)).
```

The initial state is not arbitrarily set to zero. `ccc_initial_state()` solves the inverse
relation:

```text
alpha_0 = -asinh(p0_initial / K) / xi
```

so a newly seeded particle starts on the configured ellipse. This is verified in
`scripts/test_material_ccc.py:113-117` for the configured initial pressure and again for
all five Li et al. regimes.

The trial invariants at `mpm_material_ccc.wgsl:80-100` are:

```wgsl
let p = -k * trace;
let q = sqrt(1.5) * 2.0 * mu * dev_norm;
let y = (1.0 + 2.0 * beta) * q * q
      + m * m * (p + beta * p0) * (p - p0);
```

Thus the code uses compressive-positive pressure `p`, and the admissible pressure interval
is `[-beta p0, p0]`. This sign convention differs from papers that take tension-positive
mean stress; comparing plots or equations without converting signs can make a correct
implementation appear inverted.

The cap branch is:

```wgsl
if p > p0 {
    new_eps = vec3f(-p0 / (3.0 * k));
    new_alpha = alpha + (trace + p0 / k);
}
```

The returned trace is `-p0/k`, so the returned pressure is exactly `p0`, while the
deviatoric strain is discarded (`q = 0`). The removed volumetric strain is added to
`alpha`; because compression has negative `trace`, `alpha` decreases and `p0` increases.

The tensile branch is:

```wgsl
else if p < -beta * p0 {
    new_eps = vec3f(beta * p0 / (3.0 * k));
    new_alpha = alpha + (trace - beta * p0 / k);
}
```

The returned trace is `beta*p0/k`, hence `p = -beta*p0`, and the state softens. This is
the implementation’s tensile fracture/granulation mechanism; it is not a separate damage
variable or a crack topology operation.

For shear failure inside the pressure interval, the source keeps the trial `p` and computes

```wgsl
let q_new = m * sqrt(max(
    (p + beta * p0) * (p0 - p) / (1.0 + 2.0 * beta), 0.0));
new_eps = vec3f(trace / 3.0) + dev * (q_new / max(q, 1e-12));
```

This is the fixed-pressure projection onto the ellipse. It is the key Wolper/NACC
approximation: no plastic volumetric increment is generated in Case 3, so `alpha` and
`p0` are unchanged by shear alone. Gaume’s associative flow rule would generally move in
both the pressure and equivalent-stress directions according to the yield-surface normal.
The implementation therefore matches the selected Wolper return algorithm, while matching
Gaume’s surface and hardening variables.

### 2.4 Runtime dispatch and state semantics

The model dispatcher makes the material choice a runtime uniform branch, not a compile-time
shader variant. `mpm_material.wgsl:40-78` calls the model-specific initial-state and
plasticity functions. The one scalar `Particle.plastic_state` has three different meanings:

| Model | State meaning | Initial value | Feeds back into stress/yield? |
|---|---|---:|---|
| Stomakhin | plastic volume ratio `Jp` | `1` | Yes, through hardening |
| Drucker–Prager | accumulated projection distance | `0` | No, diagnostic only |
| Cam Clay | plastic volumetric strain `alpha` | inverse-mapped from `p0` | Yes, through `p0(alpha)` |

This is why switching models without reseeding would be invalid: the same bit pattern would
change physical meaning. The C++ panel forces reset on a model change. That behaviour is
an implementation invariant, not a paper equation, and should remain part of any future
model-dispatch refactor.

## 3. Boundary conditions and avalanche-specific additions

The code correctly keeps basal friction separate from the constitutive model. Grid-level
terrain contact applies Coulomb friction; particle-level correction prevents particles
from passing between grid nodes. The Voellmy option adds quadratic tangential drag only at
the grid contact call, avoiding double application at the particle call.

These are boundary-model choices, not consequences of the cited MPM material papers.
The Voellmy term uses `slab_thickness` as a reference depth and therefore inherits a
depth-averaged avalanche-model convention. It should not be presented as a 3-D contact
derivation from Tonnel et al. The energy-line test is a useful force-balance regression,
but a low residual on a synthetic plane does not validate terrain roughness, entrainment,
or runout.

The geographic coupling is functional: the solver samples the decoded DEM, estimates
terrain normals, follows a terrain-relative grid band, and exposes diagnostics. The
proposal’s intended 3-D depth-tested particle renderer is still absent; the current
output is a top-down density raster.

### 3.1 Coulomb contact: equation-to-code comparison

For a velocity `v` and outward terrain normal `n`, the implementation decomposes

```text
vn = v dot n
vt = v - vn n.
```

When `vn < 0`, the Coulomb impulse removes tangential speed by `mu (-vn)`. If the
tangential speed is smaller than that impulse, the node sticks; otherwise it slides:

```text
|vt| <= -mu vn       -> v_new = 0
otherwise            -> v_new = vt (1 + mu vn / |vt|).
```

This is exactly `coulomb_friction()` at `mpm_friction.wgsl:42-51`. The separating case
`vn >= 0` is handled by `resolve_terrain_collision()` at lines 76-88. The call sites are
intentionally different:

* `mpm_grid_update.wgsl:57-60` applies contact to grid nodes below the DEM;
* `mpm_g2p.wgsl:64-69` clamps a particle that crossed the interpolated surface.

The second call is a geometric correction for particles between grid nodes. It is not a
second independent basal-friction law: after the grid response, the normal component is
approximately zero, so the Coulomb impulse is normally zero or small.

### 3.2 Voellmy extension

Tonnel et al. [the Voellmy reference in `refs.md`] use the depth-integrated resistance

```text
tau = mu sigma_n + rho g |v|^2 / xi.
```

The implementation converts the turbulent term to a tangential deceleration:

```wgsl
let reference_depth = max(settings.slab_thickness, 0.1);
let deceleration = settings.gravity * speed * speed
    / (settings.voellmy_xi * reference_depth);
let new_speed = max(speed - deceleration * settings.dt, 0.0);
```

This is `mpm_friction.wgsl:58-73`. It is a depth-averaged avalanche closure coupled to a
3-D MPM boundary, not a constitutive law derived from the MPM papers. The code applies the
quadratic drag only on the grid-level call (`apply_basal_drag = true`); applying it again
after G2P would double count a velocity-dependent force. The analytic steady speed on a
constant slope is

```text
v_infinity = sqrt(xi h (sin(theta) - mu cos(theta))),
```

and `scripts/test_friction.py` checks this against the discrete implementation.

### 3.3 Terrain-following grid versus a dense Eulerian grid

The papers describe a background grid, but do not prescribe this repository’s terrain
band layout. `grid_slot()` in `mpm_common.wgsl:272-292` stores only `grid_res.z` layers
above a per-column terrain floor. A stencil node outside the stored band is skipped.
This reduces memory and work for kilometre-scale geographic domains, but it introduces a
model-specific boundary: particles are capped at the top of the band in
`mpm_g2p.wgsl:77-84`.

Consequently, a flow reaching the layer ceiling is not equivalent to an unbounded MPM
domain. Any quantitative runout study must show that the ceiling is inactive or repeat the
run with more layers.

## 4. Verification status

### Verified or meaningfully checked

* WGSL validation for the eight kernels with the include preprocessor resolved.
* SVD reconstruction, orthogonality, near-degenerate matrices, and inverted matrices
  against NumPy.
* Offline mass conservation, free fall, settling, and Stomakhin compaction.
* Drucker–Prager yield residuals and return-map properties over random inputs.
* Cam-Clay initial-state round trips, cap/tip/shear cases, hardening/softening monotonicity,
  and Li 2021 parameter finiteness.
* Coulomb and Voellmy slope checks, including the analytic terminal-velocity result.
* Fixed-point arithmetic at device-scale cell/slab ratios.
* Native shader/pipeline creation and a limited real-terrain diagnostic readback.

### Not validated

* Quantitative comparison with a measured avalanche, com1DFA, Flow-Py, or an independent
  MPM implementation.
* Calibration of the Li et al. presets for this DEM, particle count, grid spacing, or
  release geometry.
* Associative Gaume Cam-Clay response.
* Entrainment, snow-depth evolution from terrain pickup, or mass exchange with the DEM.
* Convergence under grid refinement and particle-count refinement.
* Conservation/error budgets across long real-terrain runs for every material/friction
  combination.

## 4.1 Verification-to-equation matrix

| Claim being tested | Test/evidence | What it proves | What it does not prove |
|---|---|---|---|
| B-spline transfer and APIC loop | `test_mpm.py`, mass/free-fall checks | Transfer ordering and basic momentum response | Convergence or 3-D terrain accuracy |
| SVD and return-map input | `test_svd.py` | Reconstruction and orthogonality over selected matrices | Robustness for all ill-conditioned GPU inputs |
| Stomakhin plasticity | drop test, `jp` decrease | Plastic compaction is active | Correct snow fracture calibration |
| DP yield surface | `test_material_dp.py`, 500 random trials | Outputs satisfy the fixed cone | Klár hardening, real sand/snow calibration |
| CCC yield surface | `test_material_ccc.py`, 500 random trials | Outputs satisfy the selected ellipse | Associative Gaume flow |
| CCC regime mapping | Li parameter round trips | Preset values are finite and internally consistent | Reproduction of Li’s measured avalanche |
| Coulomb/Voellmy | `test_friction.py`, slope solutions | Boundary force signs and terminal-speed law | DEM roughness and 3-D contact accuracy |
| Fixed-point arithmetic | `test_sheet_fixed_point.py` | Production-scale quantisation no longer pumps energy in the tested case | All particle counts, speeds, and geometries |
| Real terrain | limited device readback | End-to-end shader/resource path works | Quantitative runout or measured avalanche agreement |

The distinction is important: a return-map test can establish that `y <= 0` after a
projection, but it cannot establish that the chosen yield surface is the right snow model
or that the discrete flow converges to a physical solution.

## 4.2 Reproducibility details

The strongest offline checks can be rerun without the application:

```bash
cd mpm-mls-doc/scripts
./.venv/bin/python test_material_dp.py
./.venv/bin/python test_material_ccc.py
./.venv/bin/python test_friction.py
./.venv/bin/python test_mpm.py
./.venv/bin/python test_sheet_fixed_point.py
```

The material tests are scalar/vector ports of the WGSL functions, not GPU execution. They
therefore validate formula correspondence and invariants, while `validate_wgsl.py` validates
shader syntax and the runtime pipeline check validates device compilation. A future audit
should add a GPU readback comparison for selected single-particle states so that the Python
ports cannot silently diverge from WGSL through a transcription error.

## 5. Recommended next steps

1. **Keep the model limitations visible.** Rename the CCC documentation and UI tooltip to
   mention Wolper/NACC fixed-pressure return mapping; label DP hardening as not implemented.
2. **Add convergence tests.** Repeat flat-plane and inclined-plane tests at at least two
   `dx` values and particle-per-cell densities, measuring mass, energy-line slope,
   penetration, and runout.
3. **Add a reference comparison.** Export a simple prescribed-slope scenario and compare
   acceleration, terminal speed, deposit/runout, and energy loss against closed-form
   Coulomb/Voellmy solutions and one independent solver.
4. **Treat calibration as separate work.** Tune `E`, `nu`, `M`, `beta`, `xi`, `p0`, basal
   friction, and release thickness at the chosen resolution instead of copying paper
   values directly into claims about physical realism.
5. **Implement entrainment and 3-D rendering only as separate feature work.** They are
   proposal gaps, not bugs in the current constitutive implementation.
6. **Add equation-level GPU golden cases.** Seed one particle or a small fixed set, run one
   substep per material, read back `F`, plastic state, and `tau`, and compare with the
   Python reference at a stated tolerance.
7. **Test boundary activation explicitly.** Include a case that reaches the terrain band
   ceiling, a case that crosses a DEM cell boundary, and a case with `voellmy_xi` disabled
   or very large. Record whether the domain ceiling contributes to the measured runout.
8. **Publish line-specific source references with revisions.** The line ranges in this report
   identify the current working tree; a release should also record the commit SHA because
   shader line numbers will move as kernels evolve.

## Sources

The paper list and source-to-file mapping are maintained in [`refs.md`](refs.md). Relevant
implementation entry points are:

* [`mpm_p2g.wgsl`](../webgpu/compute/shaders/mpm_p2g.wgsl),
  [`mpm_g2p.wgsl`](../webgpu/compute/shaders/mpm_g2p.wgsl), and
  [`mpm_grid_update.wgsl`](../webgpu/compute/shaders/mpm_grid_update.wgsl);
* [`mpm_material_stomakhin.wgsl`](../webgpu/compute/shaders/mpm_material_stomakhin.wgsl);
* [`mpm_material_drucker_prager.wgsl`](../webgpu/compute/shaders/mpm_material_drucker_prager.wgsl);
* [`mpm_material_ccc.wgsl`](../webgpu/compute/shaders/mpm_material_ccc.wgsl);
* [`mpm_friction.wgsl`](../webgpu/compute/shaders/mpm_friction.wgsl);
* [`06-verification.md`](06-verification.md) and
  [`07-constitutive-models.md`](07-constitutive-models.md).
