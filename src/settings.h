// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_SETTINGS_H
#define CLOUD_CORE_SETTINGS_H

#include "json.hpp"

#include "settings_base.h"

class Core;

class Settings : public SettingsBase {

    public:
        explicit Settings(Core& core);
    void initialize(const nlohmann::json& json) override;

private:
    void bind_data(nlohmann::json& json) override;
};



#endif //CLOUD_CORE_SETTINGS_H
