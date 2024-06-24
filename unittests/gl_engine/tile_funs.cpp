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

#include <unordered_set>

#include <QImage>
#include <catch2/catch_test_macros.hpp>

#include <gl_engine/Framebuffer.h>
#include <gl_engine/ShaderProgram.h>
#include <gl_engine/Texture.h>
#include <gl_engine/helpers.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/srs.h>
#include <nucleus/tile_scheduler/utils.h>
#include <nucleus/utils/ColourTexture.h>
#include <radix/TileHeights.h>
#include <radix/quad_tree.h>
#include <radix/tile.h>

using gl_engine::Framebuffer;
using gl_engine::ShaderProgram;
using nucleus::srs::hash_uint16;
using nucleus::srs::pack;
using nucleus::srs::unpack;

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
                mediump uint hash_glsl = hash_tile_id(uvec3(%2u, %3u, %4u));
                out_color = vec4((hash_ref == hash_glsl) ? 121.0 / 255.0 : 9.0 / 255.0, 0, 0, 1);
            }
        )")
                                                       .arg(hash_uint16(id))
                                                       .arg(id.coords.x)
                                                       .arg(id.coords.y)
                                                       .arg(id.zoom_level));
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
                                                       .arg(hash_uint16(id)),
            QString(R"(
            #include "tile_id.glsl"

            out highp vec2 texcoords;
            flat out mediump uint hash;
            void main() {
                hash = hash_tile_id(uvec3(%1u, %2u, %3u));
                vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
                gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
                texcoords = 0.5 * gl_Position.xy + vec2(0.5);
            })")
                .arg(id.coords.x)
                .arg(id.coords.y)
                .arg(id.zoom_level));
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 121);
    }
}

void packing_cpp_same_as_glsl(const tile::Id& id)
{
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
        b.bind();

        ShaderProgram shader = create_debug_shader(QString(R"(
            #include "tile_id.glsl"

            out lowp vec4 out_color;
            void main() {
                highp uvec2 cpp_packed_id = uvec2(%1u, %2u);
                highp uvec3 id = uvec3(%3u, %4u, %5u);
                highp uvec3 unpacked_id = unpack_tile_id(cpp_packed_id);
                bool unpack_ok = id == unpacked_id;
                highp uvec2 packed_id = pack_tile_id(id);
                bool pack_ok = packed_id == cpp_packed_id;
                out_color = vec4(unpack_ok ? 121.0 / 255.0 : 9.0 / 255.0, pack_ok ? 122.0 / 255.0 : 9.0 / 255.0, 0, 1);
            }
        )")
                                                       .arg(pack(id).x)
                                                       .arg(pack(id).y)
                                                       .arg(id.coords.x)
                                                       .arg(id.coords.y)
                                                       .arg(id.zoom_level));
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 121);
        CHECK(qGreen(render_result.pixel(0, 0)) == 122);
    }
    {
        Framebuffer b(Framebuffer::DepthFormat::None, { Framebuffer::ColourFormat::RGBA8 }, { 1, 1 });
        b.bind();

        ShaderProgram shader = create_debug_shader(QString(R"(
            out lowp vec4 out_color;
            flat in lowp vec2 ok;
            void main() {
                out_color = vec4(ok, 0, 1);
            }
            )"),
            QString(R"(
            #include "tile_id.glsl"

            out highp vec2 texcoords;
            flat out lowp vec2 ok;
            void main() {
                highp uvec2 cpp_packed_id = uvec2(%1u, %2u);
                highp uvec3 id = uvec3(%3u, %4u, %5u);
                highp uvec3 unpacked_id = unpack_tile_id(cpp_packed_id);
                bool unpack_ok = id == unpacked_id;
                highp uvec2 packed_id = pack_tile_id(id);
                bool pack_ok = packed_id == cpp_packed_id;
                ok = vec2(unpack_ok ? 121.0 / 255.0 : 9.0 / 255.0, pack_ok ? 122.0 / 255.0 : 9.0 / 255.0);

                vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
                gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
                texcoords = 0.5 * gl_Position.xy + vec2(0.5);
            })")
                .arg(pack(id).x)
                .arg(pack(id).y)
                .arg(id.coords.x)
                .arg(id.coords.y)
                .arg(id.zoom_level));
        shader.bind();
        gl_engine::helpers::create_screen_quad_geometry().draw();

        const QImage render_result = b.read_colour_attachment(0);
        Framebuffer::unbind();
        CHECK(qRed(render_result.pixel(0, 0)) == 121);
        CHECK(qGreen(render_result.pixel(0, 0)) == 122);
    }
}

} // namespace

TEST_CASE("glsl tile functions")
{
    UnittestGLContext::initialise();
    std::unordered_set<tile::Id, tile::Id::Hasher> ids;
    {
        TileHeights h;
        h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
        auto aabb_decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h));

        const auto add_tiles = [&](auto camera) {
            camera.set_viewport_size({ 1920, 1080 });
            const auto all_leaves = quad_tree::onTheFlyTraverse(
                tile::Id { 0, { 0, 0 } }, nucleus::tile_scheduler::utils::refineFunctor(camera, aabb_decorator, 3, 64), [&ids](const tile::Id& v) {
                    ids.insert(v);
                    return v.children();
                });
        };
        add_tiles(nucleus::camera::stored_positions::grossglockner());
    }

    SECTION("hashing c++ same as glsl")
    {
        hashing_cpp_same_as_glsl({ 0, { 0, 0 } });
        hashing_cpp_same_as_glsl({ 1, { 0, 0 } });
        hashing_cpp_same_as_glsl({ 1, { 1, 1 } });
        hashing_cpp_same_as_glsl({ 14, { 2673, 12038 } });
        hashing_cpp_same_as_glsl({ 20, { 430489, 100204 } });
        for (const auto id : ids) {
            hashing_cpp_same_as_glsl(id);
        }
    }

    SECTION("packing c++ same as glsl")
    {
        packing_cpp_same_as_glsl({ 0, { 0, 0 } });
        packing_cpp_same_as_glsl({ 1, { 0, 0 } });
        packing_cpp_same_as_glsl({ 1, { 1, 1 } });
        packing_cpp_same_as_glsl({ 14, { 2673, 12038 } });
        packing_cpp_same_as_glsl({ 20, { 430489, 100204 } });
        for (const auto id : ids) {
            packing_cpp_same_as_glsl(id);
        }
    }
}
