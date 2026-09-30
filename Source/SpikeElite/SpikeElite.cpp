// SPDX-License-Identifier: MIT
#include "SpikeElite.h"
#include "Modules/ModuleManager.h"

// The PRIMARY game module macro is required for monolithic (packaged Game)
// builds: it statically defines GInternalProjectName / GIsGameAgnosticExe and
// the foreign-engine-dir etc. symbols that CoreGlobals.cpp leaves to the game's
// main module. A plain IMPLEMENT_MODULE links fine for the Editor target but
// the packaged SpikeElite.exe fails with LNK2001 on those symbols.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, SpikeElite, "SpikeElite");
