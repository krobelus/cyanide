import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow
{
    initialPage: Component { FriendList { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")

    /* used in Friend.qml to identify the friend object */
    property int currentFid: 0

    /* the list of friends with */
    property ListModel friendList: ListModel { id: friendList }

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
        onSignal_name_change: {
            var i = fid + 1
            friendList.setProperty(i, "friend_name", cyanide.get_friend_name(fid))
        }
        onSignal_connection_status: {
            var i = fid + 1
            friendList.setProperty(i, "friend_connection_status", cyanide.get_friend_connection_status(fid))
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
        onSignal_status_message: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_message", cyanide.get_friend_status_message(fid))
        }
        onSignal_friend_message: {
            var i = fid + 1
            friendList.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
        }
    }
}

