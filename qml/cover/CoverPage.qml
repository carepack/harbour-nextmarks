import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Icon {
        anchors.centerIn: parent
        source: "image://theme/icon-m-cloud-upload"
    }

    Label {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge
            margins: Theme.paddingMedium
        }
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: nextcloudClient.isLoggedIn ? nextcloudClient.loginName : qsTr("Not logged in")
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }
}
