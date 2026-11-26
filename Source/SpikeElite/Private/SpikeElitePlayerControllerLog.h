// SPDX-License-Identifier: MIT
#pragma once

#include "Logging/LogMacros.h"

// One shared category preserves filtering across controller translation units.
DECLARE_LOG_CATEGORY_EXTERN(LogSEMenu, Log, All);
