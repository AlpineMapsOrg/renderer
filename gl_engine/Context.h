/*****************************************************************************
 * AlpineMaps.org
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

#include "TrackManager.h"
#include <gl_engine/AvalancheReportManager.h>
#include <nucleus/EngineContext.h>

namespace nucleus::avalanche {
class UIntIdManager;
}

namespace gl_engine {
class MapLabels;
class ShaderRegistry;
class TileGeometry;
class TextureLayer;
class AvalancheWarningLayer;

class Context : public nucleus::EngineContext {
private:
public:
    Context(QObject* parent = nullptr);
    Context(Context const&) = delete;
    ~Context() override;
    void operator=(Context const&) = delete;

    [[nodiscard]] TrackManager* track_manager() override;
    [[nodiscard]] ShaderRegistry* shader_registry();

    [[nodiscard]] gl_engine::MapLabels* map_label_manager() const;
    void set_map_label_manager(std::shared_ptr<gl_engine::MapLabels> new_map_label_manager);

    [[nodiscard]] TileGeometry* tile_geometry() const;
    void set_tile_geometry(std::shared_ptr<TileGeometry> new_tile_geometry);

    [[nodiscard]] TextureLayer* ortho_layer() const;
    void set_ortho_layer(std::shared_ptr<TextureLayer> new_ortho_layer);

    [[nodiscard]] AvalancheWarningLayer* eaws_layer() const;
    void set_eaws_layer(std::shared_ptr<AvalancheWarningLayer> new_ortho_layer);

    [[nodiscard]] std::shared_ptr<nucleus::avalanche::UIntIdManager> eaws_id_manager() const;
    void set_eaws_id_manager(const std::shared_ptr<nucleus::avalanche::UIntIdManager>& new_manager);

    [[nodiscard]] std::shared_ptr<gl_engine::AvalancheReportManager> avalanche_report_manager();
    void set_avalanche_report_manager(std::shared_ptr<gl_engine::AvalancheReportManager> avalanche_report_manager);

protected:
    void internal_initialise() override;
    void internal_destroy() override;

private:
    std::shared_ptr<TileGeometry> m_tile_geometry;
    std::shared_ptr<TextureLayer> m_ortho_layer;
    std::shared_ptr<AvalancheWarningLayer> m_eaws_layer;
    std::shared_ptr<nucleus::avalanche::UIntIdManager> m_eaws_id_manager;
    std::shared_ptr<gl_engine::AvalancheReportManager> m_avalanche_report_manager;
    std::shared_ptr<gl_engine::MapLabels> m_map_label_manager;
    std::shared_ptr<gl_engine::TrackManager> m_track_manager;
    std::shared_ptr<gl_engine::ShaderRegistry> m_shader_registry;
};
} // namespace gl_engine
