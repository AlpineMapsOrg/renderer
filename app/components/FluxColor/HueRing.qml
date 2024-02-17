/*
 * Copyright (C) 2020-2021 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see LICENSE-zlib).
 *
 */

import QtQuick 2.6

Canvas {
    anchors.fill: parent
    property real thickness: radius * 0.3
    readonly property real radius: Math.min(width, height) / 2

    onPaint: {
        function degree(angle) {
            return Math.PI * angle / 180
        }
        let context = getContext("2d")
        let centerX = width / 2
        let centerY = height / 2
        const step = degree(1)
        const overlap = degree(1)
        for (let alpha = 0; alpha < degree(360); alpha += step) {
            let color = Qt.hsva(alpha / degree(360), 1, 1, 1)
            context.beginPath()
            context.fillStyle = color
            context.arc(centerX, centerY, radius, -alpha, -(alpha + step + overlap), true)
            context.arc(centerX, centerY, radius - thickness, -(alpha + step + overlap), -alpha, false)
            context.fill()
        }
    }
}
