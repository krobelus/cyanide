import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import "../js/Misc.js" as Misc

Page {
    id: page
    property string name: "friend"
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("Friend.qml")
    }
    Component.onDestruction: {
        pages.pop()
        friendNumberStack.pop()
        if(inputField.focus)
            cyanide.send_typing_notification(f, false)
    }

    RemorsePopup { id: remorsePopup }

    Notification { id: notification }

    property int f: activeFriend()

    DockedPanel {
        id: fileControlPanel

        width: parent.width
        height: Theme.itemSizeExtraLarge

        dock: Dock.Top

        property int m: -1
        property int file_status: 99
        property bool incoming: false

        function togglePaused() {
            console.log("togglePaused() was called")
            var errmsg
            if(file_status == 1 || file_status == -2) {
                errmsg = cyanide.pause_transfer(f, m)
                if(errmsg === "") {
                    console.log("paused successfully, closing panel")
                } else {
                    notify(notification, qsTr("Failed to pause transfer"), errmsg)
                }
            } else if(file_status == -1 || file_status == -3) {
                errmsg = cyanide.resume_transfer(f, m)
                if(errmsg === "") {
                    console.log("paused successfully, closing panel")
                } else {
                    notify(notification, qsTr("Failed to resume transfer"), errmsg)
                }
            } else if(file_status == 0) {
                console.log("attempted to pause/resume a cancelled transfer")
            } else if(file_status == 2) {
                console.log("attempted to pause/resume a finished transfer")
            }
            console.log("file_status was: "+file_status)
            console.log("setting to: "+cyanide.get_file_status(f, m))
            file_status = cyanide.get_file_status(f, m)
            open = false
        }
        function cancel() {
            console.log("cancel() was called")
            if(file_status == 2 || file_status == 0) {
                console.log("attempted to cancel a finished/cancelled transfer")
            } else {
                var errmsg =  cyanide.cancel_transfer(f, m)
                if(errmsg === "")
                    open = false
                else
                    notify(notification, qsTr("Failed to cancel transfer"), qsTr(errmsg))
            }
            open = false
        }

        function toggleIcons() {
            if(file_status == -1 || file_status == -3) {
                togglePaused.icon.source = "image://theme/icon-cover-play"
                togglePaused.visible = true
                cancel.visible = true
            } else if(file_status == 1 || file_status == -2) {
                togglePaused.icon.source = "image://theme/icon-cover-pause"
                togglePaused.visible = true
                cancel.visible = true
            } else if(file_status == 0 || file_status == 2) {
                togglePaused.visible = false
                cancel.visible = false
            }
        }

        onFile_statusChanged: {
            console.log("file status changed")
            toggleIcons()
        }

        Row {
            anchors {
                centerIn: parent
            }
            spacing: Theme.itemSizeLarge
            IconButton {
                id: togglePaused
                onClicked: fileControlPanel.togglePaused()
            }
            IconButton {
                id: cancel
                onClicked: fileControlPanel.cancel()
                icon.source: "image://theme/icon-cover-cancel"
            }
        }
    }

    SilicaFlickable {
        anchors {
            fill: parent
            topMargin: page.isPortrait ? fileControlPanel.visibleSize : 0
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("Settings.qml"))
            }
            MenuItem {
                text: qsTr("Remove friend")
                onClicked: {
                    remorsePopup.execute(qsTr("Removing friend"), function() {
                        cyanide.remove_friend(f)
                        refreshFriendList()
                        pageStack.pop()
                    })
                }
            }
            MenuItem {
                text: qsTr("Copy Tox ID to clipboard")
                enabled: friendList.get(f+1).friend_address !== ""
                onClicked: {
                    clipboard.setClipboard(friendList.get(f+1).friend_address)
                }
            }
            MenuItem {
                text: qsTr("Send a file")
                onClicked: {
                    fileChooserProperties = {
                        target: "fileToSend",
                        nameFilters: []
                    }
                    pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/" } )
                }
            }
        }

        PageHeader {
            id: pageHeader
            title: friendList.get(f+1).friend_name
            anchors {
                right: parent.right
                rightMargin: 2 * Theme.paddingLarge + friendStatusIcon.width
            }
        }
        Image {
            id: friendStatusIcon
            source: friendList.get(f+1).friend_status_icon
            y: pageHeader.height / 2 - height / 2
            anchors {
                right: parent.right
                rightMargin: Theme.paddingLarge
            }
        }

        SilicaListView {
            id: messageList

            model: ListModel { id: model }

            anchors {
                fill: parent
                topMargin: pageHeader.height
                bottomMargin: pageHeader.height
            }

            function appendMessage(mid) {
                var fid = f
                var m_type = cyanide.get_message_type(fid, mid)
                var m_author = cyanide.get_message_author(fid, mid)
                var m_text = cyanide.get_message_text(fid, mid)
                var m_timestamp = new Date(cyanide.get_message_timestamp(fid, mid))
                var isFile = m_type == msgtype_file || m_type == msgtype_image

                if(!isFile) {
                    model.append({"m_type": m_type
                                 ,"m_author": m_author
                                 ,"m_text": m_text
                                 })
                } else if(isFile) {
                    model.append({"m_type": m_type
                                 ,"m_author": m_author
                                 ,"m_text": m_text
                                 ,"mid": mid
                                 ,"f_link": cyanide.get_file_link(fid, mid)
                                 ,"f_status": cyanide.get_file_status(fid, mid)
                                 ,"f_progress": cyanide.get_file_progress(fid, mid)
                                 })
                }
            }
            Component.onCompleted: {
                var mids = cyanide.get_message_numbers(f)
                for(var mid in mids) {
                    messageList.appendMessage(mid)
                }
                cyanide.set_friend_notification(f, false)
                messageList.positionViewAtEnd()
            }
            Connections {
                target: cyanide
                onSignal_friend_message: {
                    if(fid == f)
                        cyanide.set_friend_notification(fid, false)
                    if(fid == f || fid == self_friend_number) {
                        messageList.appendMessage(mid)
                        messageList.positionViewAtEnd()
                    }
                }
                onSignal_friend_typing: {
                    if(fid == f) {
                        inputField.label = is_typing
                            ? friendList.get(f+1).friend_name + qsTr(" is typing...")
                            : ""
                        inputField.placeholderText = inputField.label
                    }
                }
                onSignal_file_status: {
                    if(fid == f && fileControlPanel.m == mid) {
                        console.log("signal status: "+status)
                        model.setProperty(mid, "f_status", status)
                    }

                }
                onSignal_file_progress: {
                    if(fid == f) {
                        console.log("signal progress: "+progress)
                        model.setProperty(mid, "f_progress", progress)
                    }
                }
            }

            delegate: Item {
                id: delegate
                height: Theme.paddingMedium + message.height
                x: m_author ? Theme.paddingLarge : page.width/3 - Theme.paddingLarge

                Image {
                    id: attach
                    visible: message.file
                    y: message.y
                    source: "image://theme/icon-s-attach"
                }
                Label {
                    id: message
                    property bool file: m_type == msgtype_file || m_type == msgtype_image
                    text: file ? f_status == 0 ?
                              "<s>(" + f_progress + "%) " + m_text.replace(/.*\//, "") + "</s>"
                               : "(" + f_progress + "%) " + f_link
                               : m_text
                    width: page.width * 2/3
                    x: file ? attach.x + attach.width : attach.x
                    font.pixelSize: Theme.fontSizeSmall
                    color: m_author ? Theme.secondaryColor : Theme.primaryColor
                    horizontalAlignment: m_author ? Text.AlignLeft : Text.AlignRight
                    wrapMode: Text.Wrap
                    textFormat: file && f_status == 0 ? Text.RichText : Text.StyledText

                    linkColor: Theme.highlightColor
                    onLinkActivated: {
                        console.log("text:"+text)
                        if(!file) {
                            notify(notification, qsTr("Opening URL..."), "")
                            Misc.openUrl(link)
                        } else {
                            if(fileControlPanel.open) {
                                fileControlPanel.open = false
                            } else if(f_status == 1 /* active  */
                                    ||f_status < 0) /* paused  */ {
                                openFileControlPanel(mid, f_status)
                            } else if(f_status == 0) /* cancelled */ {
                                ;
                            } else if(f_status == 4) /* finished */ {
                                notify(notification, qsTr("Opening file..."), "")
                                Misc.openUrl(link)
                            }
                        }
                    }
                }
                function openFileControlPanel(mid, f_status) {
                    fileControlPanel.m = mid
                    fileControlPanel.incoming = !m_author
                    fileControlPanel.file_status = f_status
                    fileControlPanel.open = true
                    fileControlPanel.toggleIcons()
                }

                /*
                Label {
                    id: timestampLabel
                    text: qsTr(Misc.elapsedTime(timestamp))
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    width: page.width/3 - 3*Theme.paddingMedium
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    horizontalAlignment: m_author ? Text.AlignRight : Text.AlignLeft
                    anchors {
                        right: m_author ? parent.right : messageLabel.left
                        left: m_author ? messageLabel.right : parent.left
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
            focus: model.count == 0
            onFocusChanged: cyanide.send_typing_notification(f, focus)
            onYChanged: messageList.positionViewAtEnd()
            anchors {
                bottom: parent.bottom
            }
            EnterKey.onClicked: {
                // TODO split long messages
                if(text === "")
                    return
                if(text !== "" && cyanide.send_friend_message(f, text)) {
                    text = ""
                    parent.focus = true;
                } else {
                    notify(notification, qsTr("Failed to send message"), "")
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
    }
}
