// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "VolleyballEnums.h"
#include "VolleyballRules.generated.h"

/**
 * Pure-logic volleyball rules core (SPIKE ELITE M10).
 *
 * Everything in SEVolleyballRules is a plain function over plain state:
 * no UObject, no AActor, no World, no ticking. This is what the Unreal
 * Automation Tests exercise, and what ASpikeEliteGameMode drives each rally.
 *
 * Rules modelled (simplified 2025-2028 FIVB subset):
 *  - Rally point scoring; side-out (rally winner serves next).
 *  - A team may touch the ball at most 3 times; a 4th touch is a fault
 *    (opponent scores). No block system yet — blocking does NOT reset the
 *    count in this milestone (documented as a future item).
 *  - A player may not touch the ball twice in a row; doing so is a fault.
 *  - After the ball legally crosses the net, possession switches and the
 *    new side's touch counter restarts at 0.
 *  - In/out: lines count IN; OUT is awarded to the opponent of the last
 *    touching team.
 *  - Sets to 25 (win by 2); deciding set to 15 (win by 2); best of 5.
 */

/** How a touch attempt was resolved. */
UENUM(BlueprintType)
enum class ETouchResult : uint8
{
	Allowed          UMETA(DisplayName = "Allowed"),
	RallySettled     UMETA(DisplayName = "Rally already settled"),
	WrongPhase       UMETA(DisplayName = "Not a legal phase to touch"),
	WrongTeam        UMETA(DisplayName = "Not the possessing team"),
	FourTouchesFault UMETA(DisplayName = "Fourth touch by same team"),
	DoubleTouchFault UMETA(DisplayName = "Same player twice in a row"),
	NotInRange       UMETA(DisplayName = "Ball out of reach")
};

/** Why a rally ended (drives UI messages and logs). */
UENUM(BlueprintType)
enum class ERallyEndReason : uint8
{
	BallIn        UMETA(DisplayName = "Ball landed IN"),
	BallOut       UMETA(DisplayName = "Ball landed OUT"),
	FourTouches   UMETA(DisplayName = "Four touches by one team"),
	DoubleTouch   UMETA(DisplayName = "Same player touched twice"),
	ServeFault    UMETA(DisplayName = "Serve fault"),
	Cancelled     UMETA(DisplayName = "Rally cancelled (no point)")
};

/** AI role assigned to a character by the GameMode coordinator. */
UENUM(BlueprintType)
enum class EAIBehavior : uint8
{
	ReturnHome      UMETA(DisplayName = "Return to home position"),
	MoveToReceive   UMETA(DisplayName = "Move to predicted landing (receive)"),
	Set             UMETA(DisplayName = "Move to set position"),
	Attack          UMETA(DisplayName = "Move to attack point"),
	Wait            UMETA(DisplayName = "Hold defensive position")
};

/** What kind of touch is happening (drives hit direction + UI text). */
UENUM(BlueprintType)
enum class EBallTouchType : uint8
{
	Serve   UMETA(DisplayName = "Serve"),
	Receive UMETA(DisplayName = "Receive (first touch)"),
	Set     UMETA(DisplayName = "Set (second touch)"),
	Attack  UMETA(DisplayName = "Attack (third touch)"),
	Unknown UMETA(DisplayName = "Unknown")
};

/**
 * Mutable state of a single rally. GameMode owns one; tests construct their own.
 */
USTRUCT(BlueprintType)
struct FVolleyballRallyState
{
	GENERATED_BODY()

	/** Team currently in possession of the ball (has the right to touch). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EVolleyballTeam PossessingTeam = EVolleyballTeam::None;

	/** Touches used by the current possessing team (0..3). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TouchCount = 0;

	/** Team of the last touch (for OUT scoring). None before any touch. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EVolleyballTeam LastTouchTeam = EVolleyballTeam::None;

	/** Roster index of the last toucher within its team; -1 = none. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LastTouchPlayerIndex = -1;

	/** True once the rally has been settled; blocks re-scoring / further touches. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bRallySettled = false;

	/** True while a rally is live (ball in play). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bBallInPlay = false;
};

namespace SEVolleyballRules
{
	/** Standard win target for a set (25 for sets 1-4, 15 for the 5th). */
	inline int32 PointsToWinForSet(int32 SetNumber)
	{
		return (SetNumber >= 5) ? 15 : 25;
	}

	/** Win-by-2 check (FIVB §6.1/§6.2). */
	inline bool IsSetWon(int32 TeamAScore, int32 TeamBScore, int32 PointsToWin)
	{
		return (TeamAScore >= PointsToWin && (TeamAScore - TeamBScore) >= 2)
			|| (TeamBScore >= PointsToWin && (TeamBScore - TeamAScore) >= 2);
	}

	/** Match won when a team reaches MatchWinsNeeded sets (3 in a normal best-of-5). */
	inline bool IsMatchWon(int32 TeamASetsWon, int32 TeamBSetsWon, int32 MatchWinsNeeded)
	{
		return TeamASetsWon >= MatchWinsNeeded || TeamBSetsWon >= MatchWinsNeeded;
	}

	/** FIVB court: |X| <= HalfLength (end lines) and |Y| <= HalfWidth (side lines); lines count IN. */
	inline bool IsInBounds(const FVector& Location, float HalfLength, float HalfWidth)
	{
		return FMath::Abs(Location.X) <= HalfLength && FMath::Abs(Location.Y) <= HalfWidth;
	}

	/**
	 * Who scores when the ball lands.
	 * IN: the side defending that half loses -> opponent scores (X>0 is Team A's half).
	 * OUT: opponent of the last touching team scores. With no touch, fall back to half.
	 */
	SPIKEELITE_API EVolleyballTeam DetermineScoringTeamOnLand(bool bInBounds, EVolleyballTeam LastTouchTeam, bool bLandedOnPositiveX);

	/**
	 * Evaluate a touch attempt and mutate the rally state if allowed.
	 * Team==None or PlayerIndex<0 is rejected. Faults (4th touch / double touch)
	 * are reported but do NOT mutate state — the caller settles the rally.
	 */
	SPIKEELITE_API ETouchResult EvaluateTouch(FVolleyballRallyState& State, EVolleyballTeam Team, int32 PlayerIndex);

	/** The ball legally crossed the net into NewPossessor's half: switch possession, reset counter. */
	SPIKEELITE_API void OnBallCrossedNet(FVolleyballRallyState& State, EVolleyballTeam NewPossessor);

	/** Begin a fresh rally; the serving team starts in possession with 0 touches. */
	SPIKEELITE_API void BeginRally(FVolleyballRallyState& State, EVolleyballTeam ServingTeam);

	/** Settle the rally; returns true only on the first call (single settlement). */
	SPIKEELITE_API bool SettleRally(FVolleyballRallyState& State);

	/** Phase gate shared by GameMode::CanTouchBall and the tests: only a live,
	 *  unsettled Rally allows touches (never BetweenRallies / AwaitingServe /
	 *  ServingToss / SetOver / MatchOver / PreMatch). */
	inline bool IsTouchLegalInPhase(EMatchState State, bool bRallySettled)
	{
		return State == EMatchState::Rally && !bRallySettled;
	}

	/** Mark the rally as live. Called by GameMode when the serve is actually hit
	 *  out (ExecuteServe) and the state enters Rally. Tests assert the lifecycle
	 *  BeginRally(false) -> StartPlay(true) -> SettleRally(false). */
	inline void StartPlay(FVolleyballRallyState& State)
	{
		State.bBallInPlay = true;
	}

	/** Serve-fault classification (M11): a serve that never legally crossed the
	 *  net counts as a serve fault whether it lands in or out, so both landings
	 *  are reported as 发球失误 instead of a generic IN/OUT. */
	inline bool IsServeFault(const FVolleyballRallyState& State, bool bServeCrossedNet)
	{
		return State.TouchCount == 1 && !bServeCrossedNet;
	}

	/** Whether the serving player may auto-serve without pressing E (M11).
	 *  Bots always auto-serve; a human serves manually unless the automation
	 *  flag (-devauto) explicitly delegates control. Single source of truth so
	 *  the GameMode and the tests share one decision. */
	inline bool ShouldAutoServe(bool bIsBot, bool bDevAuto)
	{
		return bIsBot || bDevAuto;
	}

	/** FIVB §7.4 side-out rotation: the new serving team rotates clockwise, i.e.
	 *  the position-1 player (index 0) moves to the last slot. Pure array op so
	 *  the GameMode and the tests share one implementation. */
	inline void RotateRoster(TArray<int32>& Order)
	{
		if (Order.Num() == 0) { return; }
		const int32 First = Order[0];
		for (int32 i = 0; i + 1 < Order.Num(); i++) { Order[i] = Order[i + 1]; }
		Order[Order.Num() - 1] = First;
	}

	/** Human-readable touch count label, e.g. "A 2/3". */
	SPIKEELITE_API FString TouchLabel(const FVolleyballRallyState& State);

	/** Describe a reason in Chinese for the rally-result banner. */
	SPIKEELITE_API FString RallyReasonLabel(ERallyEndReason Reason);
}
