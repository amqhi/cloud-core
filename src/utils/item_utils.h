// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_ITEM_UTILS_H
#define CLOUD_CORE_ITEM_UTILS_H
#include <string>

#include "item.h"

class Core;
void item_delete_on_local(Core& core, const Item& item);
std::filesystem::path item_local_file_path(Core& core, const ItemId& id);
std::filesystem::path item_thumbnail_path(Core& core, const ItemId& id);
// ItemSummary item_to_summary(Core& core, const Item& item);

#endif //CLOUD_CORE_ITEM_UTILS_H
