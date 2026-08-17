#include "item_utils.h"

#include "core.h"

void item_delete_on_local(Core& core, const Item& item)
{
    item_delete_on_local(core.app_support_path(), core.selected_user()->local_id, core.database_provider().database(), item.id);
}

std::filesystem::path item_local_file_path(Core& core, const std::string& id)
{
    return item_local_file_path(core.app_support_path(), core.selected_user()->local_id, id);
}

std::filesystem::path item_thumbnail_path(Core& core, const std::string& id)
{
    return item_thumbnail_path(core.app_support_path(), core.selected_user()->local_id, id);
}

ItemSummary item_to_summary(Core& core, const Item& item)
{
    ItemSummary summary;
    summary.id = item.id;
    summary.thumbnail_path = item_thumbnail_path(core, item.id);
    summary.type = item.type;
    summary.name = item.name;
    return summary;
}