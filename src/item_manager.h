// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_ITEM_MANAGER_H
#define CLOUD_CORE_ITEM_MANAGER_H

#include <map>
#include "item.h"
#include <functional>

#include "file_metadata.h"
#include "folder_metadata.h"
#include "json.hpp"

#include "item_attributes.h"
#include "sync_event.h"

class Core;
struct SettingsData;
class INetworkProvider;
class INotifier;
struct User;

struct ItemSummary
{
    std::string id;
    std::string name;
    std::string thumbnail_path;
    std::int64_t deleted_at;
    std::int64_t created_at;
    std::int64_t updated_at;
    std::int64_t size;
    std::int16_t app_scope = 63;
    std::int8_t type;
    bool encrypted = false;
    bool cached = false;
};

class ItemManager
{
public:
    explicit ItemManager(Core& core);

    const std::vector<ItemId>& id_list_by_id(const ItemId& parent_id);
    [[nodiscard]] const Item& item_by_id(const ItemId& id);
    [[nodiscard]] const FileMetadata& file_metadata_by_id(const ItemId& id);
    void initialize();
    void sync();
    void refresh();
    void sort_items(std::int8_t option, const ItemId& parent_id);
    void sort_items(const ItemId& parent_id);
    void create_file(const ItemAttributes& item_attributes, const std::string& tmp_file_path);
    void create_folder(const ItemAttributes& item_attributes);
    void complete_upload_multipart(const Item& item, const std::string& checksum, std::uint64_t size,
                                   const std::string& mime_type, const std::string& upload_id, const
                                   nlohmann::json& parts);
    void complete_upload(const Item& item, const std::string& checksum, std::uint64_t size,
                         const std::string& mime_type);
    void update_item(const ItemId& id, const ItemAttributes& item_attributes);
    void move_item(const ItemId& id, const ItemId& parent_id);
    void rename_item(const ItemId& id, const std::string& name);
    void soft_delete_item(const ItemId& id);
    void restore_item(const ItemId& id);
    void delete_item(const ItemId& id);
    void download_thumbnail(const ItemId& id) const;
    void cache_item(const ItemId& id);
    void download_item(const ItemId& id, const std::string& file_path);
    void download_item(const ItemId& id, const std::string& file_path,
                       const std::function<void(int status_code, const std::string& response)>& on_response);
    void fetch_file_download_url(const ItemId& id) const;

private:
    Core& m_core;

    std::unordered_map<ItemId, Item> m_items;
    std::unordered_map<ItemId, std::vector<ItemId>> m_id_lists;
    std::unordered_map<ItemId, FileMetadata> m_file_metadata;
    // TODO: Implement CRUD for folder customization
    std::unordered_map<ItemId, FolderMetadata> m_folder_metadata;

    // Prefix 'apply_' indicates mutating internal state (m_items, m_id_lists)
    void apply_create_item(const Item& item);
    void apply_update_item(const Item& item);
    void apply_move_item(const ItemId& id, const ItemId& old_parent_id, const ItemId& parent_id);
    void apply_delete_item(const ItemId& id, const ItemId& parent_id);
};

#endif //CLOUD_CORE_ITEM_MANAGER_H
