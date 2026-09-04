#include "ui/adminmainwindow.h"

#include "app/appinfo.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

AdminMainWindow::AdminMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电管理端"));
    resize(390, 780);
    setMinimumSize(360, 640);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("充电运营管理"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *subtitle = new QLabel(QStringLiteral("管理员手机客户端框架"), central);
    subtitle->setStyleSheet(QStringLiteral("color:#64748b;"));

    auto *form = new QFormLayout;
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_portSpin = new QSpinBox(central);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(Charging::AppInfo::DefaultServerPort);
    form->addRow(QStringLiteral("服务器"), m_hostEdit);
    form->addRow(QStringLiteral("端口"), m_portSpin);

    auto *connectButton = new QPushButton(QStringLiteral("连接后台服务器"), central);
    connect(connectButton, &QPushButton::clicked, this, [this] {
        emit connectionRequested(m_hostEdit->text().trimmed(),
                                 static_cast<quint16>(m_portSpin->value()));
    });

    m_statusLabel = new QLabel(QStringLiteral("尚未连接"), central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("padding:10px;background:#f1f5f9;color:#475569;border-radius:8px;"));

    auto *moduleGrid = new QGridLayout;
    const QStringList modules = {
        QStringLiteral("实时监控"), QStringLiteral("营收统计"),
        QStringLiteral("电桩管理"), QStringLiteral("电站管理"),
        QStringLiteral("用户管理"), QStringLiteral("告警中心")
    };
    for (int i = 0; i < modules.size(); ++i) {
        auto *card = new QLabel(modules.at(i), central);
        card->setAlignment(Qt::AlignCenter);
        card->setMinimumHeight(76);
        card->setStyleSheet(QStringLiteral("background:#eff6ff;border:1px solid #bfdbfe;border-radius:10px;color:#1e3a8a;font-weight:600;"));
        moduleGrid->addWidget(card, i / 2, i % 2);
    }

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addLayout(form);
    layout->addWidget(connectButton);
    layout->addWidget(m_statusLabel);
    layout->addLayout(moduleGrid);
    layout->addStretch();
    setCentralWidget(central);
}

void AdminMainWindow::setConnectionStatus(const QString &text, bool connected)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(connected
        ? QStringLiteral("padding:10px;background:#dcfce7;color:#166534;border-radius:8px;")
        : QStringLiteral("padding:10px;background:#fef2f2;color:#991b1b;border-radius:8px;"));
}
