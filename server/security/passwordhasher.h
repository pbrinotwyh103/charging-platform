#pragma once

#include <QString>

class PasswordHasher final
{
public:
    static QString generateSalt();
    static QString hash(const QString &password, const QString &saltHex);
    static bool verify(const QString &password, const QString &saltHex,
                       const QString &expectedHashHex);
};
