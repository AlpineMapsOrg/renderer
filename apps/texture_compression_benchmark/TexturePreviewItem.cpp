/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#include "TexturePreviewItem.h"
#include "TextureCompressionData.h"

#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QUrl>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

#include <gl_engine/Framebuffer.h>
#include <gl_engine/ShaderProgram.h>
#include <gl_engine/Texture.h>
#include <gl_engine/TextureCompressor.h>
#include <gl_engine/helpers.h>
#include <nucleus/tile/conversion.h>
#include <nucleus/utils/ColourTexture.h>
#include <nucleus/utils/image_loader.h>

namespace {
using Raster = radix::Raster<glm::u8vec4>;

using texture_compression_data::tile_groups;
using texture_compression_data::tile_url;

double srgbToLinear(uint8_t value)
{
    const auto normalised = double(value) / 255.0;
    if (normalised <= 0.04045)
        return normalised / 12.92;
    return std::pow((normalised + 0.055) / 1.055, 2.4);
}

double linearPsnr(std::span<const QImage> reconstructed, std::span<const Raster> sources)
{
    Q_ASSERT(reconstructed.size() == sources.size());
    double squared_error = 0.0;
    uint64_t channel_count = 0;
    for (size_t i = 0; i < sources.size(); ++i) {
        for (int y = 0; y < reconstructed[i].height(); ++y) {
            for (int x = 0; x < reconstructed[i].width(); ++x) {
                const auto actual = reconstructed[i].pixel(x, y);
                const auto expected = sources[i].pixel({ x, y });
                const std::array<double, 3> actual_channels {
                    qRed(actual) / 255.0,
                    qGreen(actual) / 255.0,
                    qBlue(actual) / 255.0,
                };
                const std::array<double, 3> expected_channels {
                    srgbToLinear(expected.x),
                    srgbToLinear(expected.y),
                    srgbToLinear(expected.z),
                };
                for (size_t channel = 0; channel < actual_channels.size(); ++channel) {
                    const auto difference = actual_channels[channel] - expected_channels[channel];
                    squared_error += difference * difference;
                }
            }
        }
        channel_count += uint64_t(reconstructed[i].width()) * uint64_t(reconstructed[i].height()) * 3;
    }
    const auto mse = squared_error / double(channel_count);
    return mse == 0.0 ? std::numeric_limits<double>::infinity() : 10.0 * std::log10(1.0 / mse);
}

QImage reconstruct(gl_engine::Texture& texture, unsigned resolution, unsigned layer)
{
    gl_engine::Framebuffer framebuffer(
        gl_engine::Framebuffer::DepthFormat::None, { gl_engine::Framebuffer::ColourFormat::RGBA8 }, { resolution, resolution });
    framebuffer.bind();
    gl_engine::ShaderProgram shader(R"(
        out highp vec2 texcoords;
        void main() {
            vec2 vertices[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
            gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
            texcoords = 0.5 * gl_Position.xy + vec2(0.5);
        })",
        R"(
        uniform lowp sampler2DArray texture_sampler;
        uniform highp float texture_layer;
        in highp vec2 texcoords;
        out lowp vec4 out_color;
        void main() {
            out_color = textureLod(texture_sampler, vec3(texcoords.x, 1.0 - texcoords.y, texture_layer), 0.0);
        })",
        gl_engine::ShaderCodeSource::PLAINTEXT);
    shader.bind();
    texture.bind(0);
    shader.set_uniform("texture_sampler", 0);
    shader.set_uniform("texture_layer", float(layer));
    gl_engine::helpers::create_screen_quad_geometry().draw();
    auto result = framebuffer.read_colour_attachment(0);
    gl_engine::Framebuffer::unbind();
    return result;
}

struct GpuPreview {
    const char* name;
    const char* description;
    gl_engine::TextureCompressor::Settings settings;
};

std::vector<GpuPreview> gpu_previews(nucleus::utils::ColourTexture::Format format)
{
    using Compressor = gl_engine::TextureCompressor;
    const auto search = [](unsigned effort) {
        return Compressor::Settings {
            .dxt1_algorithm = Compressor::Dxt1Algorithm::SlowSearch,
            .etc_algorithm = Compressor::EtcAlgorithm::SlowSearch,
            .search_effort = effort,
        };
    };
    std::vector<GpuPreview> result {
        { "Slow search 0",
        "Tests the average block colour with every ETC1 modifier table and keeps the lowest-error result.",
        search(0) },
        { "Slow search 1",
        "Tests two base colours around the block average with every ETC1 modifier table and keeps the lowest-error result.",
        search(1) },
        { "Slow search 2",
        "Tests three base colours around the block average with every ETC1 modifier table and keeps the lowest-error result.",
        search(2) },
        { "Slow search 3",
        "Tests four base colours around the block average with every ETC1 modifier table and keeps the lowest-error result.",
        search(3) },
        { "Slow search 4",
        "Tests five base colours around the block average with every ETC1 modifier table and keeps the lowest-error result.",
        search(4) },
        { "Slow search 10",
        "Tests eleven base colours around the block average with every ETC1 modifier table and keeps the lowest-error result.",
        search(10) },
    };
    if (format == nucleus::utils::ColourTexture::Format::ETC1) {
        result.push_back({ "Fastest",
            "Evaluates horizontal and vertical ETC splits with exact modifier selection.",
            { .etc_algorithm = Compressor::EtcAlgorithm::Fastest } });
        result.push_back({ "Fast",
            "Refits each ETC base from its average reconstruction residual and evaluates it once.",
            { .etc_algorithm = Compressor::EtcAlgorithm::Fast } });
    }
    return result;
}
} // namespace

class TexturePreviewRenderer final : public QQuickFramebufferObject::Renderer {
public:
    void synchronize(QQuickFramebufferObject* item) override
    {
        auto* preview_item = static_cast<TexturePreviewItem*>(item);
        m_item = preview_item;
        m_window = preview_item->window();
        m_preview_encoder = preview_item->m_preview_encoder;
        if (preview_item->m_request_serial == m_seen_serial)
            return;
        m_seen_serial = preview_item->m_request_serial;
        m_source_images = preview_item->m_source_images;
        m_pending = true;
    }

    void render() override
    {
        m_window->beginExternalCommands();
        if (m_pending) {
            m_pending = false;
            const auto error = generateTextures();
            QPointer<TexturePreviewItem> item = m_item;
            const auto results = m_preview_results;
            QMetaObject::invokeMethod(m_item, [item, error, results]() {
                if (item)
                    item->publishResults(error, results);
            });
        }
        drawPreview();
        m_window->endExternalCommands();
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        return new QOpenGLFramebufferObject(size.expandedTo(QSize(1, 1)), format);
    }

private:
    QString generateTextures()
    {
        constexpr unsigned resolution = 512;
        if (m_source_images.size() != tile_groups.size())
            return QStringLiteral("Preview imagery is incomplete.");
        std::vector<Raster> sources;
        sources.reserve(m_source_images.size());
        for (const auto& image : m_source_images)
            sources.push_back(nucleus::tile::conversion::to_rgba8raster(image));

        std::vector<unsigned> layers(sources.size());
        std::iota(layers.begin(), layers.end(), 0u);
        const auto algorithm = gl_engine::Texture::compression_algorithm();
        const auto filter = gl_engine::Texture::Filter::MipMapLinear;
        const auto mip_levels = gl_engine::TextureCompressor::mip_level_count(resolution, resolution);
        const auto previews = gpu_previews(algorithm);
        m_preview_results.clear();
        m_preview_results.reserve(3 + previews.size());
        m_preview_textures.clear();
        m_preview_textures.reserve(3 + previews.size());

        auto create_texture = [&](gl_engine::Texture::Format format, gl_engine::Texture::Filter min_filter) {
            auto texture = std::make_shared<gl_engine::Texture>(gl_engine::Texture::Target::_2dArray, format);
            texture->setParams(min_filter, gl_engine::Texture::Filter::Linear);
            texture->allocate_array(resolution, resolution, unsigned(sources.size()), mip_levels);
            return texture;
        };
        auto psnr = [&](gl_engine::Texture& texture) {
            std::vector<QImage> reconstructed;
            reconstructed.reserve(sources.size());
            for (unsigned layer = 0; layer < sources.size(); ++layer)
                reconstructed.push_back(reconstruct(texture, resolution, layer));
            return linearPsnr(reconstructed, sources);
        };

        auto scratch = create_texture(gl_engine::Texture::Format::RGBA8, gl_engine::Texture::Filter::Nearest);
        for (size_t layer = 0; layer < sources.size(); ++layer)
            scratch->upload(sources[layer], unsigned(layer));
        scratch->generate_mipmaps();

        auto reference = create_texture(gl_engine::Texture::Format::SRGBA8, gl_engine::Texture::Filter::Linear);
        for (size_t layer = 0; layer < sources.size(); ++layer)
            reference->upload(sources[layer], unsigned(layer));
        m_preview_textures.push_back(reference);
        m_preview_results.push_back({ QStringLiteral("Ref"),
            QStringLiteral("The original uncompressed texture array used as the visual and PSNR reference."),
            std::numeric_limits<double>::infinity() });

        auto copied = create_texture(gl_engine::Texture::Format::SRGBA8, filter);
        gl_engine::TextureCompressor copy_compressor(scratch, copied);
        if (const auto result = copy_compressor.compress(layers); !result)
            return QString::fromStdString(result.error());
        m_preview_textures.push_back(copied);
        m_preview_results.push_back({ QStringLiteral("GPU copy"),
            QStringLiteral("The RGBA8 scratch texture copied through the portable RGBA8 framebuffer path."),
            psnr(*copied) });

        auto goofy = create_texture(gl_engine::Texture::Format::CompressedRGBA8, filter);
        for (size_t layer = 0; layer < sources.size(); ++layer) {
            const auto compressed = nucleus::utils::generate_mipmapped_colour_texture(sources[layer], algorithm);
            goofy->upload(compressed, unsigned(layer));
        }
        m_preview_textures.push_back(goofy);
        m_preview_results.push_back({ QStringLiteral("Goofy"),
            QStringLiteral("CPU reference compressed by Goofy into the device's active ETC1 or DXT1 block format."),
            psnr(*goofy) });

        for (const auto& preview : previews) {
            auto texture = create_texture(gl_engine::Texture::Format::CompressedRGBA8, filter);
            gl_engine::TextureCompressor compressor(scratch, texture, preview.settings);
            if (const auto result = compressor.compress(layers); !result)
                return QString::fromStdString(result.error());
            m_preview_textures.push_back(texture);
            m_preview_results.push_back(
                { QString::fromLatin1(preview.name), QString::fromLatin1(preview.description), psnr(*texture) });
        }
        return {};
    }

    void drawPreview()
    {
        if (m_preview_encoder < 0 || size_t(m_preview_encoder) >= m_preview_textures.size()
            || !m_preview_textures[size_t(m_preview_encoder)])
            return;
        if (!m_preview_shader) {
            m_preview_shader = std::make_unique<gl_engine::ShaderProgram>(R"(
                out highp vec2 texcoords;
                void main() {
                    highp vec2 vertices[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
                    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
                    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
                })",
                R"(
                uniform lowp sampler2DArray texture_sampler;
                in highp vec2 texcoords;
                out lowp vec4 out_color;
                highp vec3 linear_to_srgb(highp vec3 linear) {
                    return mix(12.92 * linear,
                        1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055,
                        step(vec3(0.0031308), linear));
                }
                void main() {
                    highp vec2 grid_position = texcoords * 4.0;
                    highp ivec2 cell = min(ivec2(grid_position), ivec2(3));
                    highp float layer = float((3 - cell.y) * 4 + cell.x);
                    highp vec2 tile_coordinates = fract(grid_position);
                    highp vec4 linear_color = textureLod(texture_sampler,
                        vec3(tile_coordinates.x, 1.0 - tile_coordinates.y, layer), 0.0);
                    out_color = vec4(linear_to_srgb(linear_color.rgb), linear_color.a);
                })",
                gl_engine::ShaderCodeSource::PLAINTEXT);
            m_preview_geometry = gl_engine::helpers::create_screen_quad_geometry();
        }

        auto* f = QOpenGLContext::currentContext()->extraFunctions();
        framebufferObject()->bind();
        f->glViewport(0, 0, framebufferObject()->width(), framebufferObject()->height());
        f->glDisable(GL_BLEND);
        f->glDisable(GL_CULL_FACE);
        f->glDisable(GL_DEPTH_TEST);
        f->glDisable(GL_SCISSOR_TEST);
        m_preview_shader->bind();
        m_preview_textures[size_t(m_preview_encoder)]->bind(0);
        m_preview_shader->set_uniform("texture_sampler", 0);
        m_preview_geometry.draw();
        m_preview_shader->release();
    }

    QPointer<TexturePreviewItem> m_item;
    QQuickWindow* m_window = nullptr;
    unsigned m_seen_serial = 0;
    int m_preview_encoder = 0;
    bool m_pending = false;
    std::vector<QImage> m_source_images;
    std::vector<TexturePreviewItem::PreviewResult> m_preview_results;
    std::vector<std::shared_ptr<gl_engine::Texture>> m_preview_textures;
    std::unique_ptr<gl_engine::ShaderProgram> m_preview_shader;
    gl_engine::helpers::ScreenQuadGeometry m_preview_geometry;
};

TexturePreviewItem::TexturePreviewItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
    , m_network_manager(new QNetworkAccessManager(this))
{
    setMirrorVertically(true);
    downloadImages();
}

QQuickFramebufferObject::Renderer* TexturePreviewItem::createRenderer() const { return new TexturePreviewRenderer; }

QString TexturePreviewItem::status() const { return m_status; }
bool TexturePreviewItem::loading() const { return m_loading; }
bool TexturePreviewItem::ready() const { return !m_preview_results.empty(); }
int TexturePreviewItem::previewEncoder() const { return m_preview_encoder; }

void TexturePreviewItem::setPreviewEncoder(int value)
{
    value = m_preview_results.empty() ? 0 : std::clamp(value, 0, int(m_preview_results.size()) - 1);
    if (m_preview_encoder == value)
        return;
    m_preview_encoder = value;
    emit previewEncoderChanged();
    emit previewDetailsChanged();
    update();
}

QStringList TexturePreviewItem::previewEncoders() const
{
    QStringList result;
    result.reserve(qsizetype(m_preview_results.size()));
    for (const auto& preview : m_preview_results)
        result.push_back(preview.name);
    return result;
}

QString TexturePreviewItem::previewName() const
{
    return ready() ? m_preview_results[size_t(m_preview_encoder)].name : QString {};
}

QString TexturePreviewItem::previewDescription() const
{
    return ready() ? m_preview_results[size_t(m_preview_encoder)].description : QString {};
}

double TexturePreviewItem::previewPsnr() const
{
    return ready() ? m_preview_results[size_t(m_preview_encoder)].psnr : 0.0;
}

void TexturePreviewItem::downloadImages()
{
    m_downloaded_tiles.resize(tile_groups.size() * 4);
    m_downloads_remaining = int(m_downloaded_tiles.size());
    for (size_t group_index = 0; group_index < tile_groups.size(); ++group_index) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                const auto tile_index = group_index * 4 + size_t(y * 2 + x);
                const auto url = tile_url(tile_groups[group_index], x, y);
                auto* reply = m_network_manager->get(QNetworkRequest(QUrl(url)));
                connect(reply, &QNetworkReply::finished, this, [this, reply, tile_index, url]() {
                    if (reply->error() == QNetworkReply::NoError) {
                        const auto image = nucleus::utils::image_loader::rgba8(reply->readAll());
                        if (image && image->size() == glm::uvec2(256u))
                            m_downloaded_tiles[tile_index] = nucleus::tile::conversion::to_QImage(*image);
                    }
                    if (m_downloaded_tiles[tile_index].isNull())
                        m_status = QStringLiteral("Unable to download preview tile: %1").arg(url);
                    reply->deleteLater();
                    --m_downloads_remaining;
                    if (m_downloads_remaining > 0) {
                        if (!m_status.startsWith(QStringLiteral("Unable"))) {
                            m_status = QStringLiteral("Downloading preview imagery… %1/%2")
                                           .arg(int(m_downloaded_tiles.size()) - m_downloads_remaining)
                                           .arg(m_downloaded_tiles.size());
                        }
                        emit statusChanged();
                        return;
                    }
                    if (std::ranges::any_of(m_downloaded_tiles, [](const QImage& image) { return image.isNull(); })) {
                        m_loading = false;
                        emit statusChanged();
                        return;
                    }
                    stitchImages();
                });
            }
        }
    }
}

void TexturePreviewItem::stitchImages()
{
    m_source_images.clear();
    m_source_images.reserve(tile_groups.size());
    for (size_t group_index = 0; group_index < tile_groups.size(); ++group_index) {
        QImage stitched(512, 512, QImage::Format_RGBA8888);
        QPainter painter(&stitched);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x)
                painter.drawImage(QPoint(x * 256, y * 256), m_downloaded_tiles[group_index * 4 + size_t(y * 2 + x)]);
        }
        m_source_images.push_back(std::move(stitched));
    }
    m_downloaded_tiles.clear();
    m_status = QStringLiteral("Generating compressed texture arrays…");
    ++m_request_serial;
    emit statusChanged();
    update();
}

void TexturePreviewItem::publishResults(const QString& error, const std::vector<PreviewResult>& results)
{
    m_preview_results = results;
    m_preview_encoder = std::clamp(m_preview_encoder, 0, std::max(0, int(m_preview_results.size()) - 1));
    m_status = error.isEmpty() ? QStringLiteral("Texture previews ready.") : error;
    m_loading = false;
    emit statusChanged();
    emit previewResultsChanged();
    emit previewDetailsChanged();
    update();
}
