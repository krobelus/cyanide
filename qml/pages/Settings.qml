import QtQuick 2.0
import Sailfish.Silica 1.0


Page {
    function buildContextMenu(type) {
        var scr = "import QtQuick 2.0; import Sailfish.Silica 1.0; ContextMenu {"
        var values = settings.get_display_names(type)
        for(var i in values)
            scr += "MenuItem { text:\""+values[i]+"\" } "
        scr += "}"
        var menu = Qt.createQmlObject(scr, pageSettings, "myMenu")
        return menu
    }
    id: pageSettings
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        activePage = "Settings.qml"
        currentFID = 0
    }

    SilicaListView {
        id: settingsListView

        header: PageHeader {
            title: qsTr("Settings")
        }

        anchors.fill: parent
        model: settingsList

        delegate: BackgroundItem {
            id: delegate
            height: comboBox.height + Theme.paddingMedium

            ComboBox {
                id: comboBox
                x: Theme.paddingLarge
                width: parent.width - 2 * Theme.paddingLarge
                label: qsTrId(display_name)
                currentIndex: settings.get_current_index(name)

                menu: buildContextMenu(type)
                onCurrentIndexChanged: settings.set_current_index(name, currentIndex)
            }
        }
    }
}
