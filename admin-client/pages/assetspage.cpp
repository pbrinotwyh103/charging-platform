#include "pages/assetspage.h"

#include "widgets/adminuihelpers.h"
#include "widgets/paginationbar.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

namespace {

QString stationStatusText(const QString &status) {
  return status == QStringLiteral("offline") ? QStringLiteral("离线")
                                             : QStringLiteral("运营中");
}

QJsonObject stationFromDialog(QWidget *parent, const QJsonObject &initial,
                              bool *accepted) {
  QDialog dialog(parent);
  dialog.setWindowTitle(initial.isEmpty() ? QStringLiteral("新增充电站")
                                          : QStringLiteral("编辑充电站"));
  dialog.setMinimumWidth(340);
  auto *root = new QVBoxLayout(&dialog);
  auto *form = new QFormLayout;
  auto *name =
      new QLineEdit(initial.value(QStringLiteral("name")).toString(), &dialog);
  name->setMaxLength(80);
  auto *address = new QLineEdit(
      initial.value(QStringLiteral("address")).toString(), &dialog);
  address->setMaxLength(200);
  auto *longitude = new QDoubleSpinBox(&dialog);
  longitude->setRange(-180.0, 180.0);
  longitude->setDecimals(6);
  longitude->setValue(initial.value(QStringLiteral("longitude")).toDouble());
  auto *latitude = new QDoubleSpinBox(&dialog);
  latitude->setRange(-90.0, 90.0);
  latitude->setDecimals(6);
  latitude->setValue(initial.value(QStringLiteral("latitude")).toDouble());
  auto *price = new QDoubleSpinBox(&dialog);
  price->setRange(0.0, 99.99);
  price->setDecimals(2);
  price->setSuffix(QStringLiteral(" 元/kWh"));
  price->setValue(initial.value(QStringLiteral("priceCentsPerKwh")).toDouble() /
                  100.0);
  auto *status = new QComboBox(&dialog);
  status->addItem(QStringLiteral("运营中"), QStringLiteral("online"));
  status->addItem(QStringLiteral("离线"), QStringLiteral("offline"));
  const int statusIndex =
      status->findData(initial.value(QStringLiteral("status"))
                           .toString(QStringLiteral("online")));
  status->setCurrentIndex(qMax(0, statusIndex));
  form->addRow(QStringLiteral("名称"), name);
  form->addRow(QStringLiteral("地址"), address);
  form->addRow(QStringLiteral("经度"), longitude);
  form->addRow(QStringLiteral("纬度"), latitude);
  form->addRow(QStringLiteral("充电单价"), price);
  form->addRow(QStringLiteral("状态"), status);
  root->addLayout(form);
  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
    if (name->text().trimmed().isEmpty() ||
        address->text().trimmed().isEmpty()) {
      QMessageBox::warning(&dialog, QStringLiteral("输入不完整"),
                           QStringLiteral("充电站名称和地址不能为空"));
      return;
    }
    dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  root->addWidget(buttons);
  *accepted = dialog.exec() == QDialog::Accepted;
  if (!*accepted)
    return {};

  QJsonObject station = {
      {QStringLiteral("name"), name->text().trimmed()},
      {QStringLiteral("address"), address->text().trimmed()},
      {QStringLiteral("longitude"), longitude->value()},
      {QStringLiteral("latitude"), latitude->value()},
      {QStringLiteral("priceCentsPerKwh"), qRound64(price->value() * 100.0)},
      {QStringLiteral("status"), status->currentData().toString()}};
  const qint64 stationId =
      AdminUi::integerId(initial, QStringLiteral("stationId"));
  if (stationId > 0)
    station.insert(QStringLiteral("stationId"), stationId);
  return station;
}

} // namespace

AssetsPage::AssetsPage(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("assetsPage"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  auto *title = new QLabel(QStringLiteral("站点与电桩"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  root->addWidget(title);
  m_tabs = new QTabWidget(this);
  m_tabs->setObjectName(QStringLiteral("assetTabs"));
  m_tabs->addTab(createStationsTab(), QStringLiteral("充电站"));
  m_tabs->addTab(createPilesTab(), QStringLiteral("充电桩"));
  connect(m_tabs, &QTabWidget::currentChanged, this,
          [this] { requestRefresh(); });
  root->addWidget(m_tabs, 1);
  updateResponsiveLayout();
}

QWidget *AssetsPage::createStationsTab() {
  auto *page = new QWidget(this);
  auto *root = new QVBoxLayout(page);
  root->setContentsMargins(4, 8, 4, 4);
  root->setSpacing(8);
  auto *tools = new QGridLayout;
  m_stationSearch = new QLineEdit(page);
  m_stationSearch->setObjectName(QStringLiteral("stationSearchEdit"));
  m_stationSearch->setPlaceholderText(QStringLiteral("名称或地址"));
  auto *searchButton = new QPushButton(QStringLiteral("查询"), page);
  auto *addButton = new QPushButton(QStringLiteral("新增"), page);
  addButton->setObjectName(QStringLiteral("primaryButton"));
  m_editStationButton = new QPushButton(QStringLiteral("编辑"), page);
  m_editStationButton->setEnabled(false);
  tools->setHorizontalSpacing(8);
  tools->setVerticalSpacing(8);
  tools->addWidget(m_stationSearch, 0, 0);
  tools->addWidget(searchButton, 0, 1);
  tools->addWidget(addButton, 1, 0);
  tools->addWidget(m_editStationButton, 1, 1);
  tools->setColumnStretch(0, 1);
  tools->setColumnStretch(1, 1);
  root->addLayout(tools);
  connect(searchButton, &QPushButton::clicked, this,
          [this] { requestStations(1); });
  connect(m_stationSearch, &QLineEdit::returnPressed, searchButton,
          &QPushButton::click);
  connect(addButton, &QPushButton::clicked, this,
          [this] { openStationEditor(false); });
  connect(m_editStationButton, &QPushButton::clicked, this,
          [this] { openStationEditor(true); });

  m_stationTable = new QTableWidget(page);
  m_stationTable->setObjectName(QStringLiteral("stationTable"));
  m_stationTable->setColumnCount(6);
  m_stationTable->setHorizontalHeaderLabels(
      {QStringLiteral("名称"), QStringLiteral("地址"), QStringLiteral("单价"),
       QStringLiteral("状态"), QStringLiteral("电桩数"),
       QStringLiteral("更新时间")});
  m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_stationTable->setAlternatingRowColors(true);
  m_stationTable->verticalHeader()->setVisible(false);
  m_stationTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_stationTable->horizontalHeader()->setStretchLastSection(true);
  AdminUi::configureTouchTable(m_stationTable);
  connect(m_stationTable, &QTableWidget::itemSelectionChanged, this,
          &AssetsPage::updateStationSelection);
  root->addWidget(m_stationTable, 1);

  m_stationState = new QLabel(QStringLiteral("等待站点数据"), page);
  m_stationState->setObjectName(QStringLiteral("stationStateLabel"));
  m_stationState->setWordWrap(true);
  root->addWidget(m_stationState);
  m_stationDetailBox = new QGroupBox(QStringLiteral("站点详情"), page);
  m_stationDetailBox->setObjectName(QStringLiteral("stationDetailBox"));
  auto *detailLayout = new QVBoxLayout(m_stationDetailBox);
  m_stationDetail =
      new QLabel(QStringLiteral("请选择一个充电站"), m_stationDetailBox);
  m_stationDetail->setObjectName(QStringLiteral("stationDetailLabel"));
  m_stationDetail->setWordWrap(true);
  detailLayout->addWidget(m_stationDetail);
  root->addWidget(m_stationDetailBox);
  m_stationPagination = new PaginationBar(page);
  connect(m_stationPagination, &PaginationBar::pageRequested, this,
          &AssetsPage::requestStations);
  root->addWidget(m_stationPagination);
  return page;
}

QWidget *AssetsPage::createPilesTab() {
  auto *page = new QWidget(this);
  auto *root = new QVBoxLayout(page);
  root->setContentsMargins(4, 8, 4, 4);
  root->setSpacing(8);
  auto *filters = new QGridLayout;
  m_pileStationFilter = new QComboBox(page);
  m_pileStationFilter->setObjectName(QStringLiteral("pileStationFilter"));
  m_pileStationFilter->addItem(QStringLiteral("全部站点"), 0);
  m_pileStatusFilter = new QComboBox(page);
  m_pileStatusFilter->setObjectName(QStringLiteral("pileStatusFilter"));
  m_pileStatusFilter->addItem(QStringLiteral("全部状态"), QString());
  const QList<QPair<QString, QString>> statuses = {
      {QStringLiteral("空闲"), QStringLiteral("idle")},
      {QStringLiteral("已预约"), QStringLiteral("reserved")},
      {QStringLiteral("充电中"), QStringLiteral("charging")},
      {QStringLiteral("故障"), QStringLiteral("fault")},
      {QStringLiteral("离线"), QStringLiteral("offline")},
      {QStringLiteral("已停用"), QStringLiteral("disabled")}};
  for (const auto &status : statuses)
    m_pileStatusFilter->addItem(status.first, status.second);
  auto *refreshButton = new QPushButton(QStringLiteral("查询"), page);
  filters->setHorizontalSpacing(8);
  filters->setVerticalSpacing(8);
  filters->addWidget(m_pileStationFilter, 0, 0);
  filters->addWidget(m_pileStatusFilter, 0, 1);
  filters->addWidget(refreshButton, 1, 0, 1, 2);
  filters->setColumnStretch(0, 1);
  filters->setColumnStretch(1, 1);
  root->addLayout(filters);
  connect(m_pileStationFilter, &QComboBox::currentIndexChanged, this,
          [this] { requestPiles(1); });
  connect(m_pileStatusFilter, &QComboBox::currentIndexChanged, this,
          [this] { requestPiles(1); });
  connect(refreshButton, &QPushButton::clicked, this,
          [this] { requestPiles(1); });

  m_pileTable = new QTableWidget(page);
  m_pileTable->setObjectName(QStringLiteral("pileTable"));
  m_pileTable->setColumnCount(7);
  m_pileTable->setHorizontalHeaderLabels(
      {QStringLiteral("电桩编号"), QStringLiteral("站点"),
       QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态"),
       QStringLiteral("累计次数"), QStringLiteral("最后心跳")});
  m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_pileTable->setAlternatingRowColors(true);
  m_pileTable->verticalHeader()->setVisible(false);
  m_pileTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_pileTable->horizontalHeader()->setStretchLastSection(true);
  AdminUi::configureTouchTable(m_pileTable);
  connect(m_pileTable, &QTableWidget::itemSelectionChanged, this,
          &AssetsPage::updatePileSelection);
  root->addWidget(m_pileTable, 1);

  m_pileState = new QLabel(QStringLiteral("等待电桩数据"), page);
  m_pileState->setObjectName(QStringLiteral("pileStateLabel"));
  m_pileState->setWordWrap(true);
  root->addWidget(m_pileState);
  m_pileDetailBox = new QGroupBox(QStringLiteral("电桩详情"), page);
  m_pileDetailBox->setObjectName(QStringLiteral("pileDetailBox"));
  auto *detailLayout = new QVBoxLayout(m_pileDetailBox);
  m_pileDetail =
      new QLabel(QStringLiteral("请选择一个充电桩"), m_pileDetailBox);
  m_pileDetail->setObjectName(QStringLiteral("pileDetailLabel"));
  m_pileDetail->setWordWrap(true);
  detailLayout->addWidget(m_pileDetail);
  root->addWidget(m_pileDetailBox);

  auto *actions = new QGridLayout;
  m_stopPileButton = new QPushButton(QStringLiteral("停止"), page);
  m_stopPileButton->setObjectName(QStringLiteral("dangerButton"));
  m_restartPileButton = new QPushButton(QStringLiteral("重启"), page);
  m_enablePileButton = new QPushButton(QStringLiteral("启用"), page);
  m_disablePileButton = new QPushButton(QStringLiteral("停用"), page);
  actions->setHorizontalSpacing(8);
  actions->setVerticalSpacing(8);
  actions->addWidget(m_stopPileButton, 0, 0);
  actions->addWidget(m_restartPileButton, 0, 1);
  actions->addWidget(m_enablePileButton, 1, 0);
  actions->addWidget(m_disablePileButton, 1, 1);
  actions->setColumnStretch(0, 1);
  actions->setColumnStretch(1, 1);
  root->addLayout(actions);
  connect(m_stopPileButton, &QPushButton::clicked, this,
          [this] { controlSelectedPile(QStringLiteral("stop")); });
  connect(m_restartPileButton, &QPushButton::clicked, this,
          [this] { controlSelectedPile(QStringLiteral("restart")); });
  connect(m_enablePileButton, &QPushButton::clicked, this,
          [this] { controlSelectedPile(QStringLiteral("enable")); });
  connect(m_disablePileButton, &QPushButton::clicked, this,
          [this] { controlSelectedPile(QStringLiteral("disable")); });

  m_pilePagination = new PaginationBar(page);
  connect(m_pilePagination, &PaginationBar::pageRequested, this,
          &AssetsPage::requestPiles);
  root->addWidget(m_pilePagination);
  updatePileSelection();
  return page;
}

void AssetsPage::requestRefresh() {
  if (m_tabs->currentIndex() == 0)
    requestStations(m_stationPagination->currentPage());
  else
    requestPiles(m_pilePagination->currentPage());
}

void AssetsPage::setStations(const QJsonObject &payload) {
  const QJsonArray items = AdminUi::itemArray(payload);
  const QJsonObject meta = AdminUi::metaObject(payload);
  m_stationTable->setRowCount(items.size());
  for (int row = 0; row < items.size(); ++row) {
    const QJsonObject station = items.at(row).toObject();
    auto *nameItem =
        new QTableWidgetItem(station.value(QStringLiteral("name")).toString());
    nameItem->setData(Qt::UserRole, station.toVariantMap());
    m_stationTable->setItem(row, 0, nameItem);
    m_stationTable->setItem(
        row, 1,
        new QTableWidgetItem(
            station.value(QStringLiteral("address")).toString()));
    m_stationTable->setItem(
        row, 2,
        new QTableWidgetItem(QStringLiteral("¥%1").arg(
            station.value(QStringLiteral("priceCentsPerKwh")).toDouble() /
                100.0,
            0, 'f', 2)));
    const QString stationStatus =
        station.value(QStringLiteral("status")).toString();
    auto *statusItem =
        new QTableWidgetItem(stationStatusText(stationStatus));
    AdminUi::styleStateItem(statusItem,
                            stationStatus == QStringLiteral("online")
                                ? AdminUi::Tone::Success
                                : AdminUi::Tone::Neutral);
    m_stationTable->setItem(row, 3, statusItem);
    m_stationTable->setItem(
        row, 4,
        new QTableWidgetItem(QString::number(
            station.value(QStringLiteral("pileCount")).toInt())));
    m_stationTable->setItem(
        row, 5,
        new QTableWidgetItem(
            station.value(QStringLiteral("updatedAt")).toString()));
  }
  m_stationPagination->setPage(
      meta.value(QStringLiteral("page")).toInt(1),
      meta.value(QStringLiteral("totalPages")).toInt(1),
      meta.value(QStringLiteral("total")).toInt(items.size()));
  m_stationState->setText(
      items.isEmpty() ? QStringLiteral("没有匹配的充电站")
                      : QStringLiteral("本页 %1 个充电站").arg(items.size()));
  m_selectedStation = {};
  updateStationSelection();

  const QSignalBlocker blocker(m_pileStationFilter);
  const QVariant selected = m_pileStationFilter->currentData();
  m_pileStationFilter->clear();
  m_pileStationFilter->addItem(QStringLiteral("全部站点"), 0);
  for (const QJsonValue &value : items) {
    const QJsonObject station = value.toObject();
    m_pileStationFilter->addItem(
        station.value(QStringLiteral("name")).toString(),
        AdminUi::integerId(station, QStringLiteral("stationId")));
  }
  const int index = m_pileStationFilter->findData(selected);
  m_pileStationFilter->setCurrentIndex(index < 0 ? 0 : index);
}

void AssetsPage::setStationDetail(const QJsonObject &payload) {
  const QJsonObject station = AdminUi::dataObject(payload);
  if (!station.isEmpty())
    m_selectedStation = station;
  m_stationDetail->setText(
      QStringLiteral("%1\n%2\n坐标：%3, %4\n单价：%5\n电桩：%6 个 · 状态：%7")
          .arg(
              station.value(QStringLiteral("name"))
                  .toString(QStringLiteral("--")),
              station.value(QStringLiteral("address"))
                  .toString(QStringLiteral("--")),
              QString::number(
                  station.value(QStringLiteral("longitude")).toDouble(), 'f',
                  6),
              QString::number(
                  station.value(QStringLiteral("latitude")).toDouble(), 'f', 6),
              AdminUi::money(static_cast<qint64>(
                  station.value(QStringLiteral("priceCentsPerKwh"))
                      .toDouble())),
              QString::number(
                  station.value(QStringLiteral("pileCount")).toInt()),
              stationStatusText(
                  station.value(QStringLiteral("status")).toString())));
}

void AssetsPage::setPiles(const QJsonObject &payload) {
  const QJsonArray items = AdminUi::itemArray(payload);
  const QJsonObject meta = AdminUi::metaObject(payload);
  m_pileTable->setRowCount(items.size());
  for (int row = 0; row < items.size(); ++row) {
    const QJsonObject pile = items.at(row).toObject();
    auto *codeItem =
        new QTableWidgetItem(pile.value(QStringLiteral("pileCode")).toString());
    codeItem->setData(Qt::UserRole, pile.toVariantMap());
    m_pileTable->setItem(row, 0, codeItem);
    m_pileTable->setItem(
        row, 1,
        new QTableWidgetItem(
            pile.value(QStringLiteral("stationName")).toString()));
    m_pileTable->setItem(
        row, 2,
        new QTableWidgetItem(
            pile.value(QStringLiteral("chargeType")).toString() ==
                    QStringLiteral("fast")
                ? QStringLiteral("快充")
                : QStringLiteral("慢充")));
    m_pileTable->setItem(
        row, 3,
        new QTableWidgetItem(QStringLiteral("%1 kW").arg(
            pile.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 1)));
    const QString pileStatus = pile.value(QStringLiteral("status")).toString();
    auto *statusItem = new QTableWidgetItem(AdminUi::pileStatus(pileStatus));
    AdminUi::styleStateItem(statusItem, AdminUi::pileTone(pileStatus));
    m_pileTable->setItem(row, 4, statusItem);
    m_pileTable->setItem(
        row, 5,
        new QTableWidgetItem(QString::number(
            pile.value(QStringLiteral("totalChargeCount")).toInt())));
    m_pileTable->setItem(
        row, 6,
        new QTableWidgetItem(
            pile.value(QStringLiteral("lastHeartbeatAt")).toString()));
  }
  m_pilePagination->setPage(
      meta.value(QStringLiteral("page")).toInt(1),
      meta.value(QStringLiteral("totalPages")).toInt(1),
      meta.value(QStringLiteral("total")).toInt(items.size()));
  m_pileState->setText(
      items.isEmpty() ? QStringLiteral("没有匹配的电桩")
                      : QStringLiteral("本页 %1 个电桩").arg(items.size()));
  m_selectedPile = {};
  updatePileSelection();
}

void AssetsPage::setPileDetail(const QJsonObject &payload) {
  const QJsonObject pile = AdminUi::dataObject(payload);
  if (!pile.isEmpty())
    m_selectedPile = pile;
  m_pileDetail->setText(
      QStringLiteral("%1 · %2\n站点：%3\n功率：%4 kW · 状态：%5\n累计充电：%6 "
                     "次 / %7 小时\n最后心跳：%8")
          .arg(pile.value(QStringLiteral("pileCode"))
                   .toString(QStringLiteral("--")),
               pile.value(QStringLiteral("chargeType")).toString() ==
                       QStringLiteral("fast")
                   ? QStringLiteral("快充")
                   : QStringLiteral("慢充"),
               pile.value(QStringLiteral("stationName"))
                   .toString(QStringLiteral("--")),
               QString::number(pile.value(QStringLiteral("powerKw")).toDouble(),
                               'f', 1),
               AdminUi::pileStatus(
                   pile.value(QStringLiteral("status")).toString()),
               QString::number(
                   pile.value(QStringLiteral("totalChargeCount")).toInt()),
               QString::number(
                   pile.value(QStringLiteral("totalChargeSeconds")).toDouble() /
                       3600.0,
                   'f', 1),
               pile.value(QStringLiteral("lastHeartbeatAt"))
                   .toString(QStringLiteral("--"))));
}

void AssetsPage::applyDevicePush(const QJsonObject &payload) {
  const qint64 pileId = AdminUi::integerId(payload, QStringLiteral("pileId"));
  for (int row = 0; row < m_pileTable->rowCount(); ++row) {
    const QJsonObject pile = QJsonObject::fromVariantMap(
        m_pileTable->item(row, 0)->data(Qt::UserRole).toMap());
    if (AdminUi::integerId(pile, QStringLiteral("pileId")) == pileId) {
      const QString status =
          payload.value(QStringLiteral("status")).toString();
      m_pileTable->item(row, 4)->setText(AdminUi::pileStatus(status));
      AdminUi::styleStateItem(m_pileTable->item(row, 4),
                              AdminUi::pileTone(status));
      m_pileTable->item(row, 6)->setText(
          payload.value(QStringLiteral("updatedAt")).toString());
      return;
    }
  }
}

void AssetsPage::operationSucceeded(const QString &action,
                                    const QJsonObject &payload) {
  Q_UNUSED(payload)
  if (action.startsWith(QStringLiteral("stations."))) {
    m_stationState->setText(QStringLiteral("充电站保存成功"));
    requestStations(m_stationPagination->currentPage());
  } else if (action == QStringLiteral("piles.control")) {
    m_pileState->setText(QStringLiteral("远程操作已提交"));
    requestPiles(m_pilePagination->currentPage());
  }
}

void AssetsPage::setLoading(const QString &action, bool loading) {
  if (action == QStringLiteral("piles.control")) {
    const bool enabled = !loading && !m_selectedPile.isEmpty();
    for (QPushButton *button : {m_stopPileButton, m_restartPileButton,
                                m_enablePileButton, m_disablePileButton}) {
      button->setEnabled(enabled);
    }
  }
  if (!loading)
    return;
  if (action.startsWith(QStringLiteral("stations."))) {
    m_stationState->setText(QStringLiteral("正在处理充电站数据…"));
  } else if (action.startsWith(QStringLiteral("piles."))) {
    m_pileState->setText(QStringLiteral("正在处理电桩数据…"));
  }
}

void AssetsPage::setError(const QString &action, const QString &message) {
  if (action.startsWith(QStringLiteral("stations.")))
    m_stationState->setText(message);
  else
    m_pileState->setText(message);
}

void AssetsPage::requestStations(int page) {
  emit commandRequested(
      QStringLiteral("stations.list"),
      {{QStringLiteral("page"), page},
       {QStringLiteral("pageSize"), 15},
       {QStringLiteral("keyword"), m_stationSearch->text().trimmed()}});
}

void AssetsPage::requestPiles(int page) {
  emit commandRequested(QStringLiteral("piles.list"),
                        {{QStringLiteral("page"), page},
                         {QStringLiteral("pageSize"), 15},
                         {QStringLiteral("stationId"),
                          m_pileStationFilter->currentData().toInt()},
                         {QStringLiteral("status"),
                          m_pileStatusFilter->currentData().toString()}});
}

void AssetsPage::updateStationSelection() {
  const int row = m_stationTable->currentRow();
  const bool selected = row >= 0 && m_stationTable->item(row, 0);
  m_editStationButton->setEnabled(selected);
  if (!selected) {
    m_selectedStation = {};
    m_stationDetail->setText(QStringLiteral("请选择一个充电站"));
    updateResponsiveLayout();
    return;
  }
  m_selectedStation = QJsonObject::fromVariantMap(
      m_stationTable->item(row, 0)->data(Qt::UserRole).toMap());
  updateResponsiveLayout();
  setStationDetail({{QStringLiteral("data"), m_selectedStation}});
  const qint64 stationId =
      AdminUi::integerId(m_selectedStation, QStringLiteral("stationId"));
  if (stationId > 0) {
    emit commandRequested(QStringLiteral("stations.detail"),
                          {{QStringLiteral("stationId"), stationId}});
  }
}

void AssetsPage::updatePileSelection() {
  const int row = m_pileTable ? m_pileTable->currentRow() : -1;
  const bool selected = row >= 0 && m_pileTable->item(row, 0);
  for (QPushButton *button : {m_stopPileButton, m_restartPileButton,
                              m_enablePileButton, m_disablePileButton}) {
    if (button)
      button->setEnabled(selected);
  }
  if (!selected) {
    m_selectedPile = {};
    if (m_pileDetail)
      m_pileDetail->setText(QStringLiteral("请选择一个充电桩"));
    updateResponsiveLayout();
    return;
  }
  m_selectedPile = QJsonObject::fromVariantMap(
      m_pileTable->item(row, 0)->data(Qt::UserRole).toMap());
  updateResponsiveLayout();
  setPileDetail({{QStringLiteral("data"), m_selectedPile}});
  const qint64 pileId =
      AdminUi::integerId(m_selectedPile, QStringLiteral("pileId"));
  if (pileId > 0) {
    emit commandRequested(QStringLiteral("piles.detail"),
                          {{QStringLiteral("pileId"), pileId}});
  }
}

void AssetsPage::openStationEditor(bool editExisting) {
  if (editExisting && m_selectedStation.isEmpty())
    return;
  bool accepted = false;
  QJsonObject station = stationFromDialog(
      this, editExisting ? m_selectedStation : QJsonObject(), &accepted);
  if (!accepted)
    return;
  emit commandRequested(editExisting ? QStringLiteral("stations.update")
                                     : QStringLiteral("stations.create"),
                        station);
}

void AssetsPage::controlSelectedPile(const QString &command) {
  const qint64 pileId =
      AdminUi::integerId(m_selectedPile, QStringLiteral("pileId"));
  if (pileId <= 0)
    return;
  const QHash<QString, QString> labels = {
      {QStringLiteral("stop"), QStringLiteral("停止")},
      {QStringLiteral("restart"), QStringLiteral("重启")},
      {QStringLiteral("enable"), QStringLiteral("启用")},
      {QStringLiteral("disable"), QStringLiteral("停用")}};
  const QString label = labels.value(command, command);
  const auto answer = QMessageBox::question(
      this, QStringLiteral("确认远程操作"),
      QStringLiteral("确认对电桩 %1 执行“%2”？")
          .arg(m_selectedPile.value(QStringLiteral("pileCode")).toString(),
               label));
  if (answer == QMessageBox::Yes) {
    emit commandRequested(QStringLiteral("piles.control"),
                          {{QStringLiteral("pileId"), pileId},
                           {QStringLiteral("command"), command}});
  }
}

void AssetsPage::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateResponsiveLayout();
}

void AssetsPage::updateResponsiveLayout() {
  const bool compact = width() < 720;
  AdminUi::setResponsiveColumns(m_stationTable, {0, 3, 4}, compact);
  AdminUi::setResponsiveColumns(m_pileTable, {0, 4, 6}, compact);
  if (m_stationDetailBox)
    m_stationDetailBox->setVisible(!compact || !m_selectedStation.isEmpty());
  if (m_pileDetailBox)
    m_pileDetailBox->setVisible(!compact || !m_selectedPile.isEmpty());
  for (QPushButton *button : {m_stopPileButton, m_restartPileButton,
                              m_enablePileButton, m_disablePileButton}) {
    if (button)
      button->setVisible(!compact || !m_selectedPile.isEmpty());
  }
}
