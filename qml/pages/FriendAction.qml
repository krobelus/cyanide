import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    property string name: "friendaction"
    allowedOrientations: Orientation.All

    RemorsePopup { id: remorsePopup }

    property int f: activeFriend()

    Column {
        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }
        spacing: Theme.paddingLarge

        Row {
            spacing: 3 * Theme.paddingMedium

            IconButton {
                icon.source: "qrc:/images/attach_4x"
                onClicked: {
                    fileChooserProperties = {
                        target: "fileToSend",
                        nameFilters: []
                    }
                    pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/" } )
                }
            }
            IconButton {
                icon.source: "qrc:/images/phone_4x"
                onClicked: {
                    cyanide.notify_error("Not implemented yet", "")
                }
            }
            IconButton {
                icon.source: "qrc:/images/video_4x"
                onClicked: {
                    cyanide.notify_error("Not implemented yet", "")
                }
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Copy Tox ID to clipboard")
            enabled: friendList.get(f+1).friend_address !== ""
            onClicked: {
                clipboard.setClipboard(friendList.get(f+1).friend_address)
            }
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            property bool blocked: friendList.get(f+1).friend_blocked
            text: blocked ? qsTr("Unblock friend") : qsTr("Block friend")
            onClicked: {
                cyanide.set_friend_blocked(f, !blocked)
            }
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Remove friend")
            onClicked: {
                remorsePopup.execute(qsTr("Removing friend"), function() {
                    cyanide.remove_friend(f)
                    refreshFriendList()
                    pageStack.pop()
                    pageStack.pop()
                })
            }
        }
    }
}
