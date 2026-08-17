#ifndef CLOUD_CORE_SETTINGS_H
#define CLOUD_CORE_SETTINGS_H
#include <string>

class Core;

#define SETTINGS_SETTER(type, var_name) void set_##var_name(type var_name) { \
    m_data.var_name = var_name; \
}

namespace settings_defaults
{
    constexpr const char* const BACKEND_URL = "";
    // constexpr const char* const BACKEND_URL = "https://api.amqhi.com";
}

struct SettingsData
{
    std::string instance_url;
};

class Settings {

    public:
        explicit Settings(Core& core);
        [[nodiscard]] const SettingsData& data() const
        {
            return m_data;
        }

        SETTINGS_SETTER(const std::string&, instance_url)
        // void set_instance_url(const std::string& new_url);
        void get_data();
        void save() const;

    private:
        Core& m_core;
        SettingsData m_data;
};



#endif //CLOUD_CORE_SETTINGS_H
