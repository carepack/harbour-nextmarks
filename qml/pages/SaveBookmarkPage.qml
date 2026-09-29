import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    // Pre-filled when opened from the share sheet (see harbour-nextmarks.qml
    // shareInto()); empty when opened from the pulley menu.
    property string initialUrl: ""
    property string initialTitle: ""

    property int selectedFolderId: -1
    property string selectedFolderTitle: qsTr("(No folder)")

    // Not gated on urlField.text here (see NewFolderDialog.qml's comment
    // on canAccept): Sailfish's virtual keyboard buffers the word being
    // composed and only commits it into the field's text once a
    // separator is typed, so a canAccept bound to it can stay disabled
    // even though the URL is visibly fully typed. Validated here instead.
    // Qt.inputMethod.commit() forces that pending word into both fields'
    // text properties before they're read -- see NewFolderDialog.qml's
    // identical comment (confirmed live via screenshot there: the
    // composing word was still shown underlined/uncommitted at the exact
    // moment Create was tapped).
    onAccepted: {
        Qt.inputMethod.commit()
        var url = urlField.text.trim()
        if (url.length > 0) {
            nextcloudClient.saveBookmark(url, titleField.text.trim(), selectedFolderId)
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: qsTr("Save bookmark")
                acceptText: qsTr("Save")
            }

            TextField {
                id: urlField
                width: parent.width
                text: initialUrl
                label: qsTr("URL")
                placeholderText: qsTr("https://example.com/article")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                focus: initialUrl.length === 0
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: titleField.focus = true
            }

            TextField {
                id: titleField
                width: parent.width
                text: initialTitle
                label: qsTr("Title (optional)")
                placeholderText: qsTr("Custom title")
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: dialog.accept()
            }

            ValueButton {
                width: parent.width
                label: qsTr("Folder")
                value: dialog.selectedFolderTitle
                onClicked: {
                    var picker = pageStack.push(Qt.resolvedUrl("FolderPickerPage.qml"), {
                        selectedFolderId: dialog.selectedFolderId
                    })
                    picker.folderSelected.connect(function (folderId, title) {
                        dialog.selectedFolderId = folderId
                        dialog.selectedFolderTitle = title
                    })
                }
            }

            // A plain button alongside the header's swipe-to-accept
            // gesture -- confirmed live (on NewFolderDialog.qml, the same
            // Dialog pattern) that the swipe doesn't register as "accept"
            // while a TextField on the page still has keyboard focus, so
            // this is a reliable fallback that doesn't depend on it.
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Save")
                onClicked: dialog.accept()
            }
        }

        VerticalScrollDecorator {}
    }
}
