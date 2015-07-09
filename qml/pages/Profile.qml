import QtQuick 2.0
import Sailfish.Silica 1.0
import "../qqr"

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

    RemorsePopup { id: remorsePopup }

    SilicaFlickable {
        contentHeight: column.height + Theme.paddingLarge
        anchors {
            fill: parent
        }

        VerticalScrollDecorator { }

        PullDownMenu {
            MenuItem {
                text: qsTr("Delete profile");
                onClicked: {
                    remorsePopup.execute(qsTr("Deleting profile"), function() {
                        cyanide.delete_current_profile()
                        fileChooserProperties = {
                            target: "toxSaveFile",
                            nameFilters: [ '*.tox' ],
                            filter: true
                        }
                        pageStack.clear()
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/.config/tox/" } )
                    })
                }
            }
        }

        Column {
            id: column
            spacing: Theme.paddingMedium
            y: Theme.paddingLarge

            TextField {
                id: profileName
                text: cyanide.get_profile_name()
                width: name.width
                label: qsTr("Profile Name")
                color: Theme.primaryColor
                inputMethodHints: Qt.ImhNoPredictiveText + Qt.ImhNoAutoUppercase
                EnterKey.onClicked: {
                    var errmsg = cyanide.set_profile_name(text)
                    if(errmsg === "") {
                        focus = false
                    } else {
                        cyanide.notify_error("Failed to rename profile", qsTr(errmsg))
                    }
                }
            }
            TextField {
                id: name
                //: username for this profile
                label: qsTr("Name")
                width: 3/4 * page.width - Theme.paddingLarge
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
                width: name.width
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
                label: "Status"
                currentIndex: cyanide.get_self_user_status()

                menu: ContextMenu {
                    //: user status
                    MenuItem { text: qsTr("Online") }
                    MenuItem { text: qsTr("Away") }
                    MenuItem { text: qsTr("Busy") }
                }
                onCurrentIndexChanged:  cyanide.set_self_user_status(currentIndex)
            }
            Label {
                x: Theme.paddingLarge
                text: "Tox ID"
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
            }
            Text {
                id: dummy
                visible: false
                text: "A"
                font.family: "Monospace"
            }
            Text {
                id: id
                x: (page.width - width) / 2
                text: cyanide.get_self_address()
                color: Theme.primaryColor
                font.family: "Monospace"

                width: 78 * dummy.paintedWidth / 4 // 19 hex characters per line
                height: implicitHeight

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
                x: (page.width - width) / 2
                color: Theme.primaryColor
                text: qsTr("Copy my Tox ID") + " ☐"
                onClicked: {
                    clipboard.setClipboard(id.text)
                    text = qsTr("Copy my Tox ID") + " ☑"
                }
            }
            QRCode {
                x: (page.width - width) / 2
                width : 296
                height : 296
                value : "tox:"+id.text
            }
            /*
            Button {
                id: nospamButton
                x: (page.width - width) / 2
                text: qsTr("Generate random NoSpam")
                onClicked: {
                    cyanide.set_random_nospam()
                    id.text = cyanide.get_self_address()
                    copyButton.text = qsTr("Copy my Tox ID") + " ☐"
                }
            }
            */
        }
        IconButton {
            id: avatar
            visible: true
            icon.source: friendList.get(0).friend_avatar
            property int errorCount: 0
            onErrorCountChanged: {
                source = "qrc:/images/blankavatar"
            }

            onClicked: {
                fileChooserProperties = {
                    target: "selfAvatar",
                    nameFilters: ["*.png", ".gif"],
                    filter: true
                }
                pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/" } )
            }
            y: name.y
            x: name.x + name.width + Theme.paddingSmall
        }
        Image {
            id: selfStatusIcon
            source: friendList.get(0).friend_status_icon
            y: statusMenu.y + 2 * Theme.paddingLarge + Theme.paddingMedium
            x: page.width - width - Theme.paddingLarge
        }
        Connections {
            target: cyanide
            onSignal_avatar_change: avatar.icon.source = friendList.get(0).friend_avatar
            onSignal_friend_status: selfStatusIcon.source = friendList.get(0).friend_status_icon
            onSignal_friend_connection_status: selfStatusIcon.source = friendList.get(0).friend_status_icon
        }
    }
}
