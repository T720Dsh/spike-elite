// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"

ASpikeEliteGameMode::ASpikeEliteGameMode()
{
	DefaultPawnClass = ASpikeEliteCharacter::StaticClass();
}
