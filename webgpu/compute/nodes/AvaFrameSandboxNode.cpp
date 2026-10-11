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

#include <QDateTime>
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
#include <nucleus/srs.h>

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

    bool write_ascii_grid(const QString& path, const SandboxTerrain& t, const std::vector<float>& values)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&file);
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(9); // the header as precise as prepare_dataset.py writes it
        out << "ncols " << t.grid_size.x << "\nnrows " << t.grid_size.y << "\nxllcorner " << t.source_origin.x << "\nyllcorner "
            << t.source_origin.y << "\ncellsize " << t.cellsize << "\nNODATA_value -9999\n";
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(4);
        for (uint32_t y = 0; y < t.grid_size.y; y++) {
            for (uint32_t x = 0; x < t.grid_size.x; x++)
                out << (x ? " " : "") << values[size_t(y) * t.grid_size.x + x];
            out << "\n";
        }
        return out.status() == QTextStream::Ok;
    }

    /// Least-squares affine map from four point pairs; residual = largest corner error.
    glm::dmat3 fit_affine(const glm::dvec2 (&from)[4], const glm::dvec2 (&to)[4], double& residual)
    {
        // Normal equations for each output coordinate: [x y 1] a = to.
        glm::dmat3 ata(0.0);
        glm::dvec3 atx(0.0), aty(0.0);
        for (int i = 0; i < 4; i++) {
            const glm::dvec3 row(from[i], 1.0);
            ata += glm::outerProduct(row, row);
            atx += row * to[i].x;
            aty += row * to[i].y;
        }
        const glm::dmat3 inv = glm::inverse(ata);
        const glm::dvec3 ax = inv * atx;
        const glm::dvec3 ay = inv * aty;
        // Column-major: world = M * (x, y, 1).
        const glm::dmat3 m(glm::dvec3(ax.x, ay.x, 0.0), glm::dvec3(ax.y, ay.y, 0.0), glm::dvec3(ax.z, ay.z, 1.0));
        residual = 0.0;
        for (int i = 0; i < 4; i++)
            residual = std::max(residual, glm::length(glm::dvec2(m * glm::dvec3(from[i], 1.0)) - to[i]));
        return m;
    }

} // namespace

SandboxPeaks resample_sandbox_peaks(const SandboxTerrain& t, const MpmSolverNode::ColumnPeaks& peaks, float density)
{
    SandboxPeaks out;
    out.simulated_time = peaks.simulated_time;
    const size_t cells = size_t(t.grid_size.x) * t.grid_size.y;
    out.pft.assign(cells, 0.0f);
    out.pfv.assign(cells, 0.0f);
    out.ppr.assign(cells, 0.0f);
    const glm::ivec2 size(t.grid_size);
    const auto h = [&](int x, int y) { return double(t.heights.pixel(glm::uvec2(std::clamp(x, 0, size.x - 1), std::clamp(y, 0, size.y - 1)))); };
    for (int y = 0; y < size.y; y++) {
        for (int x = 0; x < size.x; x++) {
            // Cell centre in sandbox metres -> nearest grid column.
            const glm::dvec2 centre = glm::dvec2(x + 0.5, size.y - 1 - y + 0.5) * t.cellsize;
            const glm::dvec2 node = glm::round((centre - peaks.domain_origin) / double(peaks.dx));
            if (node.x < 0.0 || node.y < 0.0 || node.x >= double(peaks.size.x) || node.y >= double(peaks.size.y))
                continue;
            const size_t column = size_t(node.y) * peaks.size.x + size_t(node.x);
            const double dzdx = (h(x + 1, y) - h(x - 1, y)) / (2.0 * t.cellsize);
            const double dzdy = (h(x, y - 1) - h(x, y + 1)) / (2.0 * t.cellsize);
            const double cos_slope = 1.0 / std::sqrt(1.0 + dzdx * dzdx + dzdy * dzdy);
            const size_t i = size_t(y) * size.x + x;
            out.pft[i] = float(double(peaks.depth[column]) * cos_slope);
            out.pfv[i] = out.pft[i] > 0.0f ? peaks.speed[column] : 0.0f;
            out.ppr[i] = density * out.pfv[i] * out.pfv[i] / 1000.0f;
        }
    }
    return out;
}

SandboxScale sandbox_scale(SandboxQuantity quantity)
{
    switch (quantity) {
    case SandboxQuantity::Velocity:
        return { "peak flow velocity", "m/s", 0.0f, 100.0f, false };
    case SandboxQuantity::Pressure:
        return { "peak pressure", "kPa", 1.0f, 1000.0f, true };
    default:
        return { "peak flow thickness", "m", 0.0f, 5.0f, false };
    }
}

glm::u8vec4 sandbox_colour(SandboxQuantity quantity, float value)
{
    if (!(value > 0.0f))
        return glm::u8vec4(0);
    const SandboxScale scale = sandbox_scale(quantity);
    float f = scale.logarithmic ? (std::log10(std::max(value, scale.min)) - std::log10(scale.min)) / (std::log10(scale.max) - std::log10(scale.min))
                                : (value - scale.min) / (scale.max - scale.min);
    f = std::clamp(f, 0.0f, 1.0f);
    // Viridis at five stops.
    static constexpr glm::vec3 STOPS[] = { { 0.267f, 0.005f, 0.329f }, { 0.230f, 0.322f, 0.546f }, { 0.128f, 0.567f, 0.551f },
        { 0.369f, 0.789f, 0.383f }, { 0.993f, 0.906f, 0.144f } };
    const float s = f * 4.0f;
    const int i = std::min(int(s), 3);
    const glm::vec3 rgb = glm::mix(STOPS[i], STOPS[i + 1], s - float(i));
    return glm::u8vec4(glm::u8vec3(rgb * 255.0f), 220);
}

nucleus::Raster<glm::u8vec4> colour_sandbox_raster(
    const SandboxTerrain& t, const std::vector<float>& values, const std::vector<float>& thickness, SandboxQuantity quantity)
{
    nucleus::Raster<glm::u8vec4> image(t.grid_size, glm::u8vec4(0));
    for (size_t i = 0; i < values.size() && i < image.buffer().size(); i++) {
        glm::u8vec4 colour = sandbox_colour(quantity, values[i]);
        if (i < thickness.size() && thickness[i] < SANDBOX_FOOTPRINT_THICKNESS)
            colour.w = colour.w / 4;
        image.buffer()[i] = colour;
    }
    return image;
}

SandboxRunSummary write_sandbox_result(
    const SandboxTerrain& t, const SandboxPeaks& peaks, const MpmSolverNode::ColumnPeaks& columns, float density, const QJsonObject& run_info)
{
    SandboxRunSummary summary;
    const auto& pft = peaks.pft;
    const auto& pfv = peaks.pfv;
    const auto& ppr = peaks.ppr;
    const double cell_area = t.cellsize * t.cellsize;
    for (size_t i = 0; i < pft.size(); i++) {
        summary.max_thickness = std::max(summary.max_thickness, pft[i]);
        summary.max_speed = std::max(summary.max_speed, pfv[i]);
        summary.footprint_area += pft[i] > 0.0f ? cell_area : 0.0;
        summary.footprint_area_10cm += pft[i] > SANDBOX_FOOTPRINT_THICKNESS ? cell_area : 0.0;
    }

    QJsonObject params = run_info;
    params["dataset"] = QString::fromStdString(t.name);
    params["release_scenario"] = QString::fromStdString(t.scenario);
    params["reference"] = QString::fromStdString(t.reference_name);
    params["release_volume_m3"] = t.release_volume;
    params["simulated_time_s"] = double(columns.simulated_time);
    params["grid"] = QJsonObject { { "columns", int(columns.size.x) }, { "rows", int(columns.size.y) }, { "dx_m", double(columns.dx) },
        { "domain_origin_m", QJsonArray { columns.domain_origin.x, columns.domain_origin.y } } };
    params["density_kg_m3"] = double(density);
    params["max_pft_m"] = double(summary.max_thickness);
    params["max_pfv_m_s"] = double(summary.max_speed);
    params["footprint_area_m2"] = summary.footprint_area;
    params["footprint_area_pft_gt_0.1m_m2"] = summary.footprint_area_10cm;
    params["conventions"] = "pft slope-normal (vertical column depth * cos slope), pfv mean speed of the particles in the grid column, ppr = rho pfv^2 / 1000; "
                            "values from the nearest MPM grid column at each DEM cell centre, 0 outside the simulation domain";
    write_run_files(t, pft, pfv, ppr, params, summary);
    return summary;
}

void write_run_files(const SandboxTerrain& t, const std::vector<float>& pft, const std::vector<float>& pfv, const std::vector<float>& ppr, QJsonObject params,
    SandboxRunSummary& summary)
{
    const QString label = params["label"].toString();
    const QString run_name = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "_" + QString::fromStdString(t.scenario)
        + (label.isEmpty() ? QString() : "_" + label);
    QDir dir(QString::fromStdString(t.dataset_path));
    if (!dir.mkpath("mpm/" + run_name)) {
        summary.error = "cannot create " + dir.filePath("mpm/" + run_name).toStdString();
        return;
    }
    dir.cd("mpm/" + run_name);
    if (!write_ascii_grid(dir.filePath("pft.asc"), t, pft) || !write_ascii_grid(dir.filePath("pfv.asc"), t, pfv)
        || !write_ascii_grid(dir.filePath("ppr.asc"), t, ppr)) {
        summary.error = "cannot write the rasters in " + dir.path().toStdString();
        return;
    }
    params["dataset"] = QString::fromStdString(t.name);
    params["release_scenario"] = QString::fromStdString(t.scenario);
    params["reference"] = QString::fromStdString(t.reference_name);
    QFile params_file(dir.filePath("params.json"));
    if (!params_file.open(QIODevice::WriteOnly)) {
        summary.error = "cannot write params.json";
        return;
    }
    params_file.write(QJsonDocument(params).toJson());
    summary.directory = dir.path().toStdString();
}

void AvaFrameSandboxNode::export_trajectories(webgpu::raii::RawBuffer<uint32_t>& zdelta, webgpu::raii::RawBuffer<uint32_t>& counts, QJsonObject run_info,
    std::function<void(const SandboxRunSummary&)> done)
{
    const SandboxTerrain* t = m_terrain.get();
    if (t == nullptr || zdelta.size() != size_t(t->grid_size.x) * t->grid_size.y || counts.size() != zdelta.size()) {
        SandboxRunSummary failed;
        failed.error = "trajectory layers do not match the DEM grid (resolution multiplier must be 1)";
        done(failed);
        return;
    }
    // Two readbacks in a row; the buffers belong to the trajectory node, which must stay alive
    // until done fires (the batch waits for it).
    const std::weak_ptr<bool> alive = m_alive;
    auto* counts_buffer = &counts;
    WGPUDevice device = m_ctx->device();
    zdelta.read_back_async(device, [this, alive, counts_buffer, device, run_info, done](WGPUMapAsyncStatus status, std::vector<uint32_t> zd) {
        if (alive.expired() || status != WGPUMapAsyncStatus_Success)
            return;
        counts_buffer->read_back_async(device, [this, alive, zd = std::move(zd), run_info, done](WGPUMapAsyncStatus status, std::vector<uint32_t> cc) {
            if (alive.expired() || status != WGPUMapAsyncStatus_Success || m_terrain == nullptr)
                return;
            const SandboxTerrain& t = *m_terrain;
            // The model's own output (avalanche_trajectories_compute.wgsl): zdelta = energy-line
            // height [m, whole metres], cellCounts = steps through the cell. Mapped like
            // com4FlowPy's (run_com4flowpy.py): footprint flag, v = sqrt(2 g zdelta), rho v^2.
            const size_t cells = cc.size();
            std::vector<float> pft(cells), pfv(cells), ppr(cells);
            SandboxRunSummary summary;
            const double cell_area = t.cellsize * t.cellsize;
            for (size_t i = 0; i < cells; i++) {
                pft[i] = cc[i] > 0 ? 1.0f : 0.0f;
                pfv[i] = cc[i] > 0 ? std::sqrt(2.0f * 9.81f * float(zd[i])) : 0.0f;
                ppr[i] = 200.0f * pfv[i] * pfv[i] / 1000.0f;
                summary.max_speed = std::max(summary.max_speed, pfv[i]);
                summary.footprint_area += pft[i] > 0.0f ? cell_area : 0.0;
            }
            QJsonObject params = run_info;
            params["model"] = "weBIGeo trajectories";
            params["max_pfv_m_s"] = double(summary.max_speed);
            params["footprint_area_m2"] = summary.footprint_area;
            params["conventions"] = "pft = footprint flag (cellCounts > 0), pfv = sqrt(2 g zdelta), ppr = 200 pfv^2 / 1000 (like run_com4flowpy.py)";
            write_run_files(t, pft, pfv, ppr, params, summary);
            done(summary);
        });
    });
}

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
              OutputSocket(*this, "map texture", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_map_texture.get(); }),
              OutputSocket(*this, "map aabb", data_type<const radix::geometry::Aabb<2, double>*>(), [this]() { return &m_map_aabb; }),
              OutputSocket(*this, "release mask", data_type<const webgpu::raii::TextureWithSampler*>(), [this]() { return m_release_mask_texture.get(); }),
          })
    , m_ctx(&ctx)
    , m_settings(settings)
    , m_region_aabb { glm::dvec2(0.0), glm::dvec2(0.0) }
    , m_map_aabb { glm::dvec2(0.0), glm::dvec2(1.0) }
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
        fail_run("AvaFrameSandboxNode: no dataset.json in " + m_settings.dataset_path + " - download the AvaFrame data in the Avalanche panel, or run mpm-mls-doc/avaframe/prepare_dataset.py.");
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
        const QJsonObject files = ref["files"].toObject();
        const auto pft = read_ascii_grid(dir.filePath(files["pft"].toString()), error);
        const auto pfv = pft ? read_ascii_grid(dir.filePath(files["pfv"].toString()), error) : std::nullopt;
        const auto ppr = pfv ? read_ascii_grid(dir.filePath(files["ppr"].toString()), error) : std::nullopt;
        if (!ppr) {
            qWarning() << "AvaFrameSandboxNode: reference not loaded:" << QString::fromStdString(error);
            break;
        }
        t->reference_name = ref["benchmark"].toString().toStdString();
        t->reference_footprint = nucleus::Raster<uint8_t>(dem->size, uint8_t(0));
        const size_t cells = size_t(size.x) * size.y;
        t->reference_pft.assign(cells, 0.0f);
        t->reference_pfv.assign(cells, 0.0f);
        t->reference_ppr.assign(cells, 0.0f);
        for (int y = 0; y < size.y; y++) {
            for (int x = 0; x < size.x; x++) {
                const glm::dvec2 source = t->source_origin + glm::dvec2(x + 0.5, size.y - 1 - y + 0.5) * dem->cellsize;
                const glm::dvec2 cell = (source - glm::dvec2(pft->xll, pft->yll)) / pft->cellsize;
                if (cell.x < 0.0 || cell.y < 0.0 || cell.x >= double(pft->size.x) || cell.y >= double(pft->size.y))
                    continue;
                // The three reference rasters share com1DFA's grid.
                const uint32_t col = uint32_t(cell.x);
                const uint32_t row = pft->size.y - 1 - uint32_t(cell.y);
                const float value = pft->at(col, row);
                const size_t i = size_t(y) * size.x + x;
                t->reference_pft[i] = std::isnan(value) ? 0.0f : value;
                t->reference_pfv[i] = std::isnan(pfv->at(col, row)) ? 0.0f : pfv->at(col, row);
                t->reference_ppr[i] = std::isnan(ppr->at(col, row)) ? 0.0f : ppr->at(col, row);
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

    const QJsonObject location = meta["location"].toObject();
    const QJsonArray centre = location["centre_lat_lon"].toArray();
    if (centre.size() == 2)
        t->centre_lat_lon = glm::dvec2(centre[0].toDouble(), centre[1].toDouble());
    const QJsonArray corners = location["corners_lat_lon"].toArray();
    if (corners.size() == 4) {
        // prepare_dataset.py lists the DEM corners SW, SE, NE, NW.
        const glm::dvec2 local[4] = { { 0.0, 0.0 }, { t->size_metres.x, 0.0 }, t->size_metres, { 0.0, t->size_metres.y } };
        glm::dvec2 world[4];
        for (int i = 0; i < 4; i++) {
            const QJsonArray c = corners[i].toArray();
            world[i] = nucleus::srs::lat_long_to_world(glm::dvec2(c[0].toDouble(), c[1].toDouble()));
        }
        t->sandbox_to_world = fit_affine(local, world, t->sandbox_to_world_residual);
    }

    // GPU side: heights and release depth as R32Float (the solver uses textureLoad), the
    // preview as RGBA8 for ImGui.
    m_height_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_R32Float, "avaframe sandbox heights");
    m_height_texture->texture().write(m_ctx->queue(), t->heights);
    m_release_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_R32Float, "avaframe sandbox release depth");
    m_release_texture->texture().write(m_ctx->queue(), t->release_depth);
    m_preview_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_RGBA8Unorm, "avaframe sandbox preview");
    m_preview_texture->texture().write(m_ctx->queue(), make_preview(*t));
    // For models that take a release point mask (alpha > 0), like the trajectory model.
    nucleus::Raster<glm::u8vec4> mask(t->grid_size, glm::u8vec4(0));
    for (size_t i = 0; i < mask.buffer().size(); i++)
        mask.buffer()[i] = t->release_depth.buffer()[i] > 0.0f ? glm::u8vec4(255) : glm::u8vec4(0);
    m_release_mask_texture = create_texture(m_ctx->device(), t->grid_size, WGPUTextureFormat_RGBA8Unorm, "avaframe sandbox release mask");
    m_release_mask_texture->texture().write(m_ctx->queue(), mask);

    m_region_aabb = { glm::dvec2(0.0), t->size_metres };
    m_layer_texture.reset();

    qInfo().nospace() << "AvaFrameSandboxNode: " << QString::fromStdString(t->name) << " / " << QString::fromStdString(t->scenario) << ", DEM "
                      << t->grid_size.x << "x" << t->grid_size.y << " @ " << t->cellsize << " m, release " << t->release_volume << " m3 on "
                      << t->release_area << " m2 (max depth " << t->release_max_depth << " m), reference "
                      << (t->reference_name.empty() ? QString("none") : QString::fromStdString(t->reference_name)) << ", suggested domain "
                      << t->suggested_domain_size << " m";
    if (t->sandbox_to_world)
        qInfo().nospace() << "AvaFrameSandboxNode: map placement by an affine fit to the DEM corners, largest residual "
                          << t->sandbox_to_world_residual << " world units";
    m_terrain = std::move(t);
    update_map_overlay(nullptr);
    complete_run();
}

AvaFrameSandboxNode::~AvaFrameSandboxNode() { *m_alive = false; }

void AvaFrameSandboxNode::set_layer_image(const nucleus::Raster<glm::u8vec4>& image)
{
    if (!m_layer_texture || m_layer_texture->texture().width() != image.width() || m_layer_texture->texture().height() != image.height())
        m_layer_texture = create_texture(m_ctx->device(), image.size(), WGPUTextureFormat_RGBA8Unorm, "avaframe sandbox layer");
    m_layer_texture->texture().write(m_ctx->queue(), image);
}

void AvaFrameSandboxNode::update_map_overlay(const std::vector<float>* mpm_pft)
{
    const SandboxTerrain* t = m_terrain.get();
    if (t == nullptr || !t->sandbox_to_world) {
        // Never null: the overlay node links the texture and the overlay asserts on it.
        if (!m_map_texture || m_map_texture->texture().width() != 1) {
            m_map_texture = create_texture(m_ctx->device(), glm::uvec2(1), WGPUTextureFormat_RGBA8Unorm, "avaframe sandbox map overlay");
            m_map_texture->texture().write(m_ctx->queue(), nucleus::Raster<glm::u8vec4>(glm::uvec2(1), glm::u8vec4(0)));
        }
        m_map_aabb = { glm::dvec2(0.0), glm::dvec2(1.0) };
        return;
    }
    const glm::dmat3& to_world = *t->sandbox_to_world;
    const glm::dmat3 to_sandbox = glm::inverse(to_world);
    // World-aligned box around the DEM, at the DEM's resolution on the ground.
    glm::dvec2 lo(std::numeric_limits<double>::max()), hi(std::numeric_limits<double>::lowest());
    for (const glm::dvec2 corner : { glm::dvec2(0.0), glm::dvec2(t->size_metres.x, 0.0), t->size_metres, glm::dvec2(0.0, t->size_metres.y) }) {
        const glm::dvec2 w = glm::dvec2(to_world * glm::dvec3(corner, 1.0));
        lo = glm::min(lo, w);
        hi = glm::max(hi, w);
    }
    const double texel = t->cellsize * glm::length(glm::dvec2(to_world[0])); // world units per DEM cell along x
    const glm::uvec2 size = glm::uvec2(glm::ceil((hi - lo) / texel));
    m_map_aabb = { lo, lo + glm::dvec2(size) * texel };

    nucleus::Raster<glm::u8vec4> image(size, glm::u8vec4(0));
    const glm::ivec2 grid(t->grid_size);
    const auto cell_at = [&](int x, int y) { return x >= 0 && y >= 0 && x < grid.x && y < grid.y; };
    const auto footprint = [&](int x, int y) {
        return cell_at(x, y) && !t->reference_footprint.buffer().empty() && t->reference_footprint.pixel(glm::uvec2(x, y)) != 0;
    };
    for (uint32_t py = 0; py < size.y; py++) {
        for (uint32_t px = 0; px < size.x; px++) {
            // Texture row 0 is the northern edge of the AABB.
            const glm::dvec2 world(lo.x + (px + 0.5) * texel, m_map_aabb.max.y - (py + 0.5) * texel);
            const glm::dvec2 local = glm::dvec2(to_sandbox * glm::dvec3(world, 1.0));
            const int x = int(std::floor(local.x / t->cellsize));
            const int y = grid.y - 1 - int(std::floor(local.y / t->cellsize));
            if (!cell_at(x, y) || t->nodata.pixel(glm::uvec2(x, y)))
                continue;
            const size_t i = size_t(y) * grid.x + x;
            glm::u8vec4& out = image.pixel(glm::uvec2(px, py));
            if (t->release_depth.buffer()[i] > 0.0f)
                out = glm::u8vec4(25, 215, 240, 150);
            if (mpm_pft != nullptr && (*mpm_pft)[i] > SANDBOX_FOOTPRINT_THICKNESS)
                out = sandbox_colour(SandboxQuantity::Thickness, (*mpm_pft)[i]);
            if (footprint(x, y) && !(footprint(x - 1, y) && footprint(x + 1, y) && footprint(x, y - 1) && footprint(x, y + 1)))
                out = glm::u8vec4(25, 205, 50, 255);
        }
    }
    if (!m_map_texture || m_map_texture->texture().width() != size.x || m_map_texture->texture().height() != size.y)
        m_map_texture = create_texture(m_ctx->device(), size, WGPUTextureFormat_RGBA8Unorm, "avaframe sandbox map overlay");
    m_map_texture->texture().write(m_ctx->queue(), image);
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
