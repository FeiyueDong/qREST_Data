#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include "field_help_registry.h"
#include "qrest_view_model.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("qrest_data_tools_gui");
    QCoreApplication::setApplicationVersion("1.0.1");
    QCoreApplication::setOrganizationName("qREST");
    QGuiApplication::setApplicationDisplayName("qREST Data Tools");
    app.setWindowIcon(QIcon(":/qt/qml/qrest_data_tools_gui/icon/logo.png"));

    // 参数：(包名, 主版本, 次版本, 在 QML 中使用的类型名)
    qmlRegisterType<QrestViewModel>(
        "DataTools.Backend", 1, 0, "QrestViewModel");
    qmlRegisterType<FieldHelpRegistry>(
        "DataTools.Backend", 1, 0, "FieldHelpRegistry");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral(
        "qrc:/qt/qml/qrest_data_tools_gui/qml/main.qml")); // 从 Qt
                                                           // 资源文件中加载 QML

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
