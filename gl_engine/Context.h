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

#pragma once

#include <nucleus/EngineContext.h>

#include "TrackManager.h"

namespace gl_engine {
class ShaderManager;

class Context : public nucleus::EngineContext {
private:
    Context();

public:
    Context(Context const&) = delete;
    ~Context();
    void operator=(Context const&) = delete;
    static Context& instance()
    {
        static Context c;
        return c;
    }

    [[nodiscard]] TrackManager* track_manager() override;
    [[nodiscard]] ShaderManager* shader_manager();

protected:
    void internal_initialise() override;
    void internal_destroy() override;

private:
    std::unique_ptr<gl_engine::TrackManager> m_track_manager;
    std::unique_ptr<gl_engine::ShaderManager> m_shader_manager;
};
} // namespace gl_engine
