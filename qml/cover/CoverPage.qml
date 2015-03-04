import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {

    CoverActionList {
        id: coverAction
        /*
        CoverAction {
            iconSource: selfStatusIcon
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
        }
        */
    }

    Image {
        id: iconLabel

        anchors {
            left: parent.left
            leftMargin: 2*Theme.paddingLarge
            right: parent.right
            rightMargin: 2*Theme.paddingLarge
            top: parent.top
            bottom: parent.bottom
        }

        source: "qrc:/images/cover"

        fillMode: Image.PreserveAspectFit
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


