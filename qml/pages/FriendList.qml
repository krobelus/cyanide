import QtQuick 2.0
import Sailfish.Silica 1.0


Page {
    id: pageFriendList
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        activePage = "FriendList.qml"
        currentFID = 0
    }

    RemorsePopup { id: remorsePopup }

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        anchors.fill: parent

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
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
                title: qsTr("Friends")
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
                        left: friendAvatar.right
                        leftMargin: Theme.paddingMedium
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
                    anchors {
                        left: parent.left
                        leftMargin: Theme.paddingMedium
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
                        rightMargin: Theme.paddingMedium
                    }
                }
                Label {
                    id: friendStatusMessage
                    text: friend_status_message
                    anchors {
                        left: friendAvatar.right
                        leftMargin: Theme.paddingMedium
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
                    var fid = index - 1
                    if(fid == selfID) {
                        pageStack.push(Qt.resolvedUrl("Profile.qml"))
                    } else if (!cyanide.get_friend_accepted(fid)) {
                        currentFID = index - 1
                        pageStack.push(Qt.resolvedUrl("AcceptFriend.qml"))
                    } else {
                        currentFID = index - 1
                        pageStack.push(Qt.resolvedUrl("Friend.qml"), {})
                    }
                }
            }
            VerticalScrollDecorator {}
        }
    }
}
