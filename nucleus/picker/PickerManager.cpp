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

#include "PickerManager.h"
#include <QDebug>

namespace nucleus::picker {

// TODOs
// - connect picked id to create gui window with data

PickerManager::PickerManager(QObject* parent)
    : QObject { parent }
{
}

void PickerManager::eval_pick(uint32_t value)
{
    PickTypes type = get_picker_type((value >> 24) & 255);

    if (type == PickTypes::invalid) {
        // e.g. clicked on nothing -> hide sidebar
        emit pick_evaluated(FeatureProperties {});
        return;
    }

    if (type == PickTypes::feature) {
        const auto internal_id = value & 16777215; // 16777215 = 24bit mask
        if (!m_pickid_to_feature.contains(internal_id)) {
            qDebug() << "pickid does not exist: " + std::to_string(internal_id);
            return;
        }

        emit pick_evaluated(m_pickid_to_feature[internal_id]->get_feature_data());
        return;
    }
}

void PickerManager::update_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            if (!tile.vector_tile || tile.vector_tile->empty())
                continue;

            assert(tile.id.zoom_level < 100);

            add_tile(tile.id, *tile.vector_tile);
        }
    }
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }
}

void PickerManager::add_tile(const tile::Id id, const VectorTile& all_features)
{
    m_all_features[id] = all_features;

    for (const auto& feature : all_features) {
        m_pickid_to_feature[feature->internal_id] = feature;
    }
}

void PickerManager::remove_tile(const tile::Id id)
{

    if (m_all_features.contains(id)) {
        for (const auto& feature : m_all_features[id]) {
            m_pickid_to_feature.erase(feature->internal_id);
        }
        m_all_features.erase(id);
    }
}

void PickerManager::mouse_press_event(const event_parameter::Mouse& e)
{
    if (e.buttons == Qt::LeftButton)
        start_click_event({ e.point.position.x, e.point.position.y });
}

void PickerManager::mouse_release_event(const event_parameter::Mouse& e)
{
    if (m_in_click)
        end_click_event({ e.point.position.x, e.point.position.y });
}

void PickerManager::mouse_move_event(const event_parameter::Mouse& e)
{

    if (m_in_click && e.buttons == Qt::LeftButton) {
        if (glm::length(e.point.position - m_position) > m_valid_click_distance) {
            m_in_click = false;
        }
    }
}

void PickerManager::touch_event(const event_parameter::Touch& e)
{
    // touch clicks
    if (m_in_click) {
        if (e.points.size() > 1) {
            // is not a singular touch event anymore -> cancel it
            m_in_click = false;
        } else if (e.points[0].state == event_parameter::TouchPointMoved) {
            // moved too far away from first touch event -> cancle it
            if (glm::length(e.points[0].position - m_position) > m_valid_click_distance)
                m_in_click = false;

        } else if (e.points.size() == 1 && e.points[0].state == event_parameter::TouchPointReleased) {
            // valid touch end event
            end_click_event({ e.points[0].position.x, e.points[0].position.y });
        }
    } else {
        if (e.points.size() == 1 && e.points[0].state == event_parameter::TouchPointPressed)
            start_click_event({ e.points[0].position.x, e.points[0].position.y });
    }
}

void PickerManager::start_click_event(const glm::vec2& position)
{
    m_in_click = true;
    m_position = position;
}
void PickerManager::end_click_event(const glm::vec2& position)
{
    m_in_click = false;
    if (glm::length(position - m_position) < m_valid_click_distance) {
        emit pick_requested(position);
        //        std::cout << "click detected" << std::endl;
    }
}

} // namespace nucleus::picker
