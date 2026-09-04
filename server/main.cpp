#include "app/appinfo.h"
#include "app/serverapplication.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("charging_server"));
    application.setApplicationVersion(Charging::AppInfo::Version);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("电动汽车充电后台服务器"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{QStringLiteral("p"), QStringLiteral("port")},
                      QStringLiteral("监听端口"), QStringLiteral("port"),
                      QString::number(Charging::AppInfo::DefaultServerPort)});
    parser.addOption({{QStringLiteral("d"), QStringLiteral("database")},
                      QStringLiteral("SQLite数据库文件"), QStringLiteral("path"),
                      QStringLiteral("data/charging.db")});
    parser.process(application);

    bool portOk = false;
    const uint rawPort = parser.value(QStringLiteral("port")).toUInt(&portOk);
    if (!portOk || rawPort == 0 || rawPort > 65535) {
        qCritical() << "Invalid port:" << parser.value(QStringLiteral("port"));
        return 2;
    }

    ServerApplication server;
    QString error;
    if (!server.start(static_cast<quint16>(rawPort),
                      parser.value(QStringLiteral("database")), &error)) {
        qCritical().noquote() << QStringLiteral("服务器启动失败：%1").arg(error);
        return 1;
    }
    return application.exec();
}
