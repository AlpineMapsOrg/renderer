import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: root;

    property date selectedDate: new Date(0);

    onSelectedDateChanged: {
        label.text = Qt.formatDate(selectedDate, "dd.MM.yyyy");
    }

    // Define the Dialog
    Dialog {
        id: editDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 400
        height: 400
        background: Rectangle {
            color: Material.backgroundColor
            radius: 0
        }

        visible: false

        DateMonthTablePicker {
            id: monthTable
            onClicked: {
                editDialog.close();
                root.selectedDate = selectedDate;
            }
        }

    }

    Label {
        id: label;
        padding: 5;
        text: "#FFFFFFF";
        font.underline: true;
        Layout.fillWidth: true;

        MouseArea{
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                monthTable.set(selectedDate)
                editDialog.open()
            }
        }
    }

    height: label.height;
    Layout.fillWidth: true;

}
