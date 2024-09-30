/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include <nucleus/event_parameter.h>
#include <nucleus/picker/types.h>
#include <nucleus/tile_scheduler/tile_types.h>
#include <nucleus/vector_tile/types.h>
#include <radix/tile.h>
#include <vector>

namespace nucleus::picker {
using nucleus::vector_tile::PointOfInterest;
using nucleus::vector_tile::PointOfInterestCollectionPtr;

class PickerManager : public QObject {
    Q_OBJECT
public:
    explicit PickerManager(QObject* parent = nullptr);
    void mouse_press_event(const event_parameter::Mouse& e);
    void mouse_release_event(const event_parameter::Mouse& e);
    void mouse_move_event(const event_parameter::Mouse& e);
    void touch_event(const event_parameter::Touch& e);

public slots:
    void update_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);
    void eval_pick(uint32_t value);

signals:
    void pick_requested(const glm::dvec2& position);
    void pick_evaluated(const Feature& feature);

private:
    vector_tile::PointOfInterestTileCollection m_all_pois;
    std::unordered_map<uint32_t, const PointOfInterest*> m_pickid_to_poi;

    glm::vec2 m_position;
    bool m_in_click;

    // how much can the click position change to be still considered to be valid
    static constexpr float m_valid_click_distance = 30.0f;

    void start_click_event(const glm::vec2& position);
    void end_click_event(const glm::vec2& position);

    void add_tile(const tile::Id id, const PointOfInterestCollectionPtr& all_pois);
    void remove_tile(const tile::Id id);
};
} // namespace nucleus::picker
