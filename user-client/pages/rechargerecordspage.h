#pragma once

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

class RechargeRecordsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit RechargeRecordsPage(QWidget *parent = nullptr);

public slots:
    void showLoading();
    void showError(const QString &message);
    void setRecords(const QJsonArray &records);

signals:
    void backRequested();
    void recordsRequested();

private:
    void showState(const QString &title, const QString &description,
                   bool retryVisible);
    void renderRecord(const QJsonObject &record);

    QLabel *m_summaryLabel = nullptr;
    QLabel *m_stateTitleLabel = nullptr;
    QLabel *m_stateDescriptionLabel = nullptr;
    QListWidget *m_recordsList = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_retryButton = nullptr;
};
