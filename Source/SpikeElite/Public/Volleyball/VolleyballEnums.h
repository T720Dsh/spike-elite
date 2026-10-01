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

/** FIVB rotation slot (P1 = back-right / serve slot). */
UENUM(BlueprintType)
enum class ERotationSlot : uint8
{
	P1  UMETA(DisplayName = "P1 back-right (serve)"),
	P2  UMETA(DisplayName = "P2 front-right"),
	P3  UMETA(DisplayName = "P3 front-middle"),
	P4  UMETA(DisplayName = "P4 front-left"),
	P5  UMETA(DisplayName = "P5 back-left"),
	P6  UMETA(DisplayName = "P6 back-middle"),
	None UMETA(Hidden)
};

/** Match / rally state machine (M11b: full official pre-serve ceremony). */
UENUM(BlueprintType)
enum class EMatchState : uint8
{
	PreMatch           UMETA(DisplayName = "Pre-match"),
	BetweenRallies     UMETA(DisplayName = "Post-rally result display"),
	ResettingPositions UMETA(DisplayName = "Players returning to formation"),
	AwaitingReady      UMETA(DisplayName = "2nd referee checking readiness"),
	ServiceAuthorized  UMETA(DisplayName = "Whistle blown, service authorized"),
	ServingToss        UMETA(DisplayName = "Serving toss"),
	Rally              UMETA(DisplayName = "Rally in progress"),
	SetOver            UMETA(DisplayName = "Set finished"),
	MatchOver          UMETA(DisplayName = "Match finished")
};

/**
 * M11c-5: the human player's defensive plan, chosen in the defense planning
 * UI just before the opponent's attack crosses the net. The GameMode reads it
 * to steer block count, block lane and back-row defence positioning. Reset to
 * NoPlan after every rally / rematch (AI uses its own defaults when NoPlan).
 */
UENUM(BlueprintType)
enum class EVolleyballDefensePlan : uint8
{
	NoPlan         UMETA(DisplayName = "AI default"),
	SingleBlock    UMETA(DisplayName = "Single block"),
	DoubleBlock    UMETA(DisplayName = "Double block"),
	LineDefense    UMETA(DisplayName = "Block the line"),
	AngleDefense   UMETA(DisplayName = "Block the angle"),
	BackLine       UMETA(DisplayName = "Back-row line defence"),
	BackAngle      UMETA(DisplayName = "Back-row angle defence"),
	RegularDig     UMETA(DisplayName = "Regular move dig"),
	DiveDig        UMETA(DisplayName = "Dive dig"),
	None           UMETA(Hidden)
};

