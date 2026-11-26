// SPDX-License-Identifier: MIT
#include "SpikeEliteGameModeHelpers.h"

namespace SpikeEliteGameModeHelpers
{
	/** Map the current touch count to its phase label (Receive -> Set -> Attack). */
	EBallTouchType ResolveTouchType(int32 TouchCount)
	{
		switch (TouchCount)
		{
		case 0:  return EBallTouchType::Receive;
		case 1:  return EBallTouchType::Set;
		case 2:  return EBallTouchType::Attack;
		default: return EBallTouchType::Unknown;
		}
	}

	const TCHAR* TeamStr(EVolleyballTeam Team)
	{
		if (Team == EVolleyballTeam::TeamA) return TEXT("A");
		if (Team == EVolleyballTeam::TeamB) return TEXT("B");
		return TEXT("-");
	}
}
