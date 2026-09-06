#include "pages/navigationpage.h"

#include "map/mapnavigator.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace {

class TencentMapPage final : public QWebEnginePage
{
public:
    explicit TencentMapPage(QObject *parent = nullptr)
        : QWebEnginePage(parent)
    {
    }

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type,
                                 bool isMainFrame) override
    {
        Q_UNUSED(type);
        if (!isMainFrame) return true;
        const QString host = url.host().toLower();
        const bool allowedHost = host == QStringLiteral("apis.map.qq.com")
            || host.endsWith(QStringLiteral(".map.qq.com"));
        return url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
            && allowedHost;
    }
};

bool validCoordinate(double latitude, double longitude)
{
    return qIsFinite(latitude) && qIsFinite(longitude)
        && latitude >= -90.0 && latitude <= 90.0
        && longitude >= -180.0 && longitude <= 180.0;
}

} // namespace

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("navigationPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("返回站点"), this);
    back->setObjectName(QStringLiteral("navigationBackButton"));
    back->setFlat(true);
    back->setStyleSheet(QStringLiteral("color:#2563eb;font-weight:600;padding:6px;"));
    m_destination = new QLabel(this);
    m_destination->setObjectName(QStringLiteral("navigationDestinationLabel"));
    m_destination->setWordWrap(true);
    auto *modeLabel = new QLabel(QStringLiteral("路线"), this);
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("navigationModeCombo"));
    m_mode->addItem(QStringLiteral("驾车"), QStringLiteral("driving"));
    m_mode->addItem(QStringLiteral("步行"), QStringLiteral("walking"));
    header->addWidget(back);
    header->addWidget(m_destination, 1);
    header->addWidget(modeLabel);
    header->addWidget(m_mode);
    layout->addLayout(header);

    m_state = new QLabel(this);
    m_state->setObjectName(QStringLiteral("navigationStateLabel"));
    m_state->setWordWrap(true);
    m_state->setStyleSheet(QStringLiteral("color:#64748b;padding:8px;"));
    layout->addWidget(m_state);

    m_view = new QWebEngineView(this);
    m_view->setObjectName(QStringLiteral("navigationWebView"));
    m_view->setPage(new TencentMapPage(m_view));
    layout->addWidget(m_view, 1);

    m_retry = new QPushButton(QStringLiteral("重新加载路线"), this);
    m_retry->setObjectName(QStringLiteral("navigationRetryButton"));
    m_retry->setMinimumHeight(40);
    layout->addWidget(m_retry);

    connect(back, &QPushButton::clicked, this, &NavigationPage::backRequested);
    connect(m_mode, &QComboBox::currentIndexChanged,
            this, [this](int) { loadRoute(); });
    connect(m_retry, &QPushButton::clicked, this, &NavigationPage::loadRoute);
    connect(m_view, &QWebEngineView::loadStarted, this, [this] {
        m_retry->hide();
        m_state->setText(QStringLiteral("正在加载腾讯地图路线…"));
    });
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (m_showingFallback) return;
        if (ok) {
            m_state->setText(QStringLiteral("路线已加载，可切换驾车/步行方式"));
            m_retry->hide();
        } else {
            showFallback(QStringLiteral("腾讯地图页面加载失败，可继续手动查看站点详情"), true);
        }
    });
    m_retry->hide();
    showFallback(QStringLiteral("请选择站点后加载路线"), false);
}

void NavigationPage::setRoute(double fromLatitude, double fromLongitude,
                               double toLatitude, double toLongitude,
                               const QString &destinationName)
{
    m_fromLatitude = fromLatitude;
    m_fromLongitude = fromLongitude;
    m_toLatitude = toLatitude;
    m_toLongitude = toLongitude;
    m_destinationName = destinationName.trimmed();
    m_destination->setText(m_destinationName.isEmpty()
        ? QStringLiteral("站点路线")
        : QStringLiteral("前往：%1").arg(m_destinationName));
    loadRoute();
}

void NavigationPage::loadRoute()
{
    if (!validCoordinate(m_fromLatitude, m_fromLongitude)
        || !validCoordinate(m_toLatitude, m_toLongitude)) {
        showFallback(QStringLiteral("当前位置或站点坐标不可用，请返回后使用地址搜索"), false);
        return;
    }
    const QString mode = m_mode->currentData().toString();
    const QUrl url = MapNavigator::navigationUrl(
        m_fromLatitude, m_fromLongitude, m_toLatitude, m_toLongitude, mode);
    if (!url.isValid() || url.scheme() != QStringLiteral("https")
        || url.host() != QStringLiteral("apis.map.qq.com")) {
        showFallback(QStringLiteral("未配置腾讯地图 Key，已降级为手动找桩；不影响站点列表和距离排序"), false);
        return;
    }
    m_view->show();
    m_showingFallback = false;
    m_state->setText(QStringLiteral("正在加载腾讯地图路线…"));
    m_retry->hide();
    m_view->load(url);
}

void NavigationPage::showFallback(const QString &message, bool canRetry)
{
    m_showingFallback = true;
    m_view->stop();
    m_view->setHtml(QStringLiteral(
        "<html><body style='font-family:sans-serif;color:#475569;padding:24px'>"
        "<h3>地图暂不可用</h3><p>%1</p>"
        "<p>你仍可以返回首页，通过地址解析、模拟定位和距离排序选择充电站。</p>"
        "</body></html>").arg(message.toHtmlEscaped()));
    m_state->setText(message);
    m_retry->setVisible(canRetry);
}
