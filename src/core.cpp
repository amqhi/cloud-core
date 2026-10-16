// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "core.h"
#include <filesystem>
#include <utility>

#include "database_provider.h"
#include "json_utils.h"
#include "network_provider.h"
#include "notifier.h"
#include "platform_utils.h"

namespace fs = std::filesystem;

Core::Core(std::string app_support_path, INetworkProvider& network_provider,
           ISecureStorageProvider& secure_storage_provider,
           INotifier& notifier, IPlatformUtils& platform_utils)
    : CoreBase(std::move(app_support_path), network_provider, secure_storage_provider, notifier, platform_utils)
{
    // TODO: extract initializing pointer members and users logic into base class
    m_cached_state = std::make_unique<CachedState>(m_app_support_path);

    if (!fs::exists(m_app_support_path))
    {
        fs::create_directories(m_app_support_path);
    }

    // Load user sessions from directories and restore the cached active session.
    // If none is selected, fallback to the first session or create a new one if none exist.
    for (const auto& entry : fs::directory_iterator(m_app_support_path))
    {
        if (entry.is_directory())
        {
            const auto dir_name = entry.path().filename().string();
            if (dir_name.size() == 1 && (dir_name[0] >= '0' && dir_name[0] <= '9'))
            {
                User user;
                user.local_id = dir_name[0];
                get_user_data(m_app_support_path, user);
                m_users.push_back(user);
                if (m_cached_state->selected_user_id() == user.local_id)
                {
                    m_selected_user_id = user.local_id;
                }
            }
        }
    }

    if (selected_user() == nullptr)
    {
        if (m_users.empty())
        {
            User user;
            user.local_id = created_user_local_id(m_users);
            m_users.push_back(user);
            m_selected_user_id = user.local_id;
        }
        else
        {
            m_selected_user_id = m_users[0].local_id;
        }
        m_cached_state->set_selected_user_id(m_selected_user_id);
    }

    m_settings = std::make_unique<Settings>(*this);
    m_database_provider = std::make_unique<DatabaseProvider>(*this);

    m_item_manager = std::make_unique<ItemManager>(*this);
}

void Core::initialize()
{
    m_settings->initialize();
    m_cached_state->save();
    m_database_provider->initialize_database();
    m_item_manager->initialize();

    std::string tokens_string = m_secure_storage_provider.get_secure_string(
        std::string(1, m_selected_user_id), "");
    if (!tokens_string.empty())
    {
        auto tokens_json = nlohmann::json::parse(tokens_string, nullptr, false);
        if (!tokens_json.is_discarded())
        {
            selected_user()->access_token = tokens_json["access_token"];
            selected_user()->refresh_token = tokens_json["refresh_token"];
        }
        m_item_manager->sync();
    }
    else
    {
        if (selected_user()->email.empty())
        {
            m_notifier.notify(LOGIN_REQUIRED);
        }
        else
        {
            m_notifier.notify(RE_LOGIN_REQUIRED);
        }

    }
}
