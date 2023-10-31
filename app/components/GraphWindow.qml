import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    default property alias content: groupchildren.children
    property alias name: groupname.text;
    property alias checked: checkbox.checked;
    property bool checkBoxEnabled: false;
    id: setgroup
    Layout.fillWidth: true
    Layout.topMargin: 20
    Layout.preferredHeight: rootlayout.implicitHeight

    onCheckedChanged: {
        if (checked) groupchildren.opacity = 1.0
        else groupchildren.opacity = 0.3
    }

    ColumnLayout {
        id: rootlayout
        anchors.fill: parent
        RowLayout {
            id: header
            visible: setgroup.name !== ""
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                Layout.preferredHeight: 2
                Layout.preferredWidth: 40
                color: Material.color(Material.Grey)
                CheckBox {
                    id: checkbox
                    visible: setgroup.checkBoxEnabled
                    y: -this.height/2
                    Component.onCompleted: {
                        // NOTE: CAN BREAK IF QT DEFINITION OF CHECKBOX CHANGES
                        indicator.color = Material.color(Material.Grey);
                    }
                }
            }
            Label {
                id: groupname
                text: ""
                font.bold: true
                font.capitalization: Font.AllUppercase
                Layout.preferredWidth: implicitWidth + 10
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Qt.alpha(Material.hintTextColor, 0.2)
            }
        }


        GridLayout {
            columns: 2
            id: groupchildren
            Layout.fillWidth: true
        }
    }

}
