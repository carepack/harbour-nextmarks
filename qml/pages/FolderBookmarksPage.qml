import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property int folderId
    property string folderTitle

    onStatusChanged: {
        if (status === PageStatus.Active) {
            nextcloudClient.loadBookmarksInFolder(folderId)
        }
    }

    Connections {
        target: nextcloudClient
        onBookmarksFailed: {
            banner.text = message
            banner.visible = true
        }
        // Refreshes the moment a save actually completes server-side,
        // rather than relying on the page-active reload above -- that one
        // fires as soon as this page comes back on screen, which can be
        // *before* an in-flight save has finished, so it can reload too
        // early and still show stale data.
        onBookmarkSaved: nextcloudClient.loadBookmarksInFolder(folderId)
        onBookmarkDeleted: nextcloudClient.loadBookmarksInFolder(folderId)
        onBookmarkDeleteFailed: {
            banner.text = message
            banner.visible = true
        }
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: nextcloudClient.bookmarks

        header: PageHeader { title: folderTitle }

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: nextcloudClient.loadBookmarksInFolder(folderId)
            }
        }

        delegate: ListItem {
            id: delegate
            width: listView.width
            contentHeight: Math.max(Theme.itemSizeMedium, textColumn.implicitHeight + 2 * Theme.paddingSmall)

            // Deletes immediately, no undo countdown -- matches how
            // folder deletion ended up elsewhere in this app: every
            // Silica remorse mechanism tried there let its countdown run
            // and visibly complete without ever invoking the callback.
            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Delete")
                    onClicked: nextcloudClient.deleteBookmark(modelData.id)
                }
            }

            // Tapping opens the link the same way anywhere else in the
            // app opens an external URL -- Qt.openUrlExternally() hands
            // it to whatever the device's default browser is, which is
            // Sailfish Browser unless the user changed that.
            onClicked: Qt.openUrlExternally(modelData.url)

            // The server's captured preview image, falling back to its
            // favicon if there's no preview -- and to nothing at all if
            // neither was ever captured (most bookmarks won't have one,
            // that's routine, not an error). image://bookmarkicon/* is a
            // custom async provider (see BookmarkImageProvider) since
            // both endpoints require the same auth header every other
            // API call here uses, which a plain Image source can't send.
            Image {
                id: thumb
                property bool triedFavicon: false

                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                width: Theme.iconSizeMedium
                height: Theme.iconSizeMedium
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                source: "image://bookmarkicon/" + (triedFavicon ? "favicon/" : "image/") + modelData.id
                visible: status === Image.Ready
                onStatusChanged: {
                    if (status === Image.Error && !triedFavicon) {
                        triedFavicon = true
                    }
                }
            }

            Column {
                id: textColumn
                x: thumb.visible ? thumb.x + thumb.width + Theme.paddingMedium : Theme.horizontalPageMargin
                width: parent.width - x - Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall / 2

                Label {
                    width: parent.width
                    text: modelData.title
                    truncationMode: TruncationMode.Fade
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: modelData.url
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: delegate.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    visible: modelData.title !== modelData.url
                }
            }

            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                }
                height: 1
                color: Theme.rgba(Theme.primaryColor, 0.15)
                visible: index < nextcloudClient.bookmarks.length - 1
            }
        }

        ViewPlaceholder {
            enabled: nextcloudClient.bookmarks.length === 0 && !nextcloudClient.bookmarksLoading
            text: qsTr("No bookmarks in this folder")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: nextcloudClient.bookmarksLoading
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
