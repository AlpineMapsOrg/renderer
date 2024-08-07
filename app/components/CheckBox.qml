import QtQuick
import QtQuick.Controls.Material as QCM

QCM.CheckBox {
    id: root
    property var target;
    property string property: ""
    Binding {
        when: !!root.target
        target: root
        property: "checked"
        value: {
            root.target ? root.target[root.property] : false
        }
    }
    Binding {
        when: !!root.target
        target: root.target ? root.target : root
        property: root.target ? root.property : ""
        value: root.checked
    }
}
