#ifndef CLOUD_CORE_ITEM_MANAGER_H
#define CLOUD_CORE_ITEM_MANAGER_H

#include <map>
#include "item.h"
#include <functional>

#include "../../../shared/src/models/file_metadata.h"
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

    [[nodiscard]] std::map<std::string, Item> items() const;
    [[nodiscard]] std::map<std::string, std::vector<std::string>> id_lists() const;
    [[nodiscard]] std::map<std::string, FileMetadata> file_metadata_map() const;
    void sync();
    void fetch_items_from_cache(const std::string& parent_id);
    void fetch_items(const std::string& parent_id);
    void purge_items(const std::string& parent_id);
    void cache_metadata_of_items(const std::string& parent_id);
    const std::vector<std::string>& id_list_by_id(const std::string& id);
    std::vector<ItemSummary> summary_list_by_id(const std::string& id);
    const Item& item_by_id(const std::string& id);
    const FileMetadata& file_metadata_by_id(const std::string& id);
    void sort_items(const std::string& parent_id);
    void create_file(const ItemAttributes& item_attributes, const std::string& tmp_file_path);
    void create_folder(const ItemAttributes& item_attributes);
    void complete_upload_multipart(const Item& item, const std::string& checksum, std::uint64_t size,
                                   const std::string& mime_type, const std::string& upload_id, const
                                   nlohmann::json& parts);
    void complete_upload(const Item& item, const std::string& checksum, std::uint64_t size,
                         const std::string& mime_type);
    void update_item(const std::string& id, const ItemAttributes& item_attributes);
    void move_item(const std::string& id, const std::string& parent_id);
    void rename_item(const std::string& id, const std::string& name);
    void soft_delete_item(const std::string& id);
    void restore_item(const std::string& id);
    void delete_item(const std::string& id);
    void download_thumbnail(const std::string& id) const;
    void cache_item(const std::string& id);
    void cache_item_external(const std::string& id);
    void download_item(const std::string& id, const std::string& file_path) const;
    void download_item(const std::string& id, const std::string& file_path,
                       const std::function<void(int status_code, const std::string& response)>& on_response) const;
    void fetch_file_download_url(const std::string& id) const;

private:
    Core& m_core;
    std::queue<SyncEvent> m_event_queue;
    bool m_is_processing = false;
    std::mutex m_queue_mutex;
    std::map<std::string, Item> m_items;
    std::map<std::string, std::vector<std::string>> m_id_lists;
    std::map<std::string, FileMetadata> m_file_metadata_map;

    void sync_next_event();

    // Prefix 'apply_' indicates mutating internal state (m_items, m_id_lists)
    void apply_create_item(const Item& item);
    void apply_update_item(const Item& item);
    void apply_move_item(const std::string& id, const std::string& old_parent_id, const std::string& parent_id);
    void apply_delete_item(const std::string& id, const std::string& parent_id);
};

#endif //CLOUD_CORE_ITEM_MANAGER_H
