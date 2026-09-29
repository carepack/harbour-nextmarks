import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property int selectedFolderId: -1

    // Folder ids currently expanded to reveal their children. Folders
    // start collapsed -- with a deep or wide folder tree, showing every
    // level at once buries the folders someone actually wants under
    // ones they don't care about; expanding only the ones you need to
    // look into keeps the list manageable.
    property var expandedIds: ({})

    signal folderSelected(int folderId, string title)

    onStatusChanged: {
        if (status === PageStatus.Active && nextcloudClient.folders.length === 0) {
            nextcloudClient.loadFolders()
        }
    }

    Connections {
        target: nextcloudClient
        onFolderCreateFailed: {
            banner.text = message
            banner.visible = true
        }
        onFolderDeleteFailed: {
            banner.text = message
            banner.visible = true
        }
        onFoldersFailed: {
            banner.text = message
            banner.visible = true
        }
    }

    function toggleExpanded(folderId) {
        // Reassigned rather than mutated in place: expandedIds is a
        // plain JS object, and QML bindings that read it (visibleFolders()
        // below) only notice a *new* value, not an in-place mutation of
        // the same one.
        var updated = {}
        for (var key in expandedIds) {
            updated[key] = expandedIds[key]
        }
        if (updated[folderId]) {
            delete updated[folderId]
        } else {
            updated[folderId] = true
        }
        expandedIds = updated
    }

    // nextcloudClient.folders is a depth-first pre-order flattening of
    // the real folder tree (see flattenFolders() in nextcloudclient.cpp)
    // -- so hiding a collapsed folder's descendants is a single pass:
    // once a collapsed folder is seen, skip every following item deeper
    // than it, until an item back at its own depth (a sibling) appears.
    // No parent-id lookup needed.
    function visibleFolders() {
        var result = []
        var skipDepth = -1
        var list = nextcloudClient.folders
        for (var i = 0; i < list.length; i++) {
            var item = list[i]
            if (skipDepth !== -1 && item.depth > skipDepth) {
                continue
            }
            skipDepth = -1
            result.push(item)
            if (item.hasChildren && !expandedIds[item.id]) {
                skipDepth = item.depth
            }
        }
        return result
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: page.visibleFolders()

        header: PageHeader { title: qsTr("Choose folder") }

        PullDownMenu {
            MenuItem {
                text: qsTr("New folder")
                onClicked: pageStack.push(Qt.resolvedUrl("NewFolderDialog.qml"))
            }
        }

        delegate: ListItem {
            id: delegate
            width: listView.width
            contentHeight: Theme.itemSizeSmall

            // The synthetic root entry (id -1, "(No folder)") isn't a
            // real folder -- nothing to rename/delete, and a subfolder
            // of it is just a normal top-level folder, same as "New
            // folder" in the pulldown menu above, so no menu for it.
            menu: modelData.id !== -1 ? folderContextMenu : null

            Component {
                id: folderContextMenu
                ContextMenu {
                    MenuItem {
                        text: qsTr("New subfolder")
                        onClicked: pageStack.push(Qt.resolvedUrl("NewFolderDialog.qml"), { parentFolderId: modelData.id })
                    }
                    MenuItem {
                        text: qsTr("Delete folder")
                        onClicked: nextcloudClient.deleteFolder(modelData.id)
                    }
                }
            }

            // Selected-folder checkmark -- on the left, out of the way of
            // the expand/collapse control on the right.
            Icon {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                source: "image://theme/icon-s-accept"
                visible: modelData.id === page.selectedFolderId
            }

            Label {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin + Theme.iconSizeSmall + Theme.paddingSmall + modelData.depth * Theme.paddingLarge
                    right: plusSlot.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                text: modelData.title
                color: (modelData.id === page.selectedFolderId)
                       ? Theme.highlightColor
                       : (delegate.highlighted ? Theme.highlightColor : Theme.primaryColor)
            }

            // Expand/collapse control -- a plain "+"/"-" sign rather than
            // a chevron icon, on the right where it's easier to reach
            // with a thumb than tucked against the list's left edge. Its
            // own tap target, separate from the row's own action below,
            // so tapping it never also triggers "pick this folder" /
            // "browse into this folder". The tap target is deliberately
            // larger than the visible glyph -- the whole point of this
            // change is that the old chevron was too small/fiddly to hit.
            Item {
                id: plusSlot
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                width: Theme.itemSizeExtraSmall
                height: Theme.itemSizeExtraSmall

                Label {
                    anchors.centerIn: parent
                    visible: modelData.hasChildren
                    text: expandedIds[modelData.id] ? "−" : "+"
                    font.pixelSize: Theme.fontSizeLarge
                    color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: modelData.hasChildren
                    onClicked: page.toggleExpanded(modelData.id)
                }
            }

            onClicked: {
                page.folderSelected(modelData.id, modelData.title)
                pageStack.pop()
            }
        }

        ViewPlaceholder {
            enabled: nextcloudClient.folders.length === 0 && !nextcloudClient.foldersLoading
            text: qsTr("No folders yet")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: nextcloudClient.foldersLoading
            visible: running
        }

        VerticalScrollDecorator {}
    }

    Rectangle {
        id: banner
        property alias text: bannerLabel.text

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: visible ? bannerLabel.implicitHeight + 2 * Theme.paddingMedium : 0
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.9)
        visible: false

        Label {
            id: bannerLabel
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
                margins: Theme.horizontalPageMargin
            }
            wrapMode: Text.WordWrap
            color: Theme.errorColor
        }

        MouseArea {
            anchors.fill: parent
            onClicked: banner.visible = false
        }

        Timer {
            running: banner.visible
            interval: 4000
            onTriggered: banner.visible = false
        }
    }

}
