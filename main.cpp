#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QQuickStyle>
#include <QQuickWindow>
#include "timeline.h"
#include "EffectImageProvider.h"

int main(int argc, char *argv[])
{
    // Force the OpenGL backend via environment variable. This takes
    // effect even in cases where setGraphicsApi() alone doesn't stick.
    qputenv("QSG_RHI_BACKEND", "opengl");

    // Order matters here:
    // 1. Set the graphics API before the application object exists.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // 2. Create the application.
    QGuiApplication app(argc, argv);

    // 3. Apply the QML style.
    QQuickStyle::setStyle("Basic");

    app.setOrganizationName("MyDiplomWork");
    app.setApplicationName("VideoEditor Pro");

    // ── C++ timeline object ────────────────────────────────────────────
    Timeline timeline;

    // ── Image provider for live preview frames ───────────────────────
    // The QQmlEngine takes ownership of the provider and deletes it when
    // the engine is destroyed. Timeline only keeps a non-owning pointer.
    auto* frameProvider = new EffectImageProvider();
    timeline.setImageProvider(frameProvider);

    QQmlApplicationEngine engine;

    // Registers the provider so QML can use "image://effects/...".
    engine.addImageProvider("effects", frameProvider);

    // Expose the Timeline instance to QML as "cppTimeline".
    engine.rootContext()->setContextProperty("cppTimeline", &timeline);

    engine.rootContext()->setContextProperty("DEBUG_MODE",
#ifdef QT_DEBUG
    true
#else
    false
#endif
    );

    qDebug() << "Timeline + EffectImageProvider зарегистрированы";

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    qDebug() << "Приложение запущено!";
    return app.exec();
}
