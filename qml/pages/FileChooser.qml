import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import Qt.labs.folderlistmodel 2.1

Page {
    id: pageFileChooser
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("Friend.qml")
        folderListModel.folder = folder
        folderListModel.nameFilters =  fileChooserProperties.nameFilters
    }
    Component.onDestruction: {
        pages.pop()
    }

    property string folder: ""

    Notification { id: notification }

    function notify(summary, body) {
        notification.previewSummary = summary
        notification.previewBody = body
        notification.publish()
    }

    FolderListModel {
        id: folderListModel
        showDirs: true
        showDirsFirst: true
        showDotAndDotDot: true
    }

    SilicaListView {
        id: fileList
        anchors.fill: parent

        model: folderListModel

        PullDownMenu {
            MenuItem {
                text: qsTr("Cancel")
                onClicked: pageStack.pop(pageStack.find(function(page) {
                                return page.name === "profile"
                            }))
            }
            MenuItem {
                text:  filter + enabled
                property string filter: qsTr("Filter by extension")
                property string enabled: fileChooserProperties.nameFilters === [] ? no : yes
                property string yes: " ☑"
                property string no:  " ☐"
                onClicked: {
                    if(enabled === yes) {
                        enabled = no
                        fileChooserProperties.nameFilters = []
                    } else {
                        enabled = yes
                        fileChooserProperties.nameFilters = ["*.png", "*.gif"]
                    }
                    folderListModel.nameFilters = fileChooserProperties.nameFilters
                }
            }
        }

        header: PageHeader {
            id: pageHeader
            title: folder
            anchors {
                right: parent.right
                rightMargin: 2 * Theme.paddingLarge
            }
        }

        delegate: BackgroundItem {
            id: folderListDelegate
            height: Theme.itemSizeNormal
            width: parent.width

            onClicked: {
                if(folderListModel.isFolder(index)) {
                    if(fileName == ".") {
                        ;
                    } else if(fileName == "..") {
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"),
                                          { "folder": folder.replace(/[^\/]+\/$/, "")})
                    } else {
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"),
                                          { "folder": folder+fileName+"/"})
                    }
                } else {
                    if(fileChooserProperties.target === "selfAvatar") {
                        var errmsg = cyanide.set_self_avatar(folder+fileName)
                        if(errmsg === "") {
                                pageStack.pop(pageStack.find(function(page) {
                                    return page.name === "profile"
                                }))
                        } else {
                            notify(qsTr("Failed to set avatar"), qsTr(errmsg))
                        }
                    } else {
                    }
                }
            }

            Label {
                id: fileNameLabel
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingMedium
                }

                text: fileName
            }
        }
    }
}
