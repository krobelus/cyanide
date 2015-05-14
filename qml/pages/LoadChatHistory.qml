import QtQuick 2.0
import Sailfish.Silica 1.0


Dialog {
    id: page
    allowedOrientations: Orientation.All
    canAccept: true

    property int f: activeFriend()

    onAccepted: {
        onClicked: cyanide.load_history(f, friendList.get(f+1).friend_history_from
                                         , friendList.get(f+1).friend_history_to)
        refreshFriendList()
        returnToPage("friend")
    }

    function openDateDialog(id) {
        var dialog = pageStack.push("Sailfish.Silica.DatePickerDialog", {
                                        allowedOrientations: Orientation.All
                                    })
        dialog.accepted.connect(function() {
            if(id === dateTo) {
                friendList.setProperty(f+1, "friend_history_to", dialog.date)
            } else if(id === dateFrom) {
                friendList.setProperty(f+1, "friend_history_from", dialog.date)
            }
        })
    }

    Column {
        anchors.centerIn: parent

        Row {
            ValueButton {
                id: dateFrom
                width: page.width - clearTo.width - Theme.paddingLarge
                label: qsTr("From")
                onClicked: openDateDialog(dateFrom)
                value: friendList.get(f+1).friend_history_from === undefined ? ""
                     : (friendList.get(f+1).friend_history_from.toString() === "Invalid Date" ? ""
                     :  friendList.get(f+1).friend_history_from.toString())
            }
            IconButton {
                id: clearFrom
                icon.source: "image://theme/icon-m-clear"
                onClicked: friendList.setProperty(f+1, "friend_history_from", cyanide.null_date())
            }
        }

        Row {
            ValueButton {
                id: dateTo
                width: dateFrom.width
                label: qsTr("To")
                onClicked: openDateDialog(dateTo)
                value: friendList.get(f+1).friend_history_to === undefined ? ""
                     : (friendList.get(f+1).friend_history_to.toString() === "Invalid Date" ? ""
                     :  friendList.get(f+1).friend_history_to.toString())
            }
            IconButton {
                id: clearTo
                icon.source: "image://theme/icon-m-clear"
                onClicked: friendList.setProperty(f+1, "friend_history_to", cyanide.null_date())
            }
        }
    }
}
