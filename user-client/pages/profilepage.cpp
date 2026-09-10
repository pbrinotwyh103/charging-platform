#include "pages/profilepage.h"

#include <QBuffer>
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
#include <QRegularExpression>
#include <QTabWidget>
#include <QVBoxLayout>

ProfilePage::ProfilePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("profilePage"));
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(16, 12, 16, 12);
    m_rootLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("我的账户"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    m_avatarLabel = new QLabel(QStringLiteral("用户"), this);
    m_avatarLabel->setObjectName(QStringLiteral("profileAvatar"));
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
    m_accountNoteLabel->setObjectName(QStringLiteral("profileAccountNote"));
    m_accountNoteLabel->setWordWrap(true);
    m_accountNoteLabel->setStyleSheet(QStringLiteral("color:#475569;"));

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("profileTabs"));
    auto *accountTab = new QWidget(tabs);
    auto *accountLayout = new QVBoxLayout(accountTab);
    m_nicknameEdit = new QLineEdit(accountTab);
    m_nicknameEdit->setObjectName(QStringLiteral("nicknameEdit"));
    m_nicknameEdit->setPlaceholderText(QStringLiteral("修改昵称（2-20个字符）"));
    m_saveNicknameButton = new QPushButton(QStringLiteral("保存昵称"), accountTab);
    m_saveNicknameButton->setObjectName(QStringLiteral("saveNicknameButton"));
    m_avatarButton = new QPushButton(QStringLiteral("更换头像"), accountTab);
    m_avatarButton->setObjectName(QStringLiteral("avatarButton"));
    accountLayout->addWidget(m_nicknameEdit);
    accountLayout->addWidget(m_saveNicknameButton);
    accountLayout->addWidget(m_avatarButton);
    accountLayout->addStretch();

    auto *walletTab = new QWidget(tabs);
    auto *walletLayout = new QVBoxLayout(walletTab);
    m_rechargeLayout = new QHBoxLayout;
    m_rechargeLayout->setObjectName(QStringLiteral("rechargeLayout"));
    m_rechargeAmount = new QDoubleSpinBox(walletTab);
    m_rechargeAmount->setObjectName(QStringLiteral("rechargeAmount"));
    m_rechargeAmount->setRange(1.0, 5000.0);
    m_rechargeAmount->setDecimals(2);
    m_rechargeAmount->setSingleStep(10.0);
    m_rechargeAmount->setValue(50.0);
    m_rechargeAmount->setPrefix(QStringLiteral("¥ "));
    m_rechargeButton = new QPushButton(QStringLiteral("充值"), walletTab);
    m_rechargeButton->setObjectName(QStringLiteral("rechargeButton"));
    m_rechargeLayout->addWidget(m_rechargeAmount, 1);
    m_rechargeLayout->addWidget(m_rechargeButton);
    auto *ledgerTitle = new QLabel(QStringLiteral("充值记录（按时间倒序）"), walletTab);
    m_ledgerList = new QListWidget(walletTab);
    m_ledgerList->setObjectName(QStringLiteral("walletLedgerList"));
    m_ledgerRetryButton = new QPushButton(QStringLiteral("重新加载流水"), walletTab);
    m_ledgerRetryButton->setObjectName(QStringLiteral("ledgerRetryButton"));
    m_ledgerRetryButton->hide();
    walletLayout->addLayout(m_rechargeLayout);
    walletLayout->addWidget(ledgerTitle);
    walletLayout->addWidget(m_ledgerList, 1);
    walletLayout->addWidget(m_ledgerRetryButton);

    auto *favoriteTab = new QWidget(tabs);
    auto *favoriteLayout = new QVBoxLayout(favoriteTab);
    m_favoriteList = new QListWidget(favoriteTab);
    m_favoriteList->setObjectName(QStringLiteral("favoriteStationList"));
    m_favoriteRetryButton = new QPushButton(QStringLiteral("重新加载收藏"), favoriteTab);
    m_favoriteRetryButton->setObjectName(QStringLiteral("favoriteRetryButton"));
    m_favoriteRetryButton->hide();
    favoriteLayout->addWidget(new QLabel(QStringLiteral("常用充电站"), favoriteTab));
    favoriteLayout->addWidget(m_favoriteList, 1);
    favoriteLayout->addWidget(m_favoriteRetryButton);
    auto *orderTab = new QWidget(tabs);
    auto *orderLayout = new QVBoxLayout(orderTab);
    m_orderList = new QListWidget(orderTab);
    m_orderList->setObjectName(QStringLiteral("orderHistoryList"));
    m_orderRetryButton = new QPushButton(QStringLiteral("重新加载订单"), orderTab);
    m_orderRetryButton->setObjectName(QStringLiteral("orderRetryButton"));
    m_orderRetryButton->hide();
    orderLayout->addWidget(new QLabel(QStringLiteral("充电订单（按时间倒序）"), orderTab));
    orderLayout->addWidget(m_orderList, 1);
    orderLayout->addWidget(m_orderRetryButton);
    tabs->addTab(accountTab, QStringLiteral("资料"));
    tabs->addTab(walletTab, QStringLiteral("钱包"));
    tabs->addTab(favoriteTab, QStringLiteral("收藏"));
    tabs->addTab(orderTab, QStringLiteral("订单"));
    const int orderTabIndex = tabs->indexOf(orderTab);
    connect(tabs, &QTabWidget::currentChanged, this, [this, orderTabIndex](int index) {
        if (index != orderTabIndex || m_demoMode) return;
        showOrderHistoryLoading();
        emit orderHistoryRefreshRequested();
    });

    m_logoutButton = new QPushButton(QStringLiteral("退出登录"), this);
    connect(m_logoutButton, &QPushButton::clicked, this, &ProfilePage::logoutRequested);
    connect(m_saveNicknameButton, &QPushButton::clicked, this, [this] {
        const QString nickname = m_nicknameEdit->text().trimmed();
        static const QRegularExpression pattern(
            QStringLiteral("^[\\p{Han}A-Za-z0-9 _-]{2,20}$"));
        if (!pattern.match(nickname).hasMatch()) {
            m_accountNoteLabel->setText(
                QStringLiteral("昵称需为 2–20 个中文、字母、数字、空格、下划线或短横线。"));
            return;
        }
        if (nickname == m_confirmedNickname) {
            m_accountNoteLabel->setText(QStringLiteral("昵称没有变化，无需保存。"));
            return;
        }
        if (m_demoMode) {
            m_confirmedNickname = nickname;
            m_nicknameLabel->setText(nickname);
            m_accountNoteLabel->setText(QStringLiteral("昵称已保存（演示模式）。"));
        } else {
            m_pendingNickname = nickname;
            m_nicknameUpdatePending = true;
            m_nicknameEdit->setDisabled(true);
            m_saveNicknameButton->setDisabled(true);
            m_saveNicknameButton->setText(QStringLiteral("正在保存…"));
            m_accountNoteLabel->setText(QStringLiteral("昵称正在保存，请稍候。"));
        }
        emit nicknameUpdateRequested(nickname);
    });
    connect(m_avatarButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择头像"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
        if (path.isEmpty()) return;
        updateAvatarFromFile(path);
    });
    connect(m_rechargeButton, &QPushButton::clicked, this, [this] {
        if (m_rechargePending) return;
        const qint64 cents = qRound64(m_rechargeAmount->value() * 100.0);
        if (cents < 100 || cents > 500000) {
            m_accountNoteLabel->setText(QStringLiteral("充值金额需为 1.00–5000.00 元。"));
            return;
        }
        if (m_demoMode) {
            m_balanceCents += cents;
            updateBalance();
            m_ledgerList->insertItem(0,
                QStringLiteral("%1  +¥%2  成功\n充值后余额 ¥%3")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm")))
                    .arg(cents / 100.0, 0, 'f', 2)
                    .arg(m_balanceCents / 100.0, 0, 'f', 2));
            m_accountNoteLabel->setText(QStringLiteral("模拟支付成功，钱包余额已实时更新。"));
        } else {
            m_rechargePending = true;
            m_rechargeAmount->setDisabled(true);
            m_rechargeButton->setDisabled(true);
            m_rechargeButton->setText(QStringLiteral("支付处理中…"));
            m_accountNoteLabel->setText(QStringLiteral("充值请求已提交，请勿重复操作。"));
        }
        emit rechargeRequested(cents);
    });
    connect(m_ledgerRetryButton, &QPushButton::clicked, this, [this] {
        showLedgerLoading();
        emit ledgerRefreshRequested();
    });
    connect(m_favoriteRetryButton, &QPushButton::clicked, this, [this] {
        showFavoritesLoading();
        emit favoritesRefreshRequested();
    });
    connect(m_orderRetryButton, &QPushButton::clicked, this, [this] {
        showOrderHistoryLoading();
        emit orderHistoryRefreshRequested();
    });

    m_rootLayout->addWidget(titleLabel);
    m_rootLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    m_rootLayout->addWidget(m_nicknameLabel, 0, Qt::AlignHCenter);
    m_rootLayout->addWidget(m_phoneLabel, 0, Qt::AlignHCenter);
    m_rootLayout->addWidget(m_balanceLabel);
    m_rootLayout->addWidget(m_accountNoteLabel);
    m_rootLayout->addWidget(tabs, 1);
    m_rootLayout->addWidget(m_logoutButton);
}

void ProfilePage::setCompactLayout(bool compact)
{
    m_rechargeLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                           : QBoxLayout::LeftToRight);
    m_rechargeLayout->setSpacing(compact ? 8 : 12);
    m_rootLayout->setContentsMargins(compact ? 6 : 18, compact ? 6 : 14,
                                     compact ? 6 : 18, compact ? 6 : 14);
}

bool ProfilePage::updateAvatarFromFile(const QString &path)
{
    if (m_avatarUpdatePending) return false;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);
    const QByteArray format = reader.format().toLower();
    if (!reader.canRead() || (format != "png" && format != "jpg" && format != "jpeg")) {
        m_accountNoteLabel->setText(QStringLiteral("头像必须是有效的 PNG 或 JPEG 图片。"));
        return false;
    }
    const QSize sourceSize = reader.size();
    if (!sourceSize.isValid() || sourceSize.width() > 2048 || sourceSize.height() > 2048) {
        m_accountNoteLabel->setText(QStringLiteral("头像尺寸不能超过 2048×2048。"));
        return false;
    }
    QImage image = reader.read();
    if (image.isNull()) {
        m_accountNoteLabel->setText(QStringLiteral("头像读取失败。"));
        return false;
    }
    image = image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray encoded;
    QBuffer output(&encoded);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG")
        || encoded.isEmpty() || encoded.size() > 2 * 1024 * 1024) {
        m_accountNoteLabel->setText(QStringLiteral("头像压缩失败或文件仍超过 2 MiB。"));
        return false;
    }

    m_pendingAvatar = QPixmap::fromImage(image);
    showAvatar(m_pendingAvatar);
    if (m_demoMode) {
        m_confirmedAvatar = m_pendingAvatar;
        m_accountNoteLabel->setText(QStringLiteral("头像已更新（演示模式）。"));
        return true;
    }

    m_avatarUpdatePending = true;
    m_avatarButton->setDisabled(true);
    m_avatarButton->setText(QStringLiteral("正在上传…"));
    m_accountNoteLabel->setText(QStringLiteral("头像正在上传，请稍候。"));
    const QString dataUrl = QStringLiteral("data:image/png;base64,%1")
                                .arg(QString::fromLatin1(encoded.toBase64()));
    emit avatarUpdateRequested(dataUrl);
    return true;
}

void ProfilePage::avatarUpdateSucceeded(const QJsonObject &profile)
{
    if (!m_avatarUpdatePending) return;
    m_confirmedAvatar = m_pendingAvatar;
    m_confirmedAvatarPath = profile.value(QStringLiteral("avatar")).toString();
    m_avatarUpdatePending = false;
    m_avatarButton->setEnabled(true);
    m_avatarButton->setText(QStringLiteral("更换头像"));
    showAvatar(m_confirmedAvatar);
    m_accountNoteLabel->setText(QStringLiteral("头像已保存并与服务端同步。"));
}

void ProfilePage::avatarUpdateFailed(const QString &message)
{
    if (!m_avatarUpdatePending) return;
    m_avatarUpdatePending = false;
    m_avatarButton->setEnabled(true);
    m_avatarButton->setText(QStringLiteral("更换头像"));
    if (m_confirmedAvatar.isNull()) showDefaultAvatar();
    else showAvatar(m_confirmedAvatar);
    m_accountNoteLabel->setText(QStringLiteral("头像保存失败，已恢复原头像：%1").arg(message));
}

void ProfilePage::nicknameUpdateSucceeded(const QJsonObject &profile)
{
    if (!m_nicknameUpdatePending) return;
    const QString nickname = profile.value(QStringLiteral("nickname")).toString(m_pendingNickname);
    m_confirmedNickname = nickname;
    m_pendingNickname.clear();
    m_nicknameUpdatePending = false;
    m_nicknameLabel->setText(nickname);
    m_nicknameEdit->setText(nickname);
    m_nicknameEdit->setEnabled(true);
    m_saveNicknameButton->setEnabled(true);
    m_saveNicknameButton->setText(QStringLiteral("保存昵称"));
    m_accountNoteLabel->setText(QStringLiteral("昵称已保存并与服务端同步。"));
}

void ProfilePage::nicknameUpdateFailed(const QString &message)
{
    if (!m_nicknameUpdatePending) return;
    m_pendingNickname.clear();
    m_nicknameUpdatePending = false;
    m_nicknameLabel->setText(m_confirmedNickname);
    m_nicknameEdit->setText(m_confirmedNickname);
    m_nicknameEdit->setEnabled(true);
    m_saveNicknameButton->setEnabled(true);
    m_saveNicknameButton->setText(QStringLiteral("保存昵称"));
    m_accountNoteLabel->setText(QStringLiteral("昵称保存失败，已恢复原昵称：%1").arg(message));
}

void ProfilePage::setDemoMode(bool enabled)
{
    m_demoMode = enabled;
    m_logoutButton->setText(enabled ? QStringLiteral("退出演示") : QStringLiteral("退出登录"));
    if (!m_rechargePending)
        m_rechargeButton->setText(enabled ? QStringLiteral("模拟充值") : QStringLiteral("充值"));
}

void ProfilePage::setProfile(const QJsonObject &profile)
{
    const QString avatarPath = profile.value(QStringLiteral("avatar")).toString(
        profile.value(QStringLiteral("avatarPath")).toString());
    if (!m_avatarUpdatePending && avatarPath != m_confirmedAvatarPath) {
        m_confirmedAvatarPath = avatarPath;
        m_confirmedAvatar = QPixmap();
        showDefaultAvatar();
    }
    const QString nickname = profile.value(QStringLiteral("nickname")).toString();
    if (!m_nicknameUpdatePending) {
        m_confirmedNickname = nickname;
        m_nicknameLabel->setText(nickname);
        m_nicknameEdit->setText(nickname);
    }
    m_phoneLabel->setText(profile.value(QStringLiteral("phone")).toString());
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
        showLedgerLoading();
        showFavoritesLoading();
        showOrderHistoryLoading();
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

void ProfilePage::setFavoriteStations(const QJsonArray &stations)
{
    m_favoriteList->clear();
    m_favoriteRetryButton->hide();
    if (stations.isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("暂无收藏站点"), m_favoriteList);
        item->setFlags(Qt::NoItemFlags);
        return;
    }
    for (const auto &value : stations) {
        const QJsonObject station = value.toObject();
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(station.value(QStringLiteral("name")).toString(),
                                       station.value(QStringLiteral("address")).toString()),
            m_favoriteList);
        item->setData(Qt::UserRole, station.value(QStringLiteral("stationId")));
    }
}

void ProfilePage::showFavoritesLoading()
{
    m_favoriteRetryButton->hide();
    m_favoriteList->clear();
    auto *item = new QListWidgetItem(QStringLiteral("正在加载收藏站点…"), m_favoriteList);
    item->setFlags(Qt::NoItemFlags);
}

void ProfilePage::showFavoritesError(const QString &message)
{
    m_favoriteList->clear();
    auto *item = new QListWidgetItem(QStringLiteral("收藏加载失败：%1").arg(message), m_favoriteList);
    item->setFlags(Qt::NoItemFlags);
    m_favoriteRetryButton->show();
}

void ProfilePage::applyWalletResult(const QJsonObject &result)
{
    m_balanceCents = static_cast<qint64>(result.value(QStringLiteral("balanceCents")).toDouble());
    m_rechargePending = false;
    m_rechargeAmount->setEnabled(true);
    m_rechargeButton->setEnabled(true);
    m_rechargeButton->setText(m_demoMode ? QStringLiteral("模拟充值") : QStringLiteral("充值"));
    updateBalance();
    m_accountNoteLabel->setText(QStringLiteral("充值成功，钱包余额和流水已同步。"));
}

void ProfilePage::applySettlementResult(const QJsonObject &result)
{
    if (!result.contains(QStringLiteral("balanceCents"))) return;
    m_balanceCents = static_cast<qint64>(result.value(QStringLiteral("balanceCents")).toDouble());
    updateBalance();
    m_accountNoteLabel->setText(QStringLiteral("充电扣费已完成，钱包余额和流水已同步。"));
}

void ProfilePage::rechargeFailed(const QString &message)
{
    if (!m_rechargePending) return;
    m_rechargePending = false;
    m_rechargeAmount->setEnabled(true);
    m_rechargeButton->setEnabled(true);
    m_rechargeButton->setText(m_demoMode ? QStringLiteral("模拟充值") : QStringLiteral("充值"));
    m_accountNoteLabel->setText(QStringLiteral("充值未确认，当前页面余额未更新：%1").arg(message));
}

void ProfilePage::setLedger(const QJsonArray &items)
{
    m_ledgerList->clear();
    m_ledgerRetryButton->hide();
    for (const auto &value : items) {
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) continue;
        const QJsonValue amountValue = item.value(QStringLiteral("amountCents"));
        const QJsonValue balanceValue = item.value(QStringLiteral("balanceAfterCents"));
        const QString createdAt = item.value(QStringLiteral("createdAt")).toString().trimmed();
        const QString timeText = createdAt.isEmpty() ? QStringLiteral("时间未知") : createdAt;
        const QString recordType = item.value(QStringLiteral("recordType")).toString();
        const QString typeText = recordType == QStringLiteral("charge_payment")
            ? QStringLiteral("充电扣费")
            : recordType == QStringLiteral("refund") ? QStringLiteral("退款")
            : QStringLiteral("充值");
        const QString amountText = amountValue.isDouble()
            ? QStringLiteral("%1¥%2")
                  .arg(amountValue.toDouble() >= 0 ? QStringLiteral("+") : QString())
                  .arg(amountValue.toDouble() / 100.0, 0, 'f', 2)
            : QStringLiteral("金额未知");
        const QString balanceText = balanceValue.isDouble()
            ? QStringLiteral("余额 ¥%1").arg(balanceValue.toDouble() / 100.0, 0, 'f', 2)
            : QStringLiteral("余额未知");
        const qint64 orderId = item.value(QStringLiteral("orderId")).toInteger();
        const QString orderText = orderId > 0 ? QStringLiteral("  订单 #%1").arg(orderId) : QString();
        m_ledgerList->addItem(QStringLiteral("%1  %2  %3%4\n%5")
                                  .arg(timeText, typeText, amountText, orderText, balanceText));
    }
    if (items.isEmpty()) m_ledgerList->addItem(QStringLiteral("暂无钱包流水"));
    else if (m_ledgerList->count() == 0)
        m_ledgerList->addItem(QStringLiteral("流水数据格式异常，请稍后重试"));
}

void ProfilePage::showOrderHistoryLoading()
{
    m_orderRetryButton->hide();
    m_orderList->clear();
    auto *item = new QListWidgetItem(QStringLiteral("正在加载订单…"), m_orderList);
    item->setFlags(Qt::NoItemFlags);
}

void ProfilePage::showOrderHistoryError(const QString &message)
{
    m_orderList->clear();
    auto *item = new QListWidgetItem(QStringLiteral("订单加载失败：%1").arg(message), m_orderList);
    item->setFlags(Qt::NoItemFlags);
    m_orderRetryButton->show();
}

void ProfilePage::setOrderHistory(const QJsonArray &items)
{
    m_orderList->clear();
    m_orderRetryButton->hide();
    for (const auto &value : items) {
        const auto order = value.toObject();
        if (order.isEmpty()) continue;
        const qint64 orderId = order.value(QStringLiteral("orderId")).toInteger();
        const QString status = order.value(QStringLiteral("status")).toString();
        const QString statusText = status == QStringLiteral("completed") ? QStringLiteral("已完成")
            : status == QStringLiteral("charging") ? QStringLiteral("充电中")
            : status == QStringLiteral("cancelled") ? QStringLiteral("已取消") : status;
        const QString station = order.value(QStringLiteral("stationName")).toString();
        const QString stationText = station.isEmpty()
            ? QStringLiteral("站点 #%1").arg(order.value(QStringLiteral("stationId")).toInteger()) : station;
        const QString pile = order.value(QStringLiteral("pileCode")).toString();
        const QString pileText = pile.isEmpty()
            ? QStringLiteral("电桩 #%1").arg(order.value(QStringLiteral("pileId")).toInteger()) : pile;
        const double fee = order.value(QStringLiteral("feeCents")).toDouble();
        const QString started = order.value(QStringLiteral("startedAt")).toString();
        const QString stopped = order.value(QStringLiteral("stoppedAt")).toString();
        auto *item = new QListWidgetItem(
            QStringLiteral("订单 #%1  %2\n%3 · %4 · %5\n%6 → %7\n电量 %8 kWh  费用 ¥%9")
                .arg(orderId).arg(statusText, stationText, pileText)
                .arg(started.isEmpty() ? QStringLiteral("未开始") : started,
                     stopped.isEmpty() ? QStringLiteral("进行中") : stopped)
                .arg(order.value(QStringLiteral("energyKwh")).toDouble(), 0, 'f', 2)
                .arg(fee / 100.0, 0, 'f', 2), m_orderList);
        item->setData(Qt::UserRole, orderId);
    }
    if (items.isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("暂无充电订单"), m_orderList);
        item->setFlags(Qt::NoItemFlags);
    } else if (m_orderList->count() == 0) {
        auto *item = new QListWidgetItem(QStringLiteral("订单数据格式异常，请稍后重试"), m_orderList);
        item->setFlags(Qt::NoItemFlags);
    }
}

void ProfilePage::showLedgerLoading()
{
    if (m_demoMode) return;
    m_ledgerList->clear();
    m_ledgerList->addItem(QStringLiteral("正在同步钱包流水…"));
    m_ledgerRetryButton->hide();
}

void ProfilePage::showLedgerError(const QString &message)
{
    if (m_demoMode) return;
    m_ledgerList->clear();
    m_ledgerList->addItem(QStringLiteral("流水加载失败：%1").arg(message));
    m_ledgerRetryButton->show();
}

void ProfilePage::updateBalance()
{
    m_balanceLabel->setText(QStringLiteral("钱包余额  ¥ %1")
                                .arg(m_balanceCents / 100.0, 0, 'f', 2));
}

void ProfilePage::showAvatar(const QPixmap &pixmap)
{
    m_avatarLabel->setText({});
    m_avatarLabel->setPixmap(pixmap.scaled(m_avatarLabel->size(), Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
    m_avatarLabel->setScaledContents(false);
}

void ProfilePage::showDefaultAvatar()
{
    m_avatarLabel->setPixmap(QPixmap());
    m_avatarLabel->setText(QStringLiteral("用户"));
    m_avatarLabel->setScaledContents(false);
}
