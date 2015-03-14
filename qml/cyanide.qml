import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import "pages"

ApplicationWindow
{
    initialPage: Component { FriendList { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")

    /* used in Friend.qml to identify the selected friend
     * TODO pass a local variable with pageStack.push */
    property int currentFID: 0

    /* pass this to functions that take a friend ID to refer to self */
    property int selfID: -1

    /* the list of friends */
    property ListModel friendList: ListModel { id: friendList }

    property ListModel settingsList: ListModel { id: settingsList }

    /* the currently activated page (is there a better way to get this?) */
    property string activePage: ""

    Component.onCompleted: loadSettings()
    function loadSettings() {
        var nameList = settings.get_names()
        for(var i in nameList) {
            appendSetting(nameList[i])
        }
    }

    function appendSetting(name) {
        settingsList.append({
                        'name': name,
                        'type': settings.get_type(name),
                        'display_name': settings.get_display_name(name),
                        'current_index': settings.get_current_index(name),
                            })
    }

    function refreshFriendList() {
        friendList.clear()
        for(var i = -1; i < cyanide.get_number_of_friends(); i++)
            appendFriend(i)
    }
    function appendFriend(i) {
            friendList.append({
                        'friend_name': cyanide.get_friend_name(i),
                        'friend_avatar': cyanide.get_friend_avatar(i),
                        'friend_connection_status': cyanide.get_friend_connection_status(i),
                        'friend_status_icon': cyanide.get_friend_status_icon(i),
                        'friend_status_message': cyanide.get_friend_status_message(i)
                         })
    }
    Connections {
        target: cyanide
        onSignal_friend_added: {
            appendFriend(fid)
        }
        onSignal_name_change: {
            var i = fid + 1
            var name = cyanide.get_friend_name(fid)
            friendList.setProperty(i, "friend_name", name)
            if(fid != selfID && settings.get("notification-friend-name-change")
                && !(activePage == "Friend.qml" && currentFID == fid))
            {
                currentFID = fid
                if(previous_name !== name)
                    notify(nNameChange, previous_name + qsTr(" is now known as ") + name, "")
            }
        }
        onSignal_user_status: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_notification: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_connection_status: {
            var i = fid + 1
            var online = cyanide.get_friend_connection_status(fid)
            friendList.setProperty(i, "friend_connection_status", online)
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
            if(fid != selfID) {
                if(online) {
                    cyanide.play_sound(settings.get("sound-friend-connected"))
                    var n = settings.get("notification-friend-connected")
                    if(n === "true") {
                        currentFID = fid
                        notify(nConnectionStatus, cyanide.get_friend_name(fid), "is now online")
                    }
                }
            }
        }
        onSignal_status_message: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_message", cyanide.get_friend_status_message(fid))
        }
        onSignal_friend_message: {
            var i = fid + 1
            if(fid != selfID) {
                cyanide.play_sound(settings.get("sound-message-received"))
                if(settings.get("notification-message-received") === "true"
                    && !(activePage == "Friend.qml" && currentFID == fid))
                {
                    currentFID = fid
                    notify(nFriendMessage, cyanide.get_friend_name(fid), cyanide.get_message_text(fid, mid))
                }
            }
        }
        onSignal_friend_request: {
            cyanide.play_sound(settings.get("sound-friend-request-received"))
            if(settings.get("notification-friend-request-received"))
                notify(nDefault, "Friend request received")
        }
    }

    Notification {
        id: nDefault
    }

    Notification {
        id: nNameChange
        onClicked: {
            pageStack.push(Qt.resolvedUrl("pages/Friend.qml"))
        }
    }
    Notification {
        id: nConnectionStatus
        onClicked: {
            pageStack.push(Qt.resolvedUrl("pages/Friend.qml"))
        }
    }
    Notification {
        id: nFriendMessage
        onClicked: {
            pageStack.push(Qt.resolvedUrl("pages/Friend.qml"))
        }
    }
    function notify(n, summary, body) {
        n.previewSummary = summary
        n.previewBody = body
        n.publish()
    }
}
