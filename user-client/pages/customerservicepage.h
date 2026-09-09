#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTextBrowser;
class QTextEdit;

class CustomerServicePage final : public QWidget
{
    Q_OBJECT

public:
    explicit CustomerServicePage(QWidget *parent = nullptr);

public slots:
    void setBusy(bool busy);
    void showAnswer(const QString &answer, const QString &model,
                    bool modelAvailable);
    void showError(const QString &message);

signals:
    void questionSubmitted(const QString &question);

private:
    void submitQuestion();
    void appendMessage(const QString &speaker, const QString &message,
                       const QString &color);

    QTextBrowser *m_conversation = nullptr;
    QTextEdit *m_question = nullptr;
    QPushButton *m_sendButton = nullptr;
    QLabel *m_modelStatus = nullptr;
};
