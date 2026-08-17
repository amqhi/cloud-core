#include "item_manager.h"

#include "core.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

#include "api.h"
#include "mime_utils.h"
#include "network_error.h"
#include "network_provider.h"
#include "notifier.h"
#include "platform_utils.h"
#include "request_failure.h"
#include "item_attributes.h"
#include "date_time_utils.h"
#include "item_utils.h"
#include "map_utils.h"
#include "common_api.h"
#include "json_utils.h"
#include "sync_event.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

ItemManager::ItemManager(Core& core) : m_core(core)
{
}

std::map<std::string, Item> ItemManager::items() const
{
    return m_items;
}

std::map<std::string, std::vector<std::string>> ItemManager::id_lists() const
{
    return m_id_lists;
}

std::map<std::string, FileMetadata> ItemManager::file_metadata_map() const
{
    return m_file_metadata_map;
}

void ItemManager::sync()
{
    api::sync::get_sync_events(m_core, [this](const std::vector<SyncEvent>& sync_events)
    {
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            for (const SyncEvent& event : sync_events) {
                m_event_queue.push(event);
            }

            if (m_is_processing) return;
            m_is_processing = true;
        }

        sync_next_event();
    });
}

void ItemManager::fetch_items_from_cache(const std::string& parent_id)
{
    if (map_utils::contains_key(m_id_lists, parent_id))
    {
        m_id_lists.clear();
    }
    sqlite3_stmt* stmt;
    std::string sql;
    if (parent_id == special_folder::HOME)
    {
        sql = "SELECT * FROM items WHERE parent_id IS NULL AND deleted_at IS NULL;";
    }
    else if (parent_id == special_folder::TRASH)
    {
        sql = "SELECT * FROM items WHERE parent_id IS NULL AND deleted_at IS NOT NULL;";
    }
    else
    {
        sql = "SELECT * FROM items WHERE parent_id = '" + parent_id + "';";
    }

    if (sqlite3_prepare_v2(m_core.database_provider().database(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        m_core.notifier().notify(DATABASE_ERROR, std::string(sqlite3_errmsg(m_core.database_provider().database())));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Item item = item_from_stmt(stmt);
        m_id_lists[item.parent_id].push_back(item.id);
        m_items[item.id] = std::move(item);
    }
    sqlite3_finalize(stmt);

    stmt = nullptr;
    if (parent_id == special_folder::HOME || parent_id == special_folder::TRASH)
    {
        sql = R"(SELECT
    i.id,
    f.checksum,
    f.size,
    f.mime_type
FROM items i
INNER JOIN files f ON i.id = f.id
WHERE i.parent_id IS NULL;)";

        if (sqlite3_prepare_v2(m_core.database_provider().database(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            m_core.notifier().notify(DATABASE_ERROR, std::string(sqlite3_errmsg(m_core.database_provider().database())));
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            FileMetadata file_meta = file_metadata_from_stmt(stmt);
            m_file_metadata_map[file_meta.id] = std::move(file_meta);
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        sql = R"(SELECT
                i.id,
                f.checksum,
                f.size,
                f.mime_type
            FROM items i
            INNER JOIN files f ON i.id = f.id
            WHERE i.parent_id = ?;)";

        if (sqlite3_prepare_v2(m_core.database_provider().database(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            m_core.notifier().notify(DATABASE_ERROR, std::string(sqlite3_errmsg(m_core.database_provider().database())));
            return;
        }

        sqlite3_bind_text(stmt, 1, parent_id.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            FileMetadata file_meta = file_metadata_from_stmt(stmt);
            m_file_metadata_map[file_meta.id] = std::move(file_meta);
        }
        sqlite3_finalize(stmt);
    }
}

void ItemManager::fetch_items(const std::string& parent_id)
{
    std::string status = parent_id == special_folder::TRASH ? "deleted" : "active";
    std::string url = parent_id == special_folder::HOME || parent_id == special_folder::TRASH
                          ? m_core.settings().data().instance_url + "/items?status=" + status
                          : m_core.settings().data().instance_url + "/items?parent_id=" + parent_id + "&status=" +
                          status;
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().get(
        url,
        headers,
        [this, parent_id, url](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                auto body = json::parse(response, nullptr, false);
                if (!body.is_discarded() && body.is_array())
                {
                    m_id_lists[parent_id] = std::vector<std::string>{};
                    for (const auto& element : body)
                    {
                        Item item = item_from_json(element);
                        if (item.type == item_type::FILE && !map_utils::contains_key(m_file_metadata_map, item.id))
                        {
                            api::files::get_file_metadata(m_core, item.id, [this](FileMetadata& file_metadata)
                            {
                                cache_file_metadata(m_core.database_provider().database(), file_metadata);
                                m_file_metadata_map[file_metadata.id] = std::move(file_metadata);
                            });
                        }
                        m_id_lists[parent_id].push_back(item.id);
                        m_items[item.id] = std::move(item);
                    }
                    sort_items(parent_id);
                    cache_metadata_of_items(parent_id);
                    m_core.notifier().notify(ITEMS_FETCH_SUCCESS, parent_id);
                }
                else
                {
                    notify_request_failure(m_core.notifier(), ITEMS_FETCH_FAILURE, status_code, response, url);
                }
            }
            else
            {
                notify_request_failure(m_core.notifier(), ITEMS_FETCH_FAILURE, status_code, response, url);
            }
        }
        , [this](std::int16_t error_code, const std::string& data)
        {
            handle_network_error(m_core.notifier(), error_code, data);
        });
}

void ItemManager::purge_items(const std::string& parent_id)
{
    if (!map_utils::contains_key(m_items, parent_id))
    {
        return;
    }
    for (const auto& id : m_id_lists[parent_id])
    {
        m_items.erase(id);
        m_file_metadata_map.erase(id);
    }
    m_id_lists.erase(parent_id);
}

void ItemManager::cache_metadata_of_items(const std::string& parent_id)
{
    if (m_items.empty()) return;

    char* err_msg = nullptr;
    if (sqlite3_exec(m_core.database_provider().database(), "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg) !=
        SQLITE_OK)
    {
        sqlite3_free(err_msg);
        return;
    }

    const char* sql =
        "INSERT OR REPLACE INTO items (id, type, created_at, updated_at, event_at, deleted_at, parent_id, name, tags, comment, encrypted, app_scope) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_core.database_provider().database(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        m_core.notifier().notify(DATABASE_ERROR, std::string(sqlite3_errmsg(m_core.database_provider().database())));
        sqlite3_exec(m_core.database_provider().database(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return;
    }

    for (const auto& id : m_id_lists[parent_id])
    {
        const auto& item = m_items[id];
        sqlite_bind_item(stmt, item);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            m_core.notifier().notify(DATABASE_ERROR,
                                    std::string(sqlite3_errmsg(m_core.database_provider().database())));
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    // 4. 자원 해제 및 트랜잭션 커밋 (실제 디스크 쓰기 발생)
    sqlite3_finalize(stmt);

    if (sqlite3_exec(m_core.database_provider().database(), "COMMIT;", nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        std::cerr << "Failed to commit transaction: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        sqlite3_exec(m_core.database_provider().database(), "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}

const std::vector<std::string>& ItemManager::id_list_by_id(const std::string& id)
{
    return m_id_lists[id];
}

std::vector<ItemSummary> ItemManager::summary_list_by_id(const std::string& id)
{
    auto list_it = m_id_lists.find(id);
    if (list_it == m_id_lists.end())
    {
        return {};
    }

    const auto& child_ids = list_it->second;
    std::vector<ItemSummary> list;

    list.reserve(child_ids.size());

    for (const auto& itemId : child_ids)
    {
        auto item_it = m_items.find(itemId);
        if (item_it == m_items.end())
        {
            continue;
        }

        const Item& current_item = item_it->second;

        ItemSummary summary = item_to_summary(m_core, current_item);

        list.push_back(std::move(summary));
    }

    return list;
}

const Item& ItemManager::item_by_id(const std::string& id)
{
    return m_items[id];
}

const FileMetadata& ItemManager::file_metadata_by_id(const std::string& id)
{
    return m_file_metadata_map[id];
}

void ItemManager::sort_items(const std::string& parent_id)
{
    if (m_id_lists.find(parent_id) != m_id_lists.end())
    {
        const auto& sort_options = m_core.cached_state().get().sort_options;

        if (!map_utils::contains_key(sort_options, parent_id))
        {
            m_core.cached_state().set_sort_option(parent_id, sort_option::UPDATED_AT_DESC);
        }

        auto& id_list = m_id_lists[parent_id];

        switch (sort_options.at(parent_id))
        {
        case sort_option::NAME_ASC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].name < m_items[b].name;
                });
            break;

        case sort_option::NAME_DESC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].name > m_items[b].name;
                });
            break;

        case sort_option::CREATED_AT_ASC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].created_at < m_items[b].created_at;
                });
            break;

        case sort_option::CREATED_AT_DESC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].created_at > m_items[b].created_at;
                });
            break;

        case sort_option::UPDATED_AT_ASC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].updated_at < m_items[b].updated_at;
                });
            break;

        case sort_option::UPDATED_AT_DESC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].updated_at > m_items[b].updated_at;
                });
            break;

        case sort_option::SIZE_ASC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].updated_at < m_items[b].updated_at;
                });
            break;

            // TODO: sort items by size
        case sort_option::SIZE_DESC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].updated_at > m_items[b].updated_at;
                });
            break;

        case sort_option::TYPE_ASC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].type < m_items[b].type;
                });
            break;

        case sort_option::TYPE_DESC:
            std::sort(id_list.begin(), id_list.end(),
                [this](const std::string& a, const std::string& b) {
                    return m_items[a].type > m_items[b].type;
                });
            break;
        }
    }
}

/// Upload file to server and create metadata
void ItemManager::create_file(const ItemAttributes& item_attributes, const std::string& tmp_file_path)
{
    std::string url = m_core.settings().data().instance_url + "/files";
    std::map<std::string, std::string> headers;
    nlohmann::json body = item_attributes;
    std::string mime_type = mime_util::get_mime_type_from_file(tmp_file_path);
    std::uint64_t size = std::filesystem::file_size(tmp_file_path);

    body["mime_type"] = mime_type;
    body["size"] = size;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    std::string checksum = m_core.platform_utils().get_file_sha256_checksum(tmp_file_path);

    m_core.network_provider().post_json(url,
                                       headers,
                                       body,
                                       [this, tmp_file_path, checksum, size, mime_type, url](
                                       int status_code, const std::string& response)
                                       {
                                           if (status_code == 201)
                                           {
                                               auto body = json::parse(response, nullptr, false);
                                               if (!body.is_discarded() && body.contains("item") &&
                                                   body["item"].is_object() &&
                                                   body.contains("upload") && body["upload"].is_object())
                                               {
                                                   Item item = item_from_json(body["item"]);
                                                   item.save(m_core.database_provider().database());
                                                   auto& upload = body["upload"];
                                                   if (upload["type"] == "multipart")
                                                   {
                                                       const std::string& upload_id = upload["upload_id"];
                                                       auto target_parts = upload.at(
                                                           "parts").get<nlohmann::json>();
                                                       // Parts allocated by the server

                                                       auto completed_parts = nlohmann::json::array();
                                                       // Payload for multipart completion
                                                       m_core.network_provider().put_file(
                                                           target_parts,
                                                           mime_type,
                                                           tmp_file_path,
                                                           [this, item](std::int64_t bytes_written, std::int64_t total_bytes)
                                                           {
                                                               json data;
                                                               data["bytes_written"] = bytes_written;
                                                               data["total_bytes"] = total_bytes;
                                                               data["id"] = item.id;
                                                               data["name"] = item.name;
                                                               data["type"] = transfer_type::FILE;
                                                               m_core.notifier().notify(UPLOAD_PROGRESS, data);
                                                           },
                                                           [this, checksum, size, item, mime_type, upload_id,
                                                               completed_parts](
                                                           int status_code,
                                                           const std::string& response)
                                                           {
                                                               auto parts_json = json::parse(response, nullptr, false);
                                                               if (status_code == 200)
                                                               {
                                                                   complete_upload_multipart(
                                                                       item, checksum, size,
                                                                       mime_type, upload_id,
                                                                       parts_json);
                                                               }
                                                               else
                                                               {
                                                                   notify_request_failure(
                                                                       m_core.notifier(),
                                                                       ITEM_CREATE_FAILURE,
                                                                       status_code,
                                                                       response);
                                                               }
                                                           }
                                                           , [this](std::int16_t error_code, const std::string& data)
                                                           {
                                                               handle_network_error(m_core.notifier(), error_code, data);
                                                           });
                                                   }
                                                   else if (upload["type"] == "single")
                                                   {
                                                       const std::string& upload_url = upload.at("url").get<
                                                           std::string>();
                                                       std::map<std::string, std::string> headers;
                                                       headers["Content-Type"] = mime_type;
                                                       headers["Content-Length"] = std::to_string(size);
                                                       m_core.network_provider().put_file(
                                                           upload_url,
                                                           headers,
                                                           tmp_file_path,
                                                           [this, item](std::int64_t bytes_written, std::int64_t total_bytes)
                                                           {
                                                               json data;
                                                               data["bytes_written"] = bytes_written;
                                                               data["total_bytes"] = total_bytes;
                                                               data["id"] = item.id;
                                                               data["name"] = item.name;
                                                               data["type"] = transfer_type::FILE;
                                                               m_core.notifier().notify(UPLOAD_PROGRESS, data);
                                                           },
                                                           [this, checksum, size, mime_type, item, upload_url](
                                                           int status_code,
                                                           const std::string& response)
                                                           {
                                                               if (status_code == 200)
                                                               {
                                                                   complete_upload(item, checksum,
                                                                       size, mime_type);
                                                               }
                                                               else
                                                               {
                                                                   notify_request_failure(
                                                                       m_core.notifier(),
                                                                       ITEM_CREATE_FAILURE,
                                                                       status_code,
                                                                       response, upload_url);
                                                               }
                                                           }
                                                           , [this](std::int16_t error_code, const std::string& data)
                                                           {
                                                               handle_network_error(m_core.notifier(), error_code, data);
                                                           });
                                                   }
                                                   else
                                                   {
                                                       notify_request_failure(m_core.notifier(),
                                                                              ITEM_CREATE_FAILURE,
                                                                              status_code,
                                                                              response, url);
                                                   }
                                               }
                                               else
                                               {
                                                   notify_request_failure(m_core.notifier(),
                                                                          ITEM_CREATE_FAILURE,
                                                                          status_code,
                                                                          response, url);
                                               }
                                           }
                                           else
                                           {
                                               notify_request_failure(m_core.notifier(), ITEM_CREATE_FAILURE,
                                                                      status_code,
                                                                      response);
                                           }
                                       }, [this](std::int16_t error_code, const std::string& data)
                                       {
                                           handle_network_error(m_core.notifier(), error_code, data);
                                       });
}

void ItemManager::create_folder(const ItemAttributes& item_attributes)
{
    std::string url = m_core.settings().data().instance_url + "/folders";
    std::map<std::string, std::string> headers;
    nlohmann::json body = item_attributes;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().post_json(url,
                                       headers,
                                       body,
                                       [this, url](
                                       int status_code, const std::string& response)
                                       {
                                           if (status_code == 201)
                                           {
                                               auto body = json::parse(response, nullptr, false);
                                               if (!body.is_discarded())
                                               {
                                                   Item item = item_from_json(body);
                                                   item.save(m_core.database_provider().database());
                                                   apply_create_item(item);
                                                   json data;
                                                   data["id"] = item.id;
                                                   data["parent_id"] = item.parent_id;
                                                   data["type"] = item.type;
                                                   m_core.notifier().notify(ITEM_CREATE_SUCCESS, data);
                                               }
                                           }
                                           else
                                           {
                                               notify_request_failure(m_core.notifier(), ITEM_CREATE_FAILURE,
                                                                      status_code,
                                                                      response, url);
                                           }
                                       }, [](std::int16_t error_code, const std::string& data)
                                       {
                                       });
}

void ItemManager::complete_upload_multipart(const Item& item, const std::string& checksum, std::uint64_t size,
                                            const std::string& mime_type, const std::string& upload_id,
                                            const json& parts)
{
    std::string url = m_core.settings().data().instance_url + "/files/" + item.id + "/complete";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    body["checksum"] = checksum;
    body["size"] = size;
    body["mime_type"] = mime_type;
    body["upload_id"] = upload_id;
    body["parts"] = parts;
    m_core.network_provider().post_json(url,
                                       headers,
                                       body,
                                       [this, item, checksum, mime_type, size](int status_code, const std::string& response)
                                       {
                                           if (status_code == 200)
                                           {
                                               FileMetadata file_metadata;
                                               file_metadata.id = item.id;
                                               file_metadata.checksum = checksum;
                                               file_metadata.mime_type = mime_type;
                                               file_metadata.size = size;
                                               cache_file_metadata(m_core.database_provider().database(), file_metadata);
                                               m_items[item.id] = item;
                                               m_id_lists[item.parent_id].push_back(item.id);
                                               json data;
                                               data["id"] = item.id;
                                               data["parent_id"] = item.parent_id;
                                               m_core.notifier().notify(ITEM_CREATE_SUCCESS, data);
                                               download_thumbnail(item.id);
                                           }
                                           else
                                           {
                                               notify_request_failure(m_core.notifier(), ITEM_CREATE_FAILURE,
                                                                      status_code,
                                                                      response);
                                           }
                                       }, [](std::int16_t error_code, const std::string& data)
                                       {
                                       });
}

void ItemManager::complete_upload(const Item& item, const std::string& checksum, std::uint64_t size,
                                  const std::string& mime_type)
{
    std::string url = m_core.settings().data().instance_url + "/files/" + item.id + "/complete";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    body["checksum"] = checksum;
    body["size"] = size;
    body["mime_type"] = mime_type;
    m_core.network_provider().post_json(url,
                                       headers,
                                       body,
                                       [this, item, checksum, mime_type, size](int status_code, const std::string& response)
                                       {
                                           if (status_code == 200)
                                           {
                                               FileMetadata file_metadata;
                                        file_metadata.id = item.id;
                                        file_metadata.checksum = checksum;
                                        file_metadata.mime_type = mime_type;
                                        file_metadata.size = size;
                                        cache_file_metadata(m_core.database_provider().database(), file_metadata);
                                               m_items[item.id] = item;
                                               m_id_lists[item.parent_id].push_back(item.id);
                                               json data;
                                               data["id"] = item.id;
                                               data["parent_id"] = item.parent_id;
                                               m_core.notifier().notify(ITEM_CREATE_SUCCESS, data);
                                               download_thumbnail(item.id);
                                           }
                                           else
                                           {
                                               notify_request_failure(m_core.notifier(), ITEM_CREATE_FAILURE,
                                                                      status_code,
                                                                      response);
                                           }
                                       }, [this](std::int16_t error_code, const std::string& data)
                                       {
                                           handle_network_error(m_core.notifier(), error_code, data);
                                       });
}

void ItemManager::update_item(const std::string& id, const ItemAttributes& item_attributes)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id;
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    nlohmann::json body = item_attributes;

    m_core.network_provider().patch_json(url,
                                        headers,
                                        body,
                                        [this, id, item_attributes](int status_code, const std::string& response)
                                        {
                                            if (status_code == 200)
                                            {
                                                m_items[id].update(item_attributes);
                                                m_items[id].save(m_core.database_provider().database());
                                                json data;
                                                data["id"] = id;
                                                data["parent_id"] = m_items[id].parent_id;
                                                sort_items(m_items[id].parent_id);
                                                m_core.notifier().notify(ITEM_UPDATE_SUCCESS, data);
                                            }
                                            else
                                            {
                                                nlohmann::json args;
                                                args["status_code"] = status_code;
                                                args["response"] = response;
                                                m_core.notifier().notify(ITEM_UPDATE_FAILURE, args);
                                            }
                                        }, [this](std::int16_t error_code, const std::string& data)
                                        {
                                            handle_network_error(m_core.notifier(), error_code, data);
                                        });
}

void ItemManager::move_item(const std::string& id, const std::string& parent_id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id + "/move";
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    nlohmann::json body;
    if (parent_id != special_folder::TRASH && parent_id != special_folder::HOME)
    {
        body["parent_id"] = parent_id;
    }
    m_core.network_provider().patch_json(url,
                                        headers,
                                        body,
                                        [this, id, parent_id](int status_code, const std::string& response)
                                        {
                                            if (status_code == 200)
                                            {
                                                std::string old_parent_id = m_items[id].parent_id;
                                                json data;
                                                data["id"] = id;
                                                data["parent_id"] = parent_id;
                                                data["old_parent_id"] = old_parent_id;
                                                m_items[id].parent_id = parent_id;
                                                m_items[id].save(m_core.database_provider().database());
                                                apply_move_item(id, old_parent_id, parent_id);
                                                m_core.notifier().notify(ITEM_MOVE_SUCCESS, data);
                                            }
                                            else
                                            {
                                                nlohmann::json args;
                                                args["status_code"] = status_code;
                                                args["response"] = response;
                                                m_core.notifier().notify(ITEM_MOVE_FAILURE, args);
                                            }
                                        }, [this](std::int16_t error_code, const std::string& data)
                                        {
                                            handle_network_error(m_core.notifier(), error_code, data);
                                        });
}

void ItemManager::rename_item(const std::string& id, const std::string& name)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id;
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    nlohmann::json body;
    body["name"] = name;

    m_core.network_provider().patch_json(url,
                                        headers,
                                        body,
                                        [this, id, name](int status_code, const std::string& response)
                                        {
                                            if (status_code == 200)
                                            {
                                                m_items[id].name = name;
                                                m_items[id].save(m_core.database_provider().database());
                                                m_core.notifier().notify(ITEM_UPDATE_SUCCESS, id);
                                            }
                                            else
                                            {
                                                nlohmann::json args;
                                                args["status_code"] = status_code;
                                                args["response"] = response;
                                                m_core.notifier().notify(ITEM_UPDATE_FAILURE, args);
                                            }
                                        }, [](std::int16_t error_code, const std::string& data)
                                        {
                                        });
}

void ItemManager::soft_delete_item(const std::string& id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id;
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().destroy(url,
                                     headers,
                                     [this, id](int status_code, const std::string& response)
                                     {
                                         if (status_code == 200)
                                         {
                                             std::string old_parent_id = m_items[id].parent_id;
                                             m_items[id].deleted_at = current_date_time_utc_int64();
                                             m_items[id].parent_id = special_folder::TRASH;
                                             m_items[id].save(m_core.database_provider().database());
                                             apply_move_item(id, old_parent_id, special_folder::TRASH);
                                             m_core.notifier().notify(ITEM_UPDATE_SUCCESS, id);
                                         }
                                         else
                                         {
                                             notify_request_failure(m_core.notifier(), ITEM_UPDATE_FAILURE, status_code,
                                                                    response);
                                         }
                                     }, [this](std::int16_t error_code, const std::string& data)
                                     {
                                         handle_network_error(m_core.notifier(), error_code, data);
                                     });
}

void ItemManager::restore_item(const std::string& id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id + "/restore";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().patch_json(url,
                                       headers,
                                       body,
                                       [this, id](int status_code, const std::string& response)
                                       {
                                           if (status_code == 200)
                                           {
                                               std::string old_parent_id = m_items[id].parent_id;
                                               nlohmann::json body = json::parse(response, nullptr, false);
                                               m_items[id].deleted_at = std::nullopt;
                                               m_items[id].parent_id = json_utils::get_string(body, "parent_id", special_folder::HOME);
                                               m_items[id].save(m_core.database_provider().database());
                                               nlohmann::json data;
                                               data["id"] = id;
                                               data["parent_id"] = m_items[id].parent_id;
                                               data["old_parent_id"] = old_parent_id;
                                               apply_move_item(id, old_parent_id, m_items[id].parent_id);
                                               m_core.notifier().notify(ITEM_RESTORE_SUCCESS, data);
                                           }
                                           else
                                           {
                                               std::map<std::string, std::string> args;
                                               args["status_code"] = std::to_string(status_code);
                                               args["response"] = response;
                                               m_core.notifier().notify(ITEM_RESTORE_FAILURE, args);
                                           }
                                       }, [this](std::int16_t error_code, const std::string& data)
                                       {
                                           handle_network_error(m_core.notifier(), error_code, data);
                                       });
}

void ItemManager::delete_item(const std::string& id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id + "/permanent";
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().destroy(url,
                                     headers,
                                     [this, id](int status_code, const std::string& response)
                                     {
                                         if (status_code == 200)
                                         {
                                             std::string parent_id = m_items[id].parent_id;
                                             item_delete_on_local(m_core, m_items[id]);
                                             if (m_items[id].type == item_type::FILE)
                                             {
                                                 delete_file_metadata(m_core.database_provider().database(), m_file_metadata_map[id]);
                                             }
                                             apply_delete_item(id, m_items[id].parent_id);
                                             nlohmann::json data;
                                             data["id"] = id;
                                             data["parent_id"] = parent_id;

                                             m_core.notifier().notify(ITEM_DELETED, id);
                                         }
                                         else
                                         {
                                             std::map<std::string, std::string> args;
                                             args["status_code"] = std::to_string(status_code);
                                             args["response"] = response;
                                             m_core.notifier().notify(ITEM_DELETE_FAILURE, args);
                                         }
                                     }, [](std::int16_t error_code, const std::string& data)
                                     {
                                     });
}

void ItemManager::download_thumbnail(const std::string& id) const
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id + "/thumbnail";
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    m_core.network_provider().get(url,
                                  headers,
                                  [this, id, url](int status_code, const std::string& response)
                                  {
                                      fs::path thumbnail_path = item_thumbnail_path(m_core, id);
                                      if (!fs::exists(thumbnail_path.parent_path()))
                                      {
                                          fs::create_directories(thumbnail_path.parent_path());
                                      }
                                      if (status_code == 200)
                                      {
                                          const std::string& download_url = response;
                                          std::map<std::string, std::string> headers;
                                          m_core.network_provider().download_file(
                                              download_url,
                                              headers,
                                              thumbnail_path.string(),
                                              [](std::int64_t bytes_received, std::int64_t total_bytes)
                                              {
                                              },
                                              [this, id, download_url](int status_code, const std::string& response)
                                              {
                                                  if (status_code == 200)
                                                  {
                                                      m_core.notifier().notify(ITEM_THUMBNAIL_DOWNLOAD_SUCCESS);
                                                  }
                                                  else
                                                  {
                                                      std::map<std::string, std::string> args;
                                                      args["status_code"] = std::to_string(status_code);
                                                      args["response"] = response;
                                                      args["id"] = id;
                                                      args["url"] = download_url;
                                                      m_core.notifier().notify(ITEM_THUMBNAIL_DOWNLOAD_FAILURE, args);
                                                  }
                                              }, [](std::int16_t error_code, const std::string& data)
                                              {
                                              });
                                      }
                                      else
                                      {
                                          std::map<std::string, std::string> args;
                                          args["status_code"] = std::to_string(status_code);
                                          args["response"] = response;
                                          args["id"] = id;
                                          args["url"] = url;
                                          m_core.notifier().notify(ITEM_THUMBNAIL_DOWNLOAD_FAILURE, args);
                                      }
                                  }, [](std::int16_t error_code, const std::string& data)
                                  {
                                  });
}

void ItemManager::cache_item(const std::string& id)
{
    std::string file_path = item_local_file_path(m_core, id).string();
    download_item(id, file_path, [this, id](int status_code, const std::string& response)
    {
        if (status_code == 200)
        {
            json data;
            m_items[id].cached = true;
            data["id"] = id;
            data["parent_id"] = m_items[id].parent_id;
            m_core.notifier().notify(ITEM_CACHE_SUCCESS, id);
        }
        else
        {
            notify_request_failure(m_core.notifier(), ITEM_CACHE_FAILURE, status_code,
                                   response);
        }
    });
}

void ItemManager::cache_item_external(const std::string& id)
{
}

void ItemManager::download_item(const std::string& id, const std::string& file_path) const
{
    download_item(id, file_path, [this, id](int status_code, const std::string& response)
    {
        if (status_code == 200)
        {
            m_core.notifier().notify(FILE_DOWNLOAD_SUCCESS, id);
        }
        else
        {
            notify_request_failure(m_core.notifier(), FILE_DOWNLOAD_FAILURE, status_code,
                                   response);
        }
    });
}

void ItemManager::download_item(const std::string& id, const std::string& file_path,
                                const std::function<void(int status_code, const std::string& response)>& on_response)
const
{
    api::files::get_download_url(
        m_core,
        id,
        [this, id, file_path, on_response](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                const std::string& download_url = response;
                std::map<std::string, std::string> headers;
                m_core.network_provider().download_file(
                    download_url, headers, file_path,
                    [this, id](std::int64_t bytes_received, std::int64_t total_bytes)
                    {
                        json data;
                        data["bytes_received"] = bytes_received;
                        data["total_bytes"] = total_bytes;
                        data["id"] = id;
                        data["type"] = transfer_type::FILE;
                        m_core.notifier().notify(DOWNLOAD_PROGRESS, data);
                    }, on_response, [this](std::int16_t error_code, const std::string& data)
                    {
                        handle_network_error(m_core.notifier(), error_code, data);
                    });
            }
            else
            {
                notify_request_failure(m_core.notifier(), FILE_DOWNLOAD_FAILURE, status_code,
                                       response);
            }
        },
        [this](std::int16_t error_code, const std::string& data)
        {
            handle_network_error(m_core.notifier(), error_code, data);
        });
}

void ItemManager::fetch_file_download_url(const std::string& id) const
{
    api::files::get_download_url(m_core, id, [this, id](int status_code, const std::string& response)
                                 {
                                     if (status_code == 200)
                                     {
                                         const std::string& download_url = response;
                                         json data;
                                         data["id"] = id;
                                         data["url"] = download_url;
                                         m_core.notifier().notify(FETCH_FILE_DOWNLOAD_URL_SUCCESS, data);
                                     }
                                     else
                                     {
                                         json data;
                                         data["id"] = id;
                                         m_core.notifier().notify(FETCH_FILE_DOWNLOAD_URL_FAILURE, data);
                                     }
                                 },
                                 [this](std::int16_t error_code, const std::string& data)
                                 {
                                     handle_network_error(m_core.notifier(), error_code, data);
                                 });
}

void ItemManager::sync_next_event()
{
    SyncEvent event;
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (m_event_queue.empty()) {
            m_is_processing = false; // 더 이상 처리할 이벤트 없음
            return;
        }
        event = m_event_queue.front();
        m_event_queue.pop();
    }

    // 타입에 맞춰 작업 실행 (작업이 완전히 끝날 때 process_next_event() 재귀 호출)
    switch (event.type) {
    case SyncEventType::CREATE:
    case SyncEventType::UPDATE:
    api::items::get_item(m_core, event.item_id, [this, event](int status_code, const std::string& response)
                {
                    if (status_code == 200)
                    {
                        nlohmann::json body = nlohmann::json::parse(response, nullptr, false);
                        Item item = item_from_json(body);
                        item.save(m_core.database_provider().database());
                        if (item.type == item_type::FILE)
                        {
                            api::files::get_file_metadata(m_core, item.id, [this, item, event](FileMetadata& file_metadata)
                           {
                              cache_file_metadata(m_core.database_provider().database(), file_metadata);
                               apply_create_item(item);
                               m_file_metadata_map[file_metadata.id] = std::move(file_metadata);
                               api::sync::consume_event(m_core, event.id, [](int status_code, const std::string& response)
                               {

                                   if (status_code != 200)
                                   {
                                       // TODO: Handle failure when consuming sync event
                                   }
                               }, [](std::int16_t error_code, const std::string& data)
                                                        {
                                                        });
                           });
                        }
                        else
                        {
                            api::files::get_file_metadata(m_core, item.id, [this, item, event](FileMetadata& file_metadata)
                       {
                          cache_file_metadata(m_core.database_provider().database(), file_metadata);
                           apply_create_item(item);
                           m_file_metadata_map[file_metadata.id] = std::move(file_metadata);
                           api::sync::consume_event(m_core, event.id, [](int status_code, const std::string& response)
                           {
                               if (status_code != 200)
                               {
                                   // TODO: Handle failure when consuming sync event
                               }
                           }, [](std::int16_t error_code, const std::string& data)
                                                    {
                                                    });
                       });
                        }
                    }
                    else
                    {
                        // TODO: Handle error when failing to fetch created item during sync
                    }
                }, [](std::int16_t error_code, const std::string& data)
                                     {
                                     });
        break;
    default:
        sync_next_event();
        break;
    }
}

void ItemManager::apply_create_item(const Item& item)
{
    m_items[item.id] = item;
    if (map_utils::contains_key(m_id_lists, item.parent_id))
    {
        m_id_lists[item.parent_id].emplace_back(item.id);
    }
    sort_items(item.parent_id);
}

void ItemManager::apply_update_item(const Item& item)
{
    m_items[item.id] = item;
}

void ItemManager::apply_move_item(const std::string& id, const std::string& old_parent_id, const std::string& parent_id)
{
    m_items[id].parent_id = parent_id;
    if (m_id_lists.find(old_parent_id) != m_id_lists.end())
    {
        auto& old_list = m_id_lists[old_parent_id];
        old_list.erase(std::remove(old_list.begin(), old_list.end(), id),
                       old_list.end());
    }
    if (m_id_lists.find(parent_id) != m_id_lists.end())
    {
        auto& new_list = m_id_lists[parent_id];
        new_list.push_back(id);
    }
    sort_items(m_items[id].parent_id);
}

void ItemManager::apply_delete_item(const std::string& id, const std::string& parent_id)
{
    if (map_utils::contains_key(m_id_lists, parent_id))
    {
        auto& list = m_id_lists[parent_id];
        list.erase(std::remove(list.begin(), list.end(), id),
                       list.end());
        m_items.erase(id);
    }
}
