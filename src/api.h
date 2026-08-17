#ifndef CLOUD_CORE_API_H
#define CLOUD_CORE_API_H

#include <string>
#include <functional>

#include "file_metadata.h"

struct SyncEvent;
class Core;

using OnResponse = std::function<void(int status_code, const std::string& response)>;
using OnFailure = std::function<void(std::int16_t error_code, const std::string& data)>;

namespace api
{
    namespace sync
    {
        void get_sync_events(
            Core& core,
            const OnResponse& on_response,
            const OnFailure& on_failure);
        void get_sync_events(
            Core& core,
            const std::function<void(const std::vector<SyncEvent>& sync_events)>& on_success);
        void consume_event(
            Core& core,
            const std::string& event_id,
            const OnResponse& on_response,
            const OnFailure& on_failure);
    }

    namespace items
    {
        void get_item(
            Core& core,
            const std::string& id,
            const OnResponse& on_response,
            const OnFailure& on_failure);
        void get_thumbnail_download_url(
            Core& core,
            const std::string& id,
            const OnResponse& on_response,
            const OnFailure& on_failure);

        void get_thumbnail_download_url(
            Core& core,
            const std::string& id,
            const OnResponse& on_response);

        void get_thumbnail_download_url(
            Core& core,
            const std::string& id,
            const std::function<void(std::string& url)>& on_success);
    }

    namespace files
    {
        void get_file_metadata(
            Core& core,
            const std::string& id,
            const OnResponse& on_response,
            const OnFailure& on_failure
        );
        void get_file_metadata(
            Core& core,
            const std::string& id,
            const OnResponse& on_response
        );
        void get_file_metadata(
            Core& core,
            const std::string& id,
            const std::function<void(FileMetadata& file_metadata)>& on_success
        );
        void get_download_url(
            Core& core,
            const std::string& id,
            const std::function<void(int status_code, const std::string& response)>& on_response,
            const std::function<void(std::int16_t error_code, const std::string& data)>& on_failure
        );
    }
}

#endif //CLOUD_CORE_API_H
