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

#include "utils.h"

#include <QFile>

namespace {
template <typename T>
void write(QFile* file, T value)
{
    const auto bytes_written = file->write(reinterpret_cast<const char*>(&value), sizeof(value));
    assert(bytes_written == sizeof(value));
}

template <typename T>
std::optional<T> read(QFile* file)
{
    T value;
    const auto bytes_read = file->read(reinterpret_cast<char*>(&value), sizeof(value));
    if (bytes_read != sizeof(value))
        return {};
    return value;
}
}

void nucleus::tile_scheduler::utils::write_tile_id_2_data_map(const TileId2DataMap& map, const std::filesystem::path& path)
{
    QFile file(path);
    const auto success = file.open(QIODeviceBase::WriteOnly);
    assert(success);
    write<size_t>(&file, map.size());

    for (const auto& key_value : map) {
        write(&file, key_value.first);
        const auto data = key_value.second;
        write<qsizetype>(&file, data->size());
        const auto bytes_written = file.write(*data);
        assert(bytes_written == data->size());
    }
    file.close();
}

nucleus::tile_scheduler::TileId2DataMap nucleus::tile_scheduler::utils::read_tile_id_2_data_map(const std::filesystem::path& path)
{
    QFile file(path);
    const auto success = file.open(QIODeviceBase::ReadOnly);
    if (!success)
        return {};

    const auto size = read<size_t>(&file);
    if (!size.has_value())
        return {};
    nucleus::tile_scheduler::TileId2DataMap map;
    map.reserve(size.value());
    for (size_t i = 0; i < size; ++i) {
        const auto key = read<tile::Id>(&file);
        const auto data_size = read<qsizetype>(&file);
        if (!key.has_value() || !data_size.has_value())
            return {};
        auto data = file.read(data_size.value());
        if (data.size() != data_size)
            return {};
        map[key.value()] = std::make_shared<QByteArray>(std::move(data));
    }

    return map;
}
