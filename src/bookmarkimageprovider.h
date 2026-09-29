#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QImage>

class NextcloudClient;
class QNetworkReply;

// Serves a bookmark's server-side preview image/favicon to QML as
// "image://bookmarkicon/<kind>/<bookmarkId>" (kind is "image" or
// "favicon"). Both are authenticated Nextcloud Bookmarks endpoints, so a
// plain QML Image can't load them directly -- this issues the request
// through NextcloudClient's own network manager (reusing its Basic Auth
// header) and hands the result back asynchronously, so a slow or
// unreachable server never blocks the UI thread.
class BookmarkImageResponse : public QQuickImageResponse
{
    Q_OBJECT

public:
    BookmarkImageResponse(NextcloudClient *client, const QString &id);

    QQuickTextureFactory *textureFactory() const override;
    QString errorString() const override;

private slots:
    void handleFinished();

private:
    QImage m_image;
    QString m_error;
    QNetworkReply *m_reply = nullptr;
};

class BookmarkImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit BookmarkImageProvider(NextcloudClient *client);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    NextcloudClient *m_client;
};
