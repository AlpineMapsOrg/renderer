/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

#include "ExportNode.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <assert.h>
#include <filesystem>
#include <limits>
#include <memory>
#include <nucleus/Raster.h>
#include <nucleus/utils/geopng_decoder.h>
#include <nucleus/utils/image_writer.h>

namespace webgpu_engine::compute::nodes {

static std::string resolve_placeholders(const std::string& pattern, const std::string& node_name, uint64_t run_id, const std::string& run_datetime)
{
    std::string result = pattern;
    auto replace = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    replace(result, "{node_name}", node_name);
    replace(result, "{run_id}", std::to_string(run_id));
    replace(result, "{run_datetime}", run_datetime);
    return result;
}

static void ensure_parent_dir(const std::string& file_path) { std::filesystem::create_directories(std::filesystem::path(file_path).parent_path()); }

static void write_texture_file(const QByteArray& data, glm::uvec2 dims, const std::string& file_path)
{
    const uint32_t bpp = static_cast<uint32_t>(data.size()) / (dims.x * dims.y);
    nucleus::Raster<glm::u8vec4> raster(dims);
    auto& buf = raster.buffer();
    for (uint32_t y = 0; y < dims.y; y++) {
        for (uint32_t x = 0; x < dims.x; x++) {
            const uint32_t idx = (y * dims.x + x) * bpp;
            buf[y * dims.x + x] = glm::u8vec4(static_cast<uint8_t>(data.at(idx + 0)),
                bpp > 1 ? static_cast<uint8_t>(data.at(idx + 1)) : 0u,
                bpp > 2 ? static_cast<uint8_t>(data.at(idx + 2)) : 0u,
                bpp > 3 ? static_cast<uint8_t>(data.at(idx + 3)) : 255u);
        }
    }
    ensure_parent_dir(file_path);
    nucleus::utils::image_writer::rgba8_as_png(raster, QString::fromStdString(file_path));
    qDebug() << "[ExportNode] texture written to" << QString::fromStdString(file_path);
}

static void write_buffer_file(const std::vector<uint32_t>& data, glm::uvec2 dims, const std::string& file_path)
{
    nucleus::Raster<float> raster(dims);
    for (size_t i = 0; i < data.size(); i++)
        raster.buffer()[i] = static_cast<float>(data[i]);
    ensure_parent_dir(file_path);
    nucleus::utils::geopng::write_encoded_float_png(raster, QString::fromStdString(file_path));
    qDebug() << "[ExportNode] buffer written to" << QString::fromStdString(file_path);
}

static void write_aabb_file(const std::string& file_path, const radix::geometry::Aabb<2, double>& bounds)
{
    ensure_parent_dir(file_path);
    QFile file(QString::fromStdString(file_path));
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream.setRealNumberPrecision(30);
        stream << bounds.min.x << "\n" << bounds.min.y << "\n" << bounds.max.x << "\n" << bounds.max.y << "\n";
        file.close();
        qDebug() << "[ExportNode] aabb written to" << QString::fromStdString(file_path);
    }
}

ExportNode::ExportNode(webgpu::Context& ctx)
    : ExportNode(ctx, ExportSettings {})
{
}

ExportNode::ExportNode(webgpu::Context& ctx, const ExportSettings& settings)
    : Node(
        {
            InputSocket(*this, "texture", data_type<const webgpu::raii::TextureWithSampler*>()),
            InputSocket(*this, "buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>()),
            InputSocket(*this, "dimensions", data_type<glm::uvec2>()),
            InputSocket(*this, "region aabb", data_type<const radix::geometry::Aabb<2, double>*>()),
        },
        {})
    , m_ctx(&ctx)
    , m_settings(settings)
{
}

void ExportNode::set_settings(const ExportSettings& settings) { m_settings = settings; }

void ExportNode::run_impl()
{
    const std::string node_name = get_node_name();
    const uint64_t run_id = get_run_id();
    const std::string run_datetime = get_run_datetime();

    const bool has_texture = input_socket("texture").is_socket_connected();
    const bool has_buffer = input_socket("buffer").is_socket_connected();
    const bool has_aabb = input_socket("region aabb").is_socket_connected();

    // Shared counter: both GPU readbacks are queued simultaneously; run_completed
    // fires only when all pending async ops have finished.
    auto pending = std::make_shared<int>(0);
    auto on_done = [this, pending]() {
        if (--(*pending) == 0)
            complete_run();
    };

    if (has_texture) {
        const auto& texture = *std::get<data_type<const webgpu::raii::TextureWithSampler*>()>(input_socket("texture").get_connected_data());
        const glm::uvec2 dims { texture.texture().width(), texture.texture().height() };
        const std::string path = resolve_placeholders(m_settings.texture_output_file, node_name, run_id, run_datetime);
        (*pending)++;
        texture.texture().read_back_async(m_ctx->device(), 0, [path, dims, on_done]([[maybe_unused]] size_t, std::shared_ptr<QByteArray> data) {
            write_texture_file(*data, dims, path);
            on_done();
        });
    }

    if (has_buffer) {
        if (!input_socket("dimensions").is_socket_connected()) {
            qWarning() << "[ExportNode] Buffer needs dimensions";
        } else {
            auto& buffer = *std::get<data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(input_socket("buffer").get_connected_data());
            const auto dims = std::get<data_type<glm::uvec2>()>(input_socket("dimensions").get_connected_data());
            const size_t expected = dims.x * dims.y;

            if (buffer.size() != expected) {
                qWarning() << "[ExportNode] buffer size mismatch. Expected:" << expected << "Got:" << buffer.size();
            } else {
                const std::string path = resolve_placeholders(m_settings.buffer_output_file, node_name, run_id, run_datetime);
                (*pending)++;
                buffer.read_back_async(m_ctx->device(), [path, dims, on_done](WGPUMapAsyncStatus status, std::vector<uint32_t> data) {
                    if (status == WGPUMapAsyncStatus_Success)
                        write_buffer_file(data, dims, path);
                    else
                        qWarning() << "[ExportNode] buffer readback failed:" << status;
                    on_done();
                });
            }
        }
    }

    // AABB is synchronous -> write immediately
    if (has_aabb) {
        const auto& aabb = *std::get<data_type<const radix::geometry::Aabb<2, double>*>()>(input_socket("region aabb").get_connected_data());
        write_aabb_file(resolve_placeholders(m_settings.aabb_output_file, node_name, run_id, run_datetime), aabb);
    }

    if (*pending == 0)
        complete_run();
}

} // namespace webgpu_engine::compute::nodes
