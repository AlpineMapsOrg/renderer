import QtQuick
import QtQuick.Controls.Material as QCM

QCM.ComboBox {
    // indexOfValue works only after the component is completed, that's why currently values must be same as indices.
    id: root
    property var target;
    property string property: ""
    Binding {
        when: !!root.target
        target: root.target ? root.target : root
        property: root.target ? root.property : ""
        value: root.currentIndex
    }
    Binding {
        when: !!root.target
        target: root
        property: "currentIndex"
        value: {
            root.target ? root.target[root.property] : 0
        }
    }
}
