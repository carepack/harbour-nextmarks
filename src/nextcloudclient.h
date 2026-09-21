#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>

// Talks to a Nextcloud server: authenticates via Nextcloud's "Login Flow v2"
// (the same browser-based flow the official Nextcloud apps use -- see
// https://docs.nextcloud.com/server/latest/developer_manual/client_apis/LoginFlow/index.html),
// then uses the resulting app password to call the Nextcloud Bookmarks
// app's REST API (https://nextcloud-bookmarks.readthedocs.io/) to list
// folders and create bookmarks. There is no local bookmark store and no
// sync logic -- every save is a single POST to the server.
class NextcloudClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY isLoggedInChanged)
    Q_PROPERTY(bool loginInProgress READ loginInProgress NOTIFY loginInProgressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool foldersLoading READ foldersLoading NOTIFY foldersLoadingChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString loginName READ loginName NOTIFY loginNameChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(bool bookmarksLoading READ bookmarksLoading NOTIFY bookmarksLoadingChanged)

public:
    explicit NextcloudClient(QObject *parent = nullptr);

    bool isLoggedIn() const;
    bool loginInProgress() const;
    bool busy() const;
    bool foldersLoading() const;
    QString serverUrl() const;
    QString loginName() const;
    QString lastError() const;
    QVariantList folders() const;
    QVariantList bookmarks() const;
    bool bookmarksLoading() const;

    // Starts Login Flow v2 against serverUrl (scheme defaulted to https if
    // omitted) and begins polling for completion once the server
    // responds. Emits loginUrlReady(url) for QML to open (Qt.openUrlExternally --
    // not done here in C++ via QDesktopServices, whose sailjail behavior
    // isn't reliably documented) and loginFailed() for an unreachable/
    // invalid server.
    Q_INVOKABLE void startLogin(const QString &serverUrl);
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logout();

    // If a login is in progress, polls once immediately. The poll QTimer
    // can miss ticks while the app is backgrounded (the user is off in
    // the system browser signing in); call this from QML on every
    // foreground transition so a completed login is picked up without
    // waiting for the next scheduled tick.
    Q_INVOKABLE void pollNow();

    Q_INVOKABLE void loadFolders();
    Q_INVOKABLE void saveBookmark(const QString &url, const QString &title, int folderId);

    // parentFolderId -1 creates a top-level folder, same reserved root
    // id used everywhere else in this class. Both reload the folder list
    // on success (so the tree QML is bound to is immediately correct)
    // before emitting folderCreated()/folderDeleted() -- callers don't
    // need to separately call loadFolders() themselves.
    Q_INVOKABLE void createFolder(const QString &title, int parentFolderId);
    Q_INVOKABLE void deleteFolder(int folderId);

    // Lists the bookmarks directly inside one folder (not recursive --
    // matches how the Nextcloud Bookmarks web UI itself scopes a folder
    // view). folderId -1 is the reserved root folder id, valid the same
    // as any other. Populates the bookmarks property; errors surface via
    // bookmarksFailed rather than lastError since this can run alongside
    // other requests (e.g. the app's own save-bookmark flow) that have
    // their own error handling.
    Q_INVOKABLE void loadBookmarksInFolder(int folderId);

    // Deletes a bookmark entirely (from every folder it's in, not just
    // the one it's currently being viewed from) -- the Bookmarks REST API
    // has no separate "remove from this one folder" endpoint documented,
    // and nothing in this app distinguishes the two, so this matches
    // what "delete" means everywhere else in it (folders, etc.).
    Q_INVOKABLE void deleteBookmark(int bookmarkId);

    // Used by BookmarkImageProvider to fetch a bookmark's preview image
    // or favicon -- both are authenticated endpoints (same Basic Auth as
    // every other call here), so QML's plain Image element can't load
    // them directly; the provider issues the request through this
    // shared manager instead. kind is "image" or "favicon".
    QNetworkRequest imageRequest(int bookmarkId, const QString &kind) const;
    QNetworkAccessManager *networkManager() const { return m_manager; }

signals:
    void isLoggedInChanged();
    void loginInProgressChanged();
    void busyChanged();
    void foldersLoadingChanged();
    void serverUrlChanged();
    void loginNameChanged();
    void lastErrorChanged();
    void foldersChanged();
    void bookmarksChanged();
    void bookmarksLoadingChanged();

    void loginUrlReady(const QString &url);
    void loginFailed(const QString &message);
    void bookmarkSaved();
    void bookmarkSaveFailed(const QString &message);
    void foldersFailed(const QString &message);
    void bookmarksFailed(const QString &message);
    void folderCreated();
    void folderCreateFailed(const QString &message);
    void folderDeleted();
    void folderDeleteFailed(const QString &message);
    void bookmarkDeleted();
    void bookmarkDeleteFailed(const QString &message);

private:
    void setBusy(bool busy);
    void setLastError(const QString &error);
    void setLoginInProgress(bool inProgress);
    void restoreSession();
    void persistSession();
    QString normalizeServerUrl(const QString &input) const;
    QNetworkRequest buildRequest(const QString &path, const QUrlQuery &query = QUrlQuery()) const;
    QString extractErrorMessage(QNetworkReply *reply, const QByteArray &body) const;
    void flattenFolders(const QVariantList &nodes, int depth, QVariantList &out) const;

    QNetworkAccessManager *m_manager;
    QTimer m_pollTimer;

    QString m_serverUrl;
    QString m_loginName;
    QString m_appPassword;
    QString m_lastError;
    QVariantList m_folders;
    QVariantList m_bookmarks;
    bool m_busy = false;
    bool m_foldersLoading = false;
    bool m_bookmarksLoading = false;
    bool m_loginInProgress = false;

    // Pending Login Flow v2 state, valid only while m_loginInProgress.
    QString m_pendingServerUrl;
    QString m_pollToken;
    QString m_pollEndpoint;
    int m_pollAttempts = 0;
};
