import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("EnterPassword.qml")
    }
    Component.onDestruction: {
        pages.pop()
    }

    property var dispatchAction

    TextField {
        anchors.centerIn: parent
        width: parent.width
        height: implicitHeight
        horizontalAlignment: TextInput.AlignLeft
        echoMode: TextInput.Password

        inputMethodHints: Qt.ImhNoPredictiveText + Qt.ImhNoAutoUppercase
        placeholderText: qsTr("Enter password")
        focus: true

        EnterKey.onClicked: {
            dispatchAction(text)
        }
    }
}
