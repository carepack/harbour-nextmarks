import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    Connections {
        target: nextcloudClient
        onIsLoggedInChanged: {
            if (nextcloudClient.isLoggedIn) {
                pageStack.replace(Qt.resolvedUrl("MainPage.qml"))
            }
        }
        onLoginFailed: {
            errorLabel.text = message
            errorLabel.visible = true
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Log in to Nextcloud") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Enter your Nextcloud server address. You'll be taken to your server's own login page in the browser -- this app never sees your password.")
            }

            TextField {
                id: serverField
                width: parent.width
                label: qsTr("Server address")
                placeholderText: qsTr("cloud.example.com")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                enabled: !nextcloudClient.busy && !nextcloudClient.loginInProgress
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.enabled: text.trim().length > 0
                EnterKey.onClicked: {
                    errorLabel.visible = false
                    nextcloudClient.startLogin(text.trim())
                }
            }

            Label {
                id: errorLabel
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.errorColor
                visible: false
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Log in")
                enabled: serverField.text.trim().length > 0 && !nextcloudClient.busy && !nextcloudClient.loginInProgress
                onClicked: {
                    errorLabel.visible = false
                    nextcloudClient.startLogin(serverField.text.trim())
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: nextcloudClient.busy || nextcloudClient.loginInProgress
                visible: running
            }

            Column {
                width: parent.width
                spacing: Theme.paddingSmall
                visible: nextcloudClient.loginInProgress

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.secondaryColor
                    text: qsTr("Waiting for you to finish logging in in the browser…")
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Cancel")
                    onClicked: nextcloudClient.cancelLogin()
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
