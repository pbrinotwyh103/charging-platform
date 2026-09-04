#include "ui/usermainwindow.h"

#include "app/appinfo.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

UserMainWindow::UserMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电用户端"));
    resize(390, 780);
    setMinimumSize(360, 640);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("电动汽车充电"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *subtitle = new QLabel(QStringLiteral("用户手机客户端框架"), central);
    subtitle->setStyleSheet(QStringLiteral("color:#64748b;"));

    auto *form = new QFormLayout;
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_portSpin = new QSpinBox(central);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(Charging::AppInfo::DefaultServerPort);
    form->addRow(QStringLiteral("服务器"), m_hostEdit);
    form->addRow(QStringLiteral("端口"), m_portSpin);

    auto *connectButton = new QPushButton(QStringLiteral("连接服务器"), central);
    connect(connectButton, &QPushButton::clicked, this, [this] {
        emit connectionRequested(m_hostEdit->text().trimmed(),
                                 static_cast<quint16>(m_portSpin->value()));
    });

    m_statusLabel = new QLabel(QStringLiteral("尚未连接"), central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("padding:10px;background:#f1f5f9;color:#475569;border-radius:8px;"));

    auto *placeholder = new QLabel(
        QStringLiteral("页面模块占位\n\n登录  ·  附近电站  ·  导航\n预约  ·  充电  ·  订单  ·  我的"), central);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral("padding:32px;border:1px dashed #94a3b8;border-radius:12px;color:#475569;"));

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addLayout(form);
    layout->addWidget(connectButton);
    layout->addWidget(m_statusLabel);
    layout->addWidget(placeholder, 1);
    setCentralWidget(central);
}

void UserMainWindow::setConnectionStatus(const QString &text, bool connected)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(connected
        ? QStringLiteral("padding:10px;background:#dcfce7;color:#166534;border-radius:8px;")
        : QStringLiteral("padding:10px;background:#fef2f2;color:#991b1b;border-radius:8px;"));
}
