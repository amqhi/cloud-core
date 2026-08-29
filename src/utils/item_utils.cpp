#include "item_utils.h"

#include "core.h"

void item_delete_on_local(Core& core, const Item& item)
{
    item_delete_on_local(core.app_support_path(), core.selected_user()->local_id, core.database_provider().database(), item.id);
}

std::filesystem::path item_local_file_path(Core& core, const ItemId& id)
{
    return item_local_file_path(core.app_support_path(), core.selected_user()->local_id, id);
}

std::filesystem::path item_thumbnail_path(Core& core, const ItemId& id)
{
    return item_thumbnail_path(core.app_support_path(), core.selected_user()->local_id, id);
}

ItemSummary item_to_summary(Core& core, const Item& item)
{
    ItemSummary summary;
    //TODO: refactor
    // summary.id = item.id;
    // summary.title = item.name;
    // summary.thumbnail_path = item_thumbnail_path(core, item.id);
    // summary.type = item.type;
    //
    // summary.app_scope = item.app_scope;
    //
    // std::string id;
    // std::string title;
    // std::string snippet;
    // std::string thumbnail_path;
    // std::int64_t deleted_at;
    // std::int64_t created_at;
    // std::int64_t updated_at;
    // std::int16_t app_scope = 63;
    // std::int8_t type;
    // bool encrypted = false;
    // bool cached = false;
    return summary;
}