#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QVariantMap>

class ShareAdaptor;

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

private:
    friend class ShareAdaptor;
    void handleShare(const QVariantMap &args);

    bool m_ready = false;
    bool m_hasPending = false;
    QString m_pendingUrl;
    QString m_pendingTitle;
    QObject *m_target = nullptr;
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
