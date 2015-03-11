import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import "../js/Misc.js" as Misc

Page {
    id: pageFriend
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        activePage = "Friend.qml"
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
                text: qsTr("Remove friend")
                onClicked: {
                    remorsePopup.execute(qsTr("Removing friend"), function() {
                        cyanide.remove_friend(currentFID)
                        refreshFriendList()
                        pageStack.pop()
                    })
                }
            }
            /*
            MenuItem {
                text: qsTr("Copy Tox ID to clipboard")
                onClicked: {
                    clipboard.setClipboard(cyanide.get_friend_cid(currentFID))
                }
            }
            */
        }

        PageHeader {
            id: pageHeader
            title: friendList.get(currentFID+1).friend_name
            anchors {
                right: parent.right
                rightMargin: 2 * Theme.paddingLarge + friendStatusIcon.width
            }
        }
        Image {
            id: friendStatusIcon
            source: friendList.get(currentFID+1).friend_status_icon
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
                for(var i=0; i<cyanide.get_number_of_messages(currentFID); i++)
                    model.append({'author': cyanide.get_message_author(currentFID, i)
                                 ,'message_text': cyanide.get_message_rich_text(currentFID, i)
                                 ,'timestamp': new Date(cyanide.get_message_timestamp(currentFID, i))
                                 })
                cyanide.set_friend_notification(currentFID, false)
                //messageList.scrollToBottom()
            }
            Connections {
                target: cyanide
                onSignal_friend_message: {
                    if(fid == currentFID || fid == selfID) {
                        cyanide.set_friend_notification(fid, false)
                        model.append({'author': cyanide.get_message_author(currentFID, mid)
                                     ,'message_text': cyanide.get_message_rich_text(currentFID, mid)
                                     ,'timestamp': cyanide.get_message_timestamp(currentFID, mid)
                                     })
                        messageList.scrollToBottom()
                    }
                }
                onSignal_typing_change: {
                    if(fid == currentFID) {
                        inputField.label = is_typing
                            ? friendList.get(currentFID+1).friend_name + qsTr(" is typing...")
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
                    font.pixelSize: Theme.fontSizeMedium
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
            y: pageFriend.height - height - Theme.paddingLarge
            EnterKey.onClicked: {
                // TODO split long messages
                if(cyanide.send_friend_message(currentFID, text)) {
                    text = ""
                    parent.focus = true;
                } else {
                    // failed to send the message, keep it for now
                }
            }
        }
    }
}
