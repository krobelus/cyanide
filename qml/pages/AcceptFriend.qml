import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    Text {
        id: cid
        text: cyanide.get_friend_public_key(currentFid)
        color: Theme.primaryColor
        x: Theme.paddingMedium
        y: 2 * Theme.paddingLarge
    }
    Text {
        id: msg
        text: cyanide.get_friend_status_message(currentFid)
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
            text: "Accept"
            onClicked: {
                if(cyanide.accept_friend_request(currentFid)) {
                    refreshFriendList()
                    pageStack.pop()
                } else {
                     // failed to accept
                }
            }
        }
        Button {
            text: "Ignore"
            onClicked: {
                cyanide.remove_friend(currentFid)
                refreshFriendList()
                pageStack.pop()
            }
        }
    }
}
