import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow

    initialPage: Qt.resolvedUrl(nextcloudClient.isLoggedIn ? "pages/MainPage.qml" : "pages/LoginPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // Called when a link was shared to the app (see src/sharereceiver.h
    // for why this is a hand-rolled D-Bus receiver in C++ rather than
    // the stock Sailfish.Share QML ShareProvider).
    function shareInto(url, title) {
        if (!nextcloudClient.isLoggedIn) {
            return
        }
        pageStack.push(Qt.resolvedUrl("pages/SaveBookmarkPage.qml"), {
            initialUrl: url,
            initialTitle: title
        })
    }

    Connections {
        target: shareReceiver
        onBookmarkShared: {
            // A share can arrive while the app is behind something else.
            appWindow.activate()
            appWindow.shareInto(url, title)
        }
    }

    Connections {
        target: nextcloudClient
        // Opening the login URL lives in QML (Qt.openUrlExternally)
        // rather than C++ QDesktopServices -- see nextcloudclient.h.
        onLoginUrlReady: Qt.openUrlExternally(url)
    }

    Connections {
        target: Qt.application
        onActiveChanged: {
            if (!Qt.application.active) {
                return
            }

            // Defensive re-registration, matching harbour-readeck: a
            // share can otherwise work only once per run if the app's
            // own D-Bus name gets dropped after losing focus.
            shareReceiver.registerService()

            // The user may have just finished (or abandoned) the
            // Login Flow v2 browser step; the poll timer can miss ticks
            // while backgrounded, so check immediately on return rather
            // than waiting for its next scheduled tick.
            if (nextcloudClient.loginInProgress) {
                nextcloudClient.pollNow()
            }
        }
    }
}
