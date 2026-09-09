#include "pages/customerservicepage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>

CustomerServicePage::CustomerServicePage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("customerServicePage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("AI 智能客服"), this);
    title->setStyleSheet(QStringLiteral("font-size:22px;font-weight:900;color:#073b4c;"));
    auto *hint = new QLabel(
        QStringLiteral("可咨询找桩、预约、充电、计费、钱包和故障处理"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#64748b;"));
    m_modelStatus = new QLabel(QStringLiteral("由服务端 Qwen 轻量模型提供支持"), this);
    m_modelStatus->setObjectName(QStringLiteral("customerModelStatus"));
    m_modelStatus->setWordWrap(true);
    m_modelStatus->setStyleSheet(QStringLiteral(
        "padding:8px;background:#e6fffb;color:#0f766e;border-radius:10px;"));

    m_conversation = new QTextBrowser(this);
    m_conversation->setObjectName(QStringLiteral("customerConversation"));
    m_conversation->setOpenExternalLinks(false);
    m_conversation->setPlaceholderText(QStringLiteral("客服消息将在这里显示"));
    appendMessage(QStringLiteral("客服"),
                  QStringLiteral("您好，我是充电平台智能客服。请问有什么可以帮您？"),
                  QStringLiteral("#0f766e"));

    m_question = new QTextEdit(this);
    m_question->setObjectName(QStringLiteral("customerQuestionInput"));
    m_question->setPlaceholderText(QStringLiteral("请输入问题（最多500字）"));
    m_question->setMaximumHeight(92);
    m_question->setAcceptRichText(false);

    auto *actions = new QHBoxLayout;
    auto *clearButton = new QPushButton(QStringLiteral("清空"), this);
    clearButton->setObjectName(QStringLiteral("customerClearButton"));
    m_sendButton = new QPushButton(QStringLiteral("发送"), this);
    m_sendButton->setObjectName(QStringLiteral("userPrimaryButton"));
    actions->addWidget(clearButton);
    actions->addWidget(m_sendButton, 1);

    root->addWidget(title);
    root->addWidget(hint);
    root->addWidget(m_modelStatus);
    root->addWidget(m_conversation, 1);
    root->addWidget(m_question);
    root->addLayout(actions);

    connect(m_sendButton, &QPushButton::clicked, this,
            &CustomerServicePage::submitQuestion);
    connect(clearButton, &QPushButton::clicked, m_conversation,
            &QTextBrowser::clear);
}

void CustomerServicePage::setBusy(bool busy)
{
    m_sendButton->setDisabled(busy);
    m_question->setDisabled(busy);
    m_sendButton->setText(busy ? QStringLiteral("思考中…")
                               : QStringLiteral("发送"));
}

void CustomerServicePage::showAnswer(const QString &answer,
                                     const QString &model,
                                     bool modelAvailable)
{
    setBusy(false);
    m_modelStatus->setText(modelAvailable
        ? QStringLiteral("在线模型：%1").arg(model)
        : QStringLiteral("模型未启动，当前使用：%1").arg(model));
    m_modelStatus->setStyleSheet(modelAvailable
        ? QStringLiteral("padding:8px;background:#e6fffb;color:#0f766e;border-radius:10px;")
        : QStringLiteral("padding:8px;background:#fff7ed;color:#b45309;border-radius:10px;"));
    appendMessage(QStringLiteral("客服"), answer, QStringLiteral("#0f766e"));
}

void CustomerServicePage::showError(const QString &message)
{
    setBusy(false);
    appendMessage(QStringLiteral("系统"), message, QStringLiteral("#b91c1c"));
}

void CustomerServicePage::submitQuestion()
{
    const QString question = m_question->toPlainText().trimmed();
    if (question.isEmpty()) {
        showError(QStringLiteral("请先输入您的问题"));
        return;
    }
    if (question.size() > 500) {
        showError(QStringLiteral("问题不能超过500字"));
        return;
    }
    appendMessage(QStringLiteral("我"), question, QStringLiteral("#1d4ed8"));
    m_question->clear();
    setBusy(true);
    emit questionSubmitted(question);
}

void CustomerServicePage::appendMessage(const QString &speaker,
                                        const QString &message,
                                        const QString &color)
{
    m_conversation->append(
        QStringLiteral("<p style='margin:6px 0'><b style='color:%1'>%2：</b><br>%3</p>")
            .arg(color, speaker.toHtmlEscaped(), message.toHtmlEscaped()
                .replace(QStringLiteral("\n"), QStringLiteral("<br>"))));
}
