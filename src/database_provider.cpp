#include "database_provider.h"

#include "core.h"

namespace fs = std::filesystem;

void DatabaseProvider::initialize_database()
{
    std::filesystem::path user_dir = std::filesystem::path(m_core.app_support_path()) / m_core.selected_user()->local_id;

    fs::create_directories(user_dir);

    const fs::path db_path = user_dir / "cloud.db";
    int exit = sqlite3_open(db_path.string().c_str(), &m_database);

    if (exit == SQLITE_OK) {

        const char* schema_sql =
           "PRAGMA foreign_keys = ON;"

           "CREATE TABLE IF NOT EXISTS items ("
           "  id TEXT PRIMARY KEY NOT NULL, "
           "  type TEXT NOT NULL, "
           "  created_at INTEGER NOT NULL, "
           "  updated_at INTEGER NOT NULL, "
           "  event_at INTEGER, "
           "  deleted_at INTEGER, "
           "  parent_id TEXT, "
           "  name TEXT, "
           "  tags TEXT, "
           "  comment TEXT, "
           "  encrypted INTEGER DEFAULT 0, "
           "  app_scope INTEGER NOT NULL DEFAULT 63, "
           "  cached BOOLEAN, "
           "  FOREIGN KEY(parent_id) REFERENCES items(id) ON DELETE CASCADE"
           ");"

           "CREATE INDEX IF NOT EXISTS idx_items_parent_id ON items(parent_id);"

           "CREATE TABLE IF NOT EXISTS themes ("
           "  id TEXT PRIMARY KEY NOT NULL, "
           "  title TEXT NOT NULL, "
           "  created_at INTEGER NOT NULL, "
           "  updated_at INTEGER NOT NULL, "
           "  data TEXT NOT NULL, "
           "  FOREIGN KEY(id) REFERENCES items(id) ON DELETE CASCADE"
           ");"

           "CREATE TABLE IF NOT EXISTS files ("
           "  id TEXT PRIMARY KEY NOT NULL, "
           "  checksum TEXT NOT NULL, "
           "  size INTEGER NOT NULL, "
           "  mime_type TEXT NOT NULL, "
           "  FOREIGN KEY(id) REFERENCES items(id) ON DELETE CASCADE"
           ");";

        sqlite3_exec(m_database, schema_sql, nullptr, nullptr, nullptr);
    }
}

void DatabaseProvider::close() const
{
    sqlite3_close(m_database);
}