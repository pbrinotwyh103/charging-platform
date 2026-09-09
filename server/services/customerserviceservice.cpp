#include "services/customerserviceservice.h"

#include "services/servicehelpers.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

using namespace ServiceHelpers;

namespace {
constexpr int MaxQuestionLength = 500;

QString modelName()
{
    QString configured = qEnvironmentVariable("QWEN_MODEL").trimmed();
    if (configured.isEmpty())
        configured = qEnvironmentVariable("OLLAMA_MODEL").trimmed();
    return configured.isEmpty()
        ? QStringLiteral("qwen2.5-0.5b-instruct") : configured;
}

QUrl modelEndpoint()
{
    QString configured = qEnvironmentVariable("QWEN_CHAT_URL").trimmed();
    if (configured.isEmpty())
        configured = qEnvironmentVariable("OLLAMA_CHAT_URL").trimmed();
    return QUrl(configured.isEmpty()
                    ? QStringLiteral("http://127.0.0.1:8080/v1/chat/completions")
                    : configured);
}
}

ServiceResult CustomerServiceService::ask(qint64 userId,
                                           const QJsonObject &payload)
{
    if (userId <= 0 || !payload.value(QStringLiteral("question")).isString())
        return invalid();
    const QString question = payload.value(QStringLiteral("question")).toString().trimmed();
    if (question.isEmpty() || question.size() > MaxQuestionLength)
        return invalid();

    const QString model = modelName();
    const QUrl endpoint = modelEndpoint();
    const bool ollamaNative = endpoint.path().endsWith(QStringLiteral("/api/chat"));
    QJsonObject requestBody{
        {QStringLiteral("model"), model},
        {QStringLiteral("stream"), false},
        {QStringLiteral("messages"),
         QJsonArray{
             QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                         {QStringLiteral("content"),
                          QStringLiteral(
                              "你是电动汽车充电平台的中文客服。回答要简短、准确、友好，优先解释找站、输入电桩编号、预约、开始或停止充电、计费、钱包、故障和管理员远程停止等功能。"
                              "不要编造订单、价格或用户隐私；遇到紧急安全问题应建议立即停止充电并联系现场人员。回答控制在180字以内。")}},
             // 固定演示问答放进模型上下文，确保课堂提问时小模型也能稳定理解。
             QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                         {QStringLiteral("content"),
                          QStringLiteral("如何通过充电桩编号直连？")}},
             QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                         {QStringLiteral("content"),
                          QStringLiteral(
                              "在用户端首页输入桩体上的完整编号，例如 URBANEV-10010001，然后点击“编号直达”。系统会直接打开该电桩详情，电桩空闲时即可预约。")}},
             QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                         {QStringLiteral("content"), question}}}},
    };
    if (ollamaNative) {
        requestBody.insert(QStringLiteral("options"),
                           QJsonObject{{QStringLiteral("temperature"), 0.2},
                                       {QStringLiteral("num_ctx"), 1024},
                                       {QStringLiteral("num_predict"), 180}});
    } else {
        requestBody.insert(QStringLiteral("temperature"), 0.2);
        requestBody.insert(QStringLiteral("max_tokens"), 180);
    }

    QNetworkAccessManager manager;
    QNetworkRequest request{endpoint};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    QNetworkReply *reply = manager.post(request,
        QJsonDocument(requestBody).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        reply->abort();
        loop.quit();
    });
    timeout.start(45'000);
    loop.exec();

    const QByteArray responseBytes = reply->readAll();
    const bool networkOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (networkOk) {
        QJsonParseError parseError;
        const QJsonObject response =
            QJsonDocument::fromJson(responseBytes, &parseError).object();
        QString answer;
        if (ollamaNative) {
            answer = response.value(QStringLiteral("message"))
                         .toObject().value(QStringLiteral("content"))
                         .toString().trimmed();
        } else {
            const QJsonArray choices =
                response.value(QStringLiteral("choices")).toArray();
            if (!choices.isEmpty()) {
                answer = choices.at(0).toObject()
                             .value(QStringLiteral("message")).toObject()
                             .value(QStringLiteral("content"))
                             .toString().trimmed();
            }
        }
        if (parseError.error == QJsonParseError::NoError && !answer.isEmpty()) {
            ServiceResult result;
            result.payload = {{QStringLiteral("answer"), answer},
                              {QStringLiteral("model"), model},
                              {QStringLiteral("modelAvailable"), true}};
            return result;
        }
    }

    // 模型服务没有启动时仍提供最基本的现场帮助，并明确标记为离线知识库。
    ServiceResult result;
    result.payload = {{QStringLiteral("answer"), offlineAnswer(question)},
                      {QStringLiteral("model"), QStringLiteral("离线知识库")},
                      {QStringLiteral("modelAvailable"), false}};
    return result;
}

QString CustomerServiceService::offlineAnswer(const QString &question)
{
    if (question.contains(QStringLiteral("编号")))
        return QStringLiteral("请在首页的“输入充电桩编号”区域填写桩体编号，点击连接后可直接查看并预约该电桩。");
    if (question.contains(QStringLiteral("预约")))
        return QStringLiteral("选择充电站和空闲电桩后点击预约，系统会暂时锁定电桩；请在有效时间内开始充电。");
    if (question.contains(QStringLiteral("停止")) || question.contains(QStringLiteral("故障")))
        return QStringLiteral("请在充电页点击停止充电；若设备异常，请先确保人车安全，管理员也可以在监控页远程停止并处理告警。");
    if (question.contains(QStringLiteral("余额")) || question.contains(QStringLiteral("充值")))
        return QStringLiteral("进入“我的—钱包”可查看余额、充值和交易流水，充电结束后系统会按实际电量结算。");
    return QStringLiteral("AI客服暂时离线。您可以询问电桩编号、预约、停止充电、故障、充值或计费等问题。");
}
