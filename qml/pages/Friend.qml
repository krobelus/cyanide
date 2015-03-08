import QtQuick 2.0
import Sailfish.Silica 1.0
import "../js/Misc.js" as Misc

Page {
    id: friendPage
    allowedOrientations: Orientation.All

    RemorsePopup { id: remorsePopup }

    SilicaListView {
        id: messageList

        header: PageHeader {
            title: cyanide.get_friend_name(currentFID)
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Remove friend")
                onClicked: {
                    remorsePopup.execute("Removing friend...", function() {
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

        TextEdit {
            id: clipboard
            visible: false
            function setClipboard(value) {
                text = value
                selectAll()
                copy()
            }
        }

        model: ListModel {
            id: model
        }

        //height: parent.height - inputField.height - Theme.paddingLarge
        //spacing: Theme.paddingMedium
        //TODO fill block
        anchors.fill: parent

        Component.onCompleted: {
            for(var i=0; i<cyanide.get_number_of_messages(currentFID); i++)
                model.append({'author': cyanide.get_message_author(currentFID, i)
                             ,'message_text': cyanide.get_message_text(currentFID, i)
                             ,'timestamp': new Date(cyanide.get_message_timestamp(currentFID, i))
                             })
            cyanide.set_friend_notification(currentFID, false)
        }
        Connections {
            target: cyanide
            onSignal_friend_message: {
                if(fid == currentFID || fid == selfID) {
                    cyanide.set_friend_notification(fid, false)
                    model.append({'author': cyanide.get_message_author(currentFID, mid)
                                 ,'message_text': cyanide.get_message_text(currentFID, mid)
                                 ,'timestamp': cyanide.get_message_timestamp(currentFID, mid)
                                 })
                } else {
                    // TODO notifications
                    // also in the first branch, if not focused
                }

            }
        }

        delegate: Item {
            id: delegate
            height: messageText.height + Theme.paddingMedium
            // height: childrenRect.height
            x: author ? Theme.paddingLarge : friendPage.width/3 - Theme.paddingLarge
            //width: friendPage.width*2/3 - 2 * Theme.paddingLarge

            Text {
                id: messageText
                text: message_text
                width: friendPage.width*2/3
                font.pixelSize: Theme.fontSizeMedium
                color: author ? Theme.secondaryColor : Theme.primaryColor
                horizontalAlignment: author ? Text.AlignLeft : Text.AlignRight
                wrapMode: Text.WordWrap
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
                width: friendPage.width/3 - 3*Theme.paddingMedium
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
        //label: "Text field"
        placeholderText: "type your message here"
        inputMethodHints: Qt.ImhNoAutoUppercase
        focus: false
        y: friendPage.height - height - Theme.paddingLarge
        //horizontalAlignment: textAlignment
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
