#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTranslator>
#include <QLocale>

#include "nextcloudclient.h"
#include "sharereceiver.h"
#include "bookmarkimageprovider.h"

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);

    // Loads translations/harbour-nextmarks-<locale>.qm matching the
    // system locale (falling back through e.g. "de_DE" -> "de" -> none,
    // in which case qsTr() calls just show their English source text).
    // Without this, only Silica's own stock UI strings follow the
    // system locale while this app's own text stays English regardless
    // -- an inconsistent mixed-language result, not a missing-file one.
    QTranslator translator;
    if (translator.load(QLocale::system(), QStringLiteral("harbour-nextmarks"), QStringLiteral("-"),
                         SailfishApp::pathTo(QStringLiteral("translations")).toLocalFile())) {
        app->installTranslator(&translator);
    }

    QQuickView *view = SailfishApp::createView();

    NextcloudClient nextcloudClient;
    view->rootContext()->setContextProperty("nextcloudClient", &nextcloudClient);

    // image://bookmarkicon/<image|favicon>/<bookmarkId> -- see
    // BookmarkImageProvider for why this can't just be a plain QML
    // Image pointed at the server URL. The engine takes ownership.
    view->engine()->addImageProvider(QStringLiteral("bookmarkicon"), new BookmarkImageProvider(&nextcloudClient));

    ShareReceiver shareReceiver;
    view->rootContext()->setContextProperty("shareReceiver", &shareReceiver);

    // As early as possible, before QML is parsed -- see ShareReceiver's
    // own comments on why registration timing matters for a D-Bus
    // activated cold start.
    shareReceiver.registerService();

    view->setSource(SailfishApp::pathTo("qml/harbour-nextmarks.qml"));
    view->show();

    // QML's Connections to shareReceiver now exist; flush any share that
    // arrived while the window was still being built.
    shareReceiver.setReady();

    return app->exec();
}
