// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "VolleyballEnums.h"
#include "VolleyballRules.generated.h"

// Shared log category: defined in VolleyballRules.cpp, usable by the GameMode,
// characters and tests (replaces a file-static category).
DECLARE_LOG_CATEGORY_EXTERN(LogVolleyballRules, Log, All);

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
	MoveToBlock     UMETA(DisplayName = "Move to front-row block point"),
	Dive            UMETA(DisplayName = "Dive to save a low ball beyond normal reach"),
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
	Block   UMETA(DisplayName = "Block (not counted, front-row only)"),
	Unknown UMETA(DisplayName = "Unknown")
};

/**
 * One player's authoritative rotation view (built by the GameMode from the
 * roster order; consumed by the rotation HUD and tests).
 */
USTRUCT(BlueprintType)
struct FRotationSlotView
{
	GENERATED_BODY()

	/** 0..5 = P1..P6. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 SlotIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PlayerId = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString Jersey;

	/** True when this player is the current server (P1 of the serving team). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bServer = false;

	/** True when this is the local human-controlled player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bControlled = false;

	/** P2/P3/P4 are front-row. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bFrontRow = false;
};

/** Snapshot of the authoritative rotation state pushed to the rotation HUD. */
USTRUCT(BlueprintType)
struct FRotationViewState
{
	GENERATED_BODY()

	/** Current rotation count 1..6 (how many side-out rotations occurred). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RotationIndex = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EVolleyballTeam ServingTeam = EVolleyballTeam::TeamA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FRotationSlotView> TeamA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FRotationSlotView> TeamB;
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

	/** Type of the last touch (Block disables the double-touch check for the
	 *  blocker, who may legally touch again right after a block). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EBallTouchType LastTouchType = EBallTouchType::Unknown;

	/** True once the rally has been settled; blocks re-scoring / further touches. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bRallySettled = false;

	/** True while a rally is live (ball in play). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bBallInPlay = false;

	/** True once the serve has legally crossed the net (M11c). While a serve is
	 *  in flight (LastTouchType == Serve && !bServeCrossedNet) NOBODY may touch
	 *  the ball — not even the serving team. This is the single source of truth
	 *  shared by GameMode, AI directives and the tests. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bServeCrossedNet = false;
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
	SPIKEELITE_API ETouchResult EvaluateTouch(FVolleyballRallyState& State, EVolleyballTeam Team, int32 PlayerIndex,
		EBallTouchType Type = EBallTouchType::Attack);

	/**
	 * Record the serve touch WITHOUT consuming one of the serving team's three
	 * touches and WITHOUT granting possession. LastTouch* is recorded so a serve
	 * that lands out is awarded to the opponent; TouchCount stays 0 and
	 * PossessingTeam stays None until the ball legally crosses the net.
	 */
	SPIKEELITE_API void RecordServeTouch(FVolleyballRallyState& State, EVolleyballTeam Team, int32 PlayerIndex);

	/** The ball legally crossed the net into NewPossessor's half: switch possession, reset counter. */
	SPIKEELITE_API void OnBallCrossedNet(FVolleyballRallyState& State, EVolleyballTeam NewPossessor);

	/** Begin a fresh rally; nobody possesses until the serve crosses the net. */
	SPIKEELITE_API void BeginRally(FVolleyballRallyState& State, EVolleyballTeam ServingTeam);

	/** Settle the rally; returns true only on the first call (single settlement). */
	SPIKEELITE_API bool SettleRally(FVolleyballRallyState& State);

	/** Phase gate shared by GameMode::CanTouchBall and the tests: only a live,
	 *  unsettled Rally allows touches (never BetweenRallies / ServiceAuthorized /
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

	/** Serve-fault classification (M11c): a serve that never legally crossed the
	 *  net counts as a serve fault whether it lands in or out, so both landings
	 *  are reported as 发球失误 instead of a generic IN/OUT. Uses the serve
	 *  record itself (not TouchCount — the serve never consumes a touch). */
	inline bool IsServeFault(const FVolleyballRallyState& State, bool bServeCrossedNet)
	{
		return State.LastTouchType == EBallTouchType::Serve && !bServeCrossedNet;
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

	/** M11c-2: authoritative front-row test by rotation slot. Roster index is the
	 *  slot-1 (0=P1, 1=P2, 2=P3, 3=P4, 4=P5, 5=P6); the front row is P2/P3/P4.
	 *  This replaces any positional guess (|HomePosition.X| < N) — a rotated
	 *  front-row player still holds a front-row SLOT no matter where they stand. */
	inline bool IsFrontRowSlot(int32 RosterIndex)
	{
		return RosterIndex == 1 || RosterIndex == 2 || RosterIndex == 3;
	}

	/** Back row: P1 (serve slot), P5, P6. */
	inline bool IsBackRowSlot(int32 RosterIndex)
	{
		return RosterIndex == 0 || RosterIndex == 4 || RosterIndex == 5;
	}

	/** M11c-2: side-out rotation counter wraps 1..6 (never shows 7/6). The team
	 *  that just gained the serve rotates once; the serving winner does not. */
	inline int32 AdvanceRotationIndex(int32 Current)
	{
		return (Current % 6) + 1;
	}

	/** M11c-2: authoritative Team A slot formation in world cm (pure data, no
	 *  actors). Roster index = slot-1: 0=P1 back-right serve, 1=P2 front-right,
	 *  2=P3 front-middle, 3=P4 front-left, 4=P5 back-left, 5=P6 back-middle.
	 *  Front row |X| < 300 (inside the 3 m line); back row |X| > 300. Team B
	 *  mirrors BOTH axes so its left/right semantics never flip. */
	SPIKEELITE_API TArray<FVector> GetSlotFormationA();

	/** True when the ball really crossed the net plane this step (previous sample
	 *  and new sample on opposite sides). A teleport/reset never looks like a
	 *  crossing because the caller seeds PrevX with the reset X. */
	inline bool DetectNetCross(float PrevX, float NewX, float MinAbs = 5.f)
	{
		return PrevX * NewX < 0.f && FMath::Abs(NewX) > MinAbs;
	}

	/** M11c-3: a dive is never offered during the serve flight — the serve is
	 *  recorded as LastTouch=Serve with TouchCount 0 and no possession, so any
	 *  dive gate must refuse it until the serve has legally crossed the net. */
	inline bool IsDiveAllowedDuringFlight(const FVolleyballRallyState& RS)
	{
		return !(RS.TouchCount == 0 && RS.LastTouchType == EBallTouchType::Serve && !RS.bServeCrossedNet);
	}

	/** M11c-3: pure-logic dive lifecycle shared by characters and tests.
	 *  None -> Approach (fast lunge) -> Active (0.35~0.55 s extended-reach window)
	 *  -> Recovery (blocks re-dive/touch) -> None. A save inside Active ends it
	 *  immediately; an empty window times out into a Miss and then Recovery. */
	struct SPIKEELITE_API FVolleyballDiveState
	{
		enum class EPhase : uint8 { None, Approach, Active, Recovery };

		EPhase Phase = EPhase::None;
		float ActiveTimer = 0.f;
		float RecoveryTimer = 0.f;
		bool bSaveRecorded = false;

		static constexpr float ActiveWindow = 0.45f;
		static constexpr float RecoveryDuration = 0.8f;

		bool IsActive() const      { return Phase == EPhase::Active; }
		bool IsRecovering() const  { return Phase == EPhase::Recovery; }
		bool CanTouch() const      { return Phase == EPhase::Active && !bSaveRecorded; }

		void Reset() { *this = FVolleyballDiveState(); }

		/** Start of the lunge approach (already sprinting to the save point). */
		void StartDive()
		{
			if (Phase == EPhase::None) { Phase = EPhase::Approach; }
		}

		/** Enter the extended-reach contact window when close to the save point. */
		void EnterActive()
		{
			if (Phase == EPhase::Approach)
			{
				Phase = EPhase::Active;
				ActiveTimer = ActiveWindow;
				bSaveRecorded = false;
			}
		}

		/** A real touch happened inside the window -> DiveSave, then Recovery. */
		void RecordSave()
		{
			if (Phase == EPhase::Active && !bSaveRecorded)
			{
				bSaveRecorded = true;
				Phase = EPhase::Recovery;
				RecoveryTimer = RecoveryDuration;
			}
		}

		/** Advance timers. Returns true when a phase transition happened this tick
		 *  (Active timeout -> Recovery; Recovery end -> None). */
		bool Tick(float DeltaSeconds)
		{
			bool bTransitioned = false;
			if (Phase == EPhase::Active)
			{
				ActiveTimer -= DeltaSeconds;
				if (ActiveTimer <= 0.f && !bSaveRecorded)
				{
					Phase = EPhase::Recovery;   // DiveMiss
					RecoveryTimer = RecoveryDuration;
					bTransitioned = true;
				}
			}
			else if (Phase == EPhase::Recovery)
			{
				RecoveryTimer -= DeltaSeconds;
				if (RecoveryTimer <= 0.f)
				{
					Phase = EPhase::None;       // DiveRecoveryEnd
					bTransitioned = true;
				}
			}
			return bTransitioned;
		}
	};

	/** Human-readable touch count label, e.g. "A 2/3". */
	SPIKEELITE_API FString TouchLabel(const FVolleyballRallyState& State);

	/** Describe a reason in Chinese for the rally-result banner. */
	SPIKEELITE_API FString RallyReasonLabel(ERallyEndReason Reason);
	SPIKEELITE_API FString TouchTypeLabel(EBallTouchType Type);
}
