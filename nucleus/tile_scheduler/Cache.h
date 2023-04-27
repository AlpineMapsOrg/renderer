/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <algorithm>
#include <vector>

#include "sherpa/tile.h"
#include "tile_types.h"
#include "utils.h"

namespace nucleus::tile_scheduler {

template <tile_types::NamedTile T>
class Cache {
    struct CacheObject {
        uint64_t stamp;
        T data;
    };

    std::unordered_map<tile::Id, CacheObject, tile::Id::Hasher> m_data;
    unsigned m_capacity = 5000;

public:
    Cache() = default;
    void insert(const std::vector<T>& tiles);
    [[nodiscard]] bool contains(const tile::Id& id) const;
    void set_capacity(unsigned capacity);
    [[nodiscard]] unsigned n_cached_objects() const;
    template <typename VisitorFunction>
    void visit(const VisitorFunction& functor); // functor should return true, if the given tile should be marked visited. stops descending if false is returned.
    const T& peak_at(const tile::Id& id) const;
    std::vector<T> purge();

private:
    template <typename VisitorFunction>
    void visit(const tile::Id& start_node, const VisitorFunction& functor, uint64_t stamp); // functor should return true, if the given tile should be marked visited. stops descending if false is returned.
};

template <tile_types::NamedTile T>
void Cache<T>::insert(const std::vector<T>& tiles)
{
    const auto stamp = utils::time_since_epoch();
    for (const T& t : tiles) {
        m_data[t.id].stamp = stamp * 100 - t.id.zoom_level;
        m_data[t.id].data = t;
    }
}

template <tile_types::NamedTile T>
bool Cache<T>::contains(const tile::Id& id) const
{
    return m_data.contains(id);
}

template <tile_types::NamedTile T>
void Cache<T>::set_capacity(unsigned int capacity)
{
    m_capacity = capacity;
}

template <tile_types::NamedTile T>
unsigned int Cache<T>::n_cached_objects() const
{
    return m_data.size();
}

template <tile_types::NamedTile T>
const T& Cache<T>::peak_at(const tile::Id& id) const
{
    return m_data.at(id).data;
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const VisitorFunction& functor)
{
    const auto stamp = utils::time_since_epoch();
    static_assert(requires { { functor(T()) } -> utils::convertible_to<bool>; });
    const auto root = tile::Id { 0, { 0, 0 } };
    visit(root, functor, stamp);
}

template <tile_types::NamedTile T>
template <typename VisitorFunction>
void Cache<T>::visit(const tile::Id& node, const VisitorFunction& functor, uint64_t stamp)
{
    static_assert(requires { { functor(T()) } -> utils::convertible_to<bool>; });
    if (m_data.contains(node)) {
        const auto should_continue = functor(m_data[node].data);
        if (!should_continue)
            return;
        m_data[node].stamp = stamp * 100 - m_data[node].data.id.zoom_level;
        const auto children = node.children();
        for (const auto& id : children) {
            visit(id, functor, stamp);
        }
    }
}

template <tile_types::NamedTile T>
std::vector<T> Cache<T>::purge()
{
    if (m_capacity >= n_cached_objects())
        return {};
    std::vector<std::pair<tile::Id, uint64_t>> tiles;
    tiles.reserve(m_data.size());
    std::transform(m_data.cbegin(), m_data.cend(), std::back_inserter(tiles), [](const auto& entry) { return std::make_pair(entry.first, entry.second.stamp); });
    const auto nth_iter = tiles.begin() + m_capacity;
    std::nth_element(tiles.begin(), nth_iter, tiles.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<T> purged_tiles;
    purged_tiles.reserve(tiles.size() - m_capacity);
    std::for_each(nth_iter, tiles.end(), [this, &purged_tiles](const auto& v) {
        purged_tiles.push_back(m_data[v.first].data);
        m_data.erase(v.first);
    });
    return purged_tiles;
}

} // namespace nucleus::tile_scheduler
