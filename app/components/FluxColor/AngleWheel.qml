/*
 * Copyright (C) 2020-2021 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see LICENSE-zlib).
 *
 */

import QtQuick 2.0

WheelArea {
    id: control

    property string annotation: "Angle / °" ///< text for the annotation label

    Item {
        property real diameter: Math.min(parent.width, parent.height)
        anchors.centerIn: parent
        width: diameter
        height: diameter
        Rectangle {
            id: disc
            anchors.fill: parent
            anchors.margins: 40
            radius: width / 2
            color: "silver"
            Rectangle {
                id: display
                anchors.fill: parent
                anchors.margins: parent.width * 0.2
                radius: width / 2
                color: "white"
                Text {
                    id: displayValue
                    anchors.centerIn: parent
                    font.pixelSize: parent.height / 3
                    // font.bold: true
                    // font.family: "Mono"
                    color: "darkgrey"
                    text: control.value
                }
                Text {
                    id: displayLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: displayValue.bottom
                    // anchors.topMargin: font.pixelSize
                    font.pixelSize: displayValue.font.pixelSize / 4
                    color: displayValue.color
                    text: control.annotation
                }
            }
        }

        Item {
            id: handle
            anchors.centerIn: parent
            rotation: control.angle
            Rectangle {
                id: knob
                x: -width/2
                y: display.radius + 3
                width: 10
                height: disc.radius - display.radius - 6
                radius: -0.001
                color: "white"
                // smooth: true
            }
        }
    }
}
