#pragma once

class DatabaseManager;

class ServiceBase
{
public:
    explicit ServiceBase(DatabaseManager *database = nullptr)
        : m_database(database)
    {
    }

    virtual ~ServiceBase() = default;
    void setDatabase(DatabaseManager *database) { m_database = database; }

protected:
    DatabaseManager *database() const { return m_database; }

private:
    DatabaseManager *m_database = nullptr;
};
