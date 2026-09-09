#include "app/appinfo.h"
#include "app/serverapplication.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

namespace {
QString defaultDatabasePath()
{
    // The imported demo database is kept at the project root. Resolve it
    // from both the project root and the usual build directory so the
    // client's default Shenzhen location has nearby stations available.
    const QStringList candidates{
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../database/charging.db")),
        QStringLiteral("database/charging.db"),
        QStringLiteral("../database/charging.db"),
        QStringLiteral("../../database/charging.db")};
    for (const auto &candidate : candidates)
        if (QFileInfo::exists(candidate)) return candidate;
    return QStringLiteral("data/charging.db");
}
}

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
                      defaultDatabasePath()});
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
