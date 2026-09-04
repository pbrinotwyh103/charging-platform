#pragma once

class DatabaseManager;

class RepositoryBase
{
public:
    explicit RepositoryBase(DatabaseManager *database = nullptr)
        : m_database(database)
    {
    }
    virtual ~RepositoryBase() = default;

protected:
    DatabaseManager *database() const { return m_database; }

private:
    DatabaseManager *m_database = nullptr;
};
