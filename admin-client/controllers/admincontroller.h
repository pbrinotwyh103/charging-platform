#pragma once

#include "network/clientconnection.h"

#include <QObject>

class AdminController final : public QObject
{
    Q_OBJECT

public:
    explicit AdminController(QObject *parent = nullptr);

public slots:
    void connectToServer(const QString &host, quint16 port);

signals:
    void statusTextChanged(const QString &text, bool connected);

private:
    Charging::ClientConnection m_connection;
};
