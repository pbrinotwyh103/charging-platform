#include "pages/profilepage.h"
#include "widgets/nicknamedialog.h"
#include "widgets/rechargedialog.h"

#include <QEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QPixmap>
#include <QBuffer>
#include <QTimeZone>
#include <QToolTip>

#include <algorithm>
#include <initializer_list>

namespace {

class AvatarEditButton final : public QPushButton
{
public:
    explicit AvatarEditButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setFixedSize(54, 54);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("修改头像"));
        setAccessibleName(QStringLiteral("修改头像"));
        setFlat(true);
    }

    void setAvatarPixmap(const QPixmap &pixmap)
    {
        m_pixmap = pixmap;
        update();
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Enter) {
            m_hovered = true;
            update();
        } else if (event->type() == QEvent::Leave) {
            m_hovered = false;
            update();
        }
        return QPushButton::event(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF avatarRect = QRectF(rect()).adjusted(2, 2, -2, -2);
        QPainterPath clipPath;
        clipPath.addEllipse(avatarRect);
        painter.setClipPath(clipPath);

        if (m_pixmap.isNull()) {
            painter.fillPath(clipPath, QColor(QStringLiteral("#cbd5e1")));
            painter.setPen(QColor(QStringLiteral("#475569")));
            QFont placeholderFont = font();
            placeholderFont.setBold(true);
            placeholderFont.setPointSizeF(font().pointSizeF() * 1.3);
            painter.setFont(placeholderFont);
            painter.drawText(avatarRect, Qt::AlignCenter, QStringLiteral("用户"));
        } else {
            const QPixmap scaled = m_pixmap.scaled(
                avatarRect.size().toSize(), Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation);
            const QPointF origin(
                avatarRect.center().x() - scaled.width() / 2.0,
                avatarRect.center().y() - scaled.height() / 2.0);
            painter.drawPixmap(origin, scaled);
        }

        if (m_hovered) {
            painter.fillPath(clipPath, QColor(15, 23, 42, 150));
            painter.setPen(Qt::white);
            QFont editFont = font();
            editFont.setPointSizeF(font().pointSizeF() * 1.9);
            editFont.setBold(true);
            painter.setFont(editFont);
            painter.drawText(avatarRect, Qt::AlignCenter, QStringLiteral("✎"));
        }

        painter.setClipping(false);
        painter.setPen(QPen(hasFocus() ? QColor(QStringLiteral("#2563eb"))
                                      : QColor(QStringLiteral("#e2e8f0")), 2));
        painter.drawEllipse(avatarRect);
    }

private:
    QPixmap m_pixmap;
    bool m_hovered = false;
};

QJsonValue firstValue(const QJsonObject &object,
                      std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull()) return value;
    }
    return {};
}

QString stringField(const QJsonObject &object,
                    std::initializer_list<const char *> keys)
{
    const QJsonValue value = firstValue(object, keys);
    if (value.isString()) return value.toString().trimmed();
    if (value.isDouble()) return QString::number(value.toVariant().toLongLong());
    return {};
}

bool integerField(const QJsonObject &object,
                  std::initializer_list<const char *> keys,
                  qint64 *result)
{
    const QJsonValue value = firstValue(object, keys);
    bool ok = false;
    const qint64 number = value.toVariant().toLongLong(&ok);
    if (ok && result) *result = number;
    return ok;
}

qint64 orderTimeMillis(const QJsonObject &order)
{
    const QJsonValue value = firstValue(
        order, {"startedAt", "createdAt", "time", "orderTime"});
    if (value.isDouble()) return value.toVariant().toLongLong();
    bool numeric = false;
    const qint64 milliseconds = value.toString().toLongLong(&numeric);
    if (numeric) return milliseconds;
    const QDateTime dateTime = QDateTime::fromString(value.toString(), Qt::ISODate);
    return dateTime.isValid() ? dateTime.toMSecsSinceEpoch() : 0;
}

QString orderTimeText(const QJsonObject &order)
{
    const qint64 milliseconds = orderTimeMillis(order);
    if (milliseconds > 0) {
        return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::utc())
            .toTimeZone(QTimeZone("Asia/Shanghai"))
            .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    const QString original = stringField(
        order, {"startedAt", "createdAt", "time", "orderTime"});
    return original.isEmpty() ? QStringLiteral("时间未知") : original;
}

QString moneyText(qint64 fen)
{
    const bool negative = fen < 0;
    const quint64 absolute = negative
        ? static_cast<quint64>(-(fen + 1)) + 1
        : static_cast<quint64>(fen);
    return QStringLiteral("%1¥%2.%3")
        .arg(negative ? QStringLiteral("-") : QString())
        .arg(absolute / 100)
        .arg(absolute % 100, 2, 10, QLatin1Char('0'));
}

QString paymentStatusText(const QString &status)
{
    const QString normalized = status.trimmed().toUpper();
    if (normalized == QStringLiteral("SUCCESS")
        || normalized == QStringLiteral("PAID"))
        return QStringLiteral("已支付");
    if (normalized == QStringLiteral("UNPAID")
        || normalized == QStringLiteral("WAITING_PAYMENT"))
        return QStringLiteral("待支付");
    if (normalized == QStringLiteral("PENDING")
        || normalized == QStringLiteral("PROCESSING"))
        return QStringLiteral("支付中");
    if (normalized == QStringLiteral("FAILED")) return QStringLiteral("支付失败");
    if (normalized == QStringLiteral("REFUNDED")) return QStringLiteral("已退款");
    if (normalized == QStringLiteral("CANCELLED")) return QStringLiteral("已取消");
    return status.trimmed().isEmpty() ? QStringLiteral("状态未知") : status;
}

QString paymentStatusStyle(const QString &status)
{
    const QString normalized = status.trimmed().toUpper();
    if (normalized == QStringLiteral("SUCCESS")
        || normalized == QStringLiteral("PAID"))
        return QStringLiteral("padding:4px 9px;background:#dcfce7;color:#166534;border-radius:9px;font-weight:600;");
    if (normalized == QStringLiteral("FAILED")
        || normalized == QStringLiteral("CANCELLED"))
        return QStringLiteral("padding:4px 9px;background:#fee2e2;color:#991b1b;border-radius:9px;font-weight:600;");
    return QStringLiteral("padding:4px 9px;background:#fef3c7;color:#92400e;border-radius:9px;font-weight:600;");
}

} // namespace

ProfilePage::ProfilePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 8);
    layout->setSpacing(8);

    auto *profileSection = new QWidget(this);
    profileSection->setObjectName(QStringLiteral("compactProfileSection"));
    profileSection->setMaximumHeight(150);
    auto *profileLayout = new QVBoxLayout(profileSection);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(6);

    auto *titleLabel = new QLabel(QStringLiteral("我的"), profileSection);

    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    auto *favoritesButton = new QPushButton(QStringLiteral("★ 收藏站点"),
                                            profileSection);
    favoritesButton->setObjectName(QStringLiteral("openFavoritesButton"));
    favoritesButton->setFlat(true);
    favoritesButton->setStyleSheet(QStringLiteral(
        "color:#92400e;font-weight:600;padding:4px 6px;"));
    auto *profileHeader = new QHBoxLayout;
    profileHeader->addWidget(titleLabel);
    profileHeader->addStretch();
    profileHeader->addWidget(favoritesButton);

    m_avatarLabel = new AvatarEditButton(profileSection);

    m_nicknameLabel = new QLabel(profileSection);
    QFont nicknameFont = m_nicknameLabel->font();
    nicknameFont.setPointSize(16);
    nicknameFont.setBold(true);
    m_nicknameLabel->setFont(nicknameFont);
    m_nicknameLabel->setStyleSheet(QStringLiteral("color:#0f172a;"));
    m_phoneLabel = new QLabel(profileSection);
    m_phoneLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
    m_balanceLabel = new QLabel(profileSection);
    m_balanceLabel->setObjectName(QStringLiteral("profileBalanceLabel"));
    QFont balanceFont = m_balanceLabel->font();
    balanceFont.setPointSize(14);
    balanceFont.setWeight(QFont::DemiBold);
    m_balanceLabel->setFont(balanceFont);

    m_balanceLabel->setStyleSheet(
        QStringLiteral(
            "padding:9px 10px;"
            "background:#eff6ff;"
            "color:#1e3a8a;"
            "border-radius:9px;"));

    m_logoutButton = new QPushButton(QStringLiteral("退出登录"), this);
    auto *nicknameRow = new QHBoxLayout;
    nicknameRow->setSpacing(4);
    auto *editNickname = new QPushButton(QStringLiteral("✎"), profileSection);
    editNickname->setObjectName(QStringLiteral("editNicknameButton"));
    editNickname->setFixedSize(30, 30);
    editNickname->setCursor(Qt::PointingHandCursor);
    editNickname->setToolTip(QStringLiteral("修改昵称"));
    editNickname->setAccessibleName(QStringLiteral("修改昵称"));
    editNickname->setFlat(true);
    QFont editNicknameFont = editNickname->font();
    editNicknameFont.setPointSize(17);
    editNickname->setFont(editNicknameFont);
    editNickname->setStyleSheet(QStringLiteral(
        "QPushButton{color:#64748b;border:0;border-radius:15px;}"
        "QPushButton:hover{background:#e2e8f0;color:#2563eb;}"));
    nicknameRow->addWidget(m_nicknameLabel);
    nicknameRow->addWidget(editNickname);
    nicknameRow->addStretch();
    auto *identityText = new QVBoxLayout;
    identityText->setSpacing(1);
    identityText->addLayout(nicknameRow);
    identityText->addWidget(m_phoneLabel);
    auto *identityRow = new QHBoxLayout;
    identityRow->setSpacing(10);
    identityRow->addWidget(m_avatarLabel, 0, Qt::AlignVCenter);
    identityRow->addLayout(identityText, 1);

    auto *walletActions = new QHBoxLayout;
    walletActions->setSpacing(6);
    auto *recharge = new QPushButton(QStringLiteral("充值"), profileSection);
    auto *rechargeRecords = new QPushButton(QStringLiteral("充值记录"), profileSection);
    recharge->setObjectName(QStringLiteral("openRechargeDialogButton"));
    rechargeRecords->setObjectName(QStringLiteral("openRechargeRecordsButton"));
    recharge->setMinimumHeight(36);
    rechargeRecords->setMinimumHeight(36);
    recharge->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:pressed{background:#1d4ed8;}"));
    rechargeRecords->setStyleSheet(QStringLiteral(
        "QPushButton{background:#eff6ff;color:#1d4ed8;border:1px solid #bfdbfe;border-radius:9px;font-weight:600;}"));
    walletActions->addWidget(recharge, 1);
    walletActions->addWidget(rechargeRecords, 1);
    auto *accountActions = new QHBoxLayout;
    accountActions->setSpacing(6);
    accountActions->addWidget(m_balanceLabel, 2);
    accountActions->addLayout(walletActions, 2);

    connect(
        m_logoutButton,
        &QPushButton::clicked,
        this,
        &ProfilePage::logoutRequested);
    connect(editNickname, &QPushButton::clicked, this, [this, editNickname] {
        NicknameDialog dialog(m_currentNickname, this);
        if (dialog.exec() != QDialog::Accepted) return;
        emit nicknameUpdateRequested(dialog.nickname());
        QToolTip::showText(editNickname->mapToGlobal(editNickname->rect().bottomLeft()),
                           QStringLiteral("昵称修改请求已提交"), editNickname);
    });
    connect(m_avatarLabel, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择头像"), {},
            QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
        if (path.isEmpty()) return;
        QImageReader reader(path);
        if (!reader.canRead()
            || !QStringList({"png", "jpg", "jpeg"}).contains(reader.format().toLower())) {
            QToolTip::showText(m_avatarLabel->mapToGlobal(m_avatarLabel->rect().bottomLeft()),
                               QStringLiteral("头像必须是 PNG 或 JPEG 图片"), m_avatarLabel);
            return;
        }
        if (reader.size().width() > 2048 || reader.size().height() > 2048) {
            QToolTip::showText(m_avatarLabel->mapToGlobal(m_avatarLabel->rect().bottomLeft()),
                               QStringLiteral("头像尺寸不能超过 2048×2048"), m_avatarLabel);
            return;
        }
        const QImage image = reader.read();
        if (image.isNull()) {
            QToolTip::showText(m_avatarLabel->mapToGlobal(m_avatarLabel->rect().bottomLeft()),
                               QStringLiteral("头像读取失败"), m_avatarLabel);
            return;
        }
        const QImage preview = image.scaled(
            512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        static_cast<AvatarEditButton *>(m_avatarLabel)
            ->setAvatarPixmap(QPixmap::fromImage(preview));
        QByteArray encoded;
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        preview.save(&buffer, "JPEG", 85);
        emit avatarUpdateRequested(path);
        emit avatarPrepared(QString::fromLatin1(encoded.toBase64()),
                            QStringLiteral("image/jpeg"));
        QToolTip::showText(m_avatarLabel->mapToGlobal(m_avatarLabel->rect().bottomLeft()),
                           QStringLiteral("头像已选择"), m_avatarLabel);
    });
    connect(recharge, &QPushButton::clicked, this, [this, recharge] {
        RechargeDialog dialog(m_balanceFen, this);
        if (dialog.exec() != QDialog::Accepted) return;
        emit rechargeRequested(dialog.amountFen());
        QToolTip::showText(recharge->mapToGlobal(recharge->rect().bottomLeft()),
                           QStringLiteral("充值请求已提交"), recharge);
    });
    connect(rechargeRecords, &QPushButton::clicked,
            this, &ProfilePage::rechargeRecordsRequested);
    connect(favoritesButton, &QPushButton::clicked,
            this, &ProfilePage::favoritesRequested);

    auto *historySection = new QWidget(this);
    historySection->setObjectName(QStringLiteral("orderHistorySection"));
    auto *historyLayout = new QVBoxLayout(historySection);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(6);

    auto *ordersHeader = new QHBoxLayout;
    auto *ordersTitle = new QLabel(QStringLiteral("历史订单"), historySection);
    QFont ordersTitleFont = ordersTitle->font();
    ordersTitleFont.setPointSize(16);
    ordersTitleFont.setBold(true);
    ordersTitle->setFont(ordersTitleFont);
    ordersTitle->setStyleSheet(QStringLiteral("color:#0f172a;"));
    m_orderSummaryLabel = new QLabel(QStringLiteral("暂无记录"), historySection);
    m_orderSummaryLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
    m_orderRefreshButton = new QPushButton(QStringLiteral("刷新"), historySection);
    m_orderRefreshButton->setObjectName(QStringLiteral("orderHistoryRefreshButton"));
    m_orderRefreshButton->setFlat(true);
    m_orderRefreshButton->setStyleSheet(QStringLiteral(
        "color:#2563eb;font-weight:600;padding:6px;"));
    ordersHeader->addWidget(ordersTitle);
    ordersHeader->addWidget(m_orderSummaryLabel);
    ordersHeader->addStretch();
    ordersHeader->addWidget(m_orderRefreshButton);

    m_ordersList = new QListWidget(historySection);
    m_ordersList->setObjectName(QStringLiteral("orderHistoryList"));
    m_ordersList->setFrameShape(QFrame::NoFrame);
    m_ordersList->setSpacing(8);
    m_ordersList->setSelectionMode(QAbstractItemView::NoSelection);
    m_ordersList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_ordersList->setStyleSheet(QStringLiteral(
        "QListWidget{background:transparent;}"
        "QListWidget::item{background:#ffffff;border:1px solid #e2e8f0;border-radius:10px;}"));

    m_orderStateLabel = new QLabel(
        QStringLiteral("暂无历史订单\n完成充电后，订单会显示在这里"), historySection);
    m_orderStateLabel->setObjectName(QStringLiteral("orderHistoryState"));
    m_orderStateLabel->setAlignment(Qt::AlignCenter);
    m_orderStateLabel->setWordWrap(true);
    m_orderStateLabel->setStyleSheet(QStringLiteral(
        "padding:28px;color:#64748b;background:#f8fafc;border:1px solid #e2e8f0;border-radius:10px;"));
    m_ordersList->hide();

    connect(m_orderRefreshButton, &QPushButton::clicked, this, [this] {
        showOrderLoading();
        emit orderHistoryRequested();
    });

    profileLayout->addLayout(profileHeader);
    profileLayout->addLayout(identityRow);
    profileLayout->addLayout(accountActions);

    historyLayout->addLayout(ordersHeader);
    historyLayout->addWidget(m_ordersList, 1);
    historyLayout->addWidget(m_orderStateLabel, 1);

    layout->addWidget(profileSection, 1);
    layout->addWidget(historySection, 3);
    layout->addWidget(m_logoutButton);
}

void ProfilePage::setProfile(const QJsonObject &profile)
{
    m_currentNickname = profile.value(QStringLiteral("nickname")).toString();
    m_nicknameLabel->setText(m_currentNickname);

    m_phoneLabel->setText(
        profile.value(QStringLiteral("phone")).toString());

    const QJsonValue balance = profile.contains(QStringLiteral("balanceFen"))
        ? profile.value(QStringLiteral("balanceFen"))
        : profile.value(QStringLiteral("balanceCents"));
    setBalanceCents(balance.toVariant().toLongLong());

    const QJsonValue orders = profile.contains(QStringLiteral("orders"))
        ? profile.value(QStringLiteral("orders"))
        : profile.value(QStringLiteral("orderHistory"));
    if (orders.isArray()) setOrders(orders.toArray());
}

void ProfilePage::setBalanceCents(qint64 balanceCents)
{
    m_balanceFen = balanceCents;
    m_balanceLabel->setText(QStringLiteral("钱包余额：%1")
                                .arg(moneyText(m_balanceFen)));
}

void ProfilePage::showOrderLoading()
{
    m_orderRefreshButton->setDisabled(true);
    m_ordersList->clear();
    m_ordersList->hide();
    m_orderStateLabel->setText(QStringLiteral("正在加载历史订单…"));
    m_orderStateLabel->show();
    m_orderSummaryLabel->setText(QStringLiteral("加载中"));
}

void ProfilePage::showOrderError(const QString &message)
{
    m_orderRefreshButton->setDisabled(false);
    m_ordersList->clear();
    m_ordersList->hide();
    m_orderStateLabel->setText(
        message.isEmpty() ? QStringLiteral("历史订单加载失败，请稍后重试") : message);
    m_orderStateLabel->show();
    m_orderSummaryLabel->setText(QStringLiteral("加载失败"));
}

void ProfilePage::setOrders(const QJsonArray &orders)
{
    m_orderRefreshButton->setDisabled(false);
    m_ordersList->clear();

    QList<QJsonObject> sorted;
    sorted.reserve(orders.size());
    for (const QJsonValue &value : orders) {
        if (value.isObject()) sorted.append(value.toObject());
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const QJsonObject &left, const QJsonObject &right) {
        const qint64 leftTime = orderTimeMillis(left);
        const qint64 rightTime = orderTimeMillis(right);
        if (leftTime != rightTime) return leftTime > rightTime;
        return stringField(left, {"orderNo", "orderNumber", "orderId"})
            > stringField(right, {"orderNo", "orderNumber", "orderId"});
    });

    if (sorted.isEmpty()) {
        m_ordersList->hide();
        m_orderStateLabel->setText(
            QStringLiteral("暂无历史订单\n完成充电后，订单会显示在这里"));
        m_orderStateLabel->show();
        m_orderSummaryLabel->setText(QStringLiteral("暂无记录"));
        return;
    }

    m_orderStateLabel->hide();
    m_ordersList->show();
    m_orderSummaryLabel->setText(QStringLiteral("共 %1 条").arg(sorted.size()));

    for (const QJsonObject &order : sorted) {
        const QString orderNo = stringField(
            order, {"orderNo", "orderNumber", "orderId", "id"});
        const QString stationName = stringField(order, {"stationName", "siteName"});
        const QString stationNo = stringField(
            order, {"stationNo", "stationCode", "stationId", "siteId"});
        const QString paymentStatus = stringField(
            order, {"paymentStatus", "payStatus"});
        qint64 feeFen = 0;
        const bool hasFee = integerField(
            order, {"feeFen", "amountFen", "feeCents", "amountCents"}, &feeFen);

        auto *item = new QListWidgetItem(m_ordersList);
        auto *card = new QWidget(m_ordersList);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(7);

        auto *topRow = new QHBoxLayout;
        auto *orderLabel = new QLabel(
            QStringLiteral("订单编号  %1")
                .arg(orderNo.isEmpty() ? QStringLiteral("--") : orderNo), card);
        orderLabel->setStyleSheet(QStringLiteral(
            "font-weight:700;color:#0f172a;"));
        auto *statusLabel = new QLabel(paymentStatusText(paymentStatus), card);
        statusLabel->setStyleSheet(paymentStatusStyle(paymentStatus));
        topRow->addWidget(orderLabel, 1);
        topRow->addWidget(statusLabel);

        auto *timeLabel = new QLabel(
            QStringLiteral("时间  %1").arg(orderTimeText(order)), card);
        timeLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
        auto *stationLabel = new QLabel(
            QStringLiteral("充电站  %1")
                .arg(stationName.isEmpty() ? QStringLiteral("--") : stationName), card);
        stationLabel->setWordWrap(true);
        stationLabel->setStyleSheet(QStringLiteral("color:#334155;"));

        auto *bottomRow = new QHBoxLayout;
        auto *stationNoLabel = new QLabel(
            QStringLiteral("站点编号  %1")
                .arg(stationNo.isEmpty() ? QStringLiteral("--") : stationNo), card);
        stationNoLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
        auto *feeLabel = new QLabel(
            QStringLiteral("消费  %1")
                .arg(hasFee ? moneyText(feeFen) : QStringLiteral("--")), card);
        feeLabel->setStyleSheet(QStringLiteral(
            "font-weight:700;color:#0f172a;"));
        bottomRow->addWidget(stationNoLabel);
        bottomRow->addStretch();
        bottomRow->addWidget(feeLabel);

        cardLayout->addLayout(topRow);
        cardLayout->addWidget(timeLabel);
        cardLayout->addWidget(stationLabel);
        cardLayout->addLayout(bottomRow);
        card->adjustSize();
        item->setSizeHint(QSize(m_ordersList->viewport()->width(),
                                card->sizeHint().height()));
        m_ordersList->setItemWidget(item, card);
    }
}
