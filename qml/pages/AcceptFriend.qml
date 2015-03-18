import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: pageAcceptFriend
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("AcceptFriend.qml")
    }
    Component.onDestruction: {
        pages.pop()
    }

    Text {
        id: cid
        text: cyanide.get_friend_cid(currentFID)
        color: Theme.primaryColor
        x: Theme.paddingMedium
        y: 2 * Theme.paddingLarge
    }
    Text {
        id: msg
        text: cyanide.get_friend_status_message(currentFID)
        color: Theme.primaryColor
        x: Theme.paddingMedium
        anchors {
            top: cid.bottom
            topMargin: Theme.paddingLarge
        }
    }

    Row {
        spacing: Theme.paddingLarge
        anchors.horizontalCenter: parent.horizontalCenter
        anchors {
            top: msg.bottom
            topMargin: Theme.paddingLarge
        }

        Button {
            text: qsTr("Accept")
            onClicked: {
                if(cyanide.accept_friend_request(currentFID)) {
                    refreshFriendList()
                    pageStack.pop()
                } else {
                     // failed to accept
                }
            }
        }
        Button {
            text: qsTr("Ignore")
            onClicked: {
                cyanide.remove_friend(currentFID)
                refreshFriendList()
                pageStack.pop()
            }
        }
    }
}
