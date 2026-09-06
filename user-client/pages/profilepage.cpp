#include "pages/profilepage.h"

#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

ProfilePage::ProfilePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("profilePage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("我的账户"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    m_avatarLabel = new QLabel(QStringLiteral("用户"), this);
    m_avatarLabel->setFixedSize(68, 68);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(QStringLiteral(
        "background:#cbd5e1;color:#475569;border-radius:34px;font-weight:700;"));
    m_nicknameLabel = new QLabel(this);
    m_nicknameLabel->setObjectName(QStringLiteral("profileNickname"));
    m_phoneLabel = new QLabel(this);
    m_balanceLabel = new QLabel(this);
    m_balanceLabel->setObjectName(QStringLiteral("profileBalance"));
    m_balanceLabel->setStyleSheet(QStringLiteral(
        "padding:14px;background:#eff6ff;color:#1e3a8a;border-radius:10px;font-size:16px;font-weight:600;"));
    m_accountNoteLabel = new QLabel(this);
    m_accountNoteLabel->setWordWrap(true);
    m_accountNoteLabel->setStyleSheet(QStringLiteral("color:#475569;"));

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("profileTabs"));
    auto *accountTab = new QWidget(tabs);
    auto *accountLayout = new QVBoxLayout(accountTab);
    m_nicknameEdit = new QLineEdit(accountTab);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("修改昵称（2-20个字符）"));
    auto *saveName = new QPushButton(QStringLiteral("保存昵称"), accountTab);
    auto *avatar = new QPushButton(QStringLiteral("更换头像"), accountTab);
    accountLayout->addWidget(m_nicknameEdit);
    accountLayout->addWidget(saveName);
    accountLayout->addWidget(avatar);
    accountLayout->addStretch();

    auto *walletTab = new QWidget(tabs);
    auto *walletLayout = new QVBoxLayout(walletTab);
    auto *rechargeRow = new QHBoxLayout;
    m_rechargeAmount = new QDoubleSpinBox(walletTab);
    m_rechargeAmount->setObjectName(QStringLiteral("rechargeAmount"));
    m_rechargeAmount->setRange(1.0, 5000.0);
    m_rechargeAmount->setValue(50.0);
    m_rechargeAmount->setPrefix(QStringLiteral("¥ "));
    auto *recharge = new QPushButton(QStringLiteral("模拟充值"), walletTab);
    recharge->setObjectName(QStringLiteral("rechargeButton"));
    rechargeRow->addWidget(m_rechargeAmount, 1);
    rechargeRow->addWidget(recharge);
    auto *ledgerTitle = new QLabel(QStringLiteral("充值记录（按时间倒序）"), walletTab);
    m_ledgerList = new QListWidget(walletTab);
    m_ledgerList->setObjectName(QStringLiteral("walletLedgerList"));
    walletLayout->addLayout(rechargeRow);
    walletLayout->addWidget(ledgerTitle);
    walletLayout->addWidget(m_ledgerList, 1);

    auto *favoriteTab = new QWidget(tabs);
    auto *favoriteLayout = new QVBoxLayout(favoriteTab);
    m_favoriteList = new QListWidget(favoriteTab);
    m_favoriteList->setObjectName(QStringLiteral("favoriteStationList"));
    favoriteLayout->addWidget(new QLabel(QStringLiteral("常用充电站"), favoriteTab));
    favoriteLayout->addWidget(m_favoriteList, 1);
    tabs->addTab(accountTab, QStringLiteral("资料"));
    tabs->addTab(walletTab, QStringLiteral("钱包"));
    tabs->addTab(favoriteTab, QStringLiteral("收藏"));

    m_logoutButton = new QPushButton(QStringLiteral("退出登录"), this);
    connect(m_logoutButton, &QPushButton::clicked, this, &ProfilePage::logoutRequested);
    connect(saveName, &QPushButton::clicked, this, [this] {
        const QString nickname = m_nicknameEdit->text().trimmed();
        if (nickname.size() < 2 || nickname.size() > 20) {
            m_accountNoteLabel->setText(QStringLiteral("昵称长度需为2-20个字符。"));
            return;
        }
        if (m_demoMode) {
            m_nicknameLabel->setText(nickname);
            m_accountNoteLabel->setText(QStringLiteral("昵称已保存（演示模式）。"));
        }
        emit nicknameUpdateRequested(nickname);
    });
    connect(avatar, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择头像"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
        if (path.isEmpty()) return;
        QImageReader reader(path);
        const QString format = QString::fromLatin1(reader.format()).toLower();
        if (!reader.canRead() || !QStringList({QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg")}).contains(format)) {
            m_accountNoteLabel->setText(QStringLiteral("头像必须是有效的PNG或JPEG图片。"));
            return;
        }
        if (reader.size().width() > 2048 || reader.size().height() > 2048) {
            m_accountNoteLabel->setText(QStringLiteral("头像尺寸不能超过2048×2048。"));
            return;
        }
        const QImage image = reader.read();
        if (image.isNull()) {
            m_accountNoteLabel->setText(QStringLiteral("头像读取失败。"));
            return;
        }
        m_avatarLabel->setPixmap(QPixmap::fromImage(
            image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        m_avatarLabel->setScaledContents(true);
        emit avatarUpdateRequested(path);
        m_accountNoteLabel->setText(m_demoMode
            ? QStringLiteral("头像已更新（演示模式）。")
            : QStringLiteral("头像已校验，正在等待服务端保存。"));
    });
    connect(recharge, &QPushButton::clicked, this, [this] {
        const qint64 cents = qRound64(m_rechargeAmount->value() * 100.0);
        if (m_demoMode) {
            m_balanceCents += cents;
            updateBalance();
            m_ledgerList->insertItem(0,
                QStringLiteral("%1  +¥%2  成功\n充值后余额 ¥%3")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm")))
                    .arg(cents / 100.0, 0, 'f', 2)
                    .arg(m_balanceCents / 100.0, 0, 'f', 2));
            m_accountNoteLabel->setText(QStringLiteral("模拟支付成功，钱包余额已实时更新。"));
        }
        emit rechargeRequested(cents);
    });

    layout->addWidget(titleLabel);
    layout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_nicknameLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_phoneLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_balanceLabel);
    layout->addWidget(m_accountNoteLabel);
    layout->addWidget(tabs, 1);
    layout->addWidget(m_logoutButton);
}

void ProfilePage::setDemoMode(bool enabled)
{
    m_demoMode = enabled;
    m_logoutButton->setText(enabled ? QStringLiteral("退出演示") : QStringLiteral("退出登录"));
}

void ProfilePage::setProfile(const QJsonObject &profile)
{
    m_nicknameLabel->setText(profile.value(QStringLiteral("nickname")).toString());
    m_phoneLabel->setText(profile.value(QStringLiteral("phone")).toString());
    m_nicknameEdit->setText(profile.value(QStringLiteral("nickname")).toString());
    m_balanceCents = static_cast<qint64>(profile.value(QStringLiteral("balanceCents")).toDouble());
    updateBalance();
    m_accountNoteLabel->setText(
        profile.value(QStringLiteral("created")).toBool()
            ? QStringLiteral("首次登录成功，系统已自动创建账号。")
            : (m_demoMode ? QStringLiteral("演示账号 · 状态正常")
                          : QStringLiteral("用户资料已从服务端同步。")));
    m_ledgerList->clear();
    if (m_demoMode) {
        m_ledgerList->addItems({
            QStringLiteral("09-06 09:18  +¥100.00  成功\n充值后余额 ¥286.50"),
            QStringLiteral("09-03 18:42  +¥50.00  成功\n充值后余额 ¥196.30")});
    } else {
        m_ledgerList->addItem(QStringLiteral("正在同步钱包流水…"));
    }
}

void ProfilePage::setFavoriteStation(const QString &name, bool favorited)
{
    const auto matches = m_favoriteList->findItems(name, Qt::MatchExactly);
    if (favorited && matches.isEmpty())
        m_favoriteList->addItem(name);
    if (!favorited)
        for (auto *item : matches) delete m_favoriteList->takeItem(m_favoriteList->row(item));
}

void ProfilePage::applyWalletResult(const QJsonObject &result)
{
    m_balanceCents = static_cast<qint64>(result.value(QStringLiteral("balanceCents")).toDouble());
    updateBalance();
    m_accountNoteLabel->setText(QStringLiteral("充值成功，钱包余额和流水已同步。"));
}

void ProfilePage::setLedger(const QJsonArray &items)
{
    m_ledgerList->clear();
    for (const auto &value : items) {
        const QJsonObject item = value.toObject();
        const qint64 amount = static_cast<qint64>(item.value(QStringLiteral("amountCents")).toDouble());
        m_ledgerList->addItem(QStringLiteral("%1  %2¥%3\n余额 ¥%4")
            .arg(item.value(QStringLiteral("createdAt")).toString(), amount >= 0 ? QStringLiteral("+") : QString())
            .arg(amount / 100.0, 0, 'f', 2)
            .arg(item.value(QStringLiteral("balanceAfterCents")).toDouble() / 100.0, 0, 'f', 2));
    }
    if (items.isEmpty()) m_ledgerList->addItem(QStringLiteral("暂无钱包流水"));
}

void ProfilePage::updateBalance()
{
    m_balanceLabel->setText(QStringLiteral("钱包余额  ¥ %1")
                                .arg(m_balanceCents / 100.0, 0, 'f', 2));
}
