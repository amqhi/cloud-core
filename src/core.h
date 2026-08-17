#ifndef CLOUD_CORE_H
#define CLOUD_CORE_H
#include <map>
#include <vector>
#include <string>
#include <functional>

#include "user.h"
#include "cached_state.h"
#include "database_provider.h"
#include "item_manager.h"
#include "settings.h"
#include "secure_storage_provider.h"

class IPlatformUtils;
class INotifier;

class Core
{
public:
  explicit Core(std::string app_support_path, INetworkProvider& network_provider, ISecureStorageProvider& secure_storage_provider, INotifier& notifier, IPlatformUtils& platform_utils);
  void initialize();
    [[nodiscard]] bool token_refresh_required() const;

  void destroy() const;

    [[nodiscard]] ItemManager& item_manager() const { return *m_item_manager; }
    [[nodiscard]] Settings& settings() const { return *m_settings; }
    [[nodiscard]] DatabaseProvider& database_provider() const { return *m_database_provider; }
    [[nodiscard]] INetworkProvider& network_provider() const { return m_network_provider; }
    [[nodiscard]] INotifier& notifier() const { return m_notifier; }
    [[nodiscard]] IPlatformUtils& platform_utils() const { return m_platform_utils; }
  [[nodiscard]] CachedState& cached_state() const { return *m_cached_state; }

    [[nodiscard]] User* selected_user();
    [[nodiscard]] const std::vector<User>& users() {return m_users;}

  [[nodiscard]] const std::string& app_support_path() const { return m_app_support_path; }

  void add_user();
  void switch_user(const std::string& user_local_id);
    void exchange_google_token(const std::string& id_token);
    void refresh_tokens(const std::function<void()>& on_complete);

  /** On first run */
  void handle_login(const std::string& instance_url, const std::string& email, const std::string& password);
    void handle_login(const std::string& email, const std::string& password);
    void fetch_user_info();

  /** On first run */
  void handle_register(const std::string& instance_url, const std::string& email, const std::string& password, const std::string& name ) const;
    void handle_register(const std::string& email, const std::string& password, const std::string& name ) const;
    void handle_logout();
    void open_folder(const std::string& parent_id) const;
private:
    std::string m_app_support_path;
    INetworkProvider& m_network_provider;
    ISecureStorageProvider& m_secure_storage_provider;
    INotifier& m_notifier;
    IPlatformUtils& m_platform_utils;
    std::unique_ptr<ItemManager> m_item_manager;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<CachedState> m_cached_state;
    std::unique_ptr<DatabaseProvider> m_database_provider;
    std::vector<User> m_users;

    /** Local ID matching the randomly generated directory name, not the actual user ID. */
    std::string m_selected_user_id;
};

#endif // CLOUD_CORE_H
