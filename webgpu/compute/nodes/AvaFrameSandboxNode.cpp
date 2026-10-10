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

#include "AvaFrameSandboxNode.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

namespace webgpu_compute::nodes {

namespace {

    /// An ESRI ASCII grid. Row 0 is the northern edge; (xll, yll) the lower-left corner.
    struct AsciiGrid {
        glm::uvec2 size = glm::uvec2(0);
        double xll = 0.0;
        double yll = 0.0;
        double cellsize = 0.0;
        std::vector<float> values; // NaN where NODATA
        float at(uint32_t col, uint32_t row) const { return values[size_t(row) * size.x + col]; }
    };

    /// Reads an ESRI ASCII grid. Accepts the xllcorner and xllcenter forms and any key case.
    std::optional<AsciiGrid> read_ascii_grid(const QString& path, std::string& error)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            error = "cannot open " + path.toStdString();
            return std::nullopt;
        }
        QTextStream in(&file);
        AsciiGrid grid;
        double nodata = -9999.0;
        bool centre = false;
        for (int i = 0; i < 6; i++) {
            QString key;
            QString value;
            in >> key >> value;
            key = key.toLower();
            if (key == "ncols")
                grid.size.x = value.toUInt();
            else if (key == "nrows")
                grid.size.y = value.toUInt();
            else if (key == "xllcorner" || key == "xllcenter") {
                grid.xll = value.toDouble();
                centre = key == "xllcenter";
            } else if (key == "yllcorner" || key == "yllcenter")
                grid.yll = value.toDouble();
            else if (key == "cellsize")
                grid.cellsize = value.toDouble();
            else if (key == "nodata_value")
                nodata = value.toDouble();
            else {
                error = "unexpected header key '" + key.toStdString() + "' in " + path.toStdString();
                return std::nullopt;
            }
        }
        if (grid.size.x == 0 || grid.size.y == 0 || grid.cellsize <= 0.0) {
            error = "incomplete header in " + path.toStdString();
            return std::nullopt;
        }
        if (centre) {
            grid.xll -= 0.5 * grid.cellsize;
            grid.yll -= 0.5 * grid.cellsize;
        }
        const size_t count = size_t(grid.size.x) * grid.size.y;
        grid.values.resize(count);
        for (size_t i = 0; i < count; i++) {
            double v = 0.0;
            in >> v;
            if (in.status() != QTextStream::Ok) {
                error = "too few values in " + path.toStdString();
                return std::nullopt;
            }
            grid.values[i] = std::abs(v - nodata) < 1e-6 ? std::numeric_limits<float>::quiet_NaN() : float(v);
        }
        return grid;
    }

    /// Fills NODATA cells with the value of the nearest valid cell (breadth-first, so nearest
    /// in steps). Outside AvaFrame's DEM outline the terrain then continues as a plateau at
    /// the edge height; the solver needs some height everywhere in its box.
    void fill_nodata(nucleus::Raster<float>& heights, const nucleus::Raster<uint8_t>& nodata)
    {
        const glm::ivec2 size(heights.size());
        std::deque<glm::ivec2> queue;
        std::vector<uint8_t> done(nodata.buffer().size(), 0);
        for (int y = 0; y < size.y; y++) {
            for (int x = 0; x < size.x; x++) {
                if (!nodata.pixel(glm::uvec2(x, y))) {
                    done[size_t(y) * size.x + x] = 1;
                    queue.emplace_back(x, y);
                }
            }
        }
        const glm::ivec2 steps[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        while (!queue.empty()) {
            const glm::ivec2 p = queue.front();
            queue.pop_front();
            for (const auto& step : steps) {
                const glm::ivec2 q = p + step;
                if (q.x < 0 || q.y < 0 || q.x >= size.x || q.y >= size.y || done[size_t(q.y) * size.x + q.x])
                    continue;
                done[size_t(q.y) * size.x + q.x] = 1;
                heights.pixel(glm::uvec2(q)) = heights.pixel(glm::uvec2(p));
                queue.push_back(q);
            }
        }
    }

    std::unique_ptr<webgpu::raii::TextureWithSampler> create_texture(WGPUDevice device, glm::uvec2 size, WGPUTextureFormat format, const char* label)
    {
        WGPUTextureDescriptor texture_desc {};
        texture_desc.label = WGPUStringView { .data = label, .length = WGPU_STRLEN };
        texture_desc.dimension = WGPUTextureDimension_2D;
        texture_desc.size = { size.x, size.y, 1 };
        texture_desc.mipLevelCount = 1;
        texture_desc.sampleCount = 1;
        texture_desc.format = format;
        texture_desc.usage = WGPUTextureUsage(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);

        WGPUSamplerDescriptor sampler_desc {};
        sampler_desc.label = WGPUStringView { .data = label, .length = WGPU_STRLEN };
        sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
        // R32Float is not filterable; the preview is, but nearest keeps DEM cells crisp when zoomed.
        sampler_desc.magFilter = WGPUFilterMode_Nearest;
        sampler_desc.minFilter = WGPUFilterMode_Nearest;
        sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        sampler_desc.lodMinClamp = 0.0f;
        sampler_desc.lodMaxClamp = 1.0f;
        sampler_desc.compare = WGPUCompareFunction_Undefined;
        sampler_desc.maxAnisotropy = 1;
        return std::make_unique<webgpu::raii::TextureWithSampler>(device, texture_desc, sampler_desc);
    }

    /// Hillshade (light from the north-west, 45 degrees up), NODATA dark, release cyan, com1DFA
    /// footprint outline green - the same colours as the Phase 1 preview images.
    nucleus::Raster<glm::u8vec4> make_preview(const SandboxTerrain& t)
    {
        const glm::ivec2 size(t.grid_size);
        nucleus::Raster<glm::u8vec4> image(t.grid_size);
        const auto h = [&](int x, int y) { return double(t.heights.pixel(glm::uvec2(std::clamp(x, 0, size.x - 1), std::clamp(y, 0, size.y - 1)))); };
        const glm::dvec3 light = glm::normalize(glm::dvec3(-1.0, 1.0, std::sqrt(2.0))); // x east, y north, z up
        const auto footprint = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < size.x && y < size.y && !t.reference_footprint.buffer().empty()
                && t.reference_footprint.pixel(glm::uvec2(x, y)) != 0;
        };
        for (int y = 0; y < size.y; y++) {
            for (int x = 0; x < size.x; x++) {
                // Row index grows southwards, so north is -y in the raster.
                const double dzdx = (h(x + 1, y) - h(x - 1, y)) / (2.0 * t.cellsize);
                const double dzdy = (h(x, y - 1) - h(x, y + 1)) / (2.0 * t.cellsize);
                const glm::dvec3 normal = glm::normalize(glm::dvec3(-dzdx, -dzdy, 1.0));
                const double shade = std::clamp(glm::dot(normal, light), 0.0, 1.0);
                glm::dvec3 colour = glm::dvec3(0.25 + 0.7 * shade);
                const glm::uvec2 p(x, y);
                if (t.nodata.pixel(p))
                    colour *= 0.35;
                if (t.release_depth.pixel(p) > 0.0f)
                    colour = glm::mix(colour, glm::dvec3(0.1, 0.85, 0.95), 0.6);
                if (footprint(x, y) && !(footprint(x - 1, y) && footprint(x + 1, y) && footprint(x, y - 1) && footprint(x, y + 1)))
                    colour = glm::dvec3(0.1, 0.8, 0.2);
                image.pixel(p) = glm::u8vec4(glm::u8vec3(glm::clamp(colour, 0.0, 1.0) * 255.0), 255);
            }
        }
        return image;
    }

} // namespace

AvaFrameSandboxNode::AvaFrameSandboxNode(webgpu::Context& ctx)
    : AvaFrameSandboxNode(ctx, AvaFrameSandboxSettings())
{
}

AvaFrameSandboxNode::AvaFrameSandboxNode(webgpu::Context& ctx, const AvaFrameSandboxSettings& settings)
    : Node({},
          {
              OutputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>(), [this]() { return &m_region_aabb; }),
              OutputSocket(*this, "height texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_height_texture.get(); }),
              OutputSocket(*this, "release texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_release_texture.get(); }),
              OutputSocket(*this, "sandbox", data_type<const SandboxTerrain*>(), [this]() { return m_terrain.get(); }),
              OutputSocket(*this, "preview texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_preview_texture.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings(settings)
    , m_region_aabb { glm::dvec2(0.0), glm::dvec2(0.0) }
{
}

void AvaFrameSandboxNode::run_impl()
{
    if (m_settings.dataset_path.empty()) {
        fail_run("AvaFrameSandboxNode: no dataset chosen (Avalanche panel, AvaFrame sandbox).");
        return;
    }
    const QDir dir(QString::fromStdString(m_settings.dataset_path));
    QFile meta_file(dir.filePath("dataset.json"));
    if (!meta_file.open(QIODevice::ReadOnly)) {
        fail_run("AvaFrameSandboxNode: no dataset.json in " + m_settings.dataset_path + " - run mpm-mls-doc/avaframe/prepare_dataset.py first.");
        return;
    }
    const QJsonObject meta = QJsonDocument::fromJson(meta_file.readAll()).object();

    // Release scenario: the requested one, else the first.
    const QJsonArray scenarios = meta["release_scenarios"].toArray();
    QJsonObject scenario;
    for (const auto& value : scenarios) {
        const QJsonObject candidate = value.toObject();
        if (scenario.isEmpty() || candidate["name"].toString().toStdString() == m_settings.release_scenario)
            scenario = candidate;
        if (candidate["name"].toString().toStdString() == m_settings.release_scenario)
            break;
    }
    if (scenario.isEmpty()) {
        fail_run("AvaFrameSandboxNode: dataset " + m_settings.dataset_path + " has no release scenario.");
        return;
    }

    std::string error;
    const auto dem = read_ascii_grid(dir.filePath("dem.asc"), error);
    const auto release = dem ? read_ascii_grid(dir.filePath(scenario["file"].toString()), error) : std::nullopt;
    if (!dem || !release) {
        fail_run("AvaFrameSandboxNode: " + error);
        return;
    }
    if (release->size != dem->size) {
        fail_run("AvaFrameSandboxNode: release grid does not match the DEM in " + m_settings.dataset_path);
        return;
    }

    auto t = std::make_unique<SandboxTerrain>();
    t->name = meta["name"].toString().toStdString();
    t->scenario = scenario["name"].toString().toStdString();
    t->dataset_path = m_settings.dataset_path;
    t->grid_size = dem->size;
    t->cellsize = dem->cellsize;
    t->size_metres = glm::dvec2(dem->size) * dem->cellsize;
    t->source_origin = glm::dvec2(dem->xll, dem->yll);

    t->heights = nucleus::Raster<float>(dem->size, 0.0f);
    t->nodata = nucleus::Raster<uint8_t>(dem->size, uint8_t(0));
    for (size_t i = 0; i < dem->values.size(); i++) {
        const bool missing = std::isnan(dem->values[i]);
        t->nodata.buffer()[i] = missing ? 1 : 0;
        t->heights.buffer()[i] = missing ? 0.0f : dem->values[i];
    }
    fill_nodata(t->heights, t->nodata);

    // Release: AvaFrame's thickness is slope-normal; the solver stacks particles vertically,
    // so it gets thickness / cos(slope). Slope from central differences on the DEM, as in
    // prepare_dataset.py (and com1DFA's own cell normals).
    const glm::ivec2 size(dem->size);
    const auto h = [&](int x, int y) { return double(t->heights.pixel(glm::uvec2(std::clamp(x, 0, size.x - 1), std::clamp(y, 0, size.y - 1)))); };
    t->release_depth = nucleus::Raster<float>(dem->size, 0.0f);
    t->release_min = glm::dvec2(std::numeric_limits<double>::max());
    t->release_max = glm::dvec2(std::numeric_limits<double>::lowest());
    const double cell_area = dem->cellsize * dem->cellsize;
    for (int y = 0; y < size.y; y++) {
        for (int x = 0; x < size.x; x++) {
            const float thickness = release->at(uint32_t(x), uint32_t(y));
            if (!(thickness > 0.0f) || t->nodata.pixel(glm::uvec2(x, y)))
                continue;
            const double dzdx = (h(x + 1, y) - h(x - 1, y)) / (2.0 * dem->cellsize);
            const double dzdy = (h(x, y - 1) - h(x, y + 1)) / (2.0 * dem->cellsize);
            const double cos_slope = 1.0 / std::sqrt(1.0 + dzdx * dzdx + dzdy * dzdy);
            const float depth = float(double(thickness) / cos_slope);
            t->release_depth.pixel(glm::uvec2(x, y)) = depth;
            t->release_volume += double(depth) * cell_area;
            t->release_area += cell_area;
            t->release_max_depth = std::max(t->release_max_depth, depth);
            const glm::dvec2 cell_min(double(x) * dem->cellsize, double(size.y - 1 - y) * dem->cellsize);
            t->release_min = glm::min(t->release_min, cell_min);
            t->release_max = glm::max(t->release_max, cell_min + glm::dvec2(dem->cellsize));
        }
    }
    if (t->release_volume <= 0.0) {
        fail_run("AvaFrameSandboxNode: release scenario " + t->scenario + " is empty.");
        return;
    }

    // com1DFA footprint of this scenario, on the DEM grid (nearest cell of the reference grid,
    // which com1DFA remeshes to its own spacing).
    glm::dvec2 extent_min = t->release_min;
    glm::dvec2 extent_max = t->release_max;
    for (const auto& value : meta["references"].toArray()) {
        const QJsonObject ref = value.toObject();
        if (ref["release_scenario"].toString().toStdString() != t->scenario)
            continue;
        const auto pft = read_ascii_grid(dir.filePath(ref["files"].toObject()["pft"].toString()), error);
        if (!pft) {
            qWarning() << "AvaFrameSandboxNode: reference not loaded:" << QString::fromStdString(error);
            break;
        }
        t->reference_name = ref["benchmark"].toString().toStdString();
        t->reference_footprint = nucleus::Raster<uint8_t>(dem->size, uint8_t(0));
        for (int y = 0; y < size.y; y++) {
            for (int x = 0; x < size.x; x++) {
                const glm::dvec2 source = t->source_origin + glm::dvec2(x + 0.5, size.y - 1 - y + 0.5) * dem->cellsize;
                const glm::dvec2 cell = (source - glm::dvec2(pft->xll, pft->yll)) / pft->cellsize;
                if (cell.x < 0.0 || cell.y < 0.0 || cell.x >= double(pft->size.x) || cell.y >= double(pft->size.y))
                    continue;
                const float value = pft->at(uint32_t(cell.x), pft->size.y - 1 - uint32_t(cell.y));
                if (value > 0.0f) {
                    t->reference_footprint.pixel(glm::uvec2(x, y)) = 1;
                    const glm::dvec2 local = glm::dvec2(x, size.y - 1 - y) * dem->cellsize;
                    extent_min = glm::min(extent_min, local);
                    extent_max = glm::max(extent_max, local + glm::dvec2(dem->cellsize));
                }
            }
        }
        break;
    }

    // Suggested domain: release and reference footprint plus 15 % (at least 100 m) on each
    // side; without a reference the whole DEM, since the runout is unknown.
    if (t->reference_footprint.buffer().empty()) {
        t->suggested_domain_centre = t->size_metres * 0.5;
        t->suggested_domain_size = std::max(t->size_metres.x, t->size_metres.y);
    } else {
        const glm::dvec2 extent = extent_max - extent_min;
        const double margin = std::max(100.0, 0.15 * std::max(extent.x, extent.y));
        t->suggested_domain_centre = (extent_min + extent_max) * 0.5;
        t->suggested_domain_size = std::max(extent.x, extent.y) + 2.0 * margin;
    }

    const QJsonArray centre = meta["location"].toObject()["centre_lat_lon"].toArray();
    if (centre.size() == 2)
        t->centre_lat_lon = glm::dvec2(centre[0].toDouble(), centre[1].toDouble());

    // GPU side: heights and release depth as R32Float (the solver uses textureLoad), the
    // preview as RGBA8 for ImGui.
    m_height_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_R32Float, "avaframe sandbox heights");
    m_height_texture->texture().write(m_ctx->queue(), t->heights);
    m_release_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_R32Float, "avaframe sandbox release depth");
    m_release_texture->texture().write(m_ctx->queue(), t->release_depth);
    m_preview_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_RGBA8Unorm, "avaframe sandbox preview");
    m_preview_texture->texture().write(m_ctx->queue(), make_preview(*t));

    m_region_aabb = { glm::dvec2(0.0), t->size_metres };

    qInfo().nospace() << "AvaFrameSandboxNode: " << QString::fromStdString(t->name) << " / " << QString::fromStdString(t->scenario) << ", DEM "
                      << t->grid_size.x << "x" << t->grid_size.y << " @ " << t->cellsize << " m, release " << t->release_volume << " m3 on "
                      << t->release_area << " m2 (max depth " << t->release_max_depth << " m), reference "
                      << (t->reference_name.empty() ? QString("none") : QString::fromStdString(t->reference_name)) << ", suggested domain "
                      << t->suggested_domain_size << " m";
    m_terrain = std::move(t);
    complete_run();
}

void AvaFrameSandboxNode::serialize_settings(QJsonObject& out) const
{
    out["dataset_path"] = QString::fromStdString(m_settings.dataset_path);
    out["release_scenario"] = QString::fromStdString(m_settings.release_scenario);
}

void AvaFrameSandboxNode::deserialize_settings(const QJsonObject& in)
{
    if (in.contains("dataset_path"))
        m_settings.dataset_path = in["dataset_path"].toString().toStdString();
    if (in.contains("release_scenario"))
        m_settings.release_scenario = in["release_scenario"].toString().toStdString();
}

} // namespace webgpu_compute::nodes
