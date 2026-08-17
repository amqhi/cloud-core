#include "api.h"

#include "core.h"
#include <map>
#include "json.hpp"
#include "json_utils.h"
#include "network_error.h"
#include "network_provider.h"
#include "common_api.h"
#include "notifier.h"
#include "request_failure.h"
#include "sync_event.h"

void api::files::get_file_metadata(Core& core, const std::string& id, const OnResponse& on_response,
                                   const OnFailure& on_failure)
{
    std::string url = core.settings().data().instance_url + "/files/" + id;
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + core.selected_user()->access_token;
    core.network_provider().get(url,
                                headers,
                                on_response,
                                on_failure);
}

void api::files::get_file_metadata(Core& core, const std::string& id, const OnResponse& on_response)
{
    get_file_metadata(core, id, on_response, [&core](std::int16_t error_code, const std::string& data)
    {
        handle_network_error(core.notifier(), error_code, data);
    });
}

void api::files::get_file_metadata(Core& core, const std::string& id,
                                   const std::function<void(FileMetadata& file_metadata)>& on_success)
{
    get_file_metadata(core, id, [on_success](int status_code, const std::string& response)
    {
        if (status_code == 200)
        {
            auto body = nlohmann::json::parse(response, nullptr, false);
            if (!body.is_discarded())
            {
                FileMetadata file_metadata;
                file_metadata.id = json_utils::get_string(body, "id");
                file_metadata.checksum = json_utils::get_string(body, "checksum");
                file_metadata.size = json_utils::get_int64_t(body, "size");
                file_metadata.mime_type = json_utils::get_string(body, "mime_type");
                on_success(file_metadata);
            }
        }
    });
}

void api::files::get_download_url(Core& core, const std::string& id,
                                  const std::function<void(int status_code, const std::string& response)>& on_response,
                                  const std::function<void(std::int16_t error_code, const std::string& data)>& on_failure)
{
    std::string url = core.settings().data().instance_url + "/files/" + id + "/download-url";
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + core.selected_user()->access_token;
    core.network_provider().get(url,
                                headers,
                                on_response,
                                on_failure);
}

void api::items::get_item(Core& core, const std::string& id, const OnResponse& on_response, const OnFailure& on_failure)
{
    get_item(id, core.settings().data().instance_url, core.selected_user()->access_token,core.network_provider(), on_response, on_failure);
}

void api::items::get_thumbnail_download_url(Core& core, const std::string& id, const OnResponse& on_response,
                                            const OnFailure& on_failure)
{
    std::string url = core.settings().data().instance_url + "/items/" + id + "/thumbnail";
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + core.selected_user()->access_token;
    core.network_provider().get(url,
                                headers,
                                on_response, on_failure);
}

void api::items::get_thumbnail_download_url(Core& core, const std::string& id, const OnResponse& on_response)
{
}

void api::items::get_thumbnail_download_url(Core& core, const std::string& id,
                                            const std::function<void(std::string& url)>& on_success)
{
}

void api::sync::get_sync_events(Core& core, const OnResponse& on_response, const OnFailure& on_failure)
{
    get_sync_events(core.settings().data().instance_url, core.selected_user()->access_token, core.network_provider(), on_response, on_failure);
}

void api::sync::get_sync_events(Core& core, const std::function<void(const std::vector<SyncEvent>& sync_events)>& on_success)
{
    get_sync_events(core, [&core, on_success](int status_code, const std::string& response)
    {
        if (status_code == 200)
        {
            nlohmann::json body = nlohmann::json::parse(response, nullptr, false);
            if (!body.is_discarded() && body.is_array())
            {
                std::vector<SyncEvent> events;
                for (auto& item : body)
                {
                    events.push_back(sync_event_from_json(item));
                }
                on_success(events);
            }
            else
            {
                notify_request_failure(core.notifier(), SYNC_EVENTS_FETCH_FAILURE, status_code, response);
            }
        }
        else
        {
            notify_request_failure(core.notifier(), SYNC_EVENTS_FETCH_FAILURE, status_code, response);
        }
    }, [&core](std::int16_t error_code, const std::string& data)
                    {
        handle_network_error(core.notifier(), error_code, data);
                    });
}

void api::sync::consume_event(Core& core, const std::string& event_id, const OnResponse& on_response,
    const OnFailure& on_failure)
{
    consume_event(event_id, core.settings().data().instance_url, core.selected_user()->access_token, core.network_provider(), on_response, on_failure);
}
