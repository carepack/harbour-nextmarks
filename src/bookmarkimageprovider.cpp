#include "bookmarkimageprovider.h"
#include "nextcloudclient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQuickTextureFactory>

BookmarkImageResponse::BookmarkImageResponse(NextcloudClient *client, const QString &id)
{
    // id is "<kind>/<bookmarkId>", e.g. "image/42" or "favicon/42" --
    // see BookmarkImageProvider::requestImageResponse for how it's built.
    const int slash = id.indexOf(QLatin1Char('/'));
    const QString kind = slash >= 0 ? id.left(slash) : id;
    const int bookmarkId = slash >= 0 ? id.mid(slash + 1).toInt() : 0;

    m_reply = client->networkManager()->get(client->imageRequest(bookmarkId, kind));
    connect(m_reply, &QNetworkReply::finished, this, &BookmarkImageResponse::handleFinished);
}

void BookmarkImageResponse::handleFinished()
{
    const QByteArray raw = m_reply->readAll();
    const bool ok = m_reply->error() == QNetworkReply::NoError && m_image.loadFromData(raw);
    if (!ok) {
        // No image captured for this bookmark, or the request failed --
        // both are routine (not every bookmark has a preview/favicon),
        // so this is reported as a plain error, not logged as a warning.
        m_error = m_reply->errorString();
        m_image = QImage();
    }
    m_reply->deleteLater();
    m_reply = nullptr;
    emit finished();
}

QQuickTextureFactory *BookmarkImageResponse::textureFactory() const
{
    return m_image.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(m_image);
}

QString BookmarkImageResponse::errorString() const
{
    return m_error;
}

BookmarkImageProvider::BookmarkImageProvider(NextcloudClient *client)
    : QQuickAsyncImageProvider()
    , m_client(client)
{
}

QQuickImageResponse *BookmarkImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    return new BookmarkImageResponse(m_client, id);
}
