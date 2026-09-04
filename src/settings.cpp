// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "settings.h"

#include <fstream>
#include <sstream>

#include "core.h"
#include "json_utils.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

Settings::Settings(Core& core) : m_core(core)
{
}

// void Settings::set_instance_url(const std::string &new_url) {
//     m_data.instance_url = new_url;
// }

void Settings::get_data()
{
    fs::path file_path = fs::path(m_core.app_support_path()) / std::string_view(& m_core.selected_user()->local_id, 1) / "settings";

    if (!std::filesystem::exists(file_path)) {
        m_data.instance_url = settings_defaults::BACKEND_URL;
        save();
        return;
    }

    std::ifstream file(file_path);
    if (!file.is_open())
    {
        m_data.instance_url = settings_defaults::BACKEND_URL;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto data = json::parse(content, nullptr, false);

    if (!data.is_discarded())
    {
        m_data.instance_url = json_utils::get_string(data, "instance_url", settings_defaults::BACKEND_URL);
    }

}

void Settings::save() const
{
    auto user = m_core.selected_user();
    if (!user) return;

    fs::path file_path = fs::path(m_core.app_support_path()) / std::string_view(&user->local_id, 1) / "settings";

    std::error_code ec;
    fs::create_directories(file_path.parent_path(), ec);
    if (ec) return;

    std::ofstream file(file_path);
    if (!file.is_open()) return;

    json data;
    data["instance_url"] = m_data.instance_url;
    file << data.dump(4) << std::endl;
}
