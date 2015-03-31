import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import "../js/Misc.js" as Misc

Page {
    id: pageFriend
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("Friend.qml")
    }
    Component.onDestruction: {
        pages.pop()
        friendNumberStack.pop()
        if(inputField.focus)
            cyanide.send_typing_notification(activeFriend(), false)
    }

    RemorsePopup { id: remorsePopup }

    Notification { id: notification }

    TextEdit {
        id: clipboard
        visible: false
        function setClipboard(value) {
        text = value
            selectAll()
            copy()
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("Settings.qml"))
            }
            MenuItem {
                text: qsTr("Remove friend")
                onClicked: {
                    remorsePopup.execute(qsTr("Removing friend"), function() {
                        cyanide.remove_friend(activeFriend())
                        refreshFriendList()
                        pageStack.pop()
                    })
                }
            }
            MenuItem {
                text: qsTr("Copy Tox ID to clipboard")
                enabled: friendList.get(activeFriend()+1).friend_address !== ""
                onClicked: {
                    clipboard.setClipboard(friendList.get(activeFriend()+1).friend_address)
                }
            }
        }

        PageHeader {
            id: pageHeader
            title: friendList.get(activeFriend()+1).friend_name
            anchors {
                right: parent.right
                rightMargin: 2 * Theme.paddingLarge + friendStatusIcon.width
            }
        }
        Image {
            id: friendStatusIcon
            source: friendList.get(activeFriend()+1).friend_status_icon
            y: pageHeader.height / 2 - height / 2
            anchors {
                right: parent.right
                rightMargin: Theme.paddingLarge
            }
        }

        SilicaListView {
            id: messageList

            model: ListModel {
                id: model
            }

            anchors {
                fill: parent
                topMargin: pageHeader.height
                bottomMargin: pageHeader.height
            }

            Component.onCompleted: {
                for(var i=0; i<cyanide.get_number_of_messages(activeFriend()); i++)
                    model.append({'author': cyanide.get_message_author(activeFriend(), i)
                                 ,'message_text': cyanide.get_message_rich_text(activeFriend(), i)
                                 ,'timestamp': new Date(cyanide.get_message_timestamp(activeFriend(), i))
                                 })
                cyanide.set_friend_notification(activeFriend(), false)
                messageList.positionViewAtEnd()
            }
            Connections {
                target: cyanide
                onSignal_friend_message: {
                    if(fid == activeFriend())
                        cyanide.set_friend_notification(fid, false)
                    if(fid == activeFriend() || fid == self_friend_number) {
                        model.append({'author': cyanide.get_message_author(activeFriend(), mid)
                                     ,'message_text': cyanide.get_message_rich_text(activeFriend(), mid)
                                     ,'timestamp': cyanide.get_message_timestamp(activeFriend(), mid)
                                     })
                        messageList.positionViewAtEnd()
                    }
                }
                onSignal_friend_typing: {
                    if(fid == activeFriend()) {
                        inputField.label = is_typing
                            ? friendList.get(activeFriend()+1).friend_name + qsTr(" is typing...")
                            : ""
                        inputField.placeholderText = inputField.label
                    }
                }
            }

            delegate: Item {
                id: delegate
                height: messageText.height + Theme.paddingMedium
                x: author ? Theme.paddingLarge : pageFriend.width/3 - Theme.paddingLarge

                Label {
                    id: messageText
                    text: message_text
                    width: pageFriend.width*2/3
                    font.pixelSize: Theme.fontSizeSmall
                    color: author ? Theme.secondaryColor : Theme.primaryColor
                    horizontalAlignment: author ? Text.AlignLeft : Text.AlignRight

                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText

                    linkColor: Theme.highlightColor
                    onLinkActivated: {
                        notify(notification, qsTr("Opening URL..."), "")
                        Misc.openUrl(link)
                    }

                    /*
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingMedium
                        top: parent.top
                        bottom: parent.bottom
                    }*/
                }
                /*
                Label {
                    id: timestampLabel
                    text: qsTr(Misc.elapsedTime(timestamp))
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    width: pageFriend.width/3 - 3*Theme.paddingMedium
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    horizontalAlignment: author ? Text.AlignRight : Text.AlignLeft
                    anchors {
                        right: author ? parent.right : messageLabel.left
                        left: author ? messageLabel.right : parent.left
                    }
                }
                */
            }
            VerticalScrollDecorator {}
        }

        TextField {
            id: inputField
            width: parent.width - Theme.paddingLarge
            inputMethodHints: Qt.ImhNoAutoUppercase
            focus: false
            onFocusChanged: cyanide.send_typing_notification(activeFriend(), focus)
            onYChanged: messageList.positionViewAtEnd()
            anchors {
                bottom: parent.bottom
            }
            EnterKey.onClicked: {
                // TODO split long messages
                if(cyanide.send_friend_message(activeFriend(), text)) {
                    text = ""
                    parent.focus = true;
                } else {
                    // failed to send the message, keep it for now
                }
            }
        }
    }
}
