/*
 * Copyright (C) 2020-2021 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see LICENSE-zlib).
 *
 */

import QtQuick

WheelArea {
    id: control
    property real colorHue: 0
    property real colorSaturation: 1
    property real colorValue: 1
    property real colorAlpha: 1

    readonly property color new_colour: Qt.hsva(control.angle / 360, control.colorSaturation, control.colorValue, control.colorAlpha)

    presetValue: control.colorHue * 360
    HueRing {
        id: hueRing
        anchors.margins: 16
        rotation: (control.angle + 90) % 360
    }

    Rectangle {
        anchors.centerIn: hueRing
        radius: hueRing.radius * 0.3
        width: 2 * radius
        height: 2 * radius
        color: new_colour
    }

    Image {
        source: _r + "icons/needle_head_down.png"
        anchors.centerIn: hueRing
        anchors.verticalCenterOffset: hueRing.radius - hueRing.thickness
    }
}
