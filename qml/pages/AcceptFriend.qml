import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    Text {
        id: cid
        text: cyanide.get_friend_public_key(currentFid)
        height: implicitHeight
        anchors {
            top: parent.top
        }
    }
    Text {
        id: msg
        text: cyanide.get_friend_status_message(currentFid)
        height: implicitHeight
        anchors {
            top: cid.bottom
        }
    }

    Row {
        spacing: Theme.paddingLarge
        anchors.horizontalCenter: parent.horizontalCenter
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
