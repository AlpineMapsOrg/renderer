/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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
#include "UnittestGLContext.h"

#include <QPainter>
#include <catch2/catch_test_macros.hpp>
#include <gl_engine/Framebuffer.h>
#include <gl_engine/ShaderProgram.h>
#include <gl_engine/Texture.h>
#include <gl_engine/helpers.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/tile_scheduler/utils.h>
#include <nucleus/utils/ColourTexture.h>
#include <radix/TileHeights.h>
#include <radix/quad_tree.h>
#include <radix/tile.h>
#include <unordered_set>

using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;

namespace tile_id_funs {
uint16_t hash_uint16(const tile::Id& id)
{
    // https://en.wikipedia.org/wiki/Linear_congruential_generator
    // could be possible to find better factors.
    uint16_t z = uint16_t(id.zoom_level) * uint16_t(4 * 199 * 59 + 1) + uint16_t(10859);
    uint16_t x = uint16_t(id.coords.x) * uint16_t(4 * 149 * 101 + 1) + uint16_t(12253);
    uint16_t y = uint16_t(id.coords.y) * uint16_t(4 * 293 * 53 + 1) + uint16_t(59119);

    return x + y + z;
}

glm::vec<2, uint32_t> pack(const tile::Id& id)
{
    uint32_t a = id.zoom_level << (32 - 5);
    a = a | (id.coords.x >> 3);
    uint32_t b = id.coords.x << (32 - 3);
    b = b | id.coords.y;
    return { a, b };
}

tile::Id unpack(const glm::vec<2, uint32_t>& packed)
{
    tile::Id id;
    id.zoom_level = packed.x >> (32 - 5);
    id.coords.x = (packed.x & ((1u << (32 - 5)) - 1)) << 3;
    id.coords.x = id.coords.x | (packed.y >> (32 - 3));
    id.coords.y = packed.y & ((1u << (32 - 3)) - 1);
    return id;
}
} // namespace tile_id_funs

namespace {
ShaderProgram create_debug_shader(const QString& fragment_shader, const QString& vertex_shader = R"(
out highp vec2 texcoords;
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
})")
{
    ShaderProgram tmp(vertex_shader, fragment_shader, gl_engine::ShaderCodeSource::PLAINTEXT);
    return tmp;
}

void hashing_cpp_same_as_glsl(const tile::Id& id)
{
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
        b.bind();

        ShaderProgram shader = create_debug_shader(QString(R"(
            #include "tile_id.glsl"

            out lowp vec4 out_color;
            void main() {
                mediump uint hash_ref = %1u;
                lowp uint zoom_level = %2u;
                highp uint x = %3u;
                highp uint y = %4u;
                mediump uint hash_glsl = hash_tile_id(zoom_level, x, y);
                out_color = vec4((hash_ref == hash_glsl) ? 121.0 / 255.0 : 9.0 / 255.0, 0, 0, 1);
            }
        )")
                                                       .arg(tile_id_funs::hash_uint16(id))
                                                       .arg(id.zoom_level)
                                                       .arg(id.coords.x)
                                                       .arg(id.coords.y));
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 121);
    }
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
        b.bind();

        ShaderProgram shader = create_debug_shader(QString(R"(
            out lowp vec4 out_color;
            flat in mediump uint hash;
            void main() {
                mediump uint hash_ref = %1u;
                out_color = vec4((hash_ref == hash) ? 121.0 / 255.0 : 9.0 / 255.0, 0, 0, 1);
            }
            )")
                                                       .arg(tile_id_funs::hash_uint16(id)),
            QString(R"(
            #include "tile_id.glsl"

            out highp vec2 texcoords;
            flat out mediump uint hash;
            void main() {
                lowp uint zoom_level = %1u;
                highp uint x = %2u;
                highp uint y = %3u;
                hash = hash_tile_id(zoom_level, x, y);
                vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
                gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
                texcoords = 0.5 * gl_Position.xy + vec2(0.5);
            })")
                .arg(id.zoom_level)
                .arg(id.coords.x)
                .arg(id.coords.y));
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 121);
    }
}

} // namespace

TEST_CASE("glsl tile functions")
{
    UnittestGLContext::initialise();
    SECTION("hashing c++ same as glsl")
    {
        hashing_cpp_same_as_glsl({ 0, { 0, 0 } });
        hashing_cpp_same_as_glsl({ 1, { 0, 0 } });
        hashing_cpp_same_as_glsl({ 1, { 1, 1 } });
        hashing_cpp_same_as_glsl({ 14, { 2673, 12038 } });
        hashing_cpp_same_as_glsl({ 20, { 430489, 100204 } });
    }

    SECTION("check conflict potential")
    {
        std::unordered_set<tile::Id, tile::Id::Hasher> ids;
        {
            TileHeights h;
            h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
            auto aabb_decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h));

            const auto add_tiles = [&](auto camera) {
                camera.set_viewport_size({ 1920, 1080 });
                const auto all_leaves = quad_tree::onTheFlyTraverse(
                    tile::Id { 0, { 0, 0 } }, nucleus::tile_scheduler::utils::refineFunctor(camera, aabb_decorator, 1, 64), [&ids](const tile::Id& v) {
                        ids.insert(v);
                        return v.children();
                    });
            };
            add_tiles(nucleus::camera::stored_positions::stephansdom());
            add_tiles(nucleus::camera::stored_positions::grossglockner());
            add_tiles(nucleus::camera::stored_positions::oestl_hochgrubach_spitze());
            add_tiles(nucleus::camera::stored_positions::wien());
            add_tiles(nucleus::camera::stored_positions::karwendel());
            add_tiles(nucleus::camera::stored_positions::schneeberg());
            add_tiles(nucleus::camera::stored_positions::weichtalhaus());
            add_tiles(nucleus::camera::stored_positions::grossglockner_shadow());
        }

        QImage data(256, 256, QImage::Format_Grayscale8);
        data.fill(0);
        unsigned conflict_chain_length = 0;
        unsigned n_conflicts = 0;
        // unsigned n_equality_checks = 0;
        for (const auto id : ids) {
            const auto hash = tile_id_funs::hash_uint16(id);
            if (hash / 65535.f > 0.98f) {
                // n_equality_checks++;
                hashing_cpp_same_as_glsl(id);
            }
            auto& bucket = (*(data.bits() + hash));
            n_conflicts += bucket != 0;
            bucket++;
            conflict_chain_length = std::max(conflict_chain_length, unsigned(bucket));
        }
        for (int i = 0; i < 256 * 256; ++i) {
            auto& bucket = (*(data.bits() + i));
            bucket *= 255 / conflict_chain_length;
        }

        // data.save("tile_id_hashing.png");
        // qDebug() << "n_tiles: " << ids.size();
        // qDebug() << "conflict_chain_length: " << conflict_chain_length;
        // qDebug() << "n_conflicts: " << n_conflicts;
        // qDebug() << "n_equality_checks: " << n_equality_checks;
        CHECK(conflict_chain_length <= 3);
        CHECK(n_conflicts <= 1000);
    }

    SECTION("packing roundtrip in c++")
    {
        const auto check = [](const tile::Id& id) {
            const auto packed = tile_id_funs::pack(id);
            const auto unpacked = tile_id_funs::unpack(packed);
            CHECK(id == unpacked);
        };
        check({ 0, { 0, 0 } });
        check({ 1, { 0, 0 } });
        check({ 1, { 1, 1 } });
        check({ 14, { 2673, 12038 } });
        check({ 20, { 430489, 100204 } });
    }

    // SECTION("packiong c++ same as glsl")
    // {
    //     packing_cpp_same_as_glsl({ 0, { 0, 0 } });
    //     packing_cpp_same_as_glsl({ 1, { 0, 0 } });
    //     packing_cpp_same_as_glsl({ 1, { 1, 1 } });
    //     packing_cpp_same_as_glsl({ 14, { 2673, 12038 } });
    //     packing_cpp_same_as_glsl({ 20, { 430489, 100204 } });
    // }
}
