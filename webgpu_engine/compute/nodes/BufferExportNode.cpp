/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "BufferExportNode.h"

#include <QDebug>
#include <QString>
#include <assert.h>
#include <filesystem>
#include <limits>
#include <nucleus/srs.h>
#include <nucleus/utils/image_writer.h>
#include <qdir.h>

namespace webgpu_engine::compute::nodes {

BufferExportNode::BufferExportNode(WGPUDevice device, const ExportSettings& settings)
    : Node(
          {
              InputSocket(*this, "buffer", data_type<webgpu::raii::RawBuffer<uint32_t>*>()),
              InputSocket(*this, "dimensions", data_type<glm::uvec2>()),
          },
          {})
    , m_device(device)
    , m_settings(settings)
{
}

void BufferExportNode::set_settings(const ExportSettings& settings) { m_settings = settings; }

void BufferExportNode::run_impl()
{
    qDebug() << "running BufferExportNode ...";

    const auto dimensions = std::get<data_type<glm::uvec2>()>(input_socket("dimensions").get_connected_data());
    auto& buffer = *std::get<data_type<webgpu::raii::RawBuffer<uint32_t>*>()>(input_socket("buffer").get_connected_data());

    // Check if buffer size matches expected dimensions
    const size_t expected_size = dimensions.x * dimensions.y;
    if (buffer.size() != expected_size) {
        qWarning() << "Buffer size mismatch. Expected:" << expected_size << "Got:" << buffer.size();
        emit this->run_completed();
        return;
    }

    buffer.read_back_async(m_device, [this, dimensions](WGPUMapAsyncStatus status, std::vector<uint32_t> data) {
        // Check if buffer mapping was successful
        if (status != WGPUMapAsyncStatus_Success) {
            qWarning() << "Buffer readback failed with status:" << status;
            emit this->run_completed();
            return;
        }

        // Verify data size matches expected dimensions
        if (data.size() != dimensions.x * dimensions.y) {
            qWarning() << "Readback data size mismatch. Expected:" << (dimensions.x * dimensions.y) << "Got:" << data.size();
            emit this->run_completed();
            return;
        }

        // Convert uint32_t buffer to rgba8 raster
        nucleus::Raster<glm::u8vec4> raster(dimensions);
        auto& raster_buffer = raster.buffer();

        // Monitor highest, lowest, and average values
        /*{
            uint32_t min_value = std::numeric_limits<uint32_t>::max();
            uint32_t max_value = std::numeric_limits<uint32_t>::min();
            uint64_t sum = 0;
            for (const auto& pixel : data) {
                min_value = std::min(min_value, pixel);
                max_value = std::max(max_value, pixel);
                sum += pixel;
            }
            double average_value = static_cast<double>(sum) / data.size();
            qDebug() << "Min value:" << min_value;
            qDebug() << "Max value:" << max_value;
            qDebug() << "Average value:" << average_value;
        }*/
        for (size_t i = 0; i < data.size(); i++) {
            const uint32_t pixel = data[i];

            const float FLOAT_MIN_ENCODING = -10000.0f;
            const float FLOAT_MAX_ENCODING = 10000.0f;

            float pixel_float = static_cast<float>(pixel);
            float clamped_pixel = glm::clamp(pixel_float, FLOAT_MIN_ENCODING, FLOAT_MAX_ENCODING);
            double normalized_pixel = (clamped_pixel - FLOAT_MIN_ENCODING) / (FLOAT_MAX_ENCODING - FLOAT_MIN_ENCODING);
            uint32_t mapped_pixel = static_cast<uint32_t>(normalized_pixel * (double)std::numeric_limits<uint32_t>::max());

            glm::u8vec4 color = glm::u8vec4((mapped_pixel >> 24) & 0xFF, // R (bits 24-31)
                (mapped_pixel >> 16) & 0xFF, // G (bits 16-23)
                (mapped_pixel >> 8) & 0xFF, // B (bits 8-15)
                mapped_pixel & 0xFF // A (bits 0-7)
            );

            raster_buffer[i] = color;
        }

        // Make sure output directory exists
        const auto output_path = std::filesystem::path(m_settings.output_file);
        std::filesystem::create_directories(output_path.parent_path());
        qDebug() << "Writing file to " << output_path.string();

        // Write to file
        nucleus::utils::image_writer::rgba8_as_png(raster, QString::fromStdString(output_path.string()));

        emit this->run_completed();
    });
}

} // namespace webgpu_engine::compute::nodes
