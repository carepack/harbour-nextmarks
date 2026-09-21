#include "sharereceiver.h"

#include <QDBusConnection>
#include <QDBusArgument>
#include <QVariantList>

namespace {

// Qt's D-Bus demarshalling of the share call's "a{sv}" argument into a
// QVariantMap converts each entry's key, but a value that is itself a
// nested container variant can come through as an opaque QDBusArgument
// rather than a native QVariantList/QVariantMap. This recursively
// unwraps it (confirmed against real share calls from Sailfish Browser
// in the sibling harbour-readeck project).
QVariant unwrapDBusArgument(const QVariant &value)
{
    if (value.userType() != qMetaTypeId<QDBusArgument>()) {
        return value;
    }
    const QDBusArgument arg = value.value<QDBusArgument>();
    switch (arg.currentType()) {
    case QDBusArgument::ArrayType: {
        QVariantList list;
        for (const QVariant &v : qdbus_cast<QVariantList>(arg)) {
            list << unwrapDBusArgument(v);
        }
        return list;
    }
    case QDBusArgument::MapType: {
        QVariantMap map;
        const QVariantMap raw = qdbus_cast<QVariantMap>(arg);
        for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
            map[it.key()] = unwrapDBusArgument(it.value());
        }
        return map;
    }
    case QDBusArgument::VariantType: {
        QVariant inner;
        arg >> inner;
        return unwrapDBusArgument(inner);
    }
    default:
        return arg.asVariant();
    }
}

} // namespace

ShareReceiver::ShareReceiver(QObject *parent)
    : QObject(parent)
{
}

void ShareReceiver::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!m_target) {
        m_target = new QObject(this);
        new ShareAdaptor(m_target, this);
    }
    bus.registerObject(QStringLiteral("/share/link"), m_target);

    // Claim the OrganizationName.ApplicationName the share sheet expects
    // (see harbour-nextmarks.desktop's [X-Sailjail] section) last: this is
    // what unblocks a pending D-Bus-activated call, so the object above
    // must already be registered by the time it can arrive.
    bus.registerService(QStringLiteral("harbour-nextmarks.harbour-nextmarks"));
}

void ShareReceiver::setReady()
{
    m_ready = true;
    if (m_hasPending) {
        m_hasPending = false;
        emit bookmarkShared(m_pendingUrl, m_pendingTitle);
    }
}

void ShareReceiver::handleShare(const QVariantMap &args)
{
    const QVariantList items = unwrapDBusArgument(args.value(QStringLiteral("resources"))).toList();
    if (items.isEmpty()) {
        return;
    }

    // "resources" is an array of shared items; each item is normally the
    // resource dict directly. Tolerate one extra "av" wrapping layer
    // too, in case some other sender double-wraps it.
    QVariant first = unwrapDBusArgument(items.first());
    if (first.userType() != QVariant::Map) {
        const QVariantList nested = first.toList();
        if (!nested.isEmpty()) {
            first = unwrapDBusArgument(nested.first());
        }
    }
    const QVariantMap resource = first.toMap();

    const QString url = resource.value(QStringLiteral("status")).toString();
    const QString title = resource.value(QStringLiteral("linkTitle")).toString();
    if (url.isEmpty()) {
        return;
    }

    // This slot must return quickly regardless of QML's load state: the
    // share sheet's own call to us appears to give up if we don't answer
    // promptly when D-Bus activation had to cold-start the app, so the
    // page push is queued for setReady() rather than made to wait here.
    if (m_ready) {
        emit bookmarkShared(url, title);
    } else {
        m_pendingUrl = url;
        m_pendingTitle = title;
        m_hasPending = true;
    }
}

ShareAdaptor::ShareAdaptor(QObject *targetObject, ShareReceiver *receiver)
    : QDBusAbstractAdaptor(targetObject)
    , m_receiver(receiver)
{
}

void ShareAdaptor::share(const QVariantMap &args)
{
    m_receiver->handleShare(args);
}
