// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_CACHE_H
#define CLOUD_CORE_CACHE_H

#include <string>

#include "cached_state_base.h"
#include "item.h"

class CachedState : public CachedStateBase {
public:
    explicit CachedState(const std::string& app_support_path);

private:
    void bind_data(nlohmann::json& data) override;
    void initialize(const nlohmann::json& data) override;
};

#endif //CLOUD_CORE_CACHE_H
