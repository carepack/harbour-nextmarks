import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    // Folder ids currently expanded to reveal their children -- same
    // collapsed-by-default approach as FolderPickerPage, and for the
    // same reason: a deep/wide tree buries the folders someone actually
    // wants under ones they don't care about.
    property var expandedIds: ({})

    onStatusChanged: {
        if (status === PageStatus.Active) {
            nextcloudClient.loadFolders()
        }
    }

    Connections {
        target: nextcloudClient
        onBookmarkSaved: {
            banner.color = Theme.rgba(Theme.secondaryHighlightColor, 0.9)
            banner.text = qsTr("Bookmark saved")
            banner.visible = true
        }
        onBookmarkSaveFailed: {
            banner.color = Theme.rgba(Theme.highlightBackgroundColor, 0.9)
            banner.text = message
            banner.visible = true
        }
        onFolderCreateFailed: {
            banner.color = Theme.rgba(Theme.highlightBackgroundColor, 0.9)
            banner.text = message
            banner.visible = true
        }
        onFolderDeleteFailed: {
            banner.color = Theme.rgba(Theme.highlightBackgroundColor, 0.9)
            banner.text = message
            banner.visible = true
        }
        // Previously unconnected: a failed folder load (a network hiccup,
        // a server error) silently left the list empty with no
        // indication why, showing "No folders yet" -- indistinguishable
        // from an account that genuinely has none.
        onFoldersFailed: {
            banner.color = Theme.rgba(Theme.highlightBackgroundColor, 0.9)
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

        header: PageHeader { title: qsTr("Nextmarks") }

        PullDownMenu {
            MenuItem {
                text: qsTr("Add bookmark")
                onClicked: pageStack.push(Qt.resolvedUrl("SaveBookmarkPage.qml"))
            }
            MenuItem {
                text: qsTr("New folder")
                onClicked: pageStack.push(Qt.resolvedUrl("NewFolderDialog.qml"))
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
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

            Label {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin + modelData.depth * Theme.paddingLarge
                    right: plusSlot.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                // The synthetic root entry's title ("(No folder)") comes
                // from NextcloudClient, shared with FolderPickerPage
                // where it reads correctly as a save-target choice ("no
                // folder"). Here it's a row you browse into like any
                // other, so it needs its own, clearer wording.
                text: modelData.id === -1 ? qsTr("Unfiled bookmarks") : modelData.title
                color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
            }

            // Expand/collapse control -- a plain "+"/"-" sign on the
            // right, its own tap target separate from the row's own
            // "browse into this folder" action below.
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

            onClicked: pageStack.push(Qt.resolvedUrl("FolderBookmarksPage.qml"), {
                folderId: modelData.id,
                folderTitle: modelData.title
            })
        }

        ViewPlaceholder {
            enabled: nextcloudClient.folders.length === 0 && !nextcloudClient.foldersLoading
            text: qsTr("No folders yet")
            hintText: qsTr("Share a link from any app and choose \"Save to Nextcloud Bookmarks\" -- or pull down here to add one manually.")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: nextcloudClient.foldersLoading
            visible: running
        }

        VerticalScrollDecorator {}
    }

    // Simple transient banner instead of a full notification system --
    // mirrors the same pattern used for errors in the sibling
    // harbour-readeck project.
    Rectangle {
        id: banner
        property alias text: bannerLabel.text

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: visible ? bannerLabel.implicitHeight + 2 * Theme.paddingMedium : 0
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
            color: Theme.primaryColor
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
