// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "cached_state.h"
#include "json.hpp"
#include "json_utils.h"

CachedState::CachedState(const std::string& app_support_path) : CachedStateBase(app_support_path)
{

}

void CachedState::bind_data(nlohmann::json& data)
{

}

void CachedState::initialize(const nlohmann::json& data)
{

}
