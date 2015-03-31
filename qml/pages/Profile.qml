import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    property string name: "profile"
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("Profile.qml")
    }
    Component.onDestruction: {
        pages.pop()
    }

    IconButton {
        id: avatar
        visible: true
        icon.source: friendList.get(0).friend_avatar
        property int errorCount: 0
        onErrorCountChanged: {
            source = "qrc:/images/blankavatar"
        }
        Connections {
            target: cyanide
            onSignal_avatar_change: avatar.icon.source = friendList.get(0).friend_avatar
        }

        onClicked: {
            fileChooserProperties = {
                target: "selfAvatar",
                nameFilters: ["*.png", ".gif"]
            }
            pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/" } )
        }
        width: 1/4 * parent.width - Theme.paddingMedium
        y: Theme.paddingLarge
        x: name.x + name.width + Theme.paddingSmall
    }

    TextField {
        id: name
        label: qsTr("Name")
        width: 3/4 * parent.width - Theme.paddingLarge
        height: implicitHeight
        y: Theme.paddingLarge
        x: Theme.paddingMedium
        color: Theme.primaryColor

        inputMethodHints: Qt.ImhNoPredictiveText + Qt.ImhNoAutoUppercase
        text: cyanide.get_friend_name(self_friend_number)

        EnterKey.onClicked: {
            cyanide.set_self_name(text)
            focus = false
        }
    }
    TextField {
        id: message
        label: qsTr("Status Message")
        width: parent.width - Theme.paddingLarge
        height: implicitHeight
        x: Theme.paddingMedium
        y: name.y + name.height + Theme.paddingLarge
        color: Theme.primaryColor

        inputMethodHints: Qt.ImhNoAutoUppercase
        text: cyanide.get_friend_status_message(self_friend_number)
        EnterKey.onClicked: {
            cyanide.set_self_status_message(text)
            focus = false
        }
    }
    ComboBox {
        id: statusMenu
        x: Theme.paddingMedium
        y: message.y + message.height + Theme.paddingLarge
        width: parent.width - 2 * Theme.paddingLarge
        label: "Status"
        currentIndex: cyanide.get_self_user_status()

        menu: ContextMenu {
            MenuItem { text: qsTr("Online") }
            MenuItem { text: qsTr("Away") }
            MenuItem { text: qsTr("Busy") }
        }
        onCurrentIndexChanged:  cyanide.set_self_user_status(currentIndex)
    }
    Image {
        id: selfStatusIcon
        source: friendList.get(0).friend_status_icon
        anchors {
            right: parent.right
            rightMargin: Theme.paddingLarge
            top: statusMenu.top
            topMargin: statusMenu.height / 2 - height / 2
        }
    }
    Text {
        id: id
        width: parent.width - Theme.paddingLarge
        height: implicitHeight
        x: Theme.paddingMedium
        y: statusMenu.y + statusMenu.height + Theme.paddingLarge
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
        y: id.y + id.height + Theme.paddingSmall
        color: Theme.primaryColor
        text: qsTr("Copy my Tox ID") + " ☐"
        onClicked: {
            clipboard.setClipboard(id.text)
            text = qsTr("Copy my Tox ID") + " ☑"
        }
    }
}
