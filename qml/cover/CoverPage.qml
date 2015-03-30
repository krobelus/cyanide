import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {

    CoverActionList {
        id: coverAction
        /*
        CoverAction {
            id: setAway
            iconSource: "qrc:/images/away_2x"
        }
        CoverAction {
            id: setBusy
            iconSource: "qrc:/images/busy_2x"
        }
        */
    }
    /*
    Connections {
        target: cyanide
        onSignal_connection_status: {
            statusIcon.source = cyanide.get_friend_status_icon(self_friend_number)
        }
    }

    Image {
        id: statusIcon
        source: cyanide.get_friend_status_icon(self_friend_number)
        fillMode: Image.Pad
        anchors {
            //leftMargin: 2*Theme.paddingLarge
            //rightMargin: 2*Theme.paddingLarge
            left: tox.left
            right: tox.right
            top: tox.top
            topMargin: 30
            bottom: tox.bottom
        }
    }
    */
    Image {
        id: tox
        source: "qrc:/images/cover"
        fillMode: Image.PreserveAspectFit
        anchors {
            left: parent.left
            leftMargin: 2*Theme.paddingLarge
            right: parent.right
            rightMargin: 2*Theme.paddingLarge
            top: parent.top
            bottom: parent.bottom
        }
    }
    /*
    OpacityRampEffect {
        id: effect
        direction: OpacityRamp.BottomToTop
        slope: 4
        offset: 0.75
        sourceItem: iconLabel
    }

    Connections {
        target: coverModel
        onIconSourceChanged: {
            iconLabel.source = iconSource
            if (iconSource.indexOf("qrc:/icons") !== -1)
                effect.offset = 1
            else
                effect.offset = 0.75
        }
    }
    */
}


