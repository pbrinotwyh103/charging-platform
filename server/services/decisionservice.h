#pragma once

#include <QJsonArray>

class DecisionService final
{
public:
    QJsonArray scheduling(const QJsonArray &predictions) const;
    QJsonArray maintenance(const QJsonArray &devices) const;
    QJsonArray expansion(const QJsonArray &stations) const;

private:
    QJsonArray ranked(const QString &kind, const QJsonArray &rows) const;
};
