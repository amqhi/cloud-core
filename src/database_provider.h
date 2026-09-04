// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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