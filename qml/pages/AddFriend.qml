import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: addFriendPage

    TextArea {
        id: toxID
        width: parent.width
        height: implicitHeight
        y: 2 * Theme.paddingLarge

        inputMethodHints: Qt.ImhNoPredictiveText + Qt.ImhNoAutoUppercase + Qt.ImhEmailCharactersOnly
        focus: true
        placeholderText: "Tox ID"
        label: "(toxdns is currently not supported)"

        EnterKey.onClicked: {
            toxID.focus = false
            message.focus = true
        }
    }
    TextArea {
        id: message
        width: parent.width
        //height: Math.max(page.width/3, implicitHeight)
        height: implicitHeight
        y: 3 * Theme.paddingLarge + toxID.height

        inputMethodHints: Qt.ImhNoAutoUppercase
        placeholderText: "Include a message for your friend"
        EnterKey.onClicked: {
            submit()
        }
    }
    Button {
        id: button
        text: "Send friend request"
        enabled: true
        onClicked: submit()
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: message.bottom
        anchors.topMargin: Theme.paddingLarge
    }
    function submit() {
        if(cyanide.send_friend_request(toxID.text, message.text)) {
            refreshFriendList()
            pageStack.pop()
        } else {
            button.text = "Failed to send friend request, try again?"
        }
    }
}
