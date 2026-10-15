// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "cached_state.h"
#include "json.hpp"

CachedState::CachedState(const std::string& app_support_path) : CachedStateBase(app_support_path)
{

}

void CachedState::bind_data(nlohmann::json& data)
{

}

void CachedState::initialize(const nlohmann::json& data)
{
    if (auto it = data.find("sort_options"); it != data.end() && it->is_object())
    {
        for (const auto& [key_str, value] : it->items())
        {
            UUID uuid = UUID::from_string(key_str);
            if (value.is_number_integer())
            {
                m_sort_options[uuid] = static_cast<char>(value.get<int>());
            }
        }
    }

    if (auto it = data.find("view_modes"); it != data.end() && it->is_object())
    {
        for (const auto& [key_str, value] : it->items())
        {
            UUID uuid = UUID::from_string(key_str);
            if (value.is_number_integer())
            {
                m_view_modes[uuid] = static_cast<char>(value.get<int>());
            }
        }
    }
}
