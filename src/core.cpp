#include "core.h"
#include <filesystem>
#include <utility>

#include "api.h"
#include "database_provider.h"
#include "date_time_utils.h"
#include "json_utils.h"
#include "network_error.h"
#include "network_provider.h"
#include "auth_tokens.h"
#include "name_generator.h"
#include "notifier.h"
#include "request_failure.h"

namespace fs = std::filesystem;

Core::Core(std::string app_support_path, INetworkProvider& network_provider, ISecureStorageProvider& secure_storage_provider,
           INotifier& notifier, IPlatformUtils& platform_utils)
    : m_app_support_path(std::move(app_support_path)), m_network_provider(network_provider),
      m_secure_storage_provider(secure_storage_provider), m_notifier(notifier), m_platform_utils(platform_utils)
{
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
            User user;
            user.local_id = entry.path().filename().string();
            get_user_data(m_app_support_path, user);
            m_users.push_back(user);
            if (m_cached_state->get().selected_session == user.local_id)
            {
                m_selected_user_id = user.local_id;
            }
        }
    }

    if (selected_user() == nullptr)
    {
        if (m_users.empty())
        {
            User user;
            user.local_id = name_generator::generated_directory_name(app_support_path);
            m_users.push_back(user);
            m_selected_user_id = user.local_id;
            m_cached_state->set_selected_user_id(m_selected_user_id);
        }
        else
        {
            m_cached_state->set_selected_user_id(m_users[0].local_id);
            m_selected_user_id = m_users[0].local_id;
            m_cached_state->set_selected_user_id(m_selected_user_id);
        }
    }

    m_settings = std::make_unique<Settings>(*this);
    m_database_provider = std::make_unique<DatabaseProvider>(*this);

    m_item_manager = std::make_unique<ItemManager>(*this);
}

void Core::initialize()
{
    m_settings->get_data();
    m_cached_state->save();
    m_database_provider->initialize_database();
    m_item_manager->fetch_items_from_cache(special_folder::HOME);

    std::string tokens_string = m_secure_storage_provider.get_secure_string(m_selected_user_id, "");
    if (!tokens_string.empty())
    {
        auto tokens_json = nlohmann::json::parse(tokens_string, nullptr, false);
        if (!tokens_json.is_discarded())
        {
            selected_user()->access_token = tokens_json["access_token"];
            selected_user()->refresh_token = tokens_json["refresh_token"];
        }
        refresh_tokens([this]()
        {
            m_item_manager->sync();
        });
    }
    else
    {
        m_notifier.notify(LOGIN_REQUIRED);
    }
}

bool Core::token_refresh_required() const
{
    // TODO: impl
    //return selected_user()->expires_at
    return true;
}

void Core::destroy() const
{
    m_database_provider->close();
}

User* Core::selected_user()
{
    for (auto& u : m_users)
    {
        if (u.local_id == m_selected_user_id) return &u;
    }
    return nullptr;
}

void Core::add_user()
{
    User user;
    user.local_id = name_generator::generated_directory_name(m_app_support_path);
    m_users.push_back(user);
    m_selected_user_id = user.local_id;
    m_database_provider->close();
    initialize();
}

void Core::switch_user(const std::string& user_local_id)
{
    m_selected_user_id = user_local_id;
    m_database_provider->close();
    initialize();
    m_notifier.notify(USER_SWITCHED, user_local_id);
}

void Core::exchange_google_token(const std::string& id_token)
{
    std::string url = m_settings->data().instance_url + "/auth/google";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    body["id_token"] = id_token;
    m_network_provider.post_json(
        url,
        headers,
        body,
        [this](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                auto json = nlohmann::json::parse(response, nullptr, false);
                if (json.is_discarded())
                {
                    selected_user()->access_token = "";
                    selected_user()->refresh_token = "";
                    m_notifier.notify(RE_LOGIN_REQUIRED);
                }
                else
                {
                    AuthTokens auth_tokens = AuthTokens::from_json(json);
                    m_secure_storage_provider.set_secure_string(m_selected_user_id, auth_tokens.serialize());
                    selected_user()->access_token = auth_tokens.access_token;
                    selected_user()->refresh_token = auth_tokens.refresh_token;
                }
            }
            else
            {
            }
        }
        , [this](std::int16_t error_code, const std::string& data)
        {
            handle_network_error(m_notifier, error_code, data);
        });
}

void Core::refresh_tokens(const std::function<void()>& on_complete)
{
    std::string url = m_settings->data().instance_url + "/auth/refresh";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    body["refresh_token"] = selected_user()->refresh_token;
    m_network_provider.post_json(
        url,
        headers,
        body,
        [this, on_complete](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                auto json = nlohmann::json::parse(response, nullptr, false);
                if (json.is_discarded())
                {
                    selected_user()->access_token = "";
                    selected_user()->refresh_token = "";
                    m_notifier.notify(RE_LOGIN_REQUIRED);
                }
                else
                {
                    AuthTokens auth_tokens = AuthTokens::from_json(json);
                    m_secure_storage_provider.set_secure_string(m_selected_user_id, auth_tokens.serialize());
                    selected_user()->access_token = auth_tokens.access_token;
                    selected_user()->refresh_token = auth_tokens.refresh_token;
                }
                on_complete();
            }
            else
            {
                selected_user()->access_token = "";
                selected_user()->refresh_token = "";
                m_secure_storage_provider.remove_secure_string(m_selected_user_id);
                m_notifier.notify(RE_LOGIN_REQUIRED);
                on_complete();
            }
        }
        , [this](std::int16_t error_code, const std::string& data)
        {
            handle_network_error(m_notifier, error_code, data);
        });
}

void Core::handle_login(const std::string& instance_url, const std::string& email, const std::string& password)
{
    m_settings->set_instance_url(instance_url);
    m_settings->save();
    handle_login(email, password);
}

void Core::handle_login(const std::string& email, const std::string& password)
{
    std::string url = m_settings->data().instance_url + "/auth/login";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    m_network_provider.post_json(
        url,
        headers,
        body,
        [this](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                auto json = nlohmann::json::parse(response, nullptr, false);
                if (json.is_discarded())
                {
                    selected_user()->access_token = "";
                    selected_user()->refresh_token = "";
                    notify_request_failure(m_notifier, LOGIN_FAILURE, status_code, response);
                }
                else
                {
                     AuthTokens auth_tokens = AuthTokens::from_json(json);
                     m_secure_storage_provider.set_secure_string(m_selected_user_id, auth_tokens.serialize());
                     selected_user()->access_token = auth_tokens.access_token;
                     selected_user()->refresh_token = auth_tokens.refresh_token;
                    m_item_manager->fetch_items(special_folder::HOME);
                    m_notifier.notify(LOGIN_SUCCESS);
                    fetch_user_info();
                }
            }
            else
            {
                selected_user()->access_token = "";
                selected_user()->refresh_token = "";
                notify_request_failure(m_notifier, LOGIN_FAILURE, status_code, response);
            }
        }
        , [](std::int16_t error_code, const std::string& data)
        {
        });
}

void Core::fetch_user_info()
{
    std::string url = m_settings->data().instance_url + "/users/me";
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + selected_user()->access_token;
    m_network_provider.get(url, headers, [this](int status_code, const std::string& response)
                           {
                               if (status_code == 200)
                               {
                                   const nlohmann::json body = nlohmann::json::parse(response, nullptr, false);
                                   if (body.is_discarded())
                                   {
                                       notify_request_failure(m_notifier, FETCH_USER_INFO_FAILURE, status_code, response);
                                   }
                                   else
                                   {
                                       selected_user()->name = json_utils::get_string(body, "name");
                                       selected_user()->email = json_utils::get_string(body, "email");
                                       std::string login_type_data = json_utils::get_string(body, "login_type");
                                       if (login_type_data == "google")
                                       {
                                           selected_user()->login_type = login_type::GOOGLE;
                                       }
                                       else
                                       {
                                           selected_user()->login_type = login_type::NORMAL;
                                       }
                                       std::string created_at_data = json_utils::get_string(body, "created_at");
                                       std::string updated_at_data = json_utils::get_string(body, "updated_at");
                                       selected_user()->created_at = parse_iso8601_to_ms(created_at_data);
                                       selected_user()->updated_at = parse_iso8601_to_ms(updated_at_data);

                                       // TODO: Download profile picture
                                       // if (auto it = body.find("profile_picture_id"); it != body.end() && it->is_string())
                                       // {
                                       //     std::string profile_picture_id = it->get<std::string>();
                                       //     api::files::get_download_url(*this, profile_picture_id,
                                       //                                  [this](int status_code, const std::string& response)
                                       //                                  {
                                       //                                      if (status_code == 200)
                                       //                                      {
                                       //                                          auto& url = response;
                                       //                                          std::map<std::string, std::string> headers;
                                       //                                          m_network_provider.download_file(url, headers, );
                                       //                                      }
                                       //                                      else
                                       //                                      {
                                       //                                      }
                                       //                                  }, [](std::int16_t error_code, const std::string& data)
                                       //                                  {
                                       //                                  });
                                       // }
                                       save_user_data(m_app_support_path, *selected_user());
                                       notifier().notify(FETCH_USER_INFO_SUCCESS);
                                   }
                               }
                               else
                               {
                                   notify_request_failure(m_notifier, FETCH_USER_INFO_FAILURE, status_code, response);
                               }
                           }, [this](std::int16_t error_code, const std::string& data)
                           {
                               handle_network_error(m_notifier, error_code, data);
                           });
}

void Core::handle_register(const std::string& instance_url, const std::string& email, const std::string& password,
    const std::string& name) const
{
    m_settings->set_instance_url(instance_url);
    m_settings->save();
    handle_register(email, password, name);
}

void Core::handle_register(const std::string& email, const std::string& password, const std::string& name) const
{
    std::string url = m_settings->data().instance_url + "/users";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    body["email"] = email;
    body["password"] = password;
    body["name"] = name;
    m_network_provider.post_json(
        url,
        headers,
        body,
        [this](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                m_notifier.notify(REGISTER_SUCCESS);
            }
            else
            {
                notify_request_failure(m_notifier, REGISTER_FAILURE, status_code, response);
            }
        }
        , [](std::int16_t error_code, const std::string& data)
        {
        });
}

void Core::handle_logout()
{
    std::string url = m_settings->data().instance_url + "/auth/login";
    std::map<std::string, std::string> headers;
    nlohmann::json body;
    m_network_provider.post_json(
        url,
        headers,
        body,
        [this](int status_code, const std::string& response)
        {
            if (status_code == 200)
            {
                m_notifier.notify(LOGOUT_SUCCESS);
                selected_user()->access_token = "";
                selected_user()->refresh_token = "";
                m_secure_storage_provider.remove_secure_string(m_selected_user_id);
            }
            else
            {
                notify_request_failure(m_notifier, LOGIN_FAILURE, status_code, response);
            }
        }
        , [](std::int16_t error_code, const std::string& data)
        {
        });
}

void Core::open_folder(const std::string& parent_id) const
{
    m_item_manager->fetch_items_from_cache(parent_id);
}
