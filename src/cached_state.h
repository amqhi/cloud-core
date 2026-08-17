#ifndef CLOUD_CORE_CACHE_H
#define CLOUD_CORE_CACHE_H

#include <map>
#include <string>

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
    std::string selected_session;
    std::map<std::string, int8_t> sort_options;
    std::map<std::string, int8_t> view_modes;
};

class CachedState {
public:
    explicit CachedState(const std::string& app_support_path);
    [[nodiscard]] const AppState& get() const { return m_state; }

    void set_sort_option(const std::string& folder_id, int8_t option) {
        m_state.sort_options[folder_id] = option;
    }

    void set_view_mode(const std::string& folder_id, int8_t mode)
    {
        m_state.view_modes[folder_id] = mode;
    }

    void set_selected_user_id(const std::string& user_id) {
        m_state.selected_session = user_id;
    }

    void save() const;

private:
    const std::string& m_app_support_path;
    AppState m_state;
};

#endif //CLOUD_CORE_CACHE_H
