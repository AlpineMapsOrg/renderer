/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2025 Patrick Komon
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

#include "Context.h"

namespace webgpu_engine {

Context::Context(QObject* parent)
    : nucleus::EngineContext(parent)
{
}

Context::~Context() = default;

void Context::internal_initialise()
{
    assert(m_webgpu_device != 0);

    // TODO maybe init externally too
    m_shader_module_manager = std::make_unique<ShaderModuleManager>(m_webgpu_device);
    m_shader_module_manager->create_shader_modules();
    m_pipeline_manager = std::make_unique<PipelineManager>(m_webgpu_device, *m_shader_module_manager);
    m_pipeline_manager->create_pipelines();

    // init of shader registry and track manager should be moved out of here for more flexibility, similar to tile_geometry
    if (m_tile_geometry) {
        m_tile_geometry->set_pipeline_manager(*m_pipeline_manager);
        m_tile_geometry->init(m_webgpu_device);
    }

    // if (m_ortho_layer)
    //     m_ortho_layer->init();
}

void Context::internal_destroy()
{
    // this is necessary for a clean shutdown (and we want a clean shutdown for the ci integration test).
    // m_ortho_layer.reset();
    m_tile_geometry.reset();
}

TileGeometry* Context::tile_geometry() const { return m_tile_geometry.get(); }

void Context::set_tile_geometry(std::shared_ptr<TileGeometry> new_tile_geometry)
{
    assert(!is_alive()); // only set before init is called.
    m_tile_geometry = std::move(new_tile_geometry);
}

WGPUInstance Context::webgpu_instance() const { return m_webgpu_instance; }

void Context::set_webgpu_instance(WGPUInstance instance) { m_webgpu_instance = instance; }

void Context::set_webgpu_device(WGPUDevice device) { m_webgpu_device = device; }

ShaderModuleManager* Context::shader_module_manager() { return m_shader_module_manager.get(); }

PipelineManager* Context::pipeline_manager() { return m_pipeline_manager.get(); }

nucleus::track::Manager* Context::track_manager() { return nullptr; }

/*TextureLayer* Context::ortho_layer() const { return m_ortho_layer.get(); }

void Context::set_ortho_layer(std::shared_ptr<TextureLayer> new_ortho_layer)
{
    assert(!is_alive()); // only set before init is called.
    m_ortho_layer = std::move(new_ortho_layer);
}*/

} // namespace webgpu_engine
