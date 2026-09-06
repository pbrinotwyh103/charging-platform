#pragma once
#include "repositories/repositorybase.h"

#include <QJsonArray>

class OrderRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool findHistoryByUser(qint64 userId, QJsonArray *items,
                           QString *error) const;
};
