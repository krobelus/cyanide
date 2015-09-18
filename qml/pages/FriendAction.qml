import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    property string name: "friendaction"
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        setProperties()
    }
    Component.onDestruction: {
        if(activePage() === "FriendAction.qml") {
            pages.pop()
        }
    }

    RemorsePopup { id: remorsePopup }

    property int f: activeFriend()
    property string friend_address: ""
    property bool friend_blocked: false

    function setProperties() {
        var entry = friendList.get(listFid(f))
        friend_address = entry.friend_address
        friend_blocked = entry.friend_blocked
    }

    Connections {
        target: cyanide
        onSignal_friend_connection_status: setProperties()
        onSignal_friend_status: setProperties()
    }

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
                enabled: !cyanide.in_call && cyanide.get_friend_connection_status(f)
                onClicked: {
                    cyanide.call(f, false)
                    pageStack.pop()
                }
            }
            IconButton {
                icon.source: "qrc:/images/video_4x"
                enabled: false
                onClicked: {
                    cyanide.call(f, true)
                    pageStack.pop()
                }
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Copy Tox ID to clipboard")
            enabled: friend_address !== ""
            onClicked: {
                clipboard.setClipboard(friend_address)
            }
        }

        ComboBox {
            id: history
            anchors.horizontalCenter: parent.horizontalCenter
            label: qsTr("Chat history")
            currentIndex: -1
            menu: ContextMenu {
                MenuItem {
                    //: chat history
                    text: qsTr("Load")
                }
                MenuItem {
                    //: chat history
                    text: qsTr("Delete")
                }
            }
            onCurrentIndexChanged: {
                if(currentIndex == 0) {
                    pageStack.push(Qt.resolvedUrl("LoadChatHistory.qml"))
                } else if(currentIndex == 1) {
                    remorsePopup.execute(qsTr("Deleting chat history"), function() {
                        cyanide.clear_history(f)
                        pageStack.pop()
                    })
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            Button {
                text: friend_blocked ? qsTr("Unblock friend") : qsTr("Block friend")
                onClicked: {
                    cyanide.set_friend_blocked(f, !friend_blocked)
                }
            }
            Button {
                text: qsTr("Remove friend")
                onClicked: {
                    remorsePopup.execute(qsTr("Removing friend"), function() {
                        cyanide.remove_friend(f)
                        refreshFriendList()
                        returnToPage("friendlist")
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
