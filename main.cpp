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
    // КРИТИЧНО! Форсируем OpenGL через переменную окружения
    // Это работает даже если setGraphicsApi не срабатывает
    qputenv("QSG_RHI_BACKEND", "opengl");

    // КРИТИЧНО! Порядок
    // 1. Сначала устанавливаем Graphics API
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // 2. Потом создаём приложение
    QGuiApplication app(argc, argv);

    // 3. Потом стиль
    QQuickStyle::setStyle("Basic");

    app.setOrganizationName("MyDiplomWork");
    app.setApplicationName("VideoEditor Pro");

    // ── C++ объект таймлайна ──────────────────────────────────────────────
    Timeline timeline;

    // ── Image Provider для live-превью кадров ────────────────────────────
    // QQmlEngine ВЛАДЕЕТ провайдером (удаляет при деструкции engine).
    // Timeline хранит не-владеющий указатель.
    auto* frameProvider = new EffectImageProvider();
    timeline.setImageProvider(frameProvider);

    QQmlApplicationEngine engine;

    // Регистрация: в QML доступно "image://effects/..."
    engine.addImageProvider("effects", frameProvider);

    // Регистрация cppTimeline
    engine.rootContext()->setContextProperty("cppTimeline", &timeline);

    engine.rootContext()->setContextProperty("DEBUG_MODE",
#ifdef QT_DEBUG
    true
#else
    false
#endif
    );

    qDebug() << "✅ Timeline + EffectImageProvider зарегистрированы";

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
