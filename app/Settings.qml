/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

import "components"

Rectangle {
    id: settings_root
    color: "#00FFFFFF"
    onWidthChanged: responsive_update()
    onHeightChanged: responsive_update()

    function responsive_update() {
        var newWidth = 400;
        if (settings_root.width < 600) newWidth = 300;
        else if (settings_root.width < 300) newWidth = settings_root.width;
        settings_frame.width = newWidth;
        if (settings_root.width >= settings_root.height) {
            // landscape
            settings_frame.anchors.left = undefined;
            settings_frame.height = settings_root.height;
        } else {
            // portrait
            settings_frame.anchors.left = settings_root.left;
            let new_height = settings_root.height / 2.0;
            if (settings_root.height < 400) new_height = settings_root.height / 1.3;
            else if (settings_root.height < 700) new_height = settings_root.height / 1.5;
            settings_frame.height = new_height
        }
    }


    Rectangle {
        id: settings_frame
        color: Qt.alpha(Material.backgroundColor, 0.7)
        clip: true
        anchors {
            right: settings_root.right
            bottom: settings_root.bottom
        }

        TabBar {
            id: navigation_bar
            currentIndex: view.currentIndex

            background: Rectangle {
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(0.0, 0.0, 0.0, 0.0) }
                    GradientStop { position: 1.0; color: Qt.rgba(0.0, 0.0, 0.0, 0.1) }
                }
            }

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            contentWidth: parent.width
            position: TabBar.Header
            TabButton {
                text: qsTr("General")
                width: implicitWidth + 20
            }
            TabButton {
                text: qsTr("GL Configuration")
                width: implicitWidth + 20
            }
        }

        SwipeView {
            id: view
            currentIndex: navigation_bar.currentIndex
            anchors {
                left: parent.left
                right: parent.right
                top: navigation_bar.bottom
                bottom: parent.bottom
            }
            background: Rectangle {
                color: Qt.rgba(0.0, 0.0, 0.0, 0.1)
            }

            GeneralSettings {}
            GlSettings {}
        }
    }
}
