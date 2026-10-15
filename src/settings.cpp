// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "settings.h"

#include "core.h"
#include "json_utils.h"

Settings::Settings(Core& core) : SettingsBase(core)
{
}

void Settings::initialize(const nlohmann::json& json)
{
}

void Settings::bind_data(nlohmann::json& json)
{
}
