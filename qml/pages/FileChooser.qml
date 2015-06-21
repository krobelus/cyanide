import QtQuick 2.0
import Sailfish.Silica 1.0
import Qt.labs.folderlistmodel 2.1

Page {
    id: page
    allowedOrientations: Orientation.All
    Component.onCompleted: {
        pages.push("FileChooser.qml")
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
                        name = "friend"
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
                    pageStack.clear()
                    pageStack.push(Qt.resolvedUrl("FriendList.qml"))
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
                var errmsg
                var path = folder + fileName
                if(folderListModel.isFolder(index)) {
                    if(fileName == ".") {
                        ;
                    } else if(fileName == "..") {
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"),
                                          { "folder": folder.replace(/[^\/]+\/$/, "")}, true)
                    } else {
                        pageStack.push(Qt.resolvedUrl("FileChooser.qml"),
                                          { "folder": path+"/"}, true)
                    }
                } else if(fileChooserProperties.target === "fileToSend") {
                    errmsg = cyanide.send_file(activeFriend(), path)
                    if(errmsg === "") {
                        returnToPage("friend")
                    } else {
                        cyanide.notify_error(qsTr("Failed to send file"), qsTr(errmsg))
                    }
                } else if(fileChooserProperties.target === "selfAvatar") {
                    errmsg = cyanide.set_self_avatar(path)
                    if(errmsg === "") {
                        returnToPage("profile")
                    } else {
                        cyanide.notify_error(qsTr("Failed to set avatar"), qsTr(errmsg))
                    }
                } else if(fileChooserProperties.target === "toxSaveFile") {
                    var ret = function() {
                            pageStack.clear()
                            pageStack.push(Qt.resolvedUrl("FriendList.qml"))
                        }

                    if(cyanide.file_is_encrypted(path)) {
                        pageStack.push(Qt.resolvedUrl("EnterPassword.qml"),
                                { dispatchAction: function(text) {
                                        if(cyanide.load_tox_save_file(path, text))
                                            ret()
                                        else
                                            cyanide.notify_error(qsTr("Decryption failed"), "")
                                }})
                    } else {
                        cyanide.load_tox_save_file(path, null)
                        ret()
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
