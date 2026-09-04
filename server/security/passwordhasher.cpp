#include "security/passwordhasher.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

QString PasswordHasher::generateSalt()
{
    QByteArray salt(16, Qt::Uninitialized);
    for (char &byte : salt) {
        byte = static_cast<char>(QRandomGenerator::system()->generate() & 0xffU);
    }
    return QString::fromLatin1(salt.toHex());
}

QString PasswordHasher::hash(const QString &password, const QString &saltHex)
{
    const QByteArray salt = QByteArray::fromHex(saltHex.toLatin1());
    QByteArray digest = password.toUtf8() + salt;
    for (int i = 0; i < 50'000; ++i) {
        digest = QCryptographicHash::hash(digest + salt, QCryptographicHash::Sha256);
    }
    return QString::fromLatin1(digest.toHex());
}

bool PasswordHasher::verify(const QString &password, const QString &saltHex,
                            const QString &expectedHashHex)
{
    const QByteArray actual = hash(password, saltHex).toLatin1();
    const QByteArray expected = expectedHashHex.toLatin1();
    if (actual.size() != expected.size()) return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < actual.size(); ++i) {
        difference |= static_cast<unsigned char>(actual.at(i) ^ expected.at(i));
    }
    return difference == 0;
}
