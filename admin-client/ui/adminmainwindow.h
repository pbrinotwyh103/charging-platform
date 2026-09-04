#pragma once

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QSpinBox;

class AdminMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminMainWindow(QWidget *parent = nullptr);

public slots:
    void setConnectionStatus(const QString &text, bool connected);

signals:
    void connectionRequested(const QString &host, quint16 port);

private:
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
};
