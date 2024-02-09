/*
 * Copyright (C) 2020-2021 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see LICENSE-zlib).
 *
 */

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Column {
    property color selected_colour
    readonly property alias new_colour: hueWheel.new_colour

    id: column
    Item {
        width: column.width
        height: width
        HueWheel {
            id: hueWheel
            anchors.fill: parent
            colorAlpha: alphaSlider.value
            colorSaturation: satSlider.value
            colorValue: valSlider.value
            colorHue: selected_colour.hsvHue
        }
    }
    GridLayout {
        columns: 2

        Label {
            text: qsTr("Saturation")
        }
        Slider {
            id: satSlider
            from: 0
            to: 1
            value: selected_colour.hsvSaturation
        }
        Label {
            text: qsTr("Value")
        }
        Slider {
            id: valSlider
            from: 0
            to: 1
            value: selected_colour.hsvValue
        }
        Label {
            text: qsTr("Alpha")
        }
        Slider {
            id: alphaSlider
            from: 0
            to: 1
            value: selected_colour.a
        }
    }
    TextField {
        id: rgbField
        width: parent.width - 2 * x
        x: 16
        text: new_colour.toString().toUpperCase();
        font.capitalization: Font.AllUppercase
        font.family: "mono"
        onFocusChanged: if (focus) selectAll()
    }
    Item { width: 1; height: 16 }
}
