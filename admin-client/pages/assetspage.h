#pragma once

#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class PaginationBar;
class QPushButton;
class QResizeEvent;
class QTableWidget;
class QTabWidget;

class AssetsPage final : public QWidget {
  Q_OBJECT

public:
  explicit AssetsPage(QWidget *parent = nullptr);

public slots:
  void requestRefresh();
  void setStations(const QJsonObject &payload);
  void setStationDetail(const QJsonObject &payload);
  void setPiles(const QJsonObject &payload);
  void setPileDetail(const QJsonObject &payload);
  void applyDevicePush(const QJsonObject &payload);
  void operationSucceeded(const QString &action, const QJsonObject &payload);
  void setLoading(const QString &action, bool loading);
  void setError(const QString &action, const QString &message);

signals:
  void commandRequested(const QString &action, const QJsonObject &parameters);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QWidget *createStationsTab();
  QWidget *createPilesTab();
  void requestStations(int page);
  void requestPiles(int page);
  void updateStationSelection();
  void updatePileSelection();
  void openStationEditor(bool editExisting);
  void controlSelectedPile(const QString &command);
  void updateResponsiveLayout();

  QTabWidget *m_tabs = nullptr;
  QLineEdit *m_stationSearch = nullptr;
  QTableWidget *m_stationTable = nullptr;
  QLabel *m_stationState = nullptr;
  QGroupBox *m_stationDetailBox = nullptr;
  QLabel *m_stationDetail = nullptr;
  PaginationBar *m_stationPagination = nullptr;
  QPushButton *m_editStationButton = nullptr;
  QJsonObject m_selectedStation;

  QComboBox *m_pileStationFilter = nullptr;
  QComboBox *m_pileStatusFilter = nullptr;
  QTableWidget *m_pileTable = nullptr;
  QLabel *m_pileState = nullptr;
  QGroupBox *m_pileDetailBox = nullptr;
  QLabel *m_pileDetail = nullptr;
  PaginationBar *m_pilePagination = nullptr;
  QPushButton *m_stopPileButton = nullptr;
  QPushButton *m_restartPileButton = nullptr;
  QPushButton *m_enablePileButton = nullptr;
  QPushButton *m_disablePileButton = nullptr;
  QJsonObject m_selectedPile;
};
