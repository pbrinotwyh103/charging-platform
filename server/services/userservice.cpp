#include "services/userservice.h"
#include "services/servicehelpers.h"
#include "repositories/userrepository.h"
#include "repositories/walletrepository.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

using namespace ServiceHelpers;

namespace {
QJsonObject profile(const UserRecord &user)
{
    return {{"userId", double(user.id)}, {"phone", user.phone}, {"nickname", user.nickname},
            {"avatar", user.avatarPath}, {"avatarPath", user.avatarPath},
            {"balanceCents", double(user.balanceCents)}, {"status", user.status}};
}

QJsonObject walletItem(const WalletRecord &record)
{
    return {{"recordId", double(record.id)}, {"transactionId", record.recordNo},
            {"orderId", record.orderId ? QJsonValue(double(record.orderId)) : QJsonValue()},
            {"recordType", record.recordType}, {"amountCents", double(record.amountCents)},
            {"balanceAfterCents", double(record.balanceAfterCents)},
            {"status", record.status}, {"createdAt", record.createdAt}};
}

// A raw Base64 image has no declared MIME; data URIs must agree with the decoder.
bool decodeAvatar(QString encoded, QByteArray *bytes, QString *extension)
{
    constexpr int maximumBytes = 2 * 1024 * 1024;
    QString mime;
    if (encoded.startsWith("data:")) {
        const int separator = encoded.indexOf(';');
        if (separator < 0 || encoded.mid(separator, 8) != ";base64,") return false;
        mime = encoded.mid(5, separator - 5);
        if (mime != "image/png" && mime != "image/jpeg") return false;
        encoded = encoded.mid(separator + 8);
    }
    if (encoded.isEmpty() || encoded.size() > ((maximumBytes + 2) / 3) * 4) return false;
    *bytes = QByteArray::fromBase64(encoded.toLatin1());
    // Round-trip validation is linear and rejects invalid alphabet/padding without
    // regex backtracking limits on otherwise valid multi-megabyte inputs.
    if (bytes->isEmpty() || bytes->size() > maximumBytes
        || QString::fromLatin1(bytes->toBase64()) != encoded) return false;
    QBuffer buffer(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const QByteArray format = reader.format().toLower();
    if (format != "png" && format != "jpeg" && format != "jpg") return false;
    const QString actualMime = format == "png" ? "image/png" : "image/jpeg";
    if ((!mime.isEmpty() && mime != actualMime) || reader.read().isNull()) return false;
    *extension = format == "png" ? "png" : "jpg";
    return true;
}
}

ServiceResult UserService::updateProfile(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    const bool nicknameProvided = payload.contains("nickname");
    const bool avatarProvided = payload.contains("avatarBase64");
    if (userId <= 0 || (!nicknameProvided && !avatarProvided)
        || (nicknameProvided && !payload.value("nickname").isString())
        || (avatarProvided && !payload.value("avatarBase64").isString())) return invalid();
    const QString nickname = payload.value("nickname").toString().trimmed();
    const QString avatar = payload.value("avatarBase64").toString();
    const bool changeNickname = !nickname.isEmpty();
    const bool changeAvatar = !avatar.isEmpty();
    if (!changeNickname && !changeAvatar) return invalid();
    static const QRegularExpression nicknamePattern("^[\\p{Han}A-Za-z0-9 _-]{2,20}$");
    if (changeNickname && !nicknamePattern.match(nickname).hasMatch()) return invalid();
    QByteArray imageBytes;
    QString extension;
    if (changeAvatar && !decodeAvatar(avatar, &imageBytes, &extension)) return invalid();
    if (!ready(database())) return databaseError();
    UserRepository users(database());
    UserRecord user;
    QString error;
    if (!users.findById(userId, &user, &error)) return databaseError();
    if (!user.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("用户不存在"));
    if (changeNickname) user.nickname = nickname;
    QString newFile;
    if (changeAvatar) {
        const QDir root = QFileInfo(database()->databasePath()).absoluteDir();
        if (!root.mkpath("avatars"))
            return failure(Charging::ErrorCode::InternalError, QStringLiteral("头像保存失败"));
        user.avatarPath = QString("avatars/%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), extension);
        newFile = root.filePath(user.avatarPath);
        QSaveFile output(newFile);
        output.setDirectWriteFallback(false);
        if (!output.open(QIODevice::WriteOnly) || output.write(imageBytes) != imageBytes.size()
            || !output.commit())
            return failure(Charging::ErrorCode::InternalError, QStringLiteral("头像保存失败"));
    }
    if (!users.updateProfile(userId, user.nickname, user.avatarPath, &error)) {
        if (!newFile.isEmpty()) QFile::remove(newFile);
        return databaseError();
    }
    ServiceResult result;
    result.payload = profile(user);
    return result;
}

ServiceResult UserService::recharge(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    const auto transaction = payload.value("transactionId");
    if (userId <= 0 || !integer(payload.value("amountCents"), 100, 1000000)
        || !transaction.isString() || transaction.toString().trimmed().isEmpty()
        || transaction.toString().size() > 128) return invalid();
    if (!ready(database())) return databaseError();
    const QString transactionId = transaction.toString();
    const qint64 amount = qint64(payload.value("amountCents").toDouble());
    WalletRepository wallet(database());
    WalletRecord record;
    QString error;
    if (!wallet.findByRecordNo(transactionId, &record, &error)) return databaseError();
    if (record.id && (record.userId != userId || record.recordType != "recharge"
                      || record.amountCents != amount))
        return failure(Charging::ErrorCode::Conflict, QStringLiteral("交易号已用于其他交易"), "transaction_conflict");
    if (!record.id) {
        UserRecord user;
        if (!UserRepository(database()).findById(userId, &user, &error)) return databaseError();
        if (!user.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("用户不存在"));
        if (user.status != "normal")
            return failure(Charging::ErrorCode::AccountDisabled, QStringLiteral("用户已冻结"));
        qint64 balance = 0;
        if (!wallet.recharge(transactionId, userId, amount, &balance, &error)) return databaseError();
        if (!wallet.findByRecordNo(transactionId, &record, &error)) return databaseError();
    }
    ServiceResult result;
    result.payload = walletItem(record);
    result.payload.insert("balanceCents", double(record.balanceAfterCents));
    return result;
}

ServiceResult UserService::walletLedger(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    Pagination page;
    if (userId <= 0 || !pagination(payload, &page)) return invalid();
    if (!ready(database())) return databaseError();
    WalletRepository wallet(database());
    QList<WalletRecord> records;
    int total = 0;
    QString error;
    if (!wallet.countByUser(userId, &total, &error)
        || !wallet.listByUser(userId, page.pageSize, page.offset, &records, &error)) return databaseError();
    QJsonArray items;
    for (const auto &record : records) items.append(walletItem(record));
    ServiceResult result;
    result.payload = {{"items", items}, {"page", page.page}, {"pageSize", page.pageSize}, {"total", total}};
    return result;
}
