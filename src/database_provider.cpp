// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "database_provider.h"

#include "core.h"

namespace fs = std::filesystem;

DatabaseProvider::DatabaseProvider(Core& core): DatabaseProviderBase(core)
{}

std::filesystem::path DatabaseProvider::database_name()
{
    return "cloud.db";
}
