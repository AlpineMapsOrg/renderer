/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

// GPU golden tests: the real WGSL functions of the MPM solver, run on the GPU, against the Python
// ports in mpm-mls-doc/scripts. The ports are checked against the papers (test_svd.py,
// test_material_*.py, test_friction.py); this closes the gap between port and shader - a slip
// in the translation, the row/column order of F and tau in struct Particle, the field order of
// the settings uniform, and float32 versus the ports' float64.
//
// Inputs and expected outputs come from golden/mpm_golden.json, written by make_golden.py. Each
// group runs a small wrapper kernel that ///uses the shader module under test and binds the
// real MpmSolverSettingsUniform at group 0, binding 0, as the solver does.

#include "../webgpu_engine/UnittestWebgpuContext.h"

#include <catch2/catch_test_macros.hpp>
#include <webgpu/base/raii/BindGroup.h>
#include <webgpu/base/raii/BindGroupLayout.h>
#include <webgpu/base/raii/CombinedComputePipeline.h>
#include <webgpu/base/raii/RawBuffer.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include <iostream>

using Settings = webgpu_compute::nodes::MpmSolverNode::MpmSolverSettingsUniform;

namespace {

QJsonObject load_golden()
{
    QFile file(MPM_GOLDEN_FILE);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll()).object();
}

glm::vec3 vec3_of(const QJsonValue& v)
{
    const auto a = v.toArray();
    return { float(a[0].toDouble()), float(a[1].toDouble()), float(a[2].toDouble()) };
}

// Row-major 3x3 from JSON as three rows.
std::array<glm::vec3, 3> rows_of(const QJsonValue& v)
{
    const auto a = v.toArray();
    return { vec3_of(a[0]), vec3_of(a[1]), vec3_of(a[2]) };
}

float max_abs(const std::array<glm::vec3, 3>& m)
{
    float r = 0.0f;
    for (const auto& row : m)
        r = std::max({ r, std::abs(row.x), std::abs(row.y), std::abs(row.z) });
    return r;
}

float max_diff(const glm::vec3& a, const glm::vec3& b) { return std::max({ std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z) }); }

float max_diff(const std::array<glm::vec3, 3>& a, const std::array<glm::vec3, 3>& b)
{
    return std::max({ max_diff(a[0], b[0]), max_diff(a[1], b[1]), max_diff(a[2], b[2]) });
}

// Fills the uniform fields a golden group names. Everything else stays zero, which the functions
// under test never read.
Settings settings_of(const QJsonObject& group)
{
    Settings s {};
    const auto values = group["settings"].toObject();
    const std::vector<std::pair<const char*, float*>> fields {
        { "mu_0", &s.mu_0 },
        { "lambda_0", &s.lambda_0 },
        { "hardening", &s.hardening },
        { "critical_compression", &s.critical_compression },
        { "critical_stretch", &s.critical_stretch },
        { "dp_alpha", &s.dp_alpha },
        { "ccc_m", &s.ccc_m },
        { "ccc_beta", &s.ccc_beta },
        { "ccc_xi", &s.ccc_xi },
        { "ccc_p0_initial", &s.ccc_p0_initial },
        { "terrain_friction", &s.terrain_friction },
        { "voellmy_xi", &s.voellmy_xi },
        { "gravity", &s.gravity },
        { "dt", &s.dt },
    };
    for (const auto& [name, field] : fields) {
        if (values.contains(name))
            *field = float(values[name].toDouble());
    }
    s.constitutive_model = uint32_t(group["constitutive_model"].toInt());
    s.basal_friction_model = uint32_t(group["basal_friction_model"].toInt());
    return s;
}

/// Runs `code` once per case: `input` holds `in_per_case` vec4 per case, the kernel writes
/// `out_per_case` vec4 per case. Returns the output.
std::vector<glm::vec4> run_kernel(UnittestWebgpuContext& context, const char* code, const Settings& settings, const std::vector<glm::vec4>& input,
    size_t in_per_case, size_t out_per_case)
{
    const size_t cases = input.size() / in_per_case;
    REQUIRE(cases > 0);

    webgpu::raii::RawBuffer<Settings> settings_buffer(context.device, WGPUBufferUsage(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst), 1, "golden settings");
    settings_buffer.write(context.queue, &settings, 1, 0);
    webgpu::raii::RawBuffer<glm::vec4> input_buffer(context.device, WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst), input.size(), "golden in");
    input_buffer.write(context.queue, input.data(), input.size(), 0);
    webgpu::raii::RawBuffer<glm::vec4> output_buffer(
        context.device, WGPUBufferUsage(WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc), cases * out_per_case, "golden out");

    WGPUBindGroupLayoutEntry uniform_entry {};
    uniform_entry.binding = 0;
    uniform_entry.visibility = WGPUShaderStage_Compute;
    uniform_entry.buffer.type = WGPUBufferBindingType_Uniform;
    uniform_entry.buffer.minBindingSize = sizeof(Settings);
    WGPUBindGroupLayoutEntry in_entry {};
    in_entry.binding = 0;
    in_entry.visibility = WGPUShaderStage_Compute;
    in_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    WGPUBindGroupLayoutEntry out_entry = in_entry;
    out_entry.binding = 1;
    out_entry.buffer.type = WGPUBufferBindingType_Storage;

    webgpu::raii::BindGroupLayout settings_layout(context.device, std::vector<WGPUBindGroupLayoutEntry> { uniform_entry }, "golden settings layout");
    webgpu::raii::BindGroupLayout io_layout(context.device, std::vector<WGPUBindGroupLayoutEntry> { in_entry, out_entry }, "golden io layout");
    webgpu::raii::BindGroup settings_group(
        context.device, settings_layout, std::vector<WGPUBindGroupEntry> { settings_buffer.create_bind_group_entry(0) }, "golden settings group");
    webgpu::raii::BindGroup io_group(context.device, io_layout,
        std::vector<WGPUBindGroupEntry> { input_buffer.create_bind_group_entry(0), output_buffer.create_bind_group_entry(1) }, "golden io group");

    const auto module = context.ctx.resource_registry().compile_shader_from_code(context.device, code, "golden kernel");
    webgpu::raii::CombinedComputePipeline pipeline(
        context.device, *module, std::vector<const webgpu::raii::BindGroupLayout*> { &settings_layout, &io_layout }, "golden pipeline");
    pipeline.set_binding(0, settings_group);
    pipeline.set_binding(1, io_group);

    {
        const webgpu::raii::CommandEncoder encoder(context.device, {});
        {
            webgpu::raii::ComputePassEncoder pass(encoder.handle(), {});
            pipeline.run(pass, glm::uvec3(uint32_t((cases + 63) / 64), 1, 1));
        }
        WGPUCommandBufferDescriptor desc {};
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &desc);
        wgpuQueueSubmit(context.queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    std::vector<glm::vec4> output;
    REQUIRE(output_buffer.read_back_sync(context.instance, context.device, output, 10000) == WGPUMapAsyncStatus_Success);
    REQUIRE(output.size() == cases * out_per_case);
    return output;
}

void report(const char* what, float error, float tolerance) { std::cout << "  " << what << ": max error " << error << " (tolerance " << tolerance << ")\n"; }

// The kernels share this I/O block: one case per thread, `n` cases (the dispatch rounds up).
#define GOLDEN_IO                                                                                                                                              \
    "@group(1) @binding(0) var<storage, read> golden_in: array<vec4f>;\n"                                                                                      \
    "@group(1) @binding(1) var<storage, read_write> golden_out: array<vec4f>;\n"

constexpr const char* svd_kernel = R"(
///use webgpu_compute::mpm_common
)" GOLDEN_IO R"(
@compute @workgroup_size(64)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if id.x >= arrayLength(&golden_in) / 3u { return; }
    let i = id.x * 3u;
    var p: Particle;
    p.f0 = golden_in[i].xyz; // rows, as struct Particle stores F
    p.f1 = golden_in[i + 1u].xyz;
    p.f2 = golden_in[i + 2u].xyz;
    let f = particle_f(p);
    let s = svd3(f);
    store_f(&p, s.u * diag3(s.sigma) * transpose(s.v));
    let ut = transpose(s.u) * s.u - identity3();
    let vt = transpose(s.v) * s.v - identity3();
    let ortho = max(max(length(ut[0]), length(ut[1])), max(max(length(ut[2]), length(vt[0])), max(length(vt[1]), length(vt[2]))));
    let o = id.x * 5u;
    golden_out[o] = vec4f(p.f0, 0.0);
    golden_out[o + 1u] = vec4f(p.f1, 0.0);
    golden_out[o + 2u] = vec4f(p.f2, 0.0);
    golden_out[o + 3u] = vec4f(s.sigma, 0.0);
    golden_out[o + 4u] = vec4f(determinant(s.u), determinant(s.v), ortho, 0.0);
}
)";

constexpr const char* material_kernel = R"(
///use webgpu_compute::mpm_material
)" GOLDEN_IO R"(
@compute @workgroup_size(64)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if id.x >= arrayLength(&golden_in) / 4u { return; }
    let i = id.x * 4u;
    var p: Particle;
    p.f0 = golden_in[i].xyz;
    p.f1 = golden_in[i + 1u].xyz;
    p.f2 = golden_in[i + 2u].xyz;
    let r = material_plasticity(particle_f(p), golden_in[i + 3u].x);
    // Through the particle, as G2P stores and P2G reads them: F as rows, tau as six components.
    store_f(&p, r.f_elastic);
    store_kirchhoff(&p, r.kirchhoff);
    let tau = particle_kirchhoff(p);
    let o = id.x * 7u;
    golden_out[o] = vec4f(p.f0, 0.0);
    golden_out[o + 1u] = vec4f(p.f1, 0.0);
    golden_out[o + 2u] = vec4f(p.f2, 0.0);
    golden_out[o + 3u] = vec4f(tau[0], 0.0); // tau is symmetric: columns = rows
    golden_out[o + 4u] = vec4f(tau[1], 0.0);
    golden_out[o + 5u] = vec4f(tau[2], 0.0);
    golden_out[o + 6u] = vec4f(r.plastic_state, select(0.0, 1.0, r.yielded), material_initial_state(), 0.0);
}
)";

constexpr const char* friction_kernel = R"(
///use webgpu_compute::mpm_friction
)" GOLDEN_IO R"(
@compute @workgroup_size(64)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if id.x >= arrayLength(&golden_in) / 2u { return; }
    let v = golden_in[id.x * 2u]; // xyz velocity, w flow depth
    let n = golden_in[id.x * 2u + 1u]; // xyz normal, w apply_basal_drag
    golden_out[id.x] = vec4f(resolve_terrain_collision(v.xyz, n.xyz, n.w > 0.5, v.w), 0.0);
}
)";

constexpr const char* kernel_kernel = R"(
///use webgpu_compute::mpm_common
)" GOLDEN_IO R"(
@compute @workgroup_size(64)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if id.x >= arrayLength(&golden_in) { return; }
    let k = compute_kernel(golden_in[id.x].xyz);
    var sum = 0.0;
    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            for (var l = 0; l < 3; l++) {
                sum += kernel_weight(k, vec3i(i, j, l));
            }
        }
    }
    let o = id.x * 5u;
    golden_out[o] = vec4f(vec3f(k.base), sum);
    golden_out[o + 1u] = vec4f(k.fx, 0.0);
    golden_out[o + 2u] = vec4f(k.w0, 0.0);
    golden_out[o + 3u] = vec4f(k.w1, 0.0);
    golden_out[o + 4u] = vec4f(k.w2, 0.0);
}
)";

} // namespace

TEST_CASE("MPM shader functions on the GPU match the Python ports")
{
    UnittestWebgpuContext context;
    const QJsonObject golden = load_golden();

    SECTION("svd3")
    {
        const auto cases = golden["svd"].toArray();
        std::vector<glm::vec4> input;
        for (const auto& c : cases)
            for (const auto& row : rows_of(c.toObject()["F"]))
                input.emplace_back(row, 0.0f);
        const auto out = run_kernel(context, svd_kernel, Settings {}, input, 3, 5);

        // f32 with the eigenvalues of F^T F: the singular values carry ~1e-6 relative error.
        constexpr float tolerance = 2e-5f;
        float reconstruction = 0.0f, sigma = 0.0f, det = 0.0f, ortho = 0.0f;
        for (qsizetype n = 0; n < cases.size(); ++n) {
            const auto c = cases[n].toObject();
            const auto f = rows_of(c["F"]);
            const std::array<glm::vec3, 3> usv { glm::vec3(out[n * 5]), glm::vec3(out[n * 5 + 1]), glm::vec3(out[n * 5 + 2]) };
            reconstruction = std::max(reconstruction, max_diff(usv, f) / std::max(1.0f, max_abs(f)));
            const glm::vec3 s = vec3_of(c["sigma"]);
            sigma = std::max(sigma, max_diff(glm::vec3(out[n * 5 + 3]), s));
            det = std::max({ det, std::abs(out[n * 5 + 4].x - 1.0f), std::abs(out[n * 5 + 4].y - 1.0f) });
            ortho = std::max(ortho, out[n * 5 + 4].z);
        }
        std::cout << "svd3, " << cases.size() << " cases\n";
        report("U diag(sigma) V^T - F, relative", reconstruction, tolerance);
        report("sigma vs port", sigma, tolerance);
        report("det U, det V - 1", det, tolerance);
        report("U^T U - I, V^T V - I", ortho, tolerance);
        CHECK(reconstruction < tolerance);
        CHECK(sigma < tolerance);
        CHECK(det < tolerance);
        CHECK(ortho < tolerance);
    }

    SECTION("material return mappings and stress")
    {
        for (const auto& group_value : golden["materials"].toArray()) {
            const auto group = group_value.toObject();
            const auto cases = group["cases"].toArray();
            const Settings settings = settings_of(group);
            std::vector<glm::vec4> input;
            for (const auto& c : cases) {
                for (const auto& row : rows_of(c.toObject()["F"]))
                    input.emplace_back(row, 0.0f);
                input.emplace_back(float(c.toObject()["state"].toDouble()), 0.0f, 0.0f, 0.0f);
            }
            const auto out = run_kernel(context, material_kernel, settings, input, 4, 7);

            // F_elastic is O(1). tau is compared relative to mu_0: f32 strains are good to ~1e-7
            // absolute, and the Hencky log / the corotated (F - R) amplify that by ~1e2 at most.
            constexpr float f_tolerance = 2e-5f;
            constexpr float tau_tolerance = 1e-4f; // times mu_0
            constexpr float state_tolerance = 2e-5f; // relative to max(1, |state|)
            float f_error = 0.0f, tau_error = 0.0f, state_error = 0.0f;
            int yield_mismatches = 0;
            for (qsizetype n = 0; n < cases.size(); ++n) {
                const auto c = cases[n].toObject();
                const auto* o = &out[size_t(n) * 7];
                f_error = std::max(f_error, max_diff({ glm::vec3(o[0]), glm::vec3(o[1]), glm::vec3(o[2]) }, rows_of(c["F_elastic"])));
                tau_error = std::max(tau_error, max_diff({ glm::vec3(o[3]), glm::vec3(o[4]), glm::vec3(o[5]) }, rows_of(c["tau"])) / settings.mu_0);
                const double state = c["plastic_state"].toDouble();
                state_error = std::max(state_error, float(std::abs(o[6].x - state) / std::max(1.0, std::abs(state))));
                if ((o[6].y > 0.5f) != c["yielded"].toBool()) {
                    ++yield_mismatches;
                    std::cout << "  case " << n << ": GPU yielded " << (o[6].y > 0.5f) << ", port " << c["yielded"].toBool() << ", port branch "
                              << c["case"].toInt() << ", GPU state " << o[6].x << ", port " << state << "\n";
                }
            }
            // The Cam Clay initial state -asinh(p0 / K) / xi: ccc_asinh is log(x + sqrt(x^2 + 1)),
            // which cancels in f32 for small x (Li case V with E = 3 MPa: x = 1.2e-3, relative error
            // ~eps / x). Measured 2.2e-5; a p0 that far off is irrelevant for the physics.
            constexpr float initial_tolerance = 1e-4f;
            const float initial_error = std::abs(out[6].z - float(group["initial_state"].toDouble())) / std::max(1.0f, std::abs(float(group["initial_state"].toDouble())));
            std::cout << group["name"].toString().toStdString() << ", " << cases.size() << " cases\n";
            report("F_elastic", f_error, f_tolerance);
            report("tau / mu_0", tau_error, tau_tolerance);
            report("plastic state, relative", state_error, state_tolerance);
            report("initial state, relative", initial_error, initial_tolerance);
            std::cout << "  yielded flag mismatches: " << yield_mismatches << "\n";
            CHECK(f_error < f_tolerance);
            CHECK(tau_error < tau_tolerance);
            CHECK(state_error < state_tolerance);
            CHECK(initial_error < initial_tolerance);
            CHECK(yield_mismatches == 0);
        }
    }

    SECTION("basal friction")
    {
        for (const auto& group_value : golden["friction"].toArray()) {
            const auto group = group_value.toObject();
            const auto cases = group["cases"].toArray();
            std::vector<glm::vec4> input;
            for (const auto& c_value : cases) {
                const auto c = c_value.toObject();
                input.emplace_back(vec3_of(c["velocity"]), float(c["flow_depth"].toDouble()));
                input.emplace_back(vec3_of(c["normal"]), c["apply_basal_drag"].toBool() ? 1.0f : 0.0f);
            }
            const auto out = run_kernel(context, friction_kernel, settings_of(group), input, 2, 1);

            constexpr float tolerance = 1e-5f; // relative to max(1, |v|)
            float error = 0.0f;
            qsizetype worst = 0;
            for (qsizetype n = 0; n < cases.size(); ++n) {
                const auto c = cases[n].toObject();
                const glm::vec3 expected = vec3_of(c["result"]);
                const float e = glm::length(glm::vec3(out[n]) - expected) / std::max(1.0f, glm::length(vec3_of(c["velocity"])));
                if (e > error) {
                    error = e;
                    worst = n;
                }
            }
            if (error >= tolerance) {
                const auto c = cases[worst].toObject();
                const glm::vec3 v = vec3_of(c["velocity"]), r = vec3_of(c["result"]);
                std::cout << "  worst case " << worst << ": v (" << v.x << ", " << v.y << ", " << v.z << "), drag " << c["apply_basal_drag"].toBool()
                          << ", depth " << c["flow_depth"].toDouble() << ": GPU (" << out[worst].x << ", " << out[worst].y << ", " << out[worst].z
                          << "), port (" << r.x << ", " << r.y << ", " << r.z << ")\n";
            }
            std::cout << group["name"].toString().toStdString() << " friction, " << cases.size() << " cases\n";
            report("velocity, relative", error, tolerance);
            CHECK(error < tolerance);
        }
    }

    SECTION("quadratic B-spline weights")
    {
        const auto cases = golden["kernel"].toArray();
        std::vector<glm::vec4> input;
        for (const auto& c : cases)
            input.emplace_back(vec3_of(c.toObject()["grid_pos"]), 0.0f);
        const auto out = run_kernel(context, kernel_kernel, Settings {}, input, 1, 5);

        constexpr float tolerance = 1e-5f; // grid positions up to 20: f32 fx is good to ~2e-6
        float error = 0.0f, sum_error = 0.0f;
        int base_mismatches = 0;
        for (qsizetype n = 0; n < cases.size(); ++n) {
            const auto c = cases[n].toObject();
            const auto* o = &out[size_t(n) * 5];
            const auto base = c["base"].toArray();
            if (glm::ivec3(glm::vec3(o[0])) != glm::ivec3(base[0].toInt(), base[1].toInt(), base[2].toInt()))
                ++base_mismatches;
            sum_error = std::max(sum_error, std::abs(o[0].w - 1.0f));
            const auto w = c["w"].toArray();
            error = std::max(error, max_diff({ glm::vec3(o[2]), glm::vec3(o[3]), glm::vec3(o[4]) }, rows_of(w)));
            error = std::max(error, max_diff(glm::vec3(o[1]), vec3_of(c["fx"])));
        }
        std::cout << "B-spline kernel, " << cases.size() << " cases\n";
        report("fx and weights", error, tolerance);
        report("sum of the 27 weights - 1", sum_error, tolerance);
        std::cout << "  base node mismatches: " << base_mismatches << "\n";
        CHECK(error < tolerance);
        CHECK(sum_error < tolerance);
        CHECK(base_mismatches == 0);
    }
}
