import QtQuick 2.0
import Sailfish.Silica 1.0
import "../js/Misc.js" as Misc

Page {
    id: page

    SilicaListView {
        id: messageList

        header: PageHeader {
            title: cyanide.get_friend_name(currentFid)
        }

        model: ListModel {
            id: model
        }

        //height: parent.height - inputField.height - Theme.paddingLarge
        //spacing: Theme.paddingMedium
        //TODO fill block
        anchors.fill: parent


        Component.onCompleted: {
            for(var i=0; i<cyanide.get_number_of_messages(currentFid); i++)
                model.append({'author': cyanide.get_message_author(currentFid, i)
                             ,'message_text': cyanide.get_message_text(currentFid, i)
                             ,'timestamp': new Date(cyanide.get_message_timestamp(currentFid, i))
                             })
            cyanide.set_friend_notification(currentFid, false)
        }
        Connections {
            target: cyanide
            onSignal_friend_message: {
                var i = listID(fid)
                if(fid == currentFid || i == 0) {
                    cyanide.set_friend_notification(fid, false)
                    model.append({'author': cyanide.get_message_author(currentFid, mid)
                                 ,'message_text': cyanide.get_message_text(currentFid, mid)
                                 ,'timestamp': cyanide.get_message_timestamp(currentFid, mid)
                                 })
                } else {
                    // TODO notifications
                    // also in the first branch, if not focused
                }

            }
        }

        //delegate: BackgroundItem {
        //    id: delegate
            //x: Theme.paddingLarge
            //width: parent.width - 2*Theme.paddingLarge
            //height: childrenRect.height

        /*
        delegate: Label {
            id: messageLabel
            text: message_text
            font.pixelSize: Theme.fontSizeSmall
            color: author ? Theme.secondaryColor : Theme.primaryColor
            horizontalAlignment: author ? Text.AlignLeft : Text.AlignRight
            wrapMode: Text.WordWrap
            //width: page.width*2/3
            //x: author ? Theme.paddingMedium : page.width/3-3*Theme.paddingMedium
            //anchors.fill: parent
            anchors {
                top: parent.top
                left: parent.left
                bottom: parent.bottom
            }
        }
        */
            /*
            Label {
                id: timestampLabel
                text: qsTr(Misc.elapsedTime(timestamp))
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                width: page.width/3 - 3*Theme.paddingMedium
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                horizontalAlignment: author ? Text.AlignRight : Text.AlignLeft
                anchors {
                    right: author ? parent.right : messageLabel.left
                    left: author ? messageLabel.right : parent.left
                }
            }
            */
        //}
        VerticalScrollDecorator {}
    }

    TextField {
        id: inputField
        width: parent.width
        //label: "Text field"
        placeholderText: "type your message here"
        inputMethodHints: Qt.ImhNoAutoUppercase
        focus: false
        y: page.height - height - Theme.paddingLarge
        //horizontalAlignment: textAlignment
        EnterKey.onClicked: {
            // TODO split long messages
            if(cyanide.send_friend_message(currentFid, text)) {
                text = ""
                parent.focus = true;
            } else {
                // failed to send the message, keep it for now
            }
        }
    }
}
