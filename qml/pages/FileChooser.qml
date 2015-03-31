import QtQuick 2.0
import Sailfish.Silica 1.0
import nemonotifications 1.0
import Qt.labs.folderlistmodel 2.1

Page {
    id: page
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
                text: qsTr("Remove my avatar")
                onClicked: {
                    var errmsg = cyanide.set_self_avatar("")
                    if(errmsg === "") {
                            pageStack.pop(pageStack.find(function(page) {
                                return page.name === "profile"
                            }))
                    } else {
                        notify(notification, qsTr("Failed to set avatar"), qsTr(errmsg))
                    }
                }
            }
            MenuItem {
                text:  filter + enabled
                visible: fileChooserProperties.target === "selfAvatar"
                property string filter: qsTr("Filter by filename extension")
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
                var errmsg;
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
                } else if(fileChooserProperties.target === "fileToSend") {
                    errmsg = cyanide.send_file(activeFriend(), folder+fileName)
                    if(errmsg === "") {
                        pageStack.pop(pageStack.find(function(page) {
                            return page.name === "friend"
                        }))
                    } else {
                        notify(notification, qsTr("Failed to send file"), qsTr(errmsg))
                    }
                } else if(fileChooserProperties.target === "selfAvatar") {
                        errmsg = cyanide.set_self_avatar(folder+fileName)
                        if(errmsg === "") {
                                pageStack.pop(pageStack.find(function(page) {
                                    return page.name === "profile"
                                }))
                        } else {
                            notify(notification, qsTr("Failed to set avatar"), qsTr(errmsg))
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
