import QtQuick 2.0
import Sailfish.Silica 1.0


Page {
    id: pageSettings
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        activePage = "Settings.qml"
        currentFID = 0
    }
}
