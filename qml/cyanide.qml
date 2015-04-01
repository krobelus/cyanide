import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import "pages"

ApplicationWindow
{
    initialPage: Component { FriendList { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")

    /* pass this to functions that takes a friend ID to refer to self */
    property int self_friend_number: -1

    /* the list of friends */
    property ListModel friendList: ListModel { id: friendList }

    property ListModel settingsList: ListModel { id: settingsList }

    /* stack of friend IDs */
    property var friendNumberStack: new Array
    /*  stack of pages */
    property var pages: new Array

    property string folder: ""
    property var fileChooserProperties: new Object

    function activeFriend() {
        if(friendNumberStack.length == 0) {
            console.log("activeFriend() called with empty friend stack")
            return -2;
        } else {
            return friendNumberStack[friendNumberStack.length-1]
        }
    }
    function chatWith(fid) {
        friendNumberStack.push(fid)
        pageStack.push(Qt.resolvedUrl("pages/Friend.qml"))
    }
    function activePage() {
        return pages[pages.length-1]
    }
    function chattingWith(fid) {
        return activePage() === "Friend.qml" && activeFriend() == fid
    }

    Component.onCompleted: loadSettings()
    function loadSettings() {
        var nameList = settings.get_names()
        for(var i in nameList) {
            appendSetting(nameList[i])
        }
    }

    function appendSetting(name) {
        settingsList.insert(0, {
                        'name': name,
                        'type': settings.get_type(name),
                        'display_name': settings.get_display_name(name),
                            })
    }

    function refreshFriendList() {
        friendList.clear()
        appendFriend(-1)
        var fids = cyanide.get_friend_numbers()
        for(var i in fids) {
            appendFriend(fids[i])
        }
    }
    function appendFriend(i) {
            friendList.append({
                        'friend_name': cyanide.get_friend_name(i),
                        'friend_avatar': cyanide.get_friend_avatar(i),
                        'friend_connection_status': cyanide.get_friend_connection_status(i),
                        'friend_status_icon': cyanide.get_friend_status_icon(i),
                        'friend_status_message': cyanide.get_friend_status_message(i),
                        'friend_address': settings.get_friend_address(cyanide.get_friend_public_key(i))
                         })
    }
    Connections {
        target: cyanide
        onSignal_friend_added: {
            appendFriend(fid)
        }
        onSignal_friend_name: {
            var i = fid + 1
            var name = cyanide.get_friend_name(fid)
            friendList.setProperty(i, "friend_name", name)
            /*
            if(fid != self_friend_number && settings.get("notification-friend-name-change")
                && !(cyanide.is_visible() && chattingWith(fid)))
            {
                nNameChange.fid = fid
                if(previous_name !== name)
                    notify(nNameChange, previous_name + qsTr(" is now known as ") + name, "")
            }
            */
        }
        property int image_refresh_count: 0
        onSignal_avatar_change: {
            var i = fid + 1
            image_refresh_count++ /* hack to change the url on each avatar change to avoid getting the cached image */
            friendList.setProperty(i, "friend_avatar", cyanide.get_friend_avatar(fid) + "?" + image_refresh_count)
        }
        onSignal_friend_status: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_notification: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_friend_connection_status: {
            var i = fid + 1
            var online = cyanide.get_friend_connection_status(fid)
            friendList.setProperty(i, "friend_connection_status", online)
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
            if(fid != self_friend_number) {
                if(online) {
                    //cyanide.play_sound(settings.get("sound-friend-connected"))
                    /*
                    if("true" === settings.get("notification-friend-connected")
                        && !(cyanide.is_visible() && chattingWith(fid)))
                    {
                        nConnectionStatus.fid = fid
                        //notify(nConnectionStatus, cyanide.get_friend_name(fid), "is now online")
                    }
                    */
                }
            }
        }
        onSignal_friend_status_message: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_message", cyanide.get_friend_status_message(fid))
        }
        onSignal_friend_message: {
            var i = fid + 1
            if(fid != self_friend_number) {
                // cyanide.play_sound(settings.get("sound-message-received"))
                cyanide.set_friend_notification(fid, true)
                if(settings.get("notification-message-received") === "true"
                    && !(cyanide.is_visible() && chattingWith(fid)))
                {
                    nFriendMessage.fid = fid
                    notify(nFriendMessage, cyanide.get_friend_name(fid), cyanide.get_message_text(fid, mid))
                }
            }
        }
        onSignal_friend_request: {
            // cyanide.play_sound(settings.get("sound-friend-request-received"))
            if(settings.get("notification-friend-request-received") === "true")
                nFriendRequest.fid = fid
                nFriendRequest.summary = qsTr("Friend request received!")
                nFriendRequest.body = cyanide.get_friend_status_message(fid)
                nFriendRequest.previewSummary = nFriendRequest.summary
                nFriendRequest.previewBody = nFriendRequest.body
                nFriendRequest.itemCount++
                nFriendRequest.publish()
        }
    }

    Notification {
        id: nDefault
    }
    /*
    Notification {
        id: nNameChange
        property int fid: 0
        onClicked: {
            cyanide.raise();
            if(!chattingWith(fid)) {
                chatWith(fid)
            }
        }
    }
    Notification {
        id: nConnectionStatus
        property int fid: 0
        onClicked: {
            cyanide.raise();
            if(!chattingWith(fid)) {
                chatWith(fid)
            }
        }
    }
    */
    Notification {
        id: nFriendRequest
        property int fid: 0
        category: "x-nemo.social.tox.message"
        onClicked: {
            itemCount = 0
            cyanide.raise()
            friendNumberStack.push(fid)
            pageStack.push(Qt.resolvedUrl("pages/AcceptFriend.qml"))
        }
        onClosed: console.log("Closed friend request notification, reason: " + reason)
    }
    Notification {
        id: nFriendMessage
        property int fid: 0
        category: "x-nemo.social.tox.message"
        summary: itemCount == 1 ? qsTr("New message") : qsTr("New messages")
        itemCount: 0
        onClicked: {
            itemCount = 0
            cyanide.raise()
            if(!chattingWith(fid)) {
                chatWith(fid)
            }
        }
        onClosed: console.log("Closed message notification, reason: " + reason)
    }
    function notify(n, summary, body) {
        n.previewSummary = summary
        n.previewBody = body
        n.itemCount++
        n.publish()
    }
    Connections {
        target: cyanide
        onSignal_close_notifications: {
            nFriendMessage.close()
            nFriendRequest.close()
        }
    }
}
