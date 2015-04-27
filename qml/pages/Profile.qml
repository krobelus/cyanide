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

    RemorsePopup { id: remorsePopup }

    SilicaFlickable {
        anchors {
            fill: parent
        }

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
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/.config/tox/" } )
                    })
                }
            }
        }

        Column {
            id: column
            spacing: Theme.paddingMedium
            y: Theme.paddingMedium

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
            Text {
                id: id
                width: page.width - Theme.paddingLarge
                height: implicitHeight
                x: Theme.paddingMedium
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
                color: Theme.primaryColor
                text: qsTr("Copy my Tox ID") + " ☐"
                onClicked: {
                    clipboard.setClipboard(id.text)
                    text = qsTr("Copy my Tox ID") + " ☑"
                }
            }
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
            y: statusMenu.y + statusMenu.height / 2 - height / 2
            x: page.width - width - Theme.paddingLarge
        }
    }
}
