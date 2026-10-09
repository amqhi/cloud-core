// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_CACHE_H
#define CLOUD_CORE_CACHE_H

#include <map>
#include <string>

#include "item.h"

namespace sort_option
{
    /// A to Z
    constexpr char NAME_ASC = 'n';
    /// Z to A
    constexpr char NAME_DESC = 'N';

    /// Oldest first
    constexpr char CREATED_AT_ASC = 'c';
    /// Newest first
    constexpr char CREATED_AT_DESC = 'C';

    /// Oldest modified first
    constexpr char UPDATED_AT_ASC = 'u';
    /// Recently modified first
    constexpr char UPDATED_AT_DESC = 'U';

    /// Smallest first
    constexpr char SIZE_ASC = 's';
    /// Largest first
    constexpr char SIZE_DESC = 'S';

    /// Type A to Z
    constexpr char TYPE_ASC = 't';
    /// Type Z to A
    constexpr char TYPE_DESC = 'T';

    constexpr char DEFAULT_OPTION = 'U';
}

struct AppState {
    std::unordered_map<UUID, char> sort_options;
    std::unordered_map<UUID, char> view_modes;
    char selected_session;
};

class CachedState {
public:
    explicit CachedState(const std::string& app_support_path);
    [[nodiscard]] const AppState& get() const { return m_state; }

    void set_sort_option(const UUID& folder_id, char option) {
        m_state.sort_options[folder_id] = option;
    }

    void set_view_mode(const UUID& folder_id, char mode)
    {
        m_state.view_modes[folder_id] = mode;
    }

    void set_selected_user_id(char user_id) {
        m_state.selected_session = user_id;
    }

    void save() const;

private:
    const std::string& m_app_support_path;
    AppState m_state;
};

#endif //CLOUD_CORE_CACHE_H
