// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Volleyball/VolleyballRules.h"

/** Private GameMode-only presentation helpers; unique namespace avoids Unity collisions. */
namespace SpikeEliteGameModeHelpers
{
	EBallTouchType ResolveTouchType(int32 TouchCount);
	const TCHAR* TeamStr(EVolleyballTeam Team);
}
