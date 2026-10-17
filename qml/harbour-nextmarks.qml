import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow

    initialPage: Qt.resolvedUrl(nextcloudClient.isLoggedIn ? "pages/MainPage.qml" : "pages/LoginPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // Set while a share arrived before nextcloudClient.isLoggedIn became
    // true -- e.g. a cold D-Bus-activated start where restoreSession()
    // is still waiting on an interactive Secrets unlock. Consumed by
    // onIsLoggedInChanged below once that resolves, instead of the
    // share being silently lost.
    property string pendingShareUrl: ""
    property string pendingShareTitle: ""

    // Delivers a share/open link that was queued during login, strictly
    // after the isLoggedInChanged signal's synchronous slot calls have
    // all returned. LoginPage.qml listens to the same nextcloudClient
    // isLoggedInChanged signal to replace itself with MainPage; Qt calls
    // connected slots in connection order, and this window's own
    // Connections exists (and so connects) before LoginPage is even
    // loaded, so without this deferral the two race -- the save dialog
    // gets pushed first, then LoginPage's replace() clobbers it right
    // back off the stack (confirmed live on-device, same bug in the
    // sibling harbour-readeck project). interval: 0 defers to the next
    // event-loop pass, after every synchronous listener (including
    // LoginPage's replace) has already run.
    Timer {
        id: pendingShareDeliveryTimer
        interval: 0
        property string url: ""
        property string title: ""
        onTriggered: appWindow.shareInto(url, title)
    }

    // Retries shareInto() shortly when the page stack hasn't finished
    // loading its initialPage yet -- confirmed live on the sibling
    // harbour-readeck project: a D-Bus-delivered Open()/share call can be
    // processed (isLoggedIn already true, no interactive unlock needed)
    // before Silica's own initialPage has loaded, landing the push on an
    // empty stack where it then gets silently overwritten the moment
    // initialPage actually loads. Polling is the only option -- there's
    // no public "page stack ready" signal to wait on instead.
    Timer {
        id: pageStackRetryTimer
        interval: 50
        property string url: ""
        property string title: ""
        onTriggered: appWindow.shareInto(url, title)
    }

    // Called when a link was shared to the app (see src/sharereceiver.h
    // for why this is a hand-rolled D-Bus receiver in C++ rather than
    // the stock Sailfish.Share QML ShareProvider).
    function shareInto(url, title) {
        if (!nextcloudClient.isLoggedIn) {
            pendingShareUrl = url
            pendingShareTitle = title
            return
        }
        if (!shareReceiver.pageStackReady || pageStack.busy) {
            // pageStackReady alone isn't enough -- it's set once the
            // initial page reaches PageStatus.Active (Component.onCompleted
            // fires too early, confirmed live on the sibling
            // harbour-readeck project: Silica's own "cannot push while
            // transition is in progress" warning fired when a push raced
            // that transition). busy guards every later transition too,
            // e.g. LoginPage's own replace() to MainPage on login
            // success, which is a second, separate transition this push
            // could just as easily race.
            pageStackRetryTimer.url = url
            pageStackRetryTimer.title = title
            pageStackRetryTimer.start()
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
            // A share (or an opened link, routed through the same signal
            // per KTD2) can arrive while the app is behind something else.
            appWindow.activate()
            appWindow.shareInto(url, title)
        }
        // A plain org.freedesktop.Application Activate() call (e.g. a
        // normal launch via the system once X-DBusActivatable=true is
        // set, with no link to open) -- just raise the window.
        onActivateRequested: appWindow.activate()
    }

    Connections {
        target: nextcloudClient
        // Opening the login URL lives in QML (Qt.openUrlExternally)
        // rather than C++ QDesktopServices -- see nextcloudclient.h.
        onLoginUrlReady: Qt.openUrlExternally(url)

        // Fires on manual login and on a background Secrets unlock
        // completing -- either way, deliver a share that arrived too
        // early instead of leaving it stuck in pendingShareUrl forever.
        onIsLoggedInChanged: {
            if (!nextcloudClient.isLoggedIn || pendingShareUrl.length === 0) {
                return
            }
            pendingShareDeliveryTimer.url = pendingShareUrl
            pendingShareDeliveryTimer.title = pendingShareTitle
            pendingShareUrl = ""
            pendingShareTitle = ""
            pendingShareDeliveryTimer.start()
        }
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
