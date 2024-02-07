/*
 * Copyright (C) 2020-2021 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see LICENSE-zlib).
 *
 */

import QtQuick
import QtQuick.Controls.Material

Column {
    readonly property color color: hueWheel.color

    id: column
    Item {
        width: column.width
        height: width
        HueWheel {
            id: hueWheel
            anchors.fill: parent
        }
    }
    Column {
        leftPadding: 16
        rightPadding: 16
        bottomPadding: 16
        spacing: 8
        Row {
            spacing: parent.spacing
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("S")
            }
            Slider {
                id: satSlider
                from: 0
                to: 1
                value: 1
                onMoved: {
                    hueWheel.colorSaturation = satSlider.value
                }
            }
        }
        Row {
            spacing: parent.spacing
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("V")
            }
            Slider {
                id: valSlider
                from: 0
                to: 1
                value: 1
                onMoved: {
                    hueWheel.colorValue = valSlider.value
                }
            }
        }
    }
    TextField {
        id: rgbField
        width: parent.width - 2 * x
        x: 16
        text: color
        font.capitalization: Font.AllUppercase
        font.family: "mono"
        onFocusChanged: if (focus) selectAll()
        Connections {
            target: hueWheel
            function onColorChanged() {
                rgbField.text = hueWheel.color
            }
        }
        Connections {
            property color newColor
            function onEditingFinished() {
                newColor = rgbField.text
                let invalid = newColor.hsvValue === 0 && rgbField.text.toLowerCase() != "black" && rgbField.text.toUpperCase() != "#000" && rgbField.text.toUpperCase() != "#000000"
                if (!invalid) {
                    satSlider.value = newColor.hsvSaturation
                    valSlider.value = newColor.hsvValue
                    satSlider.moved()
                    valSlider.moved()
                    hueWheel.setValue(newColor.hsvHue * 360)
                    rgbField.text = newColor
                    rgbField.focus = false
                }
                else {
                    rgbField.selectAll()
                }
            }
        }
    }
    Item { width: 1; height: 16 }
}
