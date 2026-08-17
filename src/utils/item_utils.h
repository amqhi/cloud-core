#ifndef CLOUD_CORE_ITEM_UTILS_H
#define CLOUD_CORE_ITEM_UTILS_H
#include <string>

#include "item.h"

class Core;
void item_delete_on_local(Core& core, const Item& item);
std::filesystem::path item_local_file_path(Core& core, const std::string& id);
std::filesystem::path item_thumbnail_path(Core& core, const std::string& id);
ItemSummary item_to_summary(Core& core, const Item& item);

#endif //CLOUD_CORE_ITEM_UTILS_H
