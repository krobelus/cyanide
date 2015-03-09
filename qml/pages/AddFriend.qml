import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: pageAddFriend
    allowedOrientations: Orientation.All
    Component.onCompleted: activePage = "AddFriend.qml"

    TextField {
        id: toxID
        width: parent.width
        height: implicitHeight
        y: 2 * Theme.paddingLarge

        inputMethodHints: Qt.ImhNoPredictiveText + Qt.ImhNoAutoUppercase + Qt.ImhEmailCharactersOnly
        focus: true
        placeholderText: "Tox ID"

        EnterKey.onClicked: {
            toxID.focus = false
            message.focus = true
        }
    }
    TextField {
        id: message
        width: parent.width
        //height: Math.max(page.width/3, implicitHeight)
        height: implicitHeight
        y: 3 * Theme.paddingLarge + toxID.height

        inputMethodHints: Qt.ImhNoAutoUppercase
        // TODO use DEFAULT_FRIEND_REQUEST_MESSAGE
        placeholderText: qsTr("Tox me maybe?")
        EnterKey.onClicked: {
            submit()
        }
    }
    Button {
        id: button
        text: qsTr("Send friend request")
        enabled: true
        onClicked: submit()
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: message.bottom
        anchors.topMargin: Theme.paddingLarge
    }
    function submit() {
        var err = cyanide.send_friend_request(toxID.text, message.text)
        if(err === "") {
            pageStack.pop()
        } else {
            button.text = err;
        }
    }
}
