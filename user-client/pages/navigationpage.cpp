#include "pages/navigationpage.h"

#include "map/mapnavigator.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace {
class RestrictedMapPage final : public QWebEnginePage
{
public:
    using QWebEnginePage::QWebEnginePage;

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type,
                                 bool isMainFrame) override
    {
        Q_UNUSED(type)
        return !isMainFrame || MapNavigator::isAllowedNavigationUrl(url);
    }
};
}

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("navigationPage"));
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(12, 12, 12, 12);
    m_rootLayout->setSpacing(10);

    auto *back = new QPushButton(QStringLiteral("← 返回充电站详情"), this);
    back->setObjectName(QStringLiteral("navigationBackButton"));
    back->setFlat(true);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("navigationTitle"));
    QFont titleFont = m_title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("navigationStatusLabel"));
    m_status->setWordWrap(true);
    m_retry = new QPushButton(QStringLiteral("重新加载路线"), this);
    m_retry->setObjectName(QStringLiteral("navigationRetryButton"));
    m_retry->hide();
    m_webView = new QWebEngineView(this);
    m_webView->setObjectName(QStringLiteral("navigationWebView"));
    m_webView->setPage(new RestrictedMapPage(m_webView));

    m_rootLayout->addWidget(back);
    m_rootLayout->addWidget(m_title);
    m_rootLayout->addWidget(m_status);
    m_rootLayout->addWidget(m_retry);
    m_rootLayout->addWidget(m_webView, 1);

    connect(back, &QPushButton::clicked, this, &NavigationPage::backRequested);
    connect(m_retry, &QPushButton::clicked, this, &NavigationPage::loadRoute);
    connect(m_webView, &QWebEngineView::loadStarted, this, [this] {
        if (m_routeUrl.isEmpty()) return;
        m_status->setText(QStringLiteral("正在加载腾讯地图路线…"));
        m_retry->hide();
    });
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (m_routeUrl.isEmpty()) return;
        m_status->setText(ok ? QStringLiteral("路线加载完成")
                             : QStringLiteral("路线加载失败，请检查网络后重试。"));
        m_retry->setVisible(!ok);
    });
}

void NavigationPage::setCompactLayout(bool compact)
{
    m_rootLayout->setContentsMargins(compact ? 4 : 16, compact ? 6 : 14,
                                     compact ? 4 : 16, compact ? 6 : 14);
    m_rootLayout->setSpacing(compact ? 7 : 10);
    m_webView->setMinimumHeight(compact ? 260 : 360);
}

void NavigationPage::showRoute(const QJsonObject &station, const QString &mode,
                               double fromLatitude, double fromLongitude, bool hasOrigin)
{
    m_station = station;
    m_mode = mode == QStringLiteral("walking") ? QStringLiteral("walking")
                                                : QStringLiteral("driving");
    const QString modeText = m_mode == QStringLiteral("walking")
        ? QStringLiteral("步行") : QStringLiteral("驾车");
    m_title->setText(QStringLiteral("%1导航 · %2")
                         .arg(modeText, station.value(QStringLiteral("name")).toString()));
    m_routeUrl = hasOrigin
        ? MapNavigator::navigationUrl(fromLatitude, fromLongitude,
                                      station.value(QStringLiteral("latitude")).toDouble(),
                                      station.value(QStringLiteral("longitude")).toDouble(), m_mode)
        : QUrl();
    if (m_routeUrl.isEmpty()) showFallback();
    else loadRoute();
}

void NavigationPage::loadRoute()
{
    if (m_routeUrl.isEmpty()) {
        showFallback();
        return;
    }
    m_status->setText(QStringLiteral("正在加载腾讯地图路线…"));
    m_retry->hide();
    m_webView->setUrl(m_routeUrl);
}

void NavigationPage::showFallback()
{
    const bool noKey = !MapNavigator::isConfigured();
    m_status->setText(noKey
        ? QStringLiteral("未配置腾讯地图 Key，当前显示路线估算。")
        : QStringLiteral("当前位置或充电站坐标无效，无法加载真实路线。"));
    m_retry->hide();
    const double distance = m_station.value(QStringLiteral("distanceKm")).toDouble();
    const bool walking = m_mode == QStringLiteral("walking");
    const int minutes = walking ? qMax(8, qRound(distance * 13))
                                : qMax(4, qRound(distance * 4));
    m_webView->setHtml(QStringLiteral(
        "<html><body style='margin:0;background:%1;font-family:sans-serif;color:%2'>"
        "<div style='padding:24px'><h2>%3路线预览</h2>"
        "<p>当前位置 → <b>%4</b></p><div style='border-left:4px solid %5;"
        "padding:12px 18px;margin-top:20px'>预计 %6 公里 · 约 %7 分钟</div>"
        "<p><small>该内容仅供演示，不代替实时导航。</small></p></div></body></html>")
        .arg(walking ? QStringLiteral("#f0fdf4") : QStringLiteral("#eff6ff"),
             walking ? QStringLiteral("#166534") : QStringLiteral("#1e3a8a"),
             walking ? QStringLiteral("步行") : QStringLiteral("驾车"),
             m_station.value(QStringLiteral("name")).toString().toHtmlEscaped(),
             walking ? QStringLiteral("#16a34a") : QStringLiteral("#2563eb"))
        .arg(distance, 0, 'f', 1).arg(minutes),
        QUrl(QStringLiteral("https://map.qq.com/offline-preview")));
}
