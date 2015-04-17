import QtQuick 2.0
import Sailfish.Silica 1.0
import Qt.labs.folderlistmodel 2.1

Page {
    id: page
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("Friend.qml")
        folderListModel.folder = folder
        folderListModel.toggleNameFilters()
    }
    Component.onDestruction: {
        pages.pop()
    }

    property string folder: ""

    FolderListModel {
        id: folderListModel
        showDirs: true
        showDirsFirst: true
        showDotAndDotDot: true

        function toggleNameFilters() {
            if(fileChooserProperties.filter)
                nameFilters = fileChooserProperties.nameFilters
            else
                nameFilters = []
        }
    }

    SilicaListView {
        id: fileList
        anchors.fill: parent

        model: folderListModel

        PullDownMenu {
            MenuItem {
                text: qsTr("Cancel")
                onClicked: {
                    var name
                    if(fileChooserProperties.target === "selfAvatar") {
                        name = "profile"
                    } else if(fileChooserProperties.target === "fileToSend") {
                        name = "friendaction"
                    } else if(fileChooserProperties.target === "toxSaveFile") {
                        name = "friendlist"
                    }

                    returnToPage(name)
                }
            }
            MenuItem {
                text:  qsTr("Filter by filename extension")+" "+checkbox
                visible: fileChooserProperties.target !== "fileToSend"
                property string checkbox: fileChooserProperties.filter ? "☑" : "☐"
                onClicked: {
                    fileChooserProperties.filter = !fileChooserProperties.filter
                    checkbox = fileChooserProperties.filter ? "☑" : "☐"
                    folderListModel.toggleNameFilters()
                }
            }
            MenuItem {
                text: qsTr("Remove my avatar")
                visible: fileChooserProperties.target === "selfAvatar"
                onClicked: {
                    var errmsg = cyanide.set_self_avatar("")
                    if(errmsg === "") {
                        returnToPage("profile")
                    } else {
                        cyanide.notify_error(qsTr("Failed to set avatar"), qsTr(errmsg))
                    }
                }
            }
            MenuItem {
                text: qsTr("New profile")
                visible: fileChooserProperties.target === "toxSaveFile"
                onClicked: {
                    cyanide.load_new_profile()
                    returnToPage("profile")
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
                                          { "folder": folder.replace(/[^\/]+\/$/, "")}, true)
                    } else {
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"),
                                          { "folder": folder+fileName+"/"}, true)
                    }
                } else if(fileChooserProperties.target === "fileToSend") {
                    errmsg = cyanide.send_file(activeFriend(), folder+fileName)
                    if(errmsg === "") {
                        returnToPage("friendaction")
                    } else {
                        cyanide.notify_error(qsTr("Failed to send file"), qsTr(errmsg))
                    }
                } else if(fileChooserProperties.target === "selfAvatar") {
                    errmsg = cyanide.set_self_avatar(folder+fileName)
                    if(errmsg === "") {
                        returnToPage("profile")
                    } else {
                        cyanide.notify_error(qsTr("Failed to set avatar"), qsTr(errmsg))
                    }
                } else if(fileChooserProperties.target === "toxSaveFile") {
                    cyanide.load_tox_save_file(folder+fileName)
                    returnToPage("friendlist")
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
