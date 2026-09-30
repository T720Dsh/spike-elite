// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "VolleyballEnums.generated.h"

/** Which side of the net a team defends. */
UENUM(BlueprintType)
enum class EVolleyballTeam : uint8
{
	TeamA       UMETA(DisplayName = "Team A (near / +X side)"),
	TeamB       UMETA(DisplayName = "Team B (far / -X side)"),
	None        UMETA(Hidden)
};

/** Match / rally state machine (M10: explicit serve and toss phases). */
UENUM(BlueprintType)
enum class EMatchState : uint8
{
	PreMatch        UMETA(DisplayName = "Pre-match"),
	BetweenRallies  UMETA(DisplayName = "Between rallies"),
	AwaitingServe   UMETA(DisplayName = "Awaiting serve"),
	ServingToss     UMETA(DisplayName = "Serving toss"),
	Rally           UMETA(DisplayName = "Rally in progress"),
	SetOver         UMETA(DisplayName = "Set finished"),
	MatchOver       UMETA(DisplayName = "Match finished")
};
