 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "Definition.h"
#include "nucleus/srs.h"
#include <map>
#include <QDebug>

#include <QList>
#include <QString>

namespace nucleus::camera::stored_positions {
// coordinate transformer: https://epsg.io/transform#s_srs=4326&t_srs=3857&x=16.3726561&y=48.2086939
inline nucleus::camera::Definition oestl_hochgrubach_spitze()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.5587933, 12.3450985, 2277.0 });
    return { { coords.x, coords.y - 500, coords.z + 500 }, coords };
}
inline nucleus::camera::Definition stephansdom()
{
    const auto coords = srs::lat_long_alt_to_world({ 48.20851144787232, 16.373082444395656, 171.28 });
    return {{coords.x, coords.y - 500, coords.z + 500}, coords};
}
inline nucleus::camera::Definition stephansdom_closeup()
{
    const auto coords = srs::lat_long_alt_to_world({48.20851144787232, 16.373082444395656, 171.28});
    return {{coords.x, coords.y - 100, coords.z + 100}, coords};
}

inline nucleus::camera::Definition grossglockner()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.07386676653372, 12.694470292406267, 3798 });
    return { { coords.x - 300, coords.y - 400, coords.z + 100 }, { coords.x, coords.y, coords.z - 100 } };
}

inline nucleus::camera::Definition grossglockner_topdown()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.07386676653372, 12.694470292406267, 3798 });
    return { { coords.x , coords.y , coords.z + 3000 }, { coords.x, coords.y, coords.z } };
}

inline nucleus::camera::Definition grossglockner_shadow()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.07386676653372, 12.694470292406267, 3798 });
    return { { coords.x + 1800, coords.y + 400 , coords.z + 650 }, { coords.x, coords.y, coords.z } };
}

inline nucleus::camera::Definition schneeberg()
{
    const auto coords = srs::lat_long_alt_to_world({47.767163598, 15.804663448, 2076});
    return {{coords.x + 2500, coords.y - 100, coords.z + 100}, {coords.x, coords.y, coords.z - 100}};
}

inline nucleus::camera::Definition karwendel()
{
    const auto coords = srs::lat_long_alt_to_world({47.416665, 11.4666648, 2000});
    return {{coords.x + 5000, coords.y + 2000, coords.z + 1000},
            {coords.x, coords.y, coords.z - 100}};
}
inline nucleus::camera::Definition weichtalhaus()
{
    const auto coords_lookat = srs::lat_long_alt_to_world({47.74977, 15.77830, 1000});
    const auto coords_position = srs::lat_long_alt_to_world({47.74562, 15.75643, 1400});
    return {coords_position,coords_lookat};
}
inline nucleus::camera::Definition wien()
{
    const auto coords = srs::lat_long_alt_to_world({48.20851144787232, 16.373082444395656, 171.28});
    return {{coords.x + 10'000, coords.y + 2'000, coords.z + 1'000}, coords};
}

} // namespace nucleus::camera::stored_positions

namespace nucleus::camera {

class PositionStorage {

private:
    PositionStorage() {
        _positions.insert({"hochgrubach_spitze", nucleus::camera::stored_positions::oestl_hochgrubach_spitze()});
        _positions.insert({"stephansdom", nucleus::camera::stored_positions::stephansdom()});
        _positions.insert({"stephansdom_closeup", nucleus::camera::stored_positions::stephansdom_closeup()});
        _positions.insert({"grossglockner", nucleus::camera::stored_positions::grossglockner()});
        _positions.insert({"grossglockner_topdown", nucleus::camera::stored_positions::grossglockner_topdown()});
        _positions.insert({"schneeberg", nucleus::camera::stored_positions::schneeberg()});
        _positions.insert({"karwendel", nucleus::camera::stored_positions::karwendel()});
        _positions.insert({"wien", nucleus::camera::stored_positions::wien()});
        _positions.insert({"grossglockner_shadow", nucleus::camera::stored_positions::grossglockner_shadow()});
        _positions.insert({"weichtalhaus", nucleus::camera::stored_positions::weichtalhaus()});
    }
    static PositionStorage* _instance;
    std::map<std::string, nucleus::camera::Definition> _positions;

public:
    PositionStorage(PositionStorage &other) = delete;
    void operator=(const PositionStorage &) = delete;
    static PositionStorage *instance();
    nucleus::camera::Definition get(const std::string& name) {
        auto it = _positions.find(name);
        if (it != _positions.end()) {
            return it->second;
        } else {
            qWarning() << "Position by name" << name << "not existing.";
        }
        return {};
    }

    nucleus::camera::Definition get_by_index(unsigned id) {
        unsigned i = 0;
        for (const auto& kv : _positions) {
            if (i == id) return kv.second;
            i++;
        }
        qWarning() << "Position at index" << id <<  "non existing";
        return {};
    }

    QList<QString> getPositionList() {
        QList<QString> res;
        for (const auto& kv : _positions) {
            res.append(kv.first.c_str());
        }
        return res;
    }
};

}
