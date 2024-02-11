/*
 * Copyright (C) 2020-2021 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see LICENSE-zlib).
 *
 */

import QtQuick 2.0

Item {
    id: control

    property int fullValue: maxValue - minValue + 1 ///< value of a full circle
    property int minValue: 0 ///< minium selectable value
    property int maxValue: 359 ///< maximum selectable value
    property int presetValue: minValue ///< default value
    property bool trueWheel: true ///< whether the knob can be dragged only
    property bool wrapAround: true ///< whether the dial wraps around
    property real wheelSpeed: 1 ///< delta value of a single mouse wheel step

    readonly property alias value: state.displayValue ///< current value
    readonly property alias sigma: state.sigma ///< current value in radiants
    readonly property alias angle: state.angle ///< current value in degree
    readonly property alias pressed: state.dragActive ///< true if the user is currently touching/holding the dial

    signal moved

    function setValue(newValue)
    {
        state.setValue(newValue)
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        onPressed: {
            let pointerPos = getRadialPosition(mouse)
            if (!control.trueWheel) {
                let oldDisplayValue = state.displayValue
                state.advanceTo(pointerPos)
                if (state.displayValue !== oldDisplayValue) control.moved()
            }
            state.dragPrevious = pointerPos
            state.dragCurrent = pointerPos
            state.dragActive = true
            mouse.accepted = true
        }

        onReleased: {
            state.dragActive = false
        }

        onPositionChanged: {
            let pointerPos = getRadialPosition(mouse)
            state.dragPrevious = state.dragCurrent
            state.dragCurrent = pointerPos
            let oldDisplayValue = state.displayValue
            state.advanceBy(state.dragPrevious, state.dragCurrent)
            if (state.displayValue !== oldDisplayValue) control.moved()
        }

        onWheel: {
            let oldDisplayValue = state.displayValue
            state.addToSigma(Math.sign(wheel.angleDelta.y) * control.wheelSpeed * Math.PI / 180)
            if (state.displayValue !== oldDisplayValue) control.moved()
        }

        function getRadialPosition(mouse)
        {
            let rx = mouse.y - height / 2
            let ry = -(mouse.x - width / 2)
            return Qt.point(rx, ry)
        }
    }

    QtObject {
        id: state
        readonly property int displayValue: sigmaToValue(sigma)
        readonly property real angle: 180 * sigma / Math.PI

        property bool dragActive: false
        property point dragCurrent
        property point dragPrevious
        property real sigma: valueToSigma(control.presetValue)

        function setValue(newValue)
        {
            if (newValue < control.minValue) newValue = control.minValue
            else if (newValue > control.maxValue) newValue = control.maxValue
            state.sigma = valueToSigma(newValue)
        }

        function sigmaToValue(sigma)
        {
            return Math.round(control.fullValue * sigma / (2 * Math.PI)) + control.minValue
        }

        function valueToSigma(value)
        {
            return (value - control.minValue) * 2 * Math.PI / control.fullValue
        }

        function angularDistance(phi1, phi2)
        {
            let deltaPhi = 0
            if (
                phi2 < phi1 &&
                phi2 < Math.PI / 4 &&
                phi1 > 7 * Math.PI / 4
            ) {
                deltaPhi = phi2 - phi1 + 2 * Math.PI
            }
            else if (
                phi2 > phi1 &&
                phi1 < Math.PI / 4 &&
                phi2 > 7 * Math.PI / 4
            ) {
                deltaPhi = phi2 - phi1 - 2 * Math.PI
            }
            else {
                deltaPhi = phi2 - phi1
            }
            return deltaPhi
        }

        function dragAngle(radialPoint)
        {
            let phi = Math.atan2(radialPoint.y, radialPoint.x)
            if (phi < 0) phi = 2 * Math.PI + phi
            return phi
        }

        function addToSigma(deltaSigma)
        {
            let newSigma = sigma + deltaSigma
            let newValue = sigmaToValue(newSigma)
            if (control.wrapAround) {
                if (newValue < control.minValue) {
                    newSigma = valueToSigma(control.maxValue) - newSigma + valueToSigma(control.minValue)
                    if (sigmaToValue(newSigma) > control.maxValue) newSigma = valueToSigma(control.maxValue)
                }
                else if (newValue > control.maxValue) {
                    newSigma -= valueToSigma(control.fullValue) - valueToSigma(control.minValue)
                    if (sigmaToValue(newSigma) < control.minValue) newSigma = valueToSigma(control.minValue)
                }
            }
            else {
                if (newValue < control.minValue) newSigma = valueToSigma(control.minValue)
                else if (newValue > control.maxValue) newSigma = valueToSigma(control.maxValue)
            }
            sigma = newSigma
        }

        function advanceTo(radialPoint)
        {
            let phi1 = (state.sigma < 0 ? 2 * Math.PI : 0) + state.sigma % (2 * Math.PI)
            let phi2 = dragAngle(radialPoint)
            addToSigma(angularDistance(phi1, phi2))
        }

        function advanceBy(radialPoint1, radialPoint2)
        {
            let phi1 = dragAngle(radialPoint1)
            let phi2 = dragAngle(radialPoint2)
            addToSigma(angularDistance(phi1, phi2))
        }
    }
}
