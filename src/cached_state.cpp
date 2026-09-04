// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "cached_state.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include "json.hpp"

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

    if (auto it = data.find("selected_user_id"); it != data.end() && it->is_number_integer())
    {
         m_state.selected_session = it->get<char>();
    }
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
