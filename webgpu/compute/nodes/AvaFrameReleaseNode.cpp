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

#include "AvaFrameReleaseNode.h"
#include "AvaFrameSandboxNode.h"

#include <QDebug>
#include <QDir>
#include <algorithm>
#include <cmath>
#include <limits>
#include <nucleus/srs.h>

namespace webgpu_compute::nodes {

namespace {
    constexpr uint32_t MAX_TEXELS_PER_SIDE = 4096;
}

ResampledRelease resample_release(const SandboxTerrain& terrain, const radix::geometry::Aabb<2, double>& region_world, double metres_per_world_unit,
    double texel_size_metres)
{
    return resample_depth(terrain, terrain.release_depth, region_world, metres_per_world_unit, texel_size_metres);
}

ResampledRelease resample_depth(const SandboxTerrain& terrain, const nucleus::Raster<float>& source, const radix::geometry::Aabb<2, double>& region_world,
    double metres_per_world_unit, double texel_size_metres)
{
    ResampledRelease out;
    const glm::dvec2 region_metres = region_world.size() * metres_per_world_unit;
    const glm::uvec2 texels = glm::clamp(glm::uvec2(glm::ceil(region_metres / std::max(texel_size_metres, 0.01))), glm::uvec2(1u), glm::uvec2(MAX_TEXELS_PER_SIDE));
    out.texel_size = region_metres / glm::dvec2(texels);
    out.depth = nucleus::Raster<float>(texels, 0.0f);
    if (!terrain.sandbox_to_world || terrain.cellsize <= 0.0 || source.buffer().empty())
        return out;

    const glm::dmat3 world_to_sandbox = glm::inverse(*terrain.sandbox_to_world);
    const glm::ivec2 grid(terrain.grid_size);
    const double texel_area = out.texel_size.x * out.texel_size.y;
    out.min = glm::dvec2(std::numeric_limits<double>::max());
    out.max = glm::dvec2(std::numeric_limits<double>::lowest());
    for (uint32_t row = 0; row < texels.y; row++) {
        for (uint32_t col = 0; col < texels.x; col++) {
            // Texel centre in region metres (y north, row 0 at the northern edge), to world.
            const glm::dvec2 centre((double(col) + 0.5) * out.texel_size.x, region_metres.y - (double(row) + 0.5) * out.texel_size.y);
            const glm::dvec2 world = region_world.min + centre / metres_per_world_unit;
            const glm::dvec2 sandbox = glm::dvec2(world_to_sandbox * glm::dvec3(world, 1.0));
            // Nearest cell of the dataset (its row 0 is the northern edge too).
            const glm::ivec2 cell(int(std::floor(sandbox.x / terrain.cellsize)), int(std::floor(sandbox.y / terrain.cellsize)));
            if (cell.x < 0 || cell.y < 0 || cell.x >= grid.x || cell.y >= grid.y)
                continue;
            const float depth = source.pixel(glm::uvec2(uint32_t(cell.x), uint32_t(grid.y - 1 - cell.y)));
            if (!(depth > 0.0f))
                continue;
            out.depth.pixel(glm::uvec2(col, row)) = depth;
            out.volume += double(depth) * texel_area;
            out.area += texel_area;
            out.centroid += centre * texel_area;
            out.max_depth = std::max(out.max_depth, depth);
            out.min = glm::min(out.min, centre - 0.5 * out.texel_size);
            out.max = glm::max(out.max, centre + 0.5 * out.texel_size);
        }
    }
    if (out.volume <= 0.0)
        out.min = out.max = glm::dvec2(0.0);
    else
        out.centroid /= out.area;
    return out;
}

AvaFrameReleaseNode::AvaFrameReleaseNode(webgpu::Context& ctx)
    : Node(
          {
              InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()),
          },
          {
              OutputSocket(*this, "release area", data_type<const ReleaseArea*>(), [this]() { return release_area(); }),
          })
    , m_ctx(&ctx)
{
}

AvaFrameReleaseNode::~AvaFrameReleaseNode() = default;

void AvaFrameReleaseNode::run_impl()
{
    m_area_valid = false;
    if (m_settings.dataset_path.empty()) {
        m_status = "no AvaFrame dataset for this scenario";
        complete_run();
        return;
    }
    const auto* region = std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data());
    if (region == nullptr) {
        m_status = "no region yet";
        complete_run();
        return;
    }

    const std::string key = m_settings.dataset_path + "/" + m_settings.release_scenario;
    if (m_terrain == nullptr || key != m_terrain_key) {
        std::string error;
        m_terrain = load_sandbox_terrain(m_settings.dataset_path, m_settings.release_scenario, error);
        m_terrain_key = key;
        if (m_terrain == nullptr) {
            // Not downloaded (or broken): the solver releases its disc instead. Not a graph failure.
            m_status = error;
            qInfo() << "AvaFrameReleaseNode:" << QString::fromStdString(error);
            complete_run();
            return;
        }
    }
    if (!m_terrain->sandbox_to_world) {
        m_status = m_terrain->name + " has no placement on the map (synthetic dataset)";
        complete_run();
        return;
    }

    // The solver's metres: Web Mercator units times cos(latitude) at the region centre.
    const glm::dvec2 centre = nucleus::srs::world_to_lat_long((region->min + region->max) * 0.5);
    const double metres_per_world_unit = std::cos(glm::radians(centre.x));
    const ResampledRelease release = resample_release(*m_terrain, *region, metres_per_world_unit, m_terrain->cellsize);
    if (release.volume <= 0.0) {
        m_status = m_terrain->name + " / " + m_terrain->scenario + ": release outside the loaded region";
        complete_run();
        return;
    }

    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "avaframe release on the map", .length = WGPU_STRLEN };
    // Same layout for the cover below.
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size = { release.depth.width(), release.depth.height(), 1 };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = WGPUTextureFormat_R32Float;
    texture_desc.usage = WGPUTextureUsage(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode_Nearest; // R32Float is not filterable; the solver uses textureLoad
    sampler_desc.minFilter = WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.maxAnisotropy = 1;
    m_texture = std::make_unique<webgpu::raii::TextureWithSampler>(m_ctx->device(), texture_desc, sampler_desc);
    m_texture->texture().write(m_ctx->queue(), release.depth);

    m_area = ReleaseArea { m_texture.get(), release.min, release.max, release.volume, release.area, release.max_depth,
        m_terrain->name + " / " + m_terrain->scenario };

    // The entrainment area, if the dataset has one (the loader removed the release cells).
    m_entrainment_texture.reset();
    if (m_terrain->entrainment_volume > 0.0) {
        const ResampledRelease cover = resample_depth(*m_terrain, m_terrain->entrainment_depth, *region, metres_per_world_unit, m_terrain->cellsize);
        if (cover.volume > 0.0) {
            texture_desc.label = WGPUStringView { .data = "avaframe entrainment on the map", .length = WGPU_STRLEN };
            texture_desc.size = { cover.depth.width(), cover.depth.height(), 1 };
            m_entrainment_texture = std::make_unique<webgpu::raii::TextureWithSampler>(m_ctx->device(), texture_desc, sampler_desc);
            m_entrainment_texture->texture().write(m_ctx->queue(), cover.depth);
            m_area.entrainment_texture = m_entrainment_texture.get();
            m_area.entrainment_min = cover.min;
            m_area.entrainment_max = cover.max;
            m_area.entrainment_volume = cover.volume;
            m_area.entrainment_area = cover.area;
            m_area.entrainment_max_depth = cover.max_depth;
        }
    }
    m_area_valid = true;
    m_status = m_area.label;
    const glm::dvec2 centroid_lat_lon = nucleus::srs::world_to_lat_long(region->min + release.centroid / metres_per_world_unit);
    qInfo().nospace() << "AvaFrameReleaseNode: " << QString::fromStdString(m_area.label) << " on the map, " << release.volume << " m3 on " << release.area
                      << " m2 (dataset: " << m_terrain->release_volume << " m3 on " << m_terrain->release_area << " m2), " << release.depth.width() << "x"
                      << release.depth.height() << " texels of " << release.texel_size.x << " m, centroid " << qSetRealNumberPrecision(8)
                      << centroid_lat_lon.x << ", " << centroid_lat_lon.y << ", placement residual " << m_terrain->sandbox_to_world_residual
                      << " world units, entrainment " << m_area.entrainment_volume << " m3 (dataset: " << m_terrain->entrainment_volume << " m3)";
    complete_run();
}

void AvaFrameReleaseNode::serialize_settings(QJsonObject& out) const
{
    out["dataset_path"] = QString::fromStdString(m_settings.dataset_path);
    out["release_scenario"] = QString::fromStdString(m_settings.release_scenario);
}

void AvaFrameReleaseNode::deserialize_settings(const QJsonObject& in)
{
    m_settings.dataset_path = in["dataset_path"].toString().toStdString();
    m_settings.release_scenario = in["release_scenario"].toString().toStdString();
}

} // namespace webgpu_compute::nodes
