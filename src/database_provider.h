#ifndef CLOUD_CORE_DATABASE_H
#define CLOUD_CORE_DATABASE_H

#include "sqlite3.h"

class Core;

class DatabaseProvider
{
    public:
        explicit DatabaseProvider(Core& core) : m_core(core) {}
        void initialize_database();
        void close() const;
        [[nodiscard]] sqlite3* database() const
        {
            return m_database;
        }
    private:
        Core& m_core;
        sqlite3* m_database = nullptr;
};

#endif //CLOUD_CORE_DATABASE_H