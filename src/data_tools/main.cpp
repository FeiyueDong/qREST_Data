#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "qrest_view_model.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // 参数：(包名, 主版本, 次版本, 在 QML 中使用的类型名)
    qmlRegisterType<QrestViewModel>(
        "DataTools.Backend", 1, 0, "QrestViewModel");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral(
        "qrc:/qt/qml/data_tools/main.qml")); // 从 Qt 资源文件中加载 QML

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
