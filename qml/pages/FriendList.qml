import QtQuick 2.0
import Sailfish.Silica 1.0


Page {
        id: page

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        anchors.fill: parent

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
            MenuItem {
                text: qsTr("\\o/")
                onClicked: pageStack.push(Qt.resolvedUrl("SecondPage.qml"))
            }
        }

        // Tell SilicaFlickable the height of its content.
        //contentHeight: friendList.height

        SilicaListView {
            id: friendList

            header: PageHeader {
                title: 'Contacts'
            }

            anchors.fill: parent
            model: ListModel {
                id: model
            }
            Component.onCompleted: {
                var i = cyanide.get_number_of_friends()
                friendList.appendFriend(i)
                i = 0
                while(i < cyanide.get_number_of_friends()){
                    friendList.appendFriend(i)
                    i++
                }
            }
            function appendFriend(i) {
                    model.append({
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
                    var i = listID(fid)
                    model.setProperty(i, "friend_name", cyanide.get_friend_name(fid))
                }
                onSignal_connection_status: {
                    var i = listID(fid)
                    model.setProperty(i, "friend_connection_status", cyanide.get_friend_connection_status(fid))
                    model.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
                }
                onSignal_status_message: {
                    var i = listID(fid)
                    model.setProperty(i, "friend_status_message", cyanide.get_friend_status_message(fid))
                }
                onSignal_friend_message: {
                    var i = listID(fid)
                    if(i > 0) {
                        model.setProperty(i, "friend_status_icon", cyanide.get_friend_status_icon(fid))
                    }
                }
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
                    if(index == 0) {
                        pageStack.push(Qt.resolvedUrl("Profile.qml"))
                    } else {
                        currentFid = index - 1
                        pageStack.push(Qt.resolvedUrl("Friend.qml"))
                    }
                }
            }
            VerticalScrollDecorator {}
        }
    }
}
