// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_ITEM_MANAGER_H
#define CLOUD_CORE_ITEM_MANAGER_H

#include "item_manager_base.h"

class Core;
class INetworkProvider;
class INotifier;
struct User;

class ItemManager : public ItemManagerBase
{
public:
    explicit ItemManager(Core& core);


};

#endif //CLOUD_CORE_ITEM_MANAGER_H
