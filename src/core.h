// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_H
#define CLOUD_CORE_H
#include <map>
#include <vector>
#include <string>
#include <functional>

#include "user.h"
#include "cached_state.h"
#include "core_base.h"
#include "database_provider.h"
#include "item_manager.h"
#include "settings.h"
#include "secure_storage_provider.h"

class IPlatformUtils;
class INotifier;

class Core : public CoreBase
{
public:
    explicit Core(std::string app_support_path, INetworkProvider& network_provider,
                  ISecureStorageProvider& secure_storage_provider, INotifier& notifier, IPlatformUtils& platform_utils);
    void initialize() override;

private:
    // std::unique_ptr<CachedStateBase> create_cached_state() override
    // {
    //     return std::make_unique<CachedState>(m_app_support_path);
    // }
    // std::unique_ptr<ItemManagerBase> create_item_manager() override
    // {
    //     return std::make_unique<ItemManager>(*this);
    // }
    // std::unique_ptr<INotifierBase> create_notifier() override
    // {
    //     return std::make_unique<INotifier>(*this);
    // }
    // std::unique_ptr<SettingsBase> create_settings() override
    // {
    //     return std::make_unique<Settings>(*this);
    // }
};

#endif // CLOUD_CORE_H
