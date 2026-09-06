#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class PaginationBar;
class QPushButton;
class QResizeEvent;
class QTableWidget;
class QTabWidget;
class QTimer;

class RecordsPage final : public QWidget {
  Q_OBJECT

public:
  explicit RecordsPage(QWidget *parent = nullptr);

  bool exportOrdersCsv(const QString &filePath, QString *error = nullptr) const;

public slots:
  void requestRefresh();
  void setUsers(const QJsonObject &payload);
  void setOrders(const QJsonObject &payload);
  void operationSucceeded(const QString &action, const QJsonObject &payload);
  void setLoading(const QString &action, bool loading);
  void setError(const QString &action, const QString &message);

signals:
  void commandRequested(const QString &action, const QJsonObject &parameters);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QWidget *createUsersTab();
  QWidget *createOrdersTab();
  void requestUsers(int page);
  void requestOrders(int page);
  void updateUserSelection();
  void updateOrderSelection();
  void updateResponsiveLayout();
  void exportOrdersInteractively();

  QTabWidget *m_tabs = nullptr;
  QLineEdit *m_phoneSearch = nullptr;
  QTimer *m_searchTimer = nullptr;
  QTableWidget *m_userTable = nullptr;
  QLabel *m_userState = nullptr;
  PaginationBar *m_userPagination = nullptr;
  QPushButton *m_freezeButton = nullptr;
  QJsonObject m_selectedUser;

  QComboBox *m_orderStatus = nullptr;
  QLineEdit *m_orderNumber = nullptr;
  QLineEdit *m_orderPhone = nullptr;
  QCheckBox *m_limitDates = nullptr;
  QDateEdit *m_fromDate = nullptr;
  QDateEdit *m_toDate = nullptr;
  QTableWidget *m_orderTable = nullptr;
  QLabel *m_orderState = nullptr;
  PaginationBar *m_orderPagination = nullptr;
  QPushButton *m_exportButton = nullptr;
  QJsonArray m_orders;
};
