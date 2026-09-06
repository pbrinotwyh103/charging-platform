#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;

class RechargeDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RechargeDialog(qint64 currentBalanceFen, QWidget *parent = nullptr);

    qint64 amountFen() const { return m_amountFen; }

private:
    bool parseAmount(qint64 *amountFen) const;
    void confirmRecharge();
    void updateAmountSummary();

    QLineEdit *m_amountEdit = nullptr;
    QLabel *m_amountSummaryLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    qint64 m_amountFen = 0;
};
