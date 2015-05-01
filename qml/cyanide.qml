import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.cyanide 1.0
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

    property ListModel messageList: ListModel { id: messageList }

    /* stack of friend IDs */
    property var friendNumberStack: new Array
    /*  stack of pages */
    property var pages: new Array

    property string folder: ""
    property var fileChooserProperties: new Object

    function activeFriend() {
        if(friendNumberStack.length == 0) {
            return -2;
        } else {
            return friendNumberStack[friendNumberStack.length-1]
        }
    }
    function chatWith(fid) {
        var p = activePage()
        if(fid == activeFriend()) {
            if(p === "friendaction.qml")
                pageStack.pop()
        } else {
            friendNumberStack.push(fid)
            pageStack.push(Qt.resolvedUrl("pages/Friend.qml"))
        }
    }
    function activePage() {
        return pages[pages.length-1]
    }
    function chattingWith(fid) {
        return activePage() === "Friend.qml" && activeFriend() == fid
    }

    function returnToPage(name) {
        pageStack.pop(pageStack.find(function(page) {
            return page.name === name
        }))
    }

    Component.onCompleted:
        loadSettings()
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
                        'friend_blocked': cyanide.get_friend_blocked(i),
                        'friend_address': settings.get_friend_address(cyanide.get_friend_public_key(i)),
                        'friend_callstate': cyanide.get_friend_callstate(i)
                         })
    }
    function refreshMessageList() {
        messageList.clear()
        var mids = cyanide.get_message_numbers(activeFriend())
        for(var mid in mids) {
            appendMessage(mid)
        }
    }
    function appendMessage(mid) {
        var fid = activeFriend()
        var m_type = cyanide.get_message_type(fid, mid)
        var m_author = cyanide.get_message_author(fid, mid)
        var m_text = cyanide.get_message_text(fid, mid)
        //var m_escaped = cyanide.get_message_html_escaped_text(fid, mid)
        var m_timestamp = new Date(cyanide.get_message_timestamp(fid, mid))

        if(!(m_type & Message_Type.File)) {
            messageList.append({"m_type": m_type
                         ,"m_author": m_author
                         ,"m_text": m_text
                         //,"m_html_escaped_text": m_escaped
                         ,"m_rich_text": cyanide.get_message_rich_text(fid, mid)
                         ,"m_id": 0
                         ,"f_link": ""
                         ,"f_status": 0
                         ,"f_progress": 0
                         })
        } else {
            messageList.append({"m_type": m_type
                         ,"m_author": m_author
                         ,"m_text": m_text
                         //, "m_html_escaped_text": m_escaped
                         ,"m_rich_text": ""
                         ,"m_id": mid
                         ,"f_link": cyanide.get_file_link(fid, mid)
                         ,"f_status": cyanide.get_file_status(fid, mid)
                         ,"f_progress": cyanide.get_file_progress(fid, mid)
                         })
        }
    }
    Connections {
        target: cyanide
        onSignal_focus_friend: {
            chatWith(fid)
        }
        onSignal_friend_added: {
            refreshFriendList()
        }
        onSignal_friend_blocked: {
            var i = fid + 1
            friendList.setProperty(i, "friend_blocked", blocked)
        }
        onSignal_friend_name: {
            var i = fid + 1
            var name = cyanide.get_friend_name(fid)
            friendList.setProperty(i, "friend_name", name)
        }
        property int image_refresh_count: 0
        onSignal_avatar_change: {
            var i = fid + 1
            image_refresh_count++ /* hack to change the url on each avatar change to avoid getting the cached image */
            friendList.setProperty(i, "friend_avatar", cyanide.get_friend_avatar(fid) + "?" + image_refresh_count)
        }
        onSignal_friend_activity: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_friend_status: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_friend_connection_status: {
            var i = fid + 1
            friendList.setProperty(i, "friend_connection_status", online)
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid, online))
        }
        onSignal_friend_status_message: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_message", cyanide.get_friend_status_message(fid))
        }
        onSignal_friend_message: {
            var i = fid + 1
            if(!cyanide.get_message_author(fid, mid)) {
                if(fid == activeFriend())
                    cyanide.set_friend_activity(fid, false)
                if(settings.get("notification-message-received") === "true"
                    && !(cyanide.is_visible() && chattingWith(fid)))
                {
                    var txt = cyanide.get_message_text(fid, mid)
                    cyanide.notify_message(fid, cyanide.get_friend_name(fid),
                                      type == Message_Type.File ? qsTr("Incoming file: ")+txt.replace(/^.*\//, "")
                                                                : txt)
                }
            }
            if(fid == activeFriend()) {
                // appendMessage(mid)
                refreshMessageList()
            }
        }
        onSignal_friend_callstate: {
            var i = fid + 1
            friendList.setProperty(i, "friend_callstate", callstate)
        }
        onSignal_av_invite: {
            // cyanide.notify_call(fid, cyanide.get_friend_name(fid)+" "+qsTr("is calling"), "")
        }
        onSignal_file_status: {
            if(fid == activeFriend()) {
                console.log("signal status: "+status)
                messageList.setProperty(mid, "f_status", status)
            }
        }
        onSignal_file_progress: {
            if(fid == activeFriend()) {
                messageList.setProperty(mid, "f_progress", progress)
            }
        }
        onSignal_friend_request: {
            if(settings.get("notification-friend-request-received") === "true")
                cyanide.notify_message(fid, qsTr("Friend request received!"), cyanide.get_friend_status_message(fid))
        }
    }
}
