import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow
{
    /* used in Friend.qml to identify the friend object */
    property int currentFid: 0

    function listID(fid) {
        if(fid === cyanide.get_number_of_friends())
            return 0
        else
            return fid + 1
    }

    Connections {
        target: cyanide
    }

    initialPage: Component { FriendList { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
}


