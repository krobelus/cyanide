import QtQuick 2.0
import Sailfish.Silica 1.0


Page {
    id: page
    property string name: "friendlist"
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("FriendList.qml")
    }
    Component.onDestruction: {
        pages.pop()
    }

    RemorsePopup { id: remorsePopup }

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        anchors.fill: parent

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("Settings.qml"))
            }
            MenuItem {
                text: qsTr("Switch profile")
                onClicked: {
                    fileChooserProperties = {
                        target: "toxSaveFile",
                        nameFilters: [ '*.tox' ],
                        filter: true
                    }
                    pageStack.push(Qt.resolvedUrl("FileChooser.qml"), { "folder": "/home/nemo/.config/tox/" } )
                }
            }
            MenuItem {
                text: qsTr("Add a friend")
                onClicked: pageStack.push(Qt.resolvedUrl("AddFriend.qml"))
            }
        }

        // Tell SilicaFlickable the height of its content.
        //contentHeight: friendListView.height

        SilicaListView {
            id: friendListView

            header: PageHeader {
                // title: qsTr("Friends")
            }

            anchors.fill: parent
            model: friendList

            Component.onCompleted: {
                refreshFriendList()
            }


            delegate: BackgroundItem {
                id: delegate

                Label {
                    id: friendName
                    text: friend_name
                    anchors {
                        top: parent.top
                        left: parent.left
                        leftMargin: Theme.paddingSmall + friendAvatar.height
                        right: friendStatusIcon.left
                        rightMargin: Theme.paddingMedium
                    }
                    color: delegate.highlighted ? Theme.highlightColor
                                                : friend_connection_status ? Theme.primaryColor : Theme.secondaryColor
                }
                Image {
                    id: friendAvatar
                    fillMode: Image.PreserveAspectFit
                    source: friend_avatar
                    width: implicitWidth > height ? height : implicitWidth
                    anchors {
                        left: parent.left
                        leftMargin: (height - width) / 2
                        top: parent.top
                        bottom: parent.bottom
                    }
                }
                Image {
                    id: friendStatusIcon
                    source: friend_status_icon
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: Theme.paddingLarge - width / 2
                    }
                }
                Label {
                    id: friendStatusMessage
                    text: friend_status_message
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingSmall + friendAvatar.height
                        top: friendName.bottom
                        topMargin: 2
                        right: friendStatusIcon.left
                        rightMargin: Theme.paddingMedium
                    }
                    horizontalAlignment: Text.AlignLeft
                    truncationMode: TruncationMode.Elide

                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }
                onClicked: {
                    var fid = friend_number
                    if(fid == self_friend_number) {
                        pageStack.push(Qt.resolvedUrl("Profile.qml"))
                    } else if(!cyanide.get_friend_accepted(fid)) {
                        friendNumberStack.push(fid)
                        pageStack.push(Qt.resolvedUrl("AcceptFriend.qml"))
                    } else {
                        chatWith(fid)
                    }
                }
            }
            VerticalScrollDecorator {}
        }
    }
}
