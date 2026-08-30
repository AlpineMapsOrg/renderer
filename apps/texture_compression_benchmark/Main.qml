import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TextureCompressionPreview

ApplicationWindow {
    id: root

    width: 900
    height: 900
    minimumWidth: 360
    minimumHeight: 480
    visible: true
    title: qsTr("Texture Compression Preview")
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Texture compression preview")
            font.pointSize: 20
            font.weight: Font.Medium
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            text: preview.status
            wrapMode: Text.Wrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 760 ? 9 : width >= 500 ? 5 : 3
            columnSpacing: 6
            rowSpacing: 6

            Repeater {
                model: preview.previewEncoders

                Button {
                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    text: modelData
                    enabled: preview.ready
                    highlighted: preview.previewEncoder === index
                    onClicked: preview.previewEncoder = index
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 300

            Flickable {
                id: previewViewport

                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, previewHost.width * previewHost.scale)
                contentHeight: Math.max(height, previewHost.height * previewHost.scale)

                Item {
                    id: previewHost

                    readonly property real fittedSize: Math.min(previewViewport.width, previewViewport.height)

                    x: (previewViewport.contentWidth - width) / 2
                    y: (previewViewport.contentHeight - height) / 2
                    width: fittedSize
                    height: width

                    TexturePreviewItem {
                        id: preview
                        anchors.fill: parent
                    }

                    PinchHandler {
                        target: previewHost
                        rotationAxis.enabled: false
                        xAxis.enabled: false
                        yAxis.enabled: false
                        scaleAxis.minimum: 1
                        scaleAxis.maximum: 8
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: preview.loading
                visible: running
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: preview.ready
                    ? (Number.isFinite(preview.previewPsnr)
                        ? qsTr("%1 — PSNR: %2 dB").arg(preview.previewName).arg(preview.previewPsnr.toFixed(2))
                        : qsTr("%1 — PSNR: ∞").arg(preview.previewName))
                    : ""
                wrapMode: Text.Wrap
            }

            Button {
                text: qsTr("Description")
                enabled: preview.ready
                onClicked: {
                    if (descriptionDialogLoader.status === Loader.Ready)
                        descriptionDialogLoader.item.open()
                    else
                        descriptionDialogLoader.active = true
                }
            }
        }
    }

    Loader {
        id: descriptionDialogLoader

        active: false
        asynchronous: true
        onLoaded: {
            if (status === Loader.Ready)
                item.open()
        }

        sourceComponent: Component {
            Dialog {
                parent: Overlay.overlay
                anchors.centerIn: parent
                width: Math.min(root.width - 40, 560)
                modal: true
                title: preview.previewName
                standardButtons: Dialog.Close

                contentItem: Label {
                    text: preview.previewDescription
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
