#include "pages/favoritespage.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

FavoritesPage::FavoritesPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("favoritesPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *backButton = new QPushButton(QStringLiteral("返回"), this);
    backButton->setFlat(true);
    backButton->setStyleSheet(QStringLiteral(
        "color:#2563eb;font-weight:600;padding:6px;"));
    auto *title = new QLabel(QStringLiteral("收藏站点"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    m_refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshButton->setFlat(true);
    m_refreshButton->setStyleSheet(QStringLiteral(
        "color:#2563eb;font-weight:600;padding:6px;"));
    header->addWidget(backButton);
    header->addStretch();
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_refreshButton);
    layout->addLayout(header);

    m_summaryLabel = new QLabel(QStringLiteral("收藏状态以服务端为准"), this);
    m_summaryLabel->setStyleSheet(QStringLiteral(
        "padding:10px;background:#fef3c7;color:#92400e;border-radius:9px;"));
    layout->addWidget(m_summaryLabel);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("favoritesList"));
    m_list->setSpacing(8);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list, 1);

    auto *statePanel = new QWidget(this);
    auto *stateLayout = new QVBoxLayout(statePanel);
    m_stateLabel = new QLabel(statePanel);
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_stateLabel->setWordWrap(true);
    m_retryButton = new QPushButton(QStringLiteral("重新加载"), statePanel);
    m_retryButton->setMinimumHeight(40);
    m_retryButton->setStyleSheet(QStringLiteral(
        "background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;"));
    stateLayout->addStretch();
    stateLayout->addWidget(m_stateLabel);
    stateLayout->addWidget(m_retryButton);
    stateLayout->addStretch();
    layout->addWidget(statePanel, 1);
    statePanel->setObjectName(QStringLiteral("favoritesStatePanel"));

    connect(backButton, &QPushButton::clicked,
            this, &FavoritesPage::backRequested);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &FavoritesPage::stationsRequested);
    connect(m_retryButton, &QPushButton::clicked,
            this, &FavoritesPage::stationsRequested);
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
        emit stationSelected(item->data(Qt::UserRole).toJsonObject());
    });

    showState(QStringLiteral("暂无收藏站点\n收藏后可从这里快速打开"), false);
}

void FavoritesPage::showLoading()
{
    m_refreshButton->setDisabled(true);
    m_list->clear();
    showState(QStringLiteral("正在加载收藏站点…"), false);
    m_summaryLabel->setText(QStringLiteral("加载中"));
}

void FavoritesPage::showError(const QString &message)
{
    m_refreshButton->setDisabled(false);
    m_list->clear();
    showState(message.isEmpty() ? QStringLiteral("收藏站点加载失败") : message,
              true);
    m_summaryLabel->setText(QStringLiteral("加载失败"));
}

void FavoritesPage::setStations(const QJsonArray &stations)
{
    m_refreshButton->setDisabled(false);
    m_stations = stations;
    renderStations();
}

void FavoritesPage::applyFavorite(qint64 stationId, bool favorited)
{
    if (favorited) return;
    for (qsizetype index = 0; index < m_stations.size(); ++index) {
        const QJsonObject station = m_stations.at(index).toObject();
        if (station.value(QStringLiteral("stationId")).toVariant().toLongLong()
            != stationId) continue;
        m_stations.removeAt(index);
        renderStations();
        return;
    }
}

void FavoritesPage::showState(const QString &text, bool retryVisible)
{
    m_list->hide();
    findChild<QWidget *>(QStringLiteral("favoritesStatePanel"))->show();
    m_stateLabel->setText(text);
    m_retryButton->setVisible(retryVisible);
}

void FavoritesPage::renderStations()
{
    m_list->clear();
    if (m_stations.isEmpty()) {
        showState(QStringLiteral("暂无收藏站点\n收藏后可从这里快速打开"), false);
        m_summaryLabel->setText(QStringLiteral("暂无收藏"));
        return;
    }

    findChild<QWidget *>(QStringLiteral("favoritesStatePanel"))->hide();
    m_list->show();
    m_summaryLabel->setText(
        QStringLiteral("共 %1 个收藏站点").arg(m_stations.size()));
    for (const QJsonValue &value : m_stations) {
        const QJsonObject station = value.toObject();
        auto *item = new QListWidgetItem(
            QStringLiteral("★ %1\n%2\n电价 ¥%3/度 · 空闲 %4/%5")
                .arg(station.value(QStringLiteral("name")).toString(),
                     station.value(QStringLiteral("address")).toString())
                .arg(station.value(QStringLiteral("priceCentsPerKwh")).toDouble()
                         / 100.0, 0, 'f', 2)
                .arg(station.value(QStringLiteral("availablePiles")).toInt())
                .arg(station.value(QStringLiteral("totalPiles")).toInt()),
            m_list);
        item->setData(Qt::UserRole, QVariant::fromValue(station));
    }
}
