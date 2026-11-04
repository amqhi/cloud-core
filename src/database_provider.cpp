// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "database_provider.h"

#include "core.h"

namespace fs = std::filesystem;

// TODO!: set version to 1 and clear migrate block before the initial release

DatabaseProvider::DatabaseProvider(Core& core): DatabaseProviderBase(core)
{}

void DatabaseProvider::migrate(int old_version) const
{
    switch (old_version)
    {
        case 0:
        case 1:
            {
                int rc = sqlite3_exec(m_database, "ALTER TABLE folders ADD COLUMN view_mode INTEGER NOT NULL DEFAULT 0; ALTER TABLE folders ADD COLUMN sort_option INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr, nullptr);
                if (rc != SQLITE_OK) {
                    break;
                }
                sqlite3_exec(m_database, "PRAGMA user_version = 2;", nullptr, nullptr, nullptr);
                break;
            }
        default:
        break;
    }
}

std::filesystem::path DatabaseProvider::database_name()
{
    return "cloud.db";
}

int DatabaseProvider::version()
{
    return 2;
}
