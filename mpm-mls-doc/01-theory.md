# Theory — what is actually implemented

Only the parts the code uses. Symbols match the shader variable names where possible.

## 1. Why MPM at all

Chain of ideas, shortest possible version:

- **FLIP** (Brackbill & Ruppel [1]) — hybrid particle/grid fluid method. Particles carry the
  state and do the advection (no numerical diffusion from advecting on a grid); a background
  grid solves for forces and pressure. Grid is scratch space, rebuilt every step.
- **MPM** (Sulsky et al. [2]) — extends the same hybrid idea from fluids to solid mechanics.
  Each particle is a *material point* carrying deformation gradient **F**, stress, mass,
  volume. Handles history-dependent materials, which snow very much is.
- **Snow MPM** (Stomakhin et al. [5]) — the reason MPM is the right tool here. Snow needs
  volume preservation, plasticity, fracture and compaction hardening simultaneously. MPM
  handles topology change (fracture, merging) for free because the grid is rebuilt each
  step and particles never need connectivity.
- **MLS-MPM** (Hu et al. [3]) — the performance fix. See §3.

## 2. The base MPM step

Per substep, four stages:

```
1. P2G          scatter particle mass + momentum onto grid nodes
2. grid update  momentum → velocity, apply gravity, resolve collisions
3. G2P          gather updated velocity (and velocity gradient) back to particles
4. advection    move particles by their new velocity
```

The grid is **cleared every substep** — it holds no state between steps. All persistent
state lives on the particles. That is what makes this cheap to implement on the GPU: the
grid is a scratch buffer, particles are the simulation.

Hu et al. [3, §1] note the grid transfers (P2G/G2P) dominate the cost — the proposal quotes
up to 85% — because they are the stages with scattered atomic writes.

## 3. MLS-MPM — the actual formulation used

Standard MPM computes internal force as a weight-gradient sum (∇w terms everywhere).
MLS-MPM (Hu et al. [3]) replaces the B-spline gradient with a **Moving Least Squares**
approximation (Lancaster & Salkauskas [4]) of the velocity field. The practical consequence:
the internal force and the APIC affine momentum collapse into **one affine matrix applied to
the node offset**, so P2G becomes a single matrix-vector product per node — no separate
gradient term, far fewer atomics.

### Interpolation weights

Quadratic B-spline, 3×3×3 node stencil. With particle position in grid units
`gp = (x − origin)/dx`:

```
base = floor(gp − 0.5)
fx   = gp − base                     ∈ [0.5, 1.5)

w₀ = ½ (1.5 − fx)²
w₁ = ¾ − (fx − 1)²
w₂ = ½ (fx − 0.5)²
```

Per-node weight is the product over the three axes. Implemented in `compute_kernel()` /
`kernel_weight()` in `mpm_common.wgsl`.

### P2G

```
stress_term = −Δt · V⁰ · (4/dx²) · (P Fᵀ)
affine      = stress_term + m·C

for each of the 27 nodes i:
    dpos     = (offset − fx) · dx           # world units
    w        = wᵢ
    mass_i  += w · m
    mom_i   += w · (m·v + affine · dpos)
```

The `4/dx²` is the inverse of the quadratic B-spline's second moment — it is what makes the
MLS estimate consistent. This is the form in Hu et al. [3, §4] and matches the reference
`mls-mpm88` implementation.

### Grid update

```
v = mom / mass
v.z −= g·Δt                    # z is altitude
resolve terrain collision      # §5
clamp at domain walls
```

Velocity is written back into the momentum slots, so G2P reads nodes as plain velocities.
Nodes with zero mass are explicitly zeroed so G2P can never gather stale data.

### G2P + advection (APIC)

```
v_new = Σ w · v_i
C_new = Σ 4·(1/dx) · w · (v_i ⊗ dpos)     # dpos in GRID units here, not world
```

Note the asymmetry: `dpos` is multiplied by `dx` in P2G and left in grid units in G2P, with
the `4/dx` factor compensating. Easy to get wrong; both follow the reference formulation.

**C** is the affine velocity matrix from APIC — it carries the local velocity gradient on the
particle, which is what preserves angular momentum and stops the rotational energy loss
plain PIC suffers from.

Then:

```
F_trial = (I + Δt·C_new) · F        # elastic predictor
F, Jp   = plasticity(F_trial, Jp)   # §4
x      += Δt · v_new
```

## 4. Snow constitutive model (Stomakhin et al. [5])

Elastoplastic, with the elastic part a fixed-corotated model and plasticity applied by
clamping singular values.

**Hardening.** Compacted snow gets stiffer. With ξ the hardening coefficient and Jp the
accumulated plastic volume change:

```
h  = exp(ξ · (1 − Jp))
μ  = μ₀ · h
λ  = λ₀ · h
```

Lamé parameters from Young's modulus E and Poisson ratio ν:

```
μ₀ = E / (2(1+ν))
λ₀ = E·ν / ((1+ν)(1−2ν))
```

**Plastic return mapping.** SVD the trial deformation gradient, clamp singular values into
the admissible range, push what was removed into Jp:

```
F_trial = U Σ Vᵀ
σ̂ᵢ      = clamp(σᵢ, 1 − θ_c, 1 + θ_s)
Jp     ← Jp · Π (σᵢ / σ̂ᵢ)
F       = U Σ̂ Vᵀ
```

θ_c (critical compression) and θ_s (critical stretch) are the plastic yield thresholds.
Snow yields in stretch very easily (θ_s ≈ 7.5e-3) — that is what produces fracture rather
than elastic stretching.

**Stress**, premultiplied by Fᵀ as MLS-MPM needs it:

```
R      = U Vᵀ                       # rotational part
J      = det(Σ̂) = σ̂₁σ̂₂σ̂₃
P Fᵀ   = 2μ (F − R) Fᵀ + λ J (J−1) I
```

First term = deviatoric (shear) response, second = volumetric. Implemented in
`snow_stress()` and `apply_plasticity()`.

**Paper parameter values** (Stomakhin et al. [5], table 1) are the defaults in the code:
E = 1.4e5 Pa, ν = 0.2, ξ = 10, θ_c = 2.5e-2, θ_s = 7.5e-3, ρ = 400 kg/m³.

> Caveat worth putting in the report: those values were tuned for metre-scale snow in a
> film-VFX context. At 12–16 m grid cells they are not obviously the right numbers, and the
> result behaves more like a viscous flow than a fracturing slab. This is a resolution
> limitation, not a bug in the model.

### The SVD

Everything above needs a 3×3 SVD, twice per particle per substep (stress + plasticity), and
WGSL has no linear algebra library. Implemented from scratch in `svd3()`:

1. `A = Fᵀ F` (symmetric, positive semi-definite)
2. Cyclic **Jacobi eigendecomposition** of A — 8 sweeps, three plane rotations each
   ((0,1), (0,2), (1,2)), operating on the 6 unique components as scalars to avoid dynamic
   matrix indexing
3. σᵢ = √λᵢ, sorted descending; V = eigenvectors, forced to det(V) = +1
4. U columns from `F·vᵢ/σᵢ`, re-orthogonalised, `u₃ = u₁ × u₂`
5. **Signed** third singular value: `σ₃ = u₃ · (F v₃)` — this reproduces det(F) < 0
   (inverted elements) correctly instead of silently producing garbage

Verified against numpy over 700+ matrices — see [06-verification.md](06-verification.md).

## 5. Terrain coupling

Not in the source papers — this is the geographic part of the project.

**Height sampling.** The stitched DEM is `R32Float` and therefore *not filterable*, so
`textureSample` is unavailable. `sample_height_uv()` does manual bilinear interpolation from
four `textureLoad` calls. Heights are metres above sea level (the decode shader scales by
`8191.875/65535`).

**Terrain normal** from central differences of the height field, sampled at grid scale
(`max(dx, 1 m)`) rather than DEM scale — sampling finer than the simulation resolves just
injects noise into the collision response:

```
n = normalize(−∂h/∂x, −∂h/∂y, 1)
```

**Collision** is resolved at *both* levels, which the research notes recommended and which
turns out to be necessary:

- **Grid level** — Coulomb friction on nodes below the surface. With `vn = v·n < 0`:
  ```
  vt = v − n·vn
  if |vt| ≤ −μ·vn:  v = 0                    # sticking
  else:             v = vt (1 + μ·vn/|vt|)   # sliding, tangential only
  ```
- **Particle level** — after advection, any particle below the surface is pushed back up to
  it and gets the same friction response. The grid condition alone is quantised to node
  spacing, so snow creeps through the surface between nodes without this.

**Vertical grid origin.** The Eulerian grid box needs a base altitude. Rather than making
the user guess one, the `mpm_prepare` kernel scans the terrain inside the domain footprint
and `atomicMin`s the minimum altitude into a state buffer; every later kernel reads it and
subtracts a 2·dx margin. Fully GPU-side, no readback.

## 6. Deviations from the literature, in one place

| What | Why |
|---|---|
| Particle mass normalised to 1, volume = 1/ρ | Keeps fixed-point grid accumulators in i32 range independent of real snow mass. Physically a pure unit change — see [04-data-layout.md](04-data-layout.md). |
| Fixed-point `atomic<i32>` accumulation | WGSL has no atomic float add. Scale 1e4, clamped before every add. |
| G2P and advection fused into one kernel | Both are per-particle passes over the same data; no reason to split. |
| Grid cleared by a compute kernel, not `clearBuffer` | Keeps the whole run inside one compute pass. |
| Terrain collision at grid *and* particle level | Grid-only lets snow leak through between nodes at these cell sizes. |
