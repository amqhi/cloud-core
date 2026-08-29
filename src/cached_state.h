#ifndef CLOUD_CORE_CACHE_H
#define CLOUD_CORE_CACHE_H

#include <map>
#include <string>

#include "item.h"

namespace sort_option
{
    /// A to Z
    constexpr int8_t NAME_ASC = 0;
    /// Z to A
    constexpr int8_t NAME_DESC = 1;

    /// Oldest first
    constexpr int8_t CREATED_AT_ASC = 2;
    /// Newest first
    constexpr int8_t CREATED_AT_DESC = 3;

    /// Oldest modified first
    constexpr int8_t UPDATED_AT_ASC = 4;
    /// Recently modified first
    constexpr int8_t UPDATED_AT_DESC = 5;

    /// Smallest first
    constexpr int8_t SIZE_ASC = 6;
    /// Largest first
    constexpr int8_t SIZE_DESC = 7;

    /// Type A to Z
    constexpr int8_t TYPE_ASC = 8;
    /// Type Z to A
    constexpr int8_t TYPE_DESC = 9;
}

struct AppState {
    std::unordered_map<ItemId, int8_t> sort_options;
    std::unordered_map<ItemId, int8_t> view_modes;
    char selected_session;
};

class CachedState {
public:
    explicit CachedState(const std::string& app_support_path);
    [[nodiscard]] const AppState& get() const { return m_state; }

    void set_sort_option(const ItemId& folder_id, int8_t option) {
        m_state.sort_options[folder_id] = option;
    }

    void set_view_mode(const ItemId& folder_id, int8_t mode)
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
