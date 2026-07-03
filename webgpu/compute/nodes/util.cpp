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

#include "util.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>

namespace webgpu_compute::nodes {

void write_timings_to_json_file(const NodeGraph& node_graph, const std::filesystem::path& output_path)
{
    QJsonObject object;
    for (const auto& [name, node] : node_graph.get_nodes()) {
        object[QString::fromStdString(name)] = node.get()->get_last_run_duration_in_ms();
    }

    QJsonDocument doc;
    doc.setObject(object);

    QFile output_file(output_path);
    if (!output_file.open(QIODevice::WriteOnly))
        qFatal("Failed to open timings file for writing: %s", output_path.string().c_str());
    output_file.write(doc.toJson());
    output_file.close();
}

// ---- enum tables ----

namespace {

    struct TextureFormatEntry {
        WGPUTextureFormat format;
        const char* name;
    };
    constexpr TextureFormatEntry texture_format_table[] = {
        { WGPUTextureFormat_R8Unorm, "R8Unorm" },
        { WGPUTextureFormat_R8Uint, "R8Uint" },
        { WGPUTextureFormat_R32Float, "R32Float" },
        { WGPUTextureFormat_RG8Unorm, "RG8Unorm" },
        { WGPUTextureFormat_RG32Float, "RG32Float" },
        { WGPUTextureFormat_RGBA8Unorm, "RGBA8Unorm" },
        { WGPUTextureFormat_RGBA8Uint, "RGBA8Uint" },
        { WGPUTextureFormat_RGBA8Sint, "RGBA8Sint" },
        { WGPUTextureFormat_BGRA8Unorm, "BGRA8Unorm" },
        { WGPUTextureFormat_RGBA16Float, "RGBA16Float" },
        { WGPUTextureFormat_RGBA32Float, "RGBA32Float" },
    };

    struct TextureUsageFlagEntry {
        uint64_t flag;
        const char* name;
    };
    const TextureUsageFlagEntry texture_usage_table[] = {
        { static_cast<uint64_t>(WGPUTextureUsage_CopySrc), "CopySrc" },
        { static_cast<uint64_t>(WGPUTextureUsage_CopyDst), "CopyDst" },
        { static_cast<uint64_t>(WGPUTextureUsage_TextureBinding), "TextureBinding" },
        { static_cast<uint64_t>(WGPUTextureUsage_StorageBinding), "StorageBinding" },
        { static_cast<uint64_t>(WGPUTextureUsage_RenderAttachment), "RenderAttachment" },
    };

    struct FilterModeEntry {
        WGPUFilterMode mode;
        const char* name;
    };
    constexpr FilterModeEntry filter_mode_table[] = {
        { WGPUFilterMode_Nearest, "Nearest" },
        { WGPUFilterMode_Linear, "Linear" },
    };

    struct MipmapFilterModeEntry {
        WGPUMipmapFilterMode mode;
        const char* name;
    };
    constexpr MipmapFilterModeEntry mipmap_filter_mode_table[] = {
        { WGPUMipmapFilterMode_Nearest, "Nearest" },
        { WGPUMipmapFilterMode_Linear, "Linear" },
    };

    struct UrlPatternEntry {
        nucleus::tile::TileLoadService::UrlPattern pattern;
        const char* name;
    };
    constexpr UrlPatternEntry url_pattern_table[] = {
        { nucleus::tile::TileLoadService::UrlPattern::ZXY, "ZXY" },
        { nucleus::tile::TileLoadService::UrlPattern::ZYX, "ZYX" },
        { nucleus::tile::TileLoadService::UrlPattern::ZXY_yPointingSouth, "ZXY_yPointingSouth" },
        { nucleus::tile::TileLoadService::UrlPattern::ZYX_yPointingSouth, "ZYX_yPointingSouth" },
    };

} // namespace

// ---- WGPU format ----

QString wgpu_format_to_string(WGPUTextureFormat format)
{
    for (const auto& e : texture_format_table) {
        if (e.format == format)
            return QLatin1String(e.name);
    }
    return QString("#%1").arg(static_cast<qint64>(format));
}

WGPUTextureFormat wgpu_format_from_string(const QString& str, WGPUTextureFormat fallback)
{
    for (const auto& e : texture_format_table) {
        if (str == QLatin1String(e.name))
            return e.format;
    }
    qWarning() << "wgpu_format_from_string: unknown format" << str << "- using fallback";
    return fallback;
}

// ---- WGPU usage ----

QJsonArray wgpu_usage_to_json(WGPUTextureUsage usage)
{
    QJsonArray arr;
    uint64_t remaining = static_cast<uint64_t>(usage);
    for (const auto& e : texture_usage_table) {
        if (remaining & e.flag) {
            arr.append(QLatin1String(e.name));
            remaining &= ~e.flag;
        }
    }
    if (remaining != 0)
        arr.append(QString("#%1").arg(remaining));
    return arr;
}

WGPUTextureUsage wgpu_usage_from_json(const QJsonArray& arr, WGPUTextureUsage fallback)
{
    uint64_t usage = 0;
    for (const QJsonValue& jv : arr) {
        if (!jv.isString()) {
            qWarning() << "wgpu_usage_from_json: expected string array - using fallback";
            return fallback;
        }
        const QString str = jv.toString();
        bool found = false;
        for (const auto& e : texture_usage_table) {
            if (str == QLatin1String(e.name)) {
                usage |= e.flag;
                found = true;
                break;
            }
        }
        if (!found) {
            qWarning() << "wgpu_usage_from_json: unknown flag" << str << "- skipping";
        }
    }
    return static_cast<WGPUTextureUsage>(usage);
}

// ---- WGPU filter mode ----

QString wgpu_filter_mode_to_string(WGPUFilterMode mode)
{
    for (const auto& e : filter_mode_table) {
        if (e.mode == mode)
            return QLatin1String(e.name);
    }
    return QString("#%1").arg(static_cast<qint64>(mode));
}

WGPUFilterMode wgpu_filter_mode_from_string(const QString& str, WGPUFilterMode fallback)
{
    for (const auto& e : filter_mode_table) {
        if (str == QLatin1String(e.name))
            return e.mode;
    }
    qWarning() << "wgpu_filter_mode_from_string: unknown mode" << str << "- using fallback";
    return fallback;
}

// ---- WGPU mipmap filter mode ----

QString wgpu_mipmap_filter_mode_to_string(WGPUMipmapFilterMode mode)
{
    for (const auto& e : mipmap_filter_mode_table) {
        if (e.mode == mode)
            return QLatin1String(e.name);
    }
    return QString("#%1").arg(static_cast<qint64>(mode));
}

WGPUMipmapFilterMode wgpu_mipmap_filter_mode_from_string(const QString& str, WGPUMipmapFilterMode fallback)
{
    for (const auto& e : mipmap_filter_mode_table) {
        if (str == QLatin1String(e.name))
            return e.mode;
    }
    qWarning() << "wgpu_mipmap_filter_mode_from_string: unknown mode" << str << "- using fallback";
    return fallback;
}

// ---- UrlPattern ----

QString url_pattern_to_string(nucleus::tile::TileLoadService::UrlPattern pattern)
{
    for (const auto& e : url_pattern_table) {
        if (e.pattern == pattern)
            return QLatin1String(e.name);
    }
    return QString("#%1").arg(static_cast<qint64>(pattern));
}

nucleus::tile::TileLoadService::UrlPattern url_pattern_from_string(const QString& str, nucleus::tile::TileLoadService::UrlPattern fallback)
{
    for (const auto& e : url_pattern_table) {
        if (str == QLatin1String(e.name))
            return e.pattern;
    }
    qWarning() << "url_pattern_from_string: unknown pattern" << str << "- using fallback";
    return fallback;
}

// ---- glm helpers ----

QJsonArray vec2_to_json(glm::vec2 v) { return { static_cast<double>(v.x), static_cast<double>(v.y) }; }

glm::vec2 vec2_from_json(const QJsonArray& arr, glm::vec2 fallback)
{
    if (arr.size() != 2 || !arr[0].isDouble() || !arr[1].isDouble()) {
        qWarning() << "vec2_from_json: expected [x, y] array - using fallback";
        return fallback;
    }
    return { static_cast<float>(arr[0].toDouble()), static_cast<float>(arr[1].toDouble()) };
}

QJsonArray uvec2_to_json(glm::uvec2 v) { return { static_cast<int>(v.x), static_cast<int>(v.y) }; }

glm::uvec2 uvec2_from_json(const QJsonArray& arr, glm::uvec2 fallback)
{
    if (arr.size() != 2 || !arr[0].isDouble() || !arr[1].isDouble()) {
        qWarning() << "uvec2_from_json: expected [x, y] array - using fallback";
        return fallback;
    }
    return { static_cast<uint32_t>(arr[0].toInt()), static_cast<uint32_t>(arr[1].toInt()) };
}

} // namespace webgpu_compute::nodes
