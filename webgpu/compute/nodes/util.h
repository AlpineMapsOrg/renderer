/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#pragma once

#include "../NodeGraph.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <glm/glm.hpp>
#include <nucleus/tile/TileLoadService.h>
#include <webgpu/webgpu.h>

namespace webgpu_compute::nodes {

void write_timings_to_json_file(const NodeGraph& node_graph, const std::filesystem::path& output_path);

// ---- WGPU enum <-> JSON helpers ----
// WGPU enum values are not stable across Dawn/webgpu.h updates, so they are stored as strings.
// WGPUTextureUsage flags are stored as a string array (the type may be 64-bit).
// On unknown strings, from_string logs a qWarning and returns the fallback.

QString wgpu_format_to_string(WGPUTextureFormat format);
WGPUTextureFormat wgpu_format_from_string(const QString& str, WGPUTextureFormat fallback);

QJsonArray wgpu_usage_to_json(WGPUTextureUsage usage);
WGPUTextureUsage wgpu_usage_from_json(const QJsonArray& arr, WGPUTextureUsage fallback);

QString wgpu_filter_mode_to_string(WGPUFilterMode mode);
WGPUFilterMode wgpu_filter_mode_from_string(const QString& str, WGPUFilterMode fallback);

QString wgpu_mipmap_filter_mode_to_string(WGPUMipmapFilterMode mode);
WGPUMipmapFilterMode wgpu_mipmap_filter_mode_from_string(const QString& str, WGPUMipmapFilterMode fallback);

QString url_pattern_to_string(nucleus::tile::TileLoadService::UrlPattern pattern);
nucleus::tile::TileLoadService::UrlPattern url_pattern_from_string(const QString& str, nucleus::tile::TileLoadService::UrlPattern fallback);

// ---- glm <-> JSON array helpers ----

QJsonArray vec2_to_json(glm::vec2 v);
glm::vec2 vec2_from_json(const QJsonArray& arr, glm::vec2 fallback);

QJsonArray uvec2_to_json(glm::uvec2 v);
glm::uvec2 uvec2_from_json(const QJsonArray& arr, glm::uvec2 fallback);

} // namespace webgpu_compute::nodes
