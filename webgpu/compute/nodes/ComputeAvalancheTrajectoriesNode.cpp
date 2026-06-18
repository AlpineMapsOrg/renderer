/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "ComputeAvalancheTrajectoriesNode.h"

#include <QDebug>

namespace webgpu_compute::nodes {

glm::uvec3 ComputeAvalancheTrajectoriesNode::SHADER_WORKGROUP_SIZE = { 16, 16, 1 };

ComputeAvalancheTrajectoriesNode::ComputeAvalancheTrajectoriesNode(webgpu::Context& ctx)
    : ComputeAvalancheTrajectoriesNode(ctx, AvalancheTrajectoriesSettings())
{
}

ComputeAvalancheTrajectoriesNode::ComputeAvalancheTrajectoriesNode(webgpu::Context& ctx, const AvalancheTrajectoriesSettings& settings)
    : Node(
          {
              InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()),
              InputSocket(*this, "normal texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>()),
              InputSocket(*this, "release point texture", data_type<const webgpu::raii::TextureWithSampler*>()),
          },
          {
              OutputSocket(*this, "storage buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_output_storage_buffer.get(); }),
              OutputSocket(*this, "raster dimensions", data_type<glm::uvec2>(), [this]() { return m_output_dimensions; }),
              OutputSocket(*this, "layer1_zdelta", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_layer1_zdelta_buffer.get(); }),
              OutputSocket(*this, "layer2_cellCounts", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_layer2_cellCounts_buffer.get(); }),
              OutputSocket(
                  *this, "layer3_travelLength", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_layer3_travelLength_buffer.get(); }),
              OutputSocket(
                  *this, "layer4_travelAngle", data_type<webgpu::raii::RawBuffer<uint32_t>*>(), [this]() { return m_layer4_travelAngle_buffer.get(); }),
              OutputSocket(*this,
                  "layer5_altitudeDifference",
                  data_type<webgpu::raii::RawBuffer<uint32_t>*>(),
                  [this]() { return m_layer5_altitudeDifference_buffer.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings { settings }
    , m_settings_uniform(ctx.device(), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform)
    , m_normal_sampler(create_normal_sampler(m_ctx->device()))
    , m_height_sampler(create_height_sampler(m_ctx->device()))
{
    auto& reg = ctx.resource_registry();
    reg.register_shader("avalanche_trajectories_compute", "webgpu_compute::avalanche_trajectories_compute");
    reg.register_bind_group_layout("avalanche_trajectories_compute", [](WGPUDevice dev) {
        WGPUBindGroupLayoutEntry e0 {};
        e0.binding = 0;
        e0.visibility = WGPUShaderStage_Compute;
        e0.buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutEntry e1 {};
        e1.binding = 1;
        e1.visibility = WGPUShaderStage_Compute;
        e1.texture.sampleType = WGPUTextureSampleType_Float;
        e1.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry e2 {};
        e2.binding = 2;
        e2.visibility = WGPUShaderStage_Compute;
        e2.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        e2.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry e3 {};
        e3.binding = 3;
        e3.visibility = WGPUShaderStage_Compute;
        e3.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        e3.texture.viewDimension = WGPUTextureViewDimension_2D;

        WGPUBindGroupLayoutEntry e4 {};
        e4.binding = 4;
        e4.visibility = WGPUShaderStage_Compute;
        e4.sampler.type = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutEntry e5 {};
        e5.binding = 5;
        e5.visibility = WGPUShaderStage_Compute;
        e5.sampler.type = WGPUSamplerBindingType_NonFiltering;

        WGPUBindGroupLayoutEntry e6 {};
        e6.binding = 6;
        e6.visibility = WGPUShaderStage_Compute;
        e6.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e7 {};
        e7.binding = 7;
        e7.visibility = WGPUShaderStage_Compute;
        e7.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e8 {};
        e8.binding = 8;
        e8.visibility = WGPUShaderStage_Compute;
        e8.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e9 {};
        e9.binding = 9;
        e9.visibility = WGPUShaderStage_Compute;
        e9.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e10 {};
        e10.binding = 10;
        e10.visibility = WGPUShaderStage_Compute;
        e10.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutEntry e11 {};
        e11.binding = 11;
        e11.visibility = WGPUShaderStage_Compute;
        e11.buffer.type = WGPUBufferBindingType_Storage;

        return std::make_unique<webgpu::raii::BindGroupLayout>(dev,
            std::vector<WGPUBindGroupLayoutEntry> { e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11 },
            "avalanche trajectories compute bind group layout");
    });
    reg.register_pipeline([this](WGPUDevice device, const webgpu::RenderResourceRegistry& reg) {
        m_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(device,
            reg.shader("avalanche_trajectories_compute"),
            std::vector<const webgpu::raii::BindGroupLayout*> { &reg.bind_group_layout("avalanche_trajectories_compute") });
    });
}

void ComputeAvalancheTrajectoriesNode::update_gpu_settings(uint32_t run)
{
    m_settings_uniform.data.num_steps = m_settings.num_steps;
    m_settings_uniform.data.step_length = m_settings.step_length;
    m_settings_uniform.data.max_perturbation = m_settings.max_perturbation;
    m_settings_uniform.data.persistence_contribution = m_settings.persistence_contribution;

    m_settings_uniform.data.model_type = m_settings.active_model;
    m_settings_uniform.data.model2_gravity = m_settings.model2.gravity;
    m_settings_uniform.data.model2_mass = m_settings.model2.mass;
    m_settings_uniform.data.model2_friction_coeff = m_settings.model2.friction_coeff;
    m_settings_uniform.data.model2_drag_coeff = m_settings.model2.drag_coeff;

    m_settings_uniform.data.runout_model_type = m_settings.active_runout_model;
    m_settings_uniform.data.runout_perla_my = m_settings.runout_perla.my;
    m_settings_uniform.data.runout_perla_md = m_settings.runout_perla.md;
    m_settings_uniform.data.runout_perla_l = m_settings.runout_perla.l;
    m_settings_uniform.data.runout_perla_g = m_settings.runout_perla.g;
    m_settings_uniform.data.runout_flowpy_alpha = glm::radians(m_settings.runout_flowpy.alpha);

    // TODO: Get activation of layer if output socket is connected and following node is enabled?
    m_settings_uniform.data.output_layer = m_settings.output_layer;

    m_settings_uniform.data.random_seed = m_settings.random_seed + run;

    m_settings_uniform.update_gpu_data(m_ctx->queue());
}

void ComputeAvalancheTrajectoriesNode::set_settings(const AvalancheTrajectoriesSettings& settings) { m_settings = settings; }

const ComputeAvalancheTrajectoriesNode::AvalancheTrajectoriesSettings& ComputeAvalancheTrajectoriesNode::get_settings() const { return m_settings; }

void ComputeAvalancheTrajectoriesNode::run_impl()
{

    const auto region_aabb = std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data());
    const auto& normal_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("normal texture").get_connected_data());
    const auto& height_texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("height texture").get_connected_data());
    const auto& release_point_texture
        = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("release point texture").get_connected_data());

    const auto input_width = normal_texture.texture().width();
    const auto input_height = normal_texture.texture().height();

    // assert input textures have same size, otherwise fail run
    if (input_width != height_texture.texture().width() || input_height != height_texture.texture().height()
        || input_width != release_point_texture.texture().width() || input_height != release_point_texture.texture().height()) {
        fail_run("failed to compute trajectories: input texture sizes must match (normals: " + std::to_string(input_width) + "x" + std::to_string(input_height)
            + ", heights: " + std::to_string(height_texture.texture().width()) + "x" + std::to_string(height_texture.texture().height())
            + ", release points: " + std::to_string(release_point_texture.texture().width()) + "x"
            + std::to_string(release_point_texture.texture().height()) + ")");
        return;
    }

    m_output_dimensions = glm::uvec2(input_width, input_height) * m_settings.resolution_multiplier;

    qDebug() << "input resolution: " << input_width << "x" << input_height;
    qDebug() << "output resolution: " << m_output_dimensions.x << "x" << m_output_dimensions.y;

    // create output storage buffer
    m_output_storage_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        m_output_dimensions.x * m_output_dimensions.y,
        "avalanche trajectories compute output storage");

    // create layer buffers
    m_layer1_zdelta_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        m_settings.output_layer.layer1_zdelta_enabled ? (m_output_dimensions.x * m_output_dimensions.y) : 1,
        "avalanche trajectories zdelta storage");
    m_layer2_cellCounts_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        m_settings.output_layer.layer2_cellCounts_enabled ? (m_output_dimensions.x * m_output_dimensions.y) : 1,
        "avalanche trajectories cellCounts storage");
    m_layer3_travelLength_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        m_settings.output_layer.layer3_travelLength_enabled ? (m_output_dimensions.x * m_output_dimensions.y) : 1,
        "avalanche trajectories travelLength storage");
    m_layer4_travelAngle_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        m_settings.output_layer.layer4_travelAngle_enabled ? (m_output_dimensions.x * m_output_dimensions.y) : 1,
        "avalanche trajectories travelAngle storage");
    m_layer5_altitudeDifference_buffer = std::make_unique<webgpu::raii::RawBuffer<uint32_t>>(m_ctx->device(),
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        m_settings.output_layer.layer5_altitudeDifference_enabled ? (m_output_dimensions.x * m_output_dimensions.y) : 1,
        "avalanche trajectories altitudeDifference storage");

    // update input settings on GPU side
    m_settings_uniform.data.output_resolution = m_output_dimensions;
    m_settings_uniform.data.region_size = glm::fvec2(region_aabb->size());
    update_gpu_settings();

    // create bind group
    std::vector<WGPUBindGroupEntry> entries {
        m_settings_uniform.raw_buffer().create_bind_group_entry(0),
        normal_texture.texture_view().create_bind_group_entry(1),
        height_texture.texture_view().create_bind_group_entry(2),
        release_point_texture.texture_view().create_bind_group_entry(3),
        m_normal_sampler->create_bind_group_entry(4),
        m_height_sampler->create_bind_group_entry(5),
        m_output_storage_buffer->create_bind_group_entry(6),
        m_layer1_zdelta_buffer->create_bind_group_entry(7),
        m_layer2_cellCounts_buffer->create_bind_group_entry(8),
        m_layer3_travelLength_buffer->create_bind_group_entry(9),
        m_layer4_travelAngle_buffer->create_bind_group_entry(10),
        m_layer5_altitudeDifference_buffer->create_bind_group_entry(11),
    };

    webgpu::raii::BindGroup compute_bind_group(
        m_ctx->device(), m_ctx->resource_registry().bind_group_layout("avalanche_trajectories_compute"), entries, "avalanche trajectories compute bind group");

    // bind GPU resources and run pipeline
    for (uint32_t run = 0; run < m_settings.num_runs; run++) {
        update_gpu_settings(run); // change seed each run
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "avalanche trajectories compute command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(m_ctx->device(), descriptor);

        {
            WGPUComputePassDescriptor compute_pass_desc {};
            compute_pass_desc.label = WGPUStringView { .data = "avalanche trajectories compute pass", .length = WGPU_STRLEN };
            webgpu::raii::ComputePassEncoder compute_pass(encoder.handle(), compute_pass_desc);

            glm::uvec3 workgroup_counts
                = glm::ceil(glm::vec3(input_width, input_height, m_settings.num_paths_per_release_cell) / glm::vec3(SHADER_WORKGROUP_SIZE));
            wgpuComputePassEncoderSetBindGroup(compute_pass.handle(), 0, compute_bind_group.handle(), 0, nullptr);
            m_pipeline->run(compute_pass, workgroup_counts);
        }

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "avalanche trajectories compute command buffer", .length = WGPU_STRLEN };
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(m_ctx->queue(), 1, &command);
        wgpuCommandBufferRelease(command);
    }

    const auto on_work_done
        = []([[maybe_unused]] WGPUQueueWorkDoneStatus status, [[maybe_unused]] WGPUStringView message, void* userdata, [[maybe_unused]] void* userdata2) {
              ComputeAvalancheTrajectoriesNode* _this = reinterpret_cast<ComputeAvalancheTrajectoriesNode*>(userdata);
              _this->complete_run();
          };

    WGPUQueueWorkDoneCallbackInfo callback_info {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_work_done,
        .userdata1 = this,
        .userdata2 = nullptr,
    };

    wgpuQueueOnSubmittedWorkDone(m_ctx->queue(), callback_info);
}

std::unique_ptr<webgpu::raii::Sampler> ComputeAvalancheTrajectoriesNode::create_normal_sampler(WGPUDevice device)
{
    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "compute trajectories normal sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;
    return std::make_unique<webgpu::raii::Sampler>(device, sampler_desc);
}

std::unique_ptr<webgpu::raii::Sampler> ComputeAvalancheTrajectoriesNode::create_height_sampler(WGPUDevice device)
{
    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "compute trajectories height sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;
    return std::make_unique<webgpu::raii::Sampler>(device, sampler_desc);
}

void ComputeAvalancheTrajectoriesNode::serialize_settings(QJsonObject& out) const
{
    const auto& s = m_settings;
    out["resolution_multiplier"] = static_cast<int>(s.resolution_multiplier);
    out["num_steps"] = static_cast<int>(s.num_steps);
    out["step_length"] = static_cast<double>(s.step_length);
    out["num_paths_per_release_cell"] = static_cast<int>(s.num_paths_per_release_cell);
    out["num_runs"] = static_cast<int>(s.num_runs);
    out["max_perturbation"] = static_cast<double>(s.max_perturbation);
    out["persistence_contribution"] = static_cast<double>(s.persistence_contribution);
    out["active_model"] = static_cast<int>(s.active_model);
    out["model2"] = QJsonObject {
        { "gravity", static_cast<double>(s.model2.gravity) },
        { "mass", static_cast<double>(s.model2.mass) },
        { "friction_coeff", static_cast<double>(s.model2.friction_coeff) },
        { "drag_coeff", static_cast<double>(s.model2.drag_coeff) },
    };
    out["active_runout_model"] = static_cast<int>(s.active_runout_model);
    out["runout_perla"] = QJsonObject {
        { "my", static_cast<double>(s.runout_perla.my) },
        { "md", static_cast<double>(s.runout_perla.md) },
        { "l", static_cast<double>(s.runout_perla.l) },
        { "g", static_cast<double>(s.runout_perla.g) },
    };
    out["runout_flowpy"] = QJsonObject { { "alpha", static_cast<double>(s.runout_flowpy.alpha) } };
    out["output_layer"] = QJsonObject {
        { "layer1_zdelta_enabled", static_cast<int>(s.output_layer.layer1_zdelta_enabled) },
        { "layer2_cellCounts_enabled", static_cast<int>(s.output_layer.layer2_cellCounts_enabled) },
        { "layer3_travelLength_enabled", static_cast<int>(s.output_layer.layer3_travelLength_enabled) },
        { "layer4_travelAngle_enabled", static_cast<int>(s.output_layer.layer4_travelAngle_enabled) },
        { "layer5_altitudeDifference_enabled", static_cast<int>(s.output_layer.layer5_altitudeDifference_enabled) },
    };
    out["random_seed"] = static_cast<int>(s.random_seed);
}

void ComputeAvalancheTrajectoriesNode::deserialize_settings(const QJsonObject& in)
{
    auto s = m_settings;
    if (in.contains("resolution_multiplier"))
        s.resolution_multiplier = static_cast<uint32_t>(in["resolution_multiplier"].toInt(static_cast<int>(s.resolution_multiplier)));
    if (in.contains("num_steps"))
        s.num_steps = static_cast<uint32_t>(in["num_steps"].toInt(static_cast<int>(s.num_steps)));
    if (in.contains("step_length"))
        s.step_length = static_cast<float>(in["step_length"].toDouble(s.step_length));
    if (in.contains("num_paths_per_release_cell"))
        s.num_paths_per_release_cell = static_cast<uint32_t>(in["num_paths_per_release_cell"].toInt(static_cast<int>(s.num_paths_per_release_cell)));
    if (in.contains("num_runs"))
        s.num_runs = static_cast<uint32_t>(in["num_runs"].toInt(static_cast<int>(s.num_runs)));
    if (in.contains("max_perturbation"))
        s.max_perturbation = static_cast<float>(in["max_perturbation"].toDouble(s.max_perturbation));
    if (in.contains("persistence_contribution"))
        s.persistence_contribution = static_cast<float>(in["persistence_contribution"].toDouble(s.persistence_contribution));
    if (in.contains("active_model"))
        s.active_model = static_cast<PhysicsModelType>(in["active_model"].toInt(static_cast<int>(s.active_model)));
    if (in.contains("model2")) {
        const QJsonObject m = in["model2"].toObject();
        if (m.contains("gravity"))
            s.model2.gravity = static_cast<float>(m["gravity"].toDouble(s.model2.gravity));
        if (m.contains("mass"))
            s.model2.mass = static_cast<float>(m["mass"].toDouble(s.model2.mass));
        if (m.contains("friction_coeff"))
            s.model2.friction_coeff = static_cast<float>(m["friction_coeff"].toDouble(s.model2.friction_coeff));
        if (m.contains("drag_coeff"))
            s.model2.drag_coeff = static_cast<float>(m["drag_coeff"].toDouble(s.model2.drag_coeff));
    }
    if (in.contains("active_runout_model"))
        s.active_runout_model = static_cast<FrictionModelType>(in["active_runout_model"].toInt(static_cast<int>(s.active_runout_model)));
    if (in.contains("runout_perla")) {
        const QJsonObject p = in["runout_perla"].toObject();
        if (p.contains("my"))
            s.runout_perla.my = static_cast<float>(p["my"].toDouble(s.runout_perla.my));
        if (p.contains("md"))
            s.runout_perla.md = static_cast<float>(p["md"].toDouble(s.runout_perla.md));
        if (p.contains("l"))
            s.runout_perla.l = static_cast<float>(p["l"].toDouble(s.runout_perla.l));
        if (p.contains("g"))
            s.runout_perla.g = static_cast<float>(p["g"].toDouble(s.runout_perla.g));
    }
    if (in.contains("runout_flowpy")) {
        const QJsonObject f = in["runout_flowpy"].toObject();
        if (f.contains("alpha"))
            s.runout_flowpy.alpha = static_cast<float>(f["alpha"].toDouble(s.runout_flowpy.alpha));
    }
    if (in.contains("output_layer")) {
        const QJsonObject l = in["output_layer"].toObject();
        if (l.contains("layer1_zdelta_enabled"))
            s.output_layer.layer1_zdelta_enabled
                = static_cast<uint32_t>(l["layer1_zdelta_enabled"].toInt(static_cast<int>(s.output_layer.layer1_zdelta_enabled)));
        if (l.contains("layer2_cellCounts_enabled"))
            s.output_layer.layer2_cellCounts_enabled
                = static_cast<uint32_t>(l["layer2_cellCounts_enabled"].toInt(static_cast<int>(s.output_layer.layer2_cellCounts_enabled)));
        if (l.contains("layer3_travelLength_enabled"))
            s.output_layer.layer3_travelLength_enabled
                = static_cast<uint32_t>(l["layer3_travelLength_enabled"].toInt(static_cast<int>(s.output_layer.layer3_travelLength_enabled)));
        if (l.contains("layer4_travelAngle_enabled"))
            s.output_layer.layer4_travelAngle_enabled
                = static_cast<uint32_t>(l["layer4_travelAngle_enabled"].toInt(static_cast<int>(s.output_layer.layer4_travelAngle_enabled)));
        if (l.contains("layer5_altitudeDifference_enabled"))
            s.output_layer.layer5_altitudeDifference_enabled
                = static_cast<uint32_t>(l["layer5_altitudeDifference_enabled"].toInt(static_cast<int>(s.output_layer.layer5_altitudeDifference_enabled)));
    }
    if (in.contains("random_seed"))
        s.random_seed = static_cast<uint32_t>(in["random_seed"].toInt(static_cast<int>(s.random_seed)));
    set_settings(s);
}

} // namespace webgpu_compute::nodes
