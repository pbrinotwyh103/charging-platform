#include "widgets/rechargedialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace {

QString formatMoney(qint64 fen)
{
    return QStringLiteral("¥ %1.%2")
        .arg(fen / 100)
        .arg(fen % 100, 2, 10, QLatin1Char('0'));
}

} // namespace

RechargeDialog::RechargeDialog(qint64 currentBalanceFen, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("rechargeDialog"));
    setWindowTitle(QStringLiteral("钱包充值"));
    setModal(true);
    setMinimumWidth(330);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("钱包充值"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(17);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *descriptionLabel = new QLabel(
        QStringLiteral("本功能为课程演示模拟充值，不会发起真实支付。"), this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet(QStringLiteral("color:#64748b;"));

    auto *balanceLabel = new QLabel(
        QStringLiteral("当前余额\n%1").arg(formatMoney(currentBalanceFen)), this);
    balanceLabel->setStyleSheet(QStringLiteral(
        "padding:14px;background:#eff6ff;color:#1e3a8a;border-radius:10px;font-size:15px;font-weight:600;"));

    auto *amountTitle = new QLabel(QStringLiteral("充值金额（元）"), this);
    amountTitle->setStyleSheet(QStringLiteral("color:#334155;font-weight:600;"));
    m_amountEdit = new QLineEdit(this);
    m_amountEdit->setObjectName(QStringLiteral("rechargeAmountEdit"));
    m_amountEdit->setPlaceholderText(QStringLiteral("请输入 1.00 - 5000.00"));
    m_amountEdit->setInputMethodHints(Qt::ImhFormattedNumbersOnly);
    m_amountEdit->setMinimumHeight(42);
    m_amountEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,4}(?:\\.[0-9]{0,2})?")),
        m_amountEdit));
    m_amountEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{padding:0 12px;border:1px solid #cbd5e1;border-radius:9px;background:white;}"
        "QLineEdit:focus{border:1px solid #2563eb;}"));

    auto *quickAmounts = new QHBoxLayout;
    quickAmounts->setSpacing(8);
    for (const int amount : {50, 100, 200}) {
        auto *button = new QPushButton(QStringLiteral("¥%1").arg(amount), this);
        button->setMinimumHeight(34);
        button->setStyleSheet(QStringLiteral(
            "QPushButton{background:#f8fafc;color:#475569;border:1px solid #cbd5e1;border-radius:8px;}"
            "QPushButton:hover{border-color:#60a5fa;color:#1d4ed8;}"));
        quickAmounts->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, amount] {
            m_amountEdit->setText(QString::number(amount));
        });
    }

    m_amountSummaryLabel = new QLabel(QStringLiteral("确认金额：--"), this);
    m_amountSummaryLabel->setStyleSheet(QStringLiteral(
        "padding:11px;background:#f8fafc;color:#475569;border-radius:8px;"));
    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("rechargeAmountErrorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#b91c1c;"));

    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    auto *confirmButton = new QPushButton(QStringLiteral("确认充值"), this);
    confirmButton->setObjectName(QStringLiteral("rechargeConfirmButton"));
    cancelButton->setMinimumHeight(42);
    confirmButton->setMinimumHeight(42);
    cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#f8fafc;color:#475569;border:1px solid #cbd5e1;border-radius:9px;font-weight:600;}"));
    confirmButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:pressed{background:#1d4ed8;}"));
    actions->addWidget(cancelButton, 1);
    actions->addWidget(confirmButton, 1);

    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);
    layout->addWidget(balanceLabel);
    layout->addWidget(amountTitle);
    layout->addWidget(m_amountEdit);
    layout->addLayout(quickAmounts);
    layout->addWidget(m_amountSummaryLabel);
    layout->addWidget(m_errorLabel);
    layout->addLayout(actions);

    connect(m_amountEdit, &QLineEdit::textChanged,
            this, &RechargeDialog::updateAmountSummary);
    connect(m_amountEdit, &QLineEdit::returnPressed,
            this, &RechargeDialog::confirmRecharge);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked,
            this, &RechargeDialog::confirmRecharge);

    m_amountEdit->setFocus();
}

bool RechargeDialog::parseAmount(qint64 *amountFen) const
{
    static const QRegularExpression pattern(
        QStringLiteral("^(0|[1-9][0-9]*)(?:\\.([0-9]{1,2}))?$"));
    const QRegularExpressionMatch match = pattern.match(m_amountEdit->text().trimmed());
    if (!match.hasMatch()) return false;

    bool ok = false;
    const qint64 yuan = match.captured(1).toLongLong(&ok);
    if (!ok) return false;
    const QString fractionText = match.captured(2).leftJustified(2, QLatin1Char('0'));
    const qint64 fraction = fractionText.isEmpty() ? 0 : fractionText.toLongLong(&ok);
    if (!ok) return false;

    const qint64 result = yuan * 100 + fraction;
    if (result < 100 || result > 500000) return false;
    if (amountFen) *amountFen = result;
    return true;
}

void RechargeDialog::confirmRecharge()
{
    qint64 amountFen = 0;
    if (!parseAmount(&amountFen)) {
        m_errorLabel->setText(QStringLiteral("充值金额需为 1-5000 元，且最多保留两位小数"));
        m_amountEdit->setFocus();
        m_amountEdit->selectAll();
        return;
    }

    m_amountFen = amountFen;
    m_errorLabel->clear();
    accept();
}

void RechargeDialog::updateAmountSummary()
{
    qint64 amountFen = 0;
    const bool valid = parseAmount(&amountFen);
    m_amountSummaryLabel->setText(valid
        ? QStringLiteral("确认金额：%1").arg(formatMoney(amountFen))
        : QStringLiteral("确认金额：--"));
    m_errorLabel->clear();
}
