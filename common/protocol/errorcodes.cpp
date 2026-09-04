#include "protocol/errorcodes.h"

namespace Charging {

QString errorMessage(ErrorCode code, const QString &detail)
{
    if (!detail.trimmed().isEmpty()) {
        return detail.trimmed();
    }

    switch (code) {
    case ErrorCode::Success: return QStringLiteral("操作成功");
    case ErrorCode::InvalidPacket: return QStringLiteral("消息格式错误");
    case ErrorCode::UnsupportedVersion: return QStringLiteral("通信协议版本不兼容");
    case ErrorCode::UnsupportedMessage: return QStringLiteral("服务器暂不支持该操作");
    case ErrorCode::InvalidPayload: return QStringLiteral("请求内容格式错误");
    case ErrorCode::Unauthorized: return QStringLiteral("请先登录");
    case ErrorCode::Forbidden: return QStringLiteral("当前账号无权执行该操作");
    case ErrorCode::SessionExpired: return QStringLiteral("登录状态已失效，请重新登录");
    case ErrorCode::InvalidCredentials: return QStringLiteral("账号或密码错误");
    case ErrorCode::AccountDisabled: return QStringLiteral("账号已被停用");
    case ErrorCode::ValidationFailed: return QStringLiteral("输入内容不符合要求");
    case ErrorCode::Conflict: return QStringLiteral("当前操作与系统状态冲突");
    case ErrorCode::NotFound: return QStringLiteral("未找到请求的数据");
    case ErrorCode::DuplicateRequest: return QStringLiteral("请勿重复提交请求");
    case ErrorCode::RateLimited: return QStringLiteral("操作过于频繁，请稍后再试");
    case ErrorCode::DatabaseError: return QStringLiteral("数据处理失败，请稍后再试");
    case ErrorCode::NetworkUnavailable: return QStringLiteral("网络连接不可用");
    case ErrorCode::RequestTimeout: return QStringLiteral("请求超时，请重试");
    case ErrorCode::InternalError: return QStringLiteral("服务器内部错误");
    }
    return QStringLiteral("未知错误");
}

} // namespace Charging
