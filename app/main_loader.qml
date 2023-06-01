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

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Alpine

ApplicationWindow {
    visible: true

//    Material.theme: Material.Light
//    Material.accent: Material.System

    Loader {
        id: mainLoader
        anchors.fill: parent
        source: _qmlPath + "/app/Main.qml"
    }

    Connections{
        target: _hotreloader
        onWatched_source_changed: {
            mainLoader.active = false;
            _hotreloader.clear_cache();
            mainLoader.setSource(_qmlPath+ "/app/Main.qml")
            mainLoader.active = true;
        }
    }

}
