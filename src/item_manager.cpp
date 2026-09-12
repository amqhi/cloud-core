// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "item_manager.h"

#include "core.h"
#include <fstream>
#include <filesystem>
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
#include "json_utils.h"
#include "map_utils.h"
#include "sqlite_utils.h"
#include "sync_event.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

ItemManager::ItemManager(Core& core) : m_core(core)
{
}

const std::vector<ItemId>& ItemManager::id_list_by_id(const ItemId& parent_id)
{
    return m_id_lists[parent_id];
}

const Item& ItemManager::item_by_id(const ItemId& id)
{
    return m_items[id];
}

const FileMetadata& ItemManager::file_metadata_by_id(const ItemId& id)
{
    return m_file_metadata[id];
}

void ItemManager::initialize()
{
    Sqlite3Stmt stmt;
    const char* sql = "SELECT * FROM items;";

    if (stmt.prepare(m_core.database_provider().database(), sql) != SQLITE_OK)
    {
        m_core.notifier().notify(DATABASE_ERROR, std::string(sqlite3_errmsg(m_core.database_provider().database())));
        return;
    }

    while (stmt.step() == SQLITE_ROW)
    {
        Item item = item_from_stmt(stmt.stmt);

        m_items[item.id] = item;
        m_id_lists[item.parent_id].push_back(item.id);
    }

    Sqlite3Stmt files_stmt;

    sql = "SELECT * FROM files;";
    files_stmt.prepare(m_core.database_provider().database(), sql);
    while (files_stmt.step() == SQLITE_ROW)
    {
        FileMetadata file_metadata = file_metadata_from_stmt(files_stmt.stmt);

        m_file_metadata[file_metadata.id] = file_metadata;
    }
}

void ItemManager::sync()
{
    api::sync::get_sync_events(m_core, [this](int status_code, const std::string& response)
                               {
                                   if (status_code == 200)
                                   {
                                       nlohmann::json body = nlohmann::json::parse(response, nullptr, false);
                                       if (!body.is_discarded())
                                       {
                                           if (auto it = body.find("events"); it != body.end() && it->is_array())
                                           {
                                               const auto& events = it->get<nlohmann::json>();
                                               std::vector<std::string> item_ids;
                                               for (const auto& event : events)
                                               {
                                                   SyncEventType event_type = string_to_sync_event_type(
                                                       json_utils::get_string(event, "type"));
                                                   std::int8_t item_type = item_type::parse(
                                                       json_utils::get_string(event, "item_type"));

                                                   if (auto item_data_raw = event.find("item"); item_data_raw != event.
                                                       end() && item_data_raw->is_object())
                                                   {
                                                       const auto& item_data = item_data_raw->get<nlohmann::json>();
                                                       auto item = item_from_json(item_data);
                                                       ItemId item_id = item.id;
                                                       item_ids.emplace_back(item_id.to_string());

                                                       // TODO: Handle sync for app scope changes
                                                       switch (event_type)
                                                       {
                                                       case SyncEventType::DELETE:
                                                           delete_item(item.id);
                                                           apply_delete_item(item.id, item.parent_id);
                                                           break;
                                                           default:
                                                           item.save(m_core.database_provider().database());
                                                           if (auto pair = m_items.find(item.id); pair != m_items.end()) // If the item is already exists
                                                           {
                                                               auto& existing_item = pair->second;

                                                               if (existing_item.parent_id != item.parent_id || item.deleted_at != existing_item.deleted_at) // Handle move or soft-delete or restore
                                                               {
                                                                   ItemId old_parent_id = existing_item.parent_id;
                                                                   m_items[item_id] = std::move(item);
                                                                   apply_move_item(item_id, old_parent_id, m_items[item_id].parent_id);
                                                               }
                                                               else // Handle update
                                                               {
                                                                   m_items[item_id] = std::move(item);
                                                                   sort_items(m_items[item_id].parent_id);
                                                               }
                                                           }
                                                           else
                                                           {
                                                               m_id_lists[item.parent_id].push_back(item.id);
                                                               apply_create_item(item);
                                                           }
                                                           break;
                                                       }

                                                       switch (item_type)
                                                       {
                                                       // TODO: Implement remaining item types for item synchronization
                                                       case item_type::FILE:
                                                           {
                                                               auto file_metadata = file_metadata_from_json(item_data);
                                                               cache_file_metadata(m_core.database_provider().database(), file_metadata);
                                                               break;
                                                           }
                                                       default:
                                                           break;
                                                       }
                                                   }
                                               }

                                               if (!item_ids.empty())
                                               {
                                                   api::sync::acknowledge_events(m_core, item_ids,
                                                                   [this](int status_code,
                                                                   const std::string& response)
                                                                   {
                                                                       if (status_code != 200)
                                                                       {
                                                                           notify_request_failure(m_core.notifier(), ACKNOWLEDGE_SYNC_EVENTS_FETCH_FAILURE,
                                                       status_code, response);
                                                                       }
                                                                   }, [this](int error_code,
                                                                   const std::string& data)
                                                                   {
                                                                       notify_request_failure(m_core.notifier(), ACKNOWLEDGE_SYNC_EVENTS_FETCH_FAILURE,
                                                       error_code, data);
                                                                   });
                                               }
                                           }

                                           if (json_utils::get_bool(body, "has_more", false))
                                           {
                                               // TODO: Fully implement sync handling when has_more is true
                                               sync();
                                           }

                                           m_core.notifier().notify(SYNC_SUCCESS);
                                       }
                                       else
                                       {
                                           notify_request_failure(m_core.notifier(), SYNC_EVENTS_FETCH_FAILURE,
                                                                  status_code, response);
                                       }
                                   }
                                   else
                                   {
                                       notify_request_failure(m_core.notifier(), SYNC_EVENTS_FETCH_FAILURE, status_code,
                                                              response);
                                   }
                               }, [this](std::int16_t error_code, const std::string& data)
                               {
                                   handle_network_error(m_core.notifier(), error_code, data);
                               });
}

// TODO: Make this safe against race conditions (e.g., database)
void ItemManager::refresh()
{
    std::string url = m_core.settings().data().instance_url + "/items?status=all";
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().get(
        url,
        headers,
        [this, url](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                auto body = json::parse(response, nullptr, false);
                if (!body.is_discarded() && body.is_array())
                {
                    for (const auto& element : body)
                    {
                        Item item = item_from_json(element);
                        if (!map_utils::contains_key(m_items, item.id))
                        {
                            m_id_lists[item.parent_id].push_back(item.id);
                            item.save(m_core.database_provider().database());

                            if (item.type == item_type::FILE)
                            {
                                api::files::get_file_metadata(m_core, item.id.to_string(),
                                                              [this](FileMetadata& file_metadata)
                                                              {
                                                                  cache_file_metadata(
                                                                      m_core.database_provider().database(),
                                                                      file_metadata);
                                                                  m_file_metadata[file_metadata.id] = std::move(
                                                                      file_metadata);
                                                              });
                            }
                            m_items[item.id] = std::move(item);
                        }
                    }
                    sort_items(special_folder::HOME);
                    m_core.notifier().notify(REFRESH_SUCCESS);
                }
                else
                {
                    notify_request_failure(m_core.notifier(), REFRESH_FAILURE, status_code, response, url);
                }
            }
            else
            {
                notify_request_failure(m_core.notifier(), REFRESH_FAILURE, status_code, response, url);
            }
        }
        , [this](std::int16_t error_code, const std::string& data)
        {
            handle_network_error(m_core.notifier(), error_code, data);
        });
}

void ItemManager::sort_items(std::int8_t option, const ItemId& parent_id)
{
    m_core.cached_state().set_sort_option(parent_id, option);
    sort_items(parent_id);
}

void ItemManager::sort_items(const ItemId& parent_id)
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
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].name < m_items[b].name;
                      });
            break;

        case sort_option::NAME_DESC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].name > m_items[b].name;
                      });
            break;

        case sort_option::CREATED_AT_ASC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].created_at < m_items[b].created_at;
                      });
            break;

        case sort_option::CREATED_AT_DESC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].created_at > m_items[b].created_at;
                      });
            break;

        case sort_option::UPDATED_AT_ASC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].updated_at < m_items[b].updated_at;
                      });
            break;

        case sort_option::UPDATED_AT_DESC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].updated_at > m_items[b].updated_at;
                      });
            break;

        case sort_option::SIZE_ASC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].updated_at < m_items[b].updated_at;
                      });
            break;

        // TODO: sort items by size
        case sort_option::SIZE_DESC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].updated_at > m_items[b].updated_at;
                      });
            break;

        case sort_option::TYPE_ASC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
                          return m_items[a].type < m_items[b].type;
                      });
            break;

        case sort_option::TYPE_DESC:
            std::sort(id_list.begin(), id_list.end(),
                      [this](const ItemId& a, const ItemId& b)
                      {
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
                                                            [this, item](
                                                            std::int64_t bytes_written, std::int64_t total_bytes)
                                                            {
                                                                json data;
                                                                data["bytes_written"] = bytes_written;
                                                                data["total_bytes"] = total_bytes;
                                                                data["id_high"] = item.id.high;
                                                                data["id_low"] = item.id.low;
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
                                                                handle_network_error(
                                                                    m_core.notifier(), error_code, data);
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
                                                            [this, item](
                                                            std::int64_t bytes_written, std::int64_t total_bytes)
                                                            {
                                                                json data;
                                                                data["bytes_written"] = bytes_written;
                                                                data["total_bytes"] = total_bytes;
                                                                data["id_high"] = item.id.high;
                                                                data["id_low"] = item.id.low;
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
                                                                handle_network_error(
                                                                    m_core.notifier(), error_code, data);
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
                                                    data["id_high"] = item.id.high;
                                                    data["id_low"] = item.id.low;
                                                    data["parent_id_high"] = item.parent_id.high;
                                                    data["parent_id_low"] = item.parent_id.low;
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
    std::string url = m_core.settings().data().instance_url + "/files/" + item.id.to_string() + "/complete";
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
                                        [this, item, checksum, mime_type, size](
                                        int status_code, const std::string& response)
                                        {
                                            if (status_code == 200)
                                            {
                                                FileMetadata file_metadata;
                                                file_metadata.id = item.id;
                                                file_metadata.checksum = checksum;
                                                file_metadata.mime_type = mime_type;
                                                file_metadata.size = size;
                                                cache_file_metadata(m_core.database_provider().database(),
                                                                    file_metadata);
                                                m_items[item.id] = item;
                                                m_id_lists[item.parent_id].push_back(item.id);
                                                json data;
                                                data["id_high"] = item.id.high;
                                                data["id_low"] = item.id.low;
                                                data["parent_id_high"] = item.parent_id.high;
                                                data["parent_id_low"] = item.parent_id.low;
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
    std::string url = m_core.settings().data().instance_url + "/files/" + item.id.to_string() + "/complete";
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
                                        [this, item, checksum, mime_type, size](
                                        int status_code, const std::string& response)
                                        {
                                            if (status_code == 200)
                                            {
                                                FileMetadata file_metadata;
                                                file_metadata.id = item.id;
                                                file_metadata.checksum = checksum;
                                                file_metadata.mime_type = mime_type;
                                                file_metadata.size = size;
                                                cache_file_metadata(m_core.database_provider().database(),
                                                                    file_metadata);
                                                m_items[item.id] = item;
                                                m_id_lists[item.parent_id].push_back(item.id);
                                                json data;
                                                data["id_high"] = item.id.high;
                                                data["id_low"] = item.id.low;
                                                data["parent_id_high"] = item.parent_id.high;
                                                data["parent_id_low"] = item.parent_id.low;
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

void ItemManager::update_item(const ItemId& id, const ItemAttributes& item_attributes)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string();
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
                                                 m_items[id].name = item_attributes.name;
                                                 m_items[id].parent_id = item_attributes.parent_id;
                                                 m_items[id].app_scope = item_attributes.app_scope;
                                                 m_items[id].comment = item_attributes.comment;
                                                 m_items[id].event_at = item_attributes.event_at;
                                                 m_items[id].encrypted = item_attributes.encrypted;
                                                 m_items[id].save(m_core.database_provider().database());
                                                 json data;
                                                 data["id_high"] = id.high;
                                                 data["id_low"] = id.low;
                                                 data["parent_id_high"] = m_items[id].parent_id.high;
                                                 data["parent_id_low"] = m_items[id].parent_id.low;
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

void ItemManager::move_item(const ItemId& id, const ItemId& parent_id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string() + "/move";
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;
    nlohmann::json body;
    if (parent_id != special_folder::TRASH && parent_id != special_folder::HOME)
    {
        body["parent_id_high"] = parent_id.high;
        body["parent_id_low"] = parent_id.low;
    }
    m_core.network_provider().patch_json(url,
                                         headers,
                                         body,
                                         [this, id, parent_id](int status_code, const std::string& response)
                                         {
                                             if (status_code == 200)
                                             {
                                                 ItemId old_parent_id = m_items[id].parent_id;
                                                 json data;
                                                 data["id_high"] = id.high;
                                                 data["id_low"] = id.low;
                                                 data["parent_id_high"] = parent_id.high;
                                                 data["parent_id_low"] = parent_id.low;
                                                 data["old_parent_id_high"] = old_parent_id.high;
                                                 data["old_parent_id_low"] = old_parent_id.low;
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

void ItemManager::rename_item(const ItemId& id, const std::string& name)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string();
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
                                                 nlohmann::json data;
                                                 m_items[id].name = name;
                                                 m_items[id].save(m_core.database_provider().database());
                                                 data["id_high"] = id.high;
                                                 data["id_low"] = id.low;
                                                 data["parent_id_high"] = m_items[id].parent_id.high;
                                                 data["parent_id_low"] = m_items[id].parent_id.low;
                                                 m_core.notifier().notify(ITEM_UPDATE_SUCCESS, data);
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

void ItemManager::soft_delete_item(const ItemId& id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string();
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().destroy(url,
                                      headers,
                                      [this, id](int status_code, const std::string& response)
                                      {
                                          if (status_code == 200)
                                          {
                                              ItemId old_parent_id = m_items[id].parent_id;
                                              m_items[id].deleted_at = current_date_time_utc_int64();
                                              m_items[id].parent_id = special_folder::TRASH;
                                              m_items[id].save(m_core.database_provider().database());
                                              apply_move_item(id, old_parent_id, special_folder::TRASH);
                                              nlohmann::json data;
                                              data["id_high"] = id.high;
                                              data["id_low"] = id.low;
                                              data["parent_id_high"] = m_items[id].parent_id.high;
                                              data["parent_id_low"] = m_items[id].parent_id.low;
                                              data["old_parent_id_high"] = old_parent_id.high;
                                              data["old_parent_id_low"] = old_parent_id.low;
                                              m_core.notifier().notify(ITEM_SOFT_DELETE_SUCCESS, data);
                                          }
                                          else
                                          {
                                              notify_request_failure(m_core.notifier(), ITEM_SOFT_DELETE_FAILURE,
                                                                     status_code,
                                                                     response);
                                          }
                                      }, [this](std::int16_t error_code, const std::string& data)
                                      {
                                          handle_network_error(m_core.notifier(), error_code, data);
                                      });
}

void ItemManager::restore_item(const ItemId& id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string() + "/restore";
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
                                                 ItemId old_parent_id = m_items[id].parent_id;
                                                 nlohmann::json body = json::parse(response, nullptr, false);
                                                 m_items[id].deleted_at = std::nullopt;
                                                 if (auto it = body.find("parent_id"); it != body.end() && it->
                                                     is_string())
                                                 {
                                                     m_items[id].parent_id =
                                                         ItemId::from_string(it->get<std::string>());
                                                 }
                                                 else
                                                 {
                                                     m_items[id].parent_id = special_folder::HOME;
                                                 }

                                                 m_items[id].save(m_core.database_provider().database());
                                                 nlohmann::json data;
                                                 data["id_high"] = id.high;
                                                 data["id_low"] = id.low;
                                                 data["parent_id_high"] = m_items[id].parent_id.high;
                                                 data["parent_id_low"] = m_items[id].parent_id.low;
                                                 data["old_parent_id_high"] = old_parent_id.high;
                                                 data["old_parent_id_low"] = old_parent_id.low;

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

void ItemManager::delete_item(const ItemId& id)
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string() + "/permanent";
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_core.selected_user()->access_token;

    m_core.network_provider().destroy(url,
                                      headers,
                                      [this, id](int status_code, const std::string& response)
                                      {
                                          if (status_code == 200)
                                          {
                                              ItemId parent_id = m_items[id].parent_id;
                                              item_delete_on_local(m_core, m_items[id]);
                                              if (m_items[id].type == item_type::FILE)
                                              {
                                                  delete_file_metadata(m_core.database_provider().database(),
                                                                       m_file_metadata[id]);
                                              }
                                              apply_delete_item(id, m_items[id].parent_id);
                                              nlohmann::json data;
                                              data["id_high"] = id.high;
                                              data["id_low"] = id.low;
                                              data["parent_id_high"] = parent_id.high;
                                              data["parent_id_low"] = parent_id.low;

                                              m_core.notifier().notify(ITEM_DELETED, data);
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

void ItemManager::download_thumbnail(const ItemId& id) const
{
    std::string url = m_core.settings().data().instance_url + "/items/" + id.to_string() + "/thumbnail";
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
                                                      nlohmann::json data;
                                                      data["status_code"] = std::to_string(status_code);
                                                      data["response"] = response;
                                                      data["id_high"] = id.high;
                                                      data["id_low"] = id.low;
                                                      data["url"] = download_url;
                                                      m_core.notifier().notify(ITEM_THUMBNAIL_DOWNLOAD_FAILURE, data);
                                                  }
                                              }, [](std::int16_t error_code, const std::string& data)
                                              {
                                              });
                                      }
                                      else
                                      {
                                          nlohmann::json data;
                                          data["status_code"] = std::to_string(status_code);
                                          data["response"] = response;
                                          data["id_high"] = id.high;
                                          data["id_low"] = id.low;
                                          data["url"] = url;
                                          m_core.notifier().notify(ITEM_THUMBNAIL_DOWNLOAD_FAILURE, data);
                                      }
                                  }, [](std::int16_t error_code, const std::string& data)
                                  {
                                  });
}

void ItemManager::cache_item(const ItemId& id)
{
    std::string file_path = item_local_file_path(m_core, id).string();
    download_item(id, file_path, [this, id](int status_code, const std::string& response)
    {
        if (status_code == 200)
        {
            json data;
            m_items[id].cached = true;
            data["id_high"] = id.high;
            data["id_low"] = id.low;
            data["parent_id_high"] = m_items[id].parent_id.high;
            data["parent_id_low"] = m_items[id].parent_id.low;
            m_core.notifier().notify(ITEM_CACHE_SUCCESS, data);
        }
        else
        {
            notify_request_failure(m_core.notifier(), ITEM_CACHE_FAILURE, status_code,
                                   response);
        }
    });
}

void ItemManager::download_item(const ItemId& id, const std::string& file_path)
{
    download_item(id, file_path, [this, id](int status_code, const std::string& response)
    {
        if (status_code == 200)
        {
            nlohmann::json data;
            data["id_high"] = id.high;
            data["id_low"] = id.low;
            data["parent_id_high"] = m_items[id].parent_id.high;
            data["parent_id_low"] = m_items[id].parent_id.low;
            m_core.notifier().notify(FILE_DOWNLOAD_SUCCESS, data);
        }
        else
        {
            notify_request_failure(m_core.notifier(), FILE_DOWNLOAD_FAILURE, status_code,
                                   response);
        }
    });
}

void ItemManager::download_item(const ItemId& id, const std::string& file_path,
                                const std::function<void(int status_code, const std::string& response)>& on_response)
{
    api::files::get_download_url(
        m_core,
        id.to_string(),
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
                        data["id_high"] = id.high;
                        data["id_low"] = id.low;
                        data["parent_id_high"] = m_items[id].parent_id.high;
                        data["parent_id_low"] = m_items[id].parent_id.low;
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

void ItemManager::fetch_file_download_url(const ItemId& id) const
{
    api::files::get_download_url(m_core, id.to_string(), [this, id](int status_code, const std::string& response)
                                 {
                                     if (status_code == 200)
                                     {
                                         const std::string& download_url = response;
                                         json data;
                                         data["id_high"] = id.high;
                                         data["id_low"] = id.low;
                                         data["url"] = download_url;
                                         m_core.notifier().notify(FETCH_FILE_DOWNLOAD_URL_SUCCESS, data);
                                     }
                                     else
                                     {
                                         json data;
                                         data["id_high"] = id.high;
                                         data["id_low"] = id.low;
                                         m_core.notifier().notify(FETCH_FILE_DOWNLOAD_URL_FAILURE, data);
                                     }
                                 },
                                 [this](std::int16_t error_code, const std::string& data)
                                 {
                                     handle_network_error(m_core.notifier(), error_code, data);
                                 });
}

void ItemManager::apply_create_item(const Item& item)
{
    m_items[item.id] = item;
    m_id_lists[item.parent_id].emplace_back(item.id);
    sort_items(item.parent_id);
}

void ItemManager::apply_update_item(const Item& item)
{
    m_items[item.id] = item;
}

void ItemManager::apply_move_item(const ItemId& id, const ItemId& old_parent_id, const ItemId& parent_id)
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

void ItemManager::apply_delete_item(const ItemId& id, const ItemId& parent_id)
{
    if (map_utils::contains_key(m_id_lists, parent_id))
    {
        auto& list = m_id_lists[parent_id];
        list.erase(std::remove(list.begin(), list.end(), id),
                   list.end());
        m_items.erase(id);
    }
}
