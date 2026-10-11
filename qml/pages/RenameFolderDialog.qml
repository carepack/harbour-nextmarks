import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property int folderId
    property string initialTitle
    property int parentFolderId: -1

    // Same virtual-keyboard word-commit lag as NewFolderDialog.qml's
    // onAccepted -- see its comment for why Qt.inputMethod.commit() runs
    // before reading nameField.text.
    onAccepted: {
        Qt.inputMethod.commit()
        var name = nameField.text.trim()
        if (name.length > 0 && name !== initialTitle) {
            nextcloudClient.renameFolder(folderId, name, parentFolderId)
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            DialogHeader { acceptText: qsTr("Rename") }

            TextField {
                id: nameField
                width: parent.width
                text: initialTitle
                label: qsTr("Folder name")
                placeholderText: qsTr("Folder name")
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: dialog.accept()
                focus: true
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Rename")
                onClicked: dialog.accept()
            }
        }

        VerticalScrollDecorator {}
    }
}
