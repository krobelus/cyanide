import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    property string name: "friendaction"
    allowedOrientations: Orientation.All
    Component.onDestruction: {
        if(activePage() === "FriendAction.qml") {
            pages.pop()
        }
    }

    RemorsePopup { id: remorsePopup }

    property int f: activeFriend()

    Column {
        anchors {
            centerIn: parent
        }
        spacing: Theme.paddingLarge

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
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
                enabled: false
                onClicked: {
                    cyanide.av_call(f)
                    pageStack.pop()
                }
            }
            IconButton {
                icon.source: "qrc:/images/video_4x"
                enabled: false
                onClicked: {
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
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            Button {
                text: qsTr("Load chat log")
                onClicked: pageStack.push(Qt.resolvedUrl("LoadChatHistory.qml"))
            }
            Button {
                text: qsTr("Clear chat log")
                onClicked: {
                    remorsePopup.execute(qsTr("Clearing chat log"), function() {
                        cyanide.clear_history(f)
                        pageStack.pop()
                    })
                }
            }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            Button {
                property bool blocked: friendList.get(f+1).friend_blocked
                text: blocked ? qsTr("Unblock friend") : qsTr("Block friend")
                onClicked: {
                    cyanide.set_friend_blocked(f, !blocked)
                }
            }
            Button {
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
}
