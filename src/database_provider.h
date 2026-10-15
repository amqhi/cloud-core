// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CLOUD_CORE_DATABASE_H
#define CLOUD_CORE_DATABASE_H

#include "database_provider_base.h"

class Core;

class DatabaseProvider : public DatabaseProviderBase
{
    public:
        explicit DatabaseProvider(Core& core);

private:
    std::filesystem::path database_name() override;
};

#endif //CLOUD_CORE_DATABASE_H