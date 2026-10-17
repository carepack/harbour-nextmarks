#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QVariantMap>
#include <QStringList>

class ShareAdaptor;
class AppActivationAdaptor;

// Receives links shared to the app via the Sailfish share sheet ("Save to
// Nextcloud", see harbour-nextmarks.desktop's X-Share-Methods), at D-Bus
// object path /share/link.
//
// The stock Sailfish.Share QML ShareProvider component expects each
// shared item as a ShareResource with 'name'/'data'/'filePath' keys and
// rejects anything else. Sailfish Browser's own "Share link" action does
// not send that shape -- captured live with dbus-monitor (see the sibling
// harbour-readeck project, where this class originates), its single dict
// argument looks like:
//
//   { mimeType: "text/x-url",
//     resources: [ { linkTitle: "<page title>", status: "<url>", type: "text/x-url" } ],
//     selectedTransferMethodInfo: {...}, title: "Share link" }
//
// ShareProvider can't be taught this shape from QML, so this class
// registers the D-Bus object itself (via ShareAdaptor, since a plain
// exported slot doesn't advertise the "org.sailfishos.share" interface
// name the caller expects -- that gets UnknownInterface) and parses the
// call by hand. Values in that dict can arrive as a nested QDBusArgument
// rather than a plain QVariantList/QVariantMap -- see sharereceiver.cpp.
class ShareReceiver : public QObject
{
    Q_OBJECT

    // Set by QML (from the actual initial page's Component.onCompleted --
    // LoginPage.qml or MainPage.qml, whichever loads) once Silica's own
    // page stack has genuinely finished loading its initial page. Lives
    // here, not as a plain QML property on the root window, because a
    // dynamically-loaded Page (pageStack.push()'d or loaded as
    // initialPage) cannot see the root file's local `id: appWindow` --
    // only a context property (like this object already is) is visible
    // from every loaded QML document. Needed because a D-Bus-delivered
    // Open()/share call can be processed early enough that pageStack's
    // own depth/currentPage read back as not-yet-meaningful (confirmed
    // live on the sibling harbour-readeck project: a push already raced
    // ahead of the real initial page and got silently overwritten when
    // it loaded) -- this flag is the one reliable, self-controlled
    // signal for "safe to push now."
    Q_PROPERTY(bool pageStackReady MEMBER m_pageStackReady NOTIFY pageStackReadyChanged)

public:
    explicit ShareReceiver(QObject *parent = nullptr);

    // Claims the D-Bus service/object that makes the share-sheet call
    // land here. Called as early as possible in main() -- right after
    // this object is exposed to QML, before the QML tree is even parsed
    // -- because the share sheet's own call to us appears to give up if
    // our service doesn't answer promptly when D-Bus activation had to
    // cold-start the app.
    //
    // Also Q_INVOKABLE so QML can call it again on every app-foreground
    // transition: a share can otherwise work only once per run if the
    // app's own D-Bus name gets dropped (e.g. after losing focus) while
    // X-Nemo-Single-Instance keeps a fresh activation from starting a
    // replacement process to re-claim it. Re-registering is idempotent
    // and cheap, so it costs nothing to call defensively.
    Q_INVOKABLE void registerService();

    // Marks QML as ready to receive bookmarkShared(). Call once after
    // the QML root object exists (its `Connections { target:
    // shareReceiver }` included). A share that arrived between
    // registerService() and this call is queued rather than lost.
    void setReady();

signals:
    void bookmarkShared(const QString &url, const QString &title);

    // Emitted for a plain org.freedesktop.Application Activate() call
    // (e.g. a desktop-environment launch once X-DBusActivatable=true is
    // set, with no URI to open) -- QML just raises the window, the same
    // way it already does for onBookmarkShared, without routing anything
    // into shareInto().
    void activateRequested();

    void pageStackReadyChanged();

private:
    friend class ShareAdaptor;
    friend class AppActivationAdaptor;
    void handleShare(const QVariantMap &args);
    void handleOpen(const QString &url);
    void deliverOrQueue(const QString &url, const QString &title);

    bool m_ready = false;
    bool m_hasPending = false;
    QString m_pendingUrl;
    QString m_pendingTitle;
    QObject *m_target = nullptr;
    QObject *m_appActivationTarget = nullptr;
    bool m_pageStackReady = false;
};

class ShareAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.sailfishos.share")

public:
    ShareAdaptor(QObject *targetObject, ShareReceiver *receiver);

public slots:
    void share(const QVariantMap &args);

private:
    ShareReceiver *m_receiver;
};

// Implements the freedesktop.org "D-Bus Activation" standard interface
// (org.freedesktop.Application) at the object path the spec derives from
// this app's own D-Bus service name (dots -> slashes, dashes ->
// underscores: harbour-nextmarks.harbour-nextmarks -> /harbour_nextmarks/
// harbour_nextmarks), registered on the same running service as
// ShareAdaptor rather than a second one. Lets the system's "open with"
// chooser (see harbour-nextmarks.desktop's MimeType/X-DBusActivatable)
// activate this app for an http/https link. See the sibling
// harbour-readeck project's docs/plans/2026-10-17-1420-feat-open-url-handler-plan.md,
// which this mirrors.
class AppActivationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")

public:
    AppActivationAdaptor(QObject *targetObject, ShareReceiver *receiver);

public slots:
    // Reduced to the first http/https entry in uris; anything else in the
    // list (and platformData) is ignored -- this app's save flow handles
    // one URL at a time, matching how Share already behaves. Q_NOREPLY:
    // the spec defines no reply, so a caller gets none regardless of
    // whether a usable URI was found.
    Q_NOREPLY void Open(const QStringList &uris, const QVariantMap &platformData);

    // Plain activation (e.g. a normal launch once X-DBusActivatable=true
    // is set, with no URI) -- just raises the window via
    // activateRequested(), independent of the share/open flow.
    Q_NOREPLY void Activate(const QVariantMap &platformData);

private:
    ShareReceiver *m_receiver;
};
