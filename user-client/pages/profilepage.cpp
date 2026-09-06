#include "pages/profilepage.h"

#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QImageReader>
#include <QIntValidator>
#include <QVBoxLayout>
#include <QPixmap>

ProfilePage::ProfilePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("我的"), this);

    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_avatarLabel = new QLabel(QStringLiteral("用户"), this);
    m_avatarLabel->setFixedSize(72, 72);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(
        QStringLiteral(
            "background:#cbd5e1;"
            "color:#475569;"
            "border-radius:36px;"
            "font-weight:700;"));

    m_nicknameLabel = new QLabel(this);
    m_phoneLabel = new QLabel(this);
    m_balanceLabel = new QLabel(this);
    m_accountNoteLabel = new QLabel(this);

    m_balanceLabel->setStyleSheet(
        QStringLiteral(
            "padding:16px;"
            "background:#eff6ff;"
            "color:#1e3a8a;"
            "border-radius:10px;"
            "font-size:16px;"));

    m_accountNoteLabel->setWordWrap(true);

    m_logoutButton = new QPushButton(QStringLiteral("退出登录"), this);
    m_nicknameEdit = new QLineEdit(this); m_nicknameEdit->setPlaceholderText(QStringLiteral("修改昵称（2-20个字符）"));
    auto *saveName = new QPushButton(QStringLiteral("保存昵称"), this); auto *avatar = new QPushButton(QStringLiteral("选择头像"), this); auto *recharge = new QPushButton(QStringLiteral("模拟充值 10 元"), this);

    connect(
        m_logoutButton,
        &QPushButton::clicked,
        this,
        &ProfilePage::logoutRequested);
    connect(saveName,&QPushButton::clicked,this,[this]{const QString n=m_nicknameEdit->text().trimmed();if(n.size()<2||n.size()>20){m_accountNoteLabel->setText(QStringLiteral("昵称长度需为 2-20 个字符"));return;}emit nicknameUpdateRequested(n);});
    connect(avatar,&QPushButton::clicked,this,[this]{const QString p=QFileDialog::getOpenFileName(this,QStringLiteral("选择头像"),{},QStringLiteral("图片 (*.png *.jpg *.jpeg)"));if(p.isEmpty())return;QImageReader r(p);if(!r.canRead()||!QStringList({"png","jpg","jpeg"}).contains(r.format().toLower())){m_accountNoteLabel->setText(QStringLiteral("头像必须是 PNG 或 JPEG 图片"));return;}if(r.size().width()>2048||r.size().height()>2048){m_accountNoteLabel->setText(QStringLiteral("头像尺寸不能超过 2048×2048"));return;}QImage image=r.read();if(image.isNull()){m_accountNoteLabel->setText(QStringLiteral("头像读取失败"));return;}const QImage preview=image.scaled(512,512,Qt::KeepAspectRatio,Qt::SmoothTransformation);m_avatarLabel->setPixmap(QPixmap::fromImage(preview));m_avatarLabel->setScaledContents(true);emit avatarUpdateRequested(p);m_accountNoteLabel->setText(QStringLiteral("头像已校验并生成预览，等待上传接口联调"));});
    connect(recharge,&QPushButton::clicked,this,[this]{emit rechargeRequested(1000);m_accountNoteLabel->setText(QStringLiteral("充值请求已记录，等待服务端确认"));});

    layout->addWidget(titleLabel);
    layout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_nicknameLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_phoneLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_balanceLabel);
    layout->addWidget(m_accountNoteLabel);
    layout->addWidget(m_nicknameEdit); layout->addWidget(saveName); layout->addWidget(avatar); layout->addWidget(recharge);

    layout->addStretch();

    layout->addWidget(m_logoutButton);
}

void ProfilePage::setProfile(const QJsonObject &profile)
{
    m_nicknameLabel->setText(
        profile.value(QStringLiteral("nickname")).toString());

    m_phoneLabel->setText(
        profile.value(QStringLiteral("phone")).toString());

    const qint64 cents = static_cast<qint64>(
        profile.value(QStringLiteral("balanceCents")).toDouble());

    m_balanceLabel->setText(
        QStringLiteral("钱包余额：¥ %1")
            .arg(cents / 100.0, 0, 'f', 2));

    m_accountNoteLabel->setText(
        profile.value(QStringLiteral("created")).toBool()
            ? QStringLiteral("首次登录成功，系统已自动创建账号。")
            : QStringLiteral("用户资料已从服务端同步。"));
}
