#include "cached_state.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include "json.hpp"
#include "utils/json_utils.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

CachedState::CachedState(const std::string& app_support_path) : m_app_support_path(app_support_path)
{
    fs::path file_path = fs::path(app_support_path) / "state";

    if (!std::filesystem::exists(file_path)) {
        return;
    }

        std::ifstream file(file_path);
        if (!file.is_open()) return;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        auto data = json::parse(content, nullptr, false);
        if (data.is_discarded())
        {
            save();
            return;
        }
        m_state.selected_session = json_utils::get_string(data, "selected_user_id");


}

void CachedState::save() const
{
    fs::path file_path = fs::path(m_app_support_path) / "state";

    if (!fs::exists(file_path.parent_path())) {
        fs::create_directories(file_path.parent_path());
    }

    std::ofstream file(file_path);

    json data;

    data["selected_user_id"] = m_state.selected_session;

    if (file.is_open() && file.good()) {
        file << data.dump(4) << std::endl;
    }
    file.close();
}
