#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QQuickStyle>
#include <QQuickWindow>

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

    app.setOrganizationName("VideoEditor");
    app.setApplicationName("VideoEditor Pro");

    QQmlApplicationEngine engine;

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
