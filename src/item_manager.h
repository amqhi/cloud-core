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

class ItemManager
{
public:
    explicit ItemManager(Core& core);

    const std::vector<UUID>& id_list_by_id(const UUID& parent_id);
    [[nodiscard]] const Item& item_by_id(const UUID& id);
    [[nodiscard]] const FileMetadata& file_metadata_by_id(const UUID& id);
    void initialize();
    void sync();
    void refresh();
    void sort_items(std::int8_t option, const UUID& parent_id);
    void sort_items(const UUID& parent_id);
    void create_file(const ItemAttributes& item_attributes, const std::string& tmp_file_path);
    void create_folder(const ItemAttributes& item_attributes);
    void complete_upload_multipart(const Item& item, const std::string& checksum, std::uint64_t size,
                                   const std::string& mime_type, const std::string& upload_id, const
                                   nlohmann::json& parts);
    void complete_upload(const Item& item, const std::string& checksum, std::uint64_t size,
                         const std::string& mime_type);
    void update_item(const UUID& id, const ItemAttributes& item_attributes);
    void move_item(const UUID& id, const UUID& parent_id);
    void rename_item(const UUID& id, const std::string& name);
    void soft_delete_item(const UUID& id);
    void restore_item(const UUID& id);
    void delete_item(const UUID& id);
    void download_thumbnail(const UUID& id) const;
    void cache_item(const UUID& id);
    void download_item(const UUID& id, const std::string& file_path);
    void download_item(const UUID& id, const std::string& file_path,
                       const std::function<void(int status_code, const std::string& response)>& on_response);
    void fetch_file_download_url(const UUID& id) const;

private:
    Core& m_core;

    std::unordered_map<UUID, Item> m_items;
    std::unordered_map<UUID, std::vector<UUID>> m_id_lists;
    std::unordered_map<UUID, FileMetadata> m_file_metadata;
    // TODO: Implement CRUD for folder customization
    std::unordered_map<UUID, FolderMetadata> m_folder_metadata;

    // Prefix 'apply_' indicates mutating internal state (m_items, m_id_lists)
    void apply_create_item(const Item& item);
    void apply_update_item(const Item& item);
    void apply_move_item(const UUID& id, const UUID& old_parent_id, const UUID& parent_id);
    void apply_delete_item(const UUID& id, const UUID& parent_id);
};

#endif //CLOUD_CORE_ITEM_MANAGER_H
