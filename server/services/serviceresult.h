#pragma once

#include "protocol/errorcodes.h"

#include <QJsonObject>
#include <QString>

struct ServiceResult {
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    QString reason;
    QJsonObject payload;

    bool succeeded() const { return error == Charging::ErrorCode::Success; }
};
