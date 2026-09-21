#include "nextcloudclient.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QCollator>
#include <algorithm>

namespace {
// Login Flow v2's token is valid for 20 minutes server-side; stop polling
// a little before that so we fail cleanly instead of hammering a dead
// token. Interval matches what the official clients use.
const int kPollIntervalMs = 2000;
const int kMaxPollAttempts = 560; // ~18.5 minutes at 2s
const char *kBookmarksApiBase = "/index.php/apps/bookmarks/public/rest/v2";
}

NextcloudClient::NextcloudClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &NextcloudClient::pollNow);
    restoreSession();
}

bool NextcloudClient::isLoggedIn() const { return !m_loginName.isEmpty() && !m_appPassword.isEmpty() && !m_serverUrl.isEmpty(); }
bool NextcloudClient::loginInProgress() const { return m_loginInProgress; }
bool NextcloudClient::busy() const { return m_busy; }
bool NextcloudClient::foldersLoading() const { return m_foldersLoading; }
QString NextcloudClient::serverUrl() const { return m_serverUrl; }
QString NextcloudClient::loginName() const { return m_loginName; }
QString NextcloudClient::lastError() const { return m_lastError; }
QVariantList NextcloudClient::folders() const { return m_folders; }
QVariantList NextcloudClient::bookmarks() const { return m_bookmarks; }
bool NextcloudClient::bookmarksLoading() const { return m_bookmarksLoading; }

void NextcloudClient::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void NextcloudClient::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

void NextcloudClient::setLoginInProgress(bool inProgress)
{
    if (m_loginInProgress == inProgress) {
        return;
    }
    m_loginInProgress = inProgress;
    emit loginInProgressChanged();
}

void NextcloudClient::restoreSession()
{
    QSettings settings;
    m_serverUrl = settings.value(QStringLiteral("serverUrl")).toString();
    m_loginName = settings.value(QStringLiteral("loginName")).toString();
    m_appPassword = settings.value(QStringLiteral("appPassword")).toString();
}

void NextcloudClient::persistSession()
{
    QSettings settings;
    settings.setValue(QStringLiteral("serverUrl"), m_serverUrl);
    settings.setValue(QStringLiteral("loginName"), m_loginName);
    settings.setValue(QStringLiteral("appPassword"), m_appPassword);
}

QString NextcloudClient::normalizeServerUrl(const QString &input) const
{
    QString url = input.trimmed();
    if (!url.startsWith(QStringLiteral("http://")) && !url.startsWith(QStringLiteral("https://"))) {
        url.prepend(QStringLiteral("https://"));
    }
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    return url;
}

QNetworkRequest NextcloudClient::buildRequest(const QString &path, const QUrlQuery &query) const
{
    QUrl url(m_serverUrl + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    const QByteArray credentials = (m_loginName + QStringLiteral(":") + m_appPassword).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + credentials);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return request;
}

QNetworkRequest NextcloudClient::imageRequest(int bookmarkId, const QString &kind) const
{
    return buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/bookmark/")
                         + QString::number(bookmarkId) + QStringLiteral("/") + kind);
}

QString NextcloudClient::extractErrorMessage(QNetworkReply *reply, const QByteArray &body) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("message")).isString()) {
            return obj.value(QStringLiteral("message")).toString();
        }
        if (obj.value(QStringLiteral("data")).isString()) {
            return obj.value(QStringLiteral("data")).toString();
        }
    }
    return reply->errorString();
}

void NextcloudClient::startLogin(const QString &serverUrl)
{
    const QString normalized = normalizeServerUrl(serverUrl);
    if (normalized.isEmpty()) {
        emit loginFailed(tr("Please enter a server address"));
        return;
    }

    setBusy(true);
    setLastError(QString());

    QNetworkRequest request(QUrl(normalized + QStringLiteral("/index.php/login/v2")));
    // Shown on Nextcloud's "grant access" page as the requesting client.
    request.setRawHeader("User-Agent", "harbour-nextmarks");
    QNetworkReply *reply = m_manager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply, normalized]() {
        const QByteArray raw = reply->readAll();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(extractErrorMessage(reply, raw));
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        const QJsonObject obj = doc.object();
        const QJsonObject poll = obj.value(QStringLiteral("poll")).toObject();
        const QString token = poll.value(QStringLiteral("token")).toString();
        const QString pollEndpoint = poll.value(QStringLiteral("endpoint")).toString();
        const QString loginUrl = obj.value(QStringLiteral("login")).toString();

        if (token.isEmpty() || pollEndpoint.isEmpty() || loginUrl.isEmpty()) {
            emit loginFailed(tr("Unexpected response from server -- is this a Nextcloud server?"));
            reply->deleteLater();
            return;
        }

        m_pendingServerUrl = normalized;
        m_pollToken = token;
        m_pollEndpoint = pollEndpoint;
        m_pollAttempts = 0;
        setLoginInProgress(true);

        emit loginUrlReady(loginUrl);
        m_pollTimer.start();

        reply->deleteLater();
    });
}

void NextcloudClient::cancelLogin()
{
    m_pollTimer.stop();
    m_pollToken.clear();
    m_pollEndpoint.clear();
    m_pendingServerUrl.clear();
    setLoginInProgress(false);
}

void NextcloudClient::logout()
{
    m_serverUrl.clear();
    m_loginName.clear();
    m_appPassword.clear();
    m_folders.clear();
    QSettings settings;
    settings.remove(QStringLiteral("serverUrl"));
    settings.remove(QStringLiteral("loginName"));
    settings.remove(QStringLiteral("appPassword"));
    emit isLoggedInChanged();
    emit serverUrlChanged();
    emit loginNameChanged();
    emit foldersChanged();
}

void NextcloudClient::pollNow()
{
    if (!m_loginInProgress || m_pollEndpoint.isEmpty()) {
        return;
    }

    m_pollAttempts++;
    if (m_pollAttempts > kMaxPollAttempts) {
        cancelLogin();
        emit loginFailed(tr("Login timed out. Please try again."));
        return;
    }

    QNetworkRequest request((QUrl(m_pollEndpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("token"), m_pollToken);
    const QByteArray data = body.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkReply *reply = m_manager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // 404 just means "not confirmed yet" -- keep polling silently.
        // Anything else transient (timeout, connection reset while the
        // user is still busy in the browser) is tolerated the same way;
        // only a clean 200 or running out of attempts ends the wait.
        if (status == 200 && reply->error() == QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            const QJsonObject obj = doc.object();
            const QString server = obj.value(QStringLiteral("server")).toString();
            const QString loginName = obj.value(QStringLiteral("loginName")).toString();
            const QString appPassword = obj.value(QStringLiteral("appPassword")).toString();

            if (server.isEmpty() || loginName.isEmpty() || appPassword.isEmpty()) {
                cancelLogin();
                emit loginFailed(tr("Unexpected response while completing login."));
                reply->deleteLater();
                return;
            }

            m_serverUrl = server;
            while (m_serverUrl.endsWith(QLatin1Char('/'))) {
                m_serverUrl.chop(1);
            }
            m_loginName = loginName;
            m_appPassword = appPassword;
            persistSession();

            m_pollTimer.stop();
            m_pollToken.clear();
            m_pollEndpoint.clear();
            setLoginInProgress(false);

            emit serverUrlChanged();
            emit loginNameChanged();
            emit isLoggedInChanged();

            loadFolders();
        }
        reply->deleteLater();
    });
}

void NextcloudClient::flattenFolders(const QVariantList &nodes, int depth, QVariantList &out) const
{
    // Depth-first pre-order, same as a nested tree would print top to
    // bottom -- QML's FolderPickerPage relies on this exact ordering to
    // collapse a subtree with a single "skip everything deeper until we
    // see this depth again" pass, rather than needing a parent-id map.
    //
    // Each level's siblings are alphabetized before recursing -- the
    // server returns folders in whatever order they were created, not
    // sorted, and a tree that shuffles every time you glance at it isn't
    // browsable. QCollator instead of a plain string compare so it sorts
    // the way a person would expect (case-insensitive, locale-aware).
    QVariantList sorted = nodes;
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(sorted.begin(), sorted.end(), [&collator](const QVariant &a, const QVariant &b) {
        return collator.compare(a.toMap().value(QStringLiteral("title")).toString(),
                                 b.toMap().value(QStringLiteral("title")).toString()) < 0;
    });

    for (const QVariant &nodeVariant : sorted) {
        const QVariantMap node = nodeVariant.toMap();
        const QVariantList children = node.value(QStringLiteral("children")).toList();

        QVariantMap item;
        item[QStringLiteral("id")] = node.value(QStringLiteral("id"));
        item[QStringLiteral("title")] = node.value(QStringLiteral("title"));
        item[QStringLiteral("depth")] = depth;
        item[QStringLiteral("hasChildren")] = !children.isEmpty();
        out << item;

        if (!children.isEmpty()) {
            flattenFolders(children, depth + 1, out);
        }
    }
}

void NextcloudClient::loadFolders()
{
    if (!isLoggedIn()) {
        return;
    }
    m_foldersLoading = true;
    emit foldersLoadingChanged();

    QNetworkReply *reply = m_manager->get(buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/folder")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        m_foldersLoading = false;
        emit foldersLoadingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            emit foldersFailed(extractErrorMessage(reply, raw));
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();

        QVariantList flat;
        // A synthetic entry for the root folder (id -1 is the reserved
        // root id for every Nextcloud Bookmarks account) so it can be
        // picked directly, the same as any other folder.
        QVariantMap rootItem;
        rootItem[QStringLiteral("id")] = -1;
        rootItem[QStringLiteral("title")] = tr("(No folder)");
        rootItem[QStringLiteral("depth")] = 0;
        rootItem[QStringLiteral("hasChildren")] = false;
        flat << rootItem;

        flattenFolders(data.toVariantList(), 0, flat);
        m_folders = flat;
        emit foldersChanged();
        reply->deleteLater();
    });
}

void NextcloudClient::saveBookmark(const QString &url, const QString &title, int folderId)
{
    setBusy(true);
    setLastError(QString());

    QJsonObject body;
    body[QStringLiteral("url")] = url;
    if (!title.isEmpty()) {
        body[QStringLiteral("title")] = title;
    }
    QJsonArray folders;
    folders.append(folderId);
    body[QStringLiteral("folders")] = folders;

    QNetworkReply *reply = m_manager->post(buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/bookmark")),
                                            QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        setBusy(false);
        if (reply->error() == QNetworkReply::NoError) {
            emit bookmarkSaved();
        } else {
            const QString message = extractErrorMessage(reply, raw);
            setLastError(message);
            emit bookmarkSaveFailed(message);
        }
        reply->deleteLater();
    });
}

void NextcloudClient::createFolder(const QString &title, int parentFolderId)
{
    setBusy(true);
    setLastError(QString());

    QJsonObject body;
    body[QStringLiteral("title")] = title;
    body[QStringLiteral("parent_folder")] = parentFolderId;

    QNetworkReply *reply = m_manager->post(buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/folder")),
                                            QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        setBusy(false);
        if (reply->error() == QNetworkReply::NoError) {
            // Refresh before telling QML it's done, so the folder tree
            // it's bound to already contains the new folder by the time
            // any success handler (e.g. closing a "new folder" dialog)
            // runs.
            loadFolders();
            emit folderCreated();
        } else {
            const QString message = extractErrorMessage(reply, raw);
            setLastError(message);
            emit folderCreateFailed(message);
        }
        reply->deleteLater();
    });
}

void NextcloudClient::deleteFolder(int folderId)
{
    setBusy(true);
    setLastError(QString());

    QNetworkReply *reply = m_manager->deleteResource(
        buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/folder/") + QString::number(folderId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        setBusy(false);
        if (reply->error() == QNetworkReply::NoError) {
            loadFolders();
            emit folderDeleted();
        } else {
            const QString message = extractErrorMessage(reply, raw);
            setLastError(message);
            emit folderDeleteFailed(message);
        }
        reply->deleteLater();
    });
}

void NextcloudClient::loadBookmarksInFolder(int folderId)
{
    if (!isLoggedIn()) {
        return;
    }
    m_bookmarksLoading = true;
    emit bookmarksLoadingChanged();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("folder"), QString::number(folderId));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("500"));

    QNetworkReply *reply = m_manager->get(buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/bookmark"), query));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        m_bookmarksLoading = false;
        emit bookmarksLoadingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            emit bookmarksFailed(extractErrorMessage(reply, raw));
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
        QVariantList items;
        for (const QJsonValue &v : data) {
            const QJsonObject obj = v.toObject();
            const QString url = obj.value(QStringLiteral("url")).toString();
            const QString title = obj.value(QStringLiteral("title")).toString();
            QVariantMap item;
            item[QStringLiteral("id")] = obj.value(QStringLiteral("id")).toInt();
            item[QStringLiteral("url")] = url;
            item[QStringLiteral("title")] = title.isEmpty() ? url : title;
            items << item;
        }
        m_bookmarks = items;
        emit bookmarksChanged();
        reply->deleteLater();
    });
}

void NextcloudClient::deleteBookmark(int bookmarkId)
{
    setBusy(true);
    setLastError(QString());

    QNetworkReply *reply = m_manager->deleteResource(
        buildRequest(QLatin1String(kBookmarksApiBase) + QStringLiteral("/bookmark/") + QString::number(bookmarkId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        setBusy(false);
        if (reply->error() == QNetworkReply::NoError) {
            emit bookmarkDeleted();
        } else {
            const QString message = extractErrorMessage(reply, raw);
            setLastError(message);
            emit bookmarkDeleteFailed(message);
        }
        reply->deleteLater();
    });
}
