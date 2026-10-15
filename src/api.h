// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_API_H
#define CLOUD_CORE_API_H

#include <string>
#include <functional>

#include "file_metadata.h"

class CoreBase;

using OnResponse = std::function<void(int status_code, const std::string& response)>;
using OnFailure = std::function<void(std::int16_t error_code, const std::string& data)>;

namespace api
{
    namespace sync
    {

    }

    namespace items
    {

    }

    namespace files
    {

    }
}

#endif //CLOUD_CORE_API_H
