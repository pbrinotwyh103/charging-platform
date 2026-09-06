#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
class QLabel; class QListWidget; class QPushButton; class QSpinBox;
class StationDetailPage final : public QWidget { Q_OBJECT
public:
    explicit StationDetailPage(QWidget *parent = nullptr);
    void setStation(const QJsonObject &station);
    void setPiles(const QJsonArray &piles);
    void showError(const QString &message);

public slots:
    void favoriteUpdated(qint64 stationId, bool favorited,
                         const QString &updatedAt);
    void favoriteUpdateFailed(qint64 stationId, const QString &message);
    void reservationAccepted();
    void reservationFailed(const QString &message);

signals:
    void backRequested();
    void pilesRequested(qint64 stationId);
    void favoriteRequested(qint64 stationId, bool favorited);
    void favoriteStateChanged(qint64 stationId, bool favorited);
    void reservationRequested(qint64 stationId, qint64 pileId,
                              int durationMinutes);

private:
    void updateFavoriteButton();
    void updateReservationButton();

    QJsonObject m_station;
    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_favoriteStatus = nullptr;
    QListWidget *m_piles = nullptr;
    QPushButton *m_favorite = nullptr;
    QPushButton *m_reserve = nullptr;
    QSpinBox *m_duration = nullptr;
    bool m_favoritePending = false;
    bool m_reservationPending = false;
    bool m_previousFavorite = false;
};
