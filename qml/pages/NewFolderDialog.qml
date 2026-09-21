import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    // -1 (the reserved root id, same convention as everywhere else in
    // this app) creates a top-level folder.
    property int parentFolderId: -1

    // Not gated on nameField.text here: Sailfish's virtual keyboard
    // buffers the word currently being composed (for predictive text)
    // and only commits it into the TextField's actual text property once
    // a separator -- a space, punctuation, or losing focus -- is typed.
    // A canAccept bound to text.trim().length lags behind what's visibly
    // on screen, so the accept swipe stays disabled until a trailing
    // space commits the pending word. Validating in onAccepted instead
    // (a no-op on empty input, rather than blocking the gesture) avoids
    // depending on that commit timing for *whether accepting is allowed*.
    //
    // But the same lag also affects the *value* read here: a screenshot
    // confirmed the composing word was still shown underlined (still
    // pending, not committed) when Create was tapped, so nameField.text
    // was still empty/stale at that exact moment even though the word
    // was clearly visible on screen -- Qt.inputMethod.commit() forces
    // that pending word into the property first, before it's read.
    onAccepted: {
        Qt.inputMethod.commit()
        var name = nameField.text.trim()
        if (name.length > 0) {
            nextcloudClient.createFolder(name, parentFolderId)
        }
    }

    // Wrapped in a SilicaFlickable (matching SaveBookmarkPage.qml) so the
    // content -- specifically the "Create" button below -- can scroll
    // into view above the on-screen keyboard instead of staying fixed in
    // a plain Column, where it could otherwise end up covered by the
    // keyboard and unreachable while the name field still has focus.
    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            DialogHeader { acceptText: qsTr("Create") }

            TextField {
                id: nameField
                width: parent.width
                label: qsTr("Folder name")
                placeholderText: qsTr("Folder name")
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: dialog.accept()
                focus: true
            }

            // A plain button alongside the header's swipe-to-accept
            // gesture and the keyboard's Enter key -- confirmed live that
            // the swipe doesn't register as "accept" while this page's
            // TextField still has keyboard focus (Enter does work), so
            // this is a reliable fallback that doesn't depend on it.
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Create")
                onClicked: dialog.accept()
            }
        }

        VerticalScrollDecorator {}
    }
}
