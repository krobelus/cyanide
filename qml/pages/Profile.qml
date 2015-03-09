import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: pageProfile
    allowedOrientations: Orientation.Landscape | Orientation.Portrait
    Component.onCompleted: {
        activePage = "Profile.qml"
        currentFID = 0
    }

    TextField {
        id: name
        label: qsTr("Name")
        width: parent.width - Theme.paddingLarge
        height: implicitHeight
        y: 2 * Theme.paddingLarge
        x: Theme.paddingMedium
        color: Theme.primaryColor

        inputMethodHints: Qt.ImhNoPredictiveText + Qt.ImhNoAutoUppercase
        text: cyanide.get_friend_name(selfID)

        EnterKey.onClicked: {
            if(cyanide.set_self_name(text))
                focus = false
            // else TODO
        }
    }
    TextField {
        id: message
        label: qsTr("Status Message")
        width: parent.width - Theme.paddingLarge
        height: implicitHeight
        x: Theme.paddingMedium
        y: 3 * Theme.paddingLarge + name.height
        color: Theme.primaryColor

        inputMethodHints: Qt.ImhNoAutoUppercase
        text: cyanide.get_friend_status_message(selfID)
        EnterKey.onClicked: {
            cyanide.set_self_status_message(text)
            focus = false
        }
    }
    Text {
        id: id
        width: parent.width - Theme.paddingLarge
        height: implicitHeight
        x: Theme.paddingMedium
        y: 4 * Theme.paddingLarge + name.height + message.height
        color: Theme.primaryColor

        text: cyanide.get_self_address()
        wrapMode: Text.WrapAnywhere
    }
    TextEdit {
        id: clipboard
        visible: false
        function setClipboard(value) {
            text = value
            selectAll()
            copy()
        }
        function getClipboard() {
            text = ""
            paste()
            return text
        }
    }
    Button {
        id: copyButton
        width: parent.width / 2
        x: Theme.paddingLarge
        y: 5 * Theme.paddingLarge + name.height + message.height + id.height
        color: Theme.primaryColor
        text: qsTr("Copy my Tox ID") + " ☐"
        onClicked: {
            clipboard.setClipboard(id.text)
            text = qsTr("Copy my Tox ID") + " ☑"
        }
    }
}
