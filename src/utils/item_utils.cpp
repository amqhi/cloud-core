// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
