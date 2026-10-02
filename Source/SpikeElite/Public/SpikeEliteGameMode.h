// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Volleyball/VolleyballEnums.h"
#include "Volleyball/VolleyballRules.h"
#include "Volleyball/VolleyballTrajectory.h"
#include "SpikeEliteGameMode.generated.h"

class AVolleyballBall;
class AVolleyballCourt;
class AVolleyballArena;
class AMatchOfficialManager;
class URotationWidget;
class ASpikeEliteCharacter;
class UScoreboardWidget;

/**
 * Default game mode for SPIKE ELITE.
 *
 * FIVB rules modelled here (2025-2028 rulebook, simplified M10 subset):
 *  - Rally point scoring: every dead ball awards a point (§12.2)
 *  - Side-out: the rally winner serves next (§12.4)
 *  - Set to 25, win by 2 (§6.1); best of 5; 5th set to 15 (§6.2)
 *  - A team may touch the ball at most 3 times; 4th touch = fault (M10)
 *  - A player may not touch twice in a row (M10)
 *  - Possession switches when the ball legally crosses the net (M10)
 *  - Team positions rotate clockwise on side-out (§7.4) — M2, data only here
 *
 * M10 authority: this GameMode is the ONLY authority for "may I touch the
 * ball" and "who may serve". Characters never ResetBall or Strike on their own;
 * they always ask the GameMode (TryTouchBall / RequestServe).
 */
UCLASS()
class SPIKEELITE_API ASpikeEliteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpikeEliteGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** M11c-5: set the active data-driven set play (tactical UI on confirm). */
	void SetActiveSetPlay(int32 PlayId) { ActiveSetPlayId = PlayId; }

	/** M11c-5: the human player's chosen defense plan (steers block/back-row). */
	void SetPlayerDefensePlan(EVolleyballDefensePlan Plan) { PlayerDefensePlan = Plan; }

	/** M11c-2: per-team rotation counters (1..6, wrap via
	 *  SEVolleyballRules::AdvanceRotationIndex). Each team owns its own rotation
	 *  so a side-out only advances the receiving team that gained the serve. */
	int32 TeamARotation = 1;
	int32 TeamBRotation = 1;

	/** M11d-3: previous serving team, so the rotation HUD can flag side-out 轮转. */
	EVolleyballTeam LastRotationServeTeam = EVolleyballTeam::None;

	/** Current serving team's rotation index (what the HUD shows). */
	int32 GetServingRotation() const { return (ServingTeam == EVolleyballTeam::TeamA) ? TeamARotation : TeamBRotation; }

	/** Public access for widgets / dev verification. */
	AMatchOfficialManager* GetOfficials() const { return Officials; }
	URotationWidget* GetRotationWidget() const { return RotationWidget; }
	float GetServeDeadlineRemaining() const { return ServeDeadlineTimer; }
	AVolleyballBall* GetBall() const { return Ball; }

	/** M11c-7: -RematchStress audit. Logs authoritative singleton/roster counts
	 *  (court/arena/ball/officials/rotation widget/scoreboard/chars) plus the
	 *  world's valid-actor count (PendingKill excluded), once per run index. */
	void DevAuditActors(int32 RunIndex);

	/** Build the authoritative rotation snapshot from the current rosters. */
	void BuildRotationView(FRotationViewState& Out) const;

	/** Push the rotation snapshot to the HUD (no-op if unchanged). */
	void RefreshRotationView();

	/** Spawn court/ball/players and start the first rally. Called from the main menu. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Flow")
	void StartMatch();

	/** Destroy all match actors and return to the main menu. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Flow")
	void ReturnToMainMenu();

	/** Call when the ball hits the floor. Location.X decides which side's court. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Rules")
	void OnBallLanded(const FVector& BallLocation);

	// ---------------- M10: single authority for serve & touch ----------------

	/**
	 * A server (human or AI) asks the GameMode to start the serve.
	 * Valid only in ServiceAuthorized, for the correct serving team, and for the
	 * roster's current server. On success the ball is tossed and the state moves
	 * to ServingToss.
	 */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Rules")
	bool RequestServe(ASpikeEliteCharacter* Server);

	/**
	 * The only entry point for touching the ball (player LMB or AI).
	 * Returns true if the touch was executed. Faults (4 touches / double touch)
	 * settle the rally against the touching team.
	 */
	bool TryTouchBall(ASpikeEliteCharacter* Toucher, EBallTouchType Type);

	/**
	 * Tactical (slow-motion) touch: the human validated a FShotIntent through the
	 * planning UI. Goes through the exact same DoTouch path as TryTouchBall, so
	 * the tested rules and phase gates are shared. TimingError (-1..1) biases the
	 * direction/power (0 = perfect).
	 */
	bool ExecuteTacticalShot(ASpikeEliteCharacter* Toucher, const FShotIntent& Intent);

	/**
	 * Front-row block attempt (M11b-5). Legal only in a live rally, for a
	 * front-row player, inside touch reach, and with the ball armed. A block
	 * does not consume a touch and the blocker may touch again immediately.
	 */
	bool TryBlockBall(ASpikeEliteCharacter* Toucher);

	/** Whether the given character is allowed to touch the ball right now. */
	bool CanTouchBall(const ASpikeEliteCharacter* Toucher) const;

	/** True while the ball may legally be touched (Rally). */
	bool IsRallyLive() const { return MatchState == EMatchState::Rally && !RallyState.bRallySettled; }

	// ---------------- M10: rally state access for UI / AI ----------------

	EVolleyballTeam GetPossessingTeam() const { return RallyState.PossessingTeam; }
	int32 GetTouchCount() const { return RallyState.TouchCount; }
	int32 GetLastTouchPlayerIndex() const { return RallyState.LastTouchPlayerIndex; }
	EVolleyballTeam GetLastTouchTeam() const { return RallyState.LastTouchTeam; }
	EBallTouchType GetLastTouchType() const { return RallyState.LastTouchType; }
	bool GetServeCrossedNet() const { return RallyState.bServeCrossedNet; }
	const TArray<TObjectPtr<ASpikeEliteCharacter>>& GetTeamPlayers(EVolleyballTeam Team) const
	{
		return (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	}
	const FString& GetRallyResultText() const { return RallyResultText; }
	float GetRallyResultDisplaySeconds() const { return RallyResultDisplayTimer; }

	/** Current score, current set. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 TeamAScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 TeamBScore = 0;

	/** Who serves next. FIVB: the rally winner serves. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	EVolleyballTeam ServingTeam = EVolleyballTeam::TeamA;

	/** How many sets each team has won. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 TeamASetsWon = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 TeamBSetsWon = 0;

	/** Which set we are in (1-based). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 CurrentSet = 1;

	/** Points to win a set (25 for sets 1-4, 15 for set 5; 3 in -QuickMatch). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 PointsToWin = 25;

	/** Sets needed to win the match (3 normal; 1 in -QuickMatch). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 MatchWinsNeeded = 3;

	/** Match winner, once MatchOver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	EVolleyballTeam MatchWinner = EVolleyballTeam::None;

	/** Current state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	EMatchState MatchState = EMatchState::PreMatch;

	/** True after StartMatch spawns the world; gates Tick logic. */
	bool bMatchActive = false;

	/** Per-set final scores for the end-of-match screen (index = set number-1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	TArray<int32> SetScoresA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	TArray<int32> SetScoresB;

	/** Seeded random stream for AI errors; deterministic via -Seed=N. */
	FRandomStream AIStream;

	// ---------------- M10: AI coordination (GameMode decides) ----------------

	/** Push a directive to a bot. The bot reads these each Tick. */
	void SetAIDirective(ASpikeEliteCharacter* Bot, EAIBehavior Behavior, const FVector& Target, bool bPrimary);

protected:
	/** Tear down court/ball/players/scoreboard. */
	void CleanupMatch();

	/** Log runtime actor counts (used to verify no duplicates across match cycles). */
	void LogActorCounts(const TCHAR* Tag) const;

	/** Persistent arena shell (hall/stands/lighting/LED). Created once; never rebuilt on Rematch. */
	UPROPERTY()
	TObjectPtr<AVolleyballArena> Arena;

	UPROPERTY()
	TObjectPtr<AVolleyballCourt> Court;

	UPROPERTY()
	TObjectPtr<AVolleyballBall> Ball;

	/** Seconds of pause between rallies (let the crowd breathe). */
	UPROPERTY(EditAnywhere, Category = "Volleyball|Rules")
	float InterRallyDelay = 1.5f;

	/** Longer pause between sets (shows the set-over banner). */
	UPROPERTY(EditAnywhere, Category = "Volleyball|Rules")
	float SetOverDelay = 3.5f;

	/** Timer for the inter-rally pause. */
	float InterRallyTimer = 0.0f;

	// ---- M11b-2: official pre-serve ceremony (ResettingPositions/AwaitingReady/ServiceAuthorized) ----
	/** -FastFlow shortens result display, readiness check and serve deadline. */
	bool bFastFlow = false;

	/** Shared phase timer for ResettingPositions/AwaitingReady. */
	float PhaseTimer = 0.0f;
	/** How long players stay in ResettingPositions before the 2nd-referee check. */
	float ResetDelay = 0.25f;
	/** How long the 2nd referee "confirms readiness" (AwaitingReady). */
	float ReadyDelay = 0.5f;
	/** Seconds the server has after the whistle (FIVB 8 s). */
	float ServeDeadline = 8.0f;
	/** Remaining time to serve after the whistle. */
	float ServeDeadlineTimer = 0.0f;

	/** Advance BetweenRallies -> ResettingPositions -> AwaitingReady -> ServiceAuthorized. */
	void AdvanceRallyPhase();
	/** Blow the service whistle, arm the 8 s serve window and start bot auto-serve timers. */
	void BeginServiceAuthorized();
	/** 8 s elapsed with no serve: serve-fault, opponent scores. */
	void HandleServeDeadline();

	/** Persistent officials (referee stands, scorer table, benches, whistle). */
	UPROPERTY()
	TObjectPtr<AMatchOfficialManager> Officials;

	/** Persistent right-top rotation HUD (created once, hidden/reshown). */
	UPROPERTY()
	TObjectPtr<URotationWidget> RotationWidget;

	// ---- Serve machine ----
	/** Serving toss: ball is tossed up for a moment before being struck. */
	bool bInToss = false;
	float TossTimer = 0.0f;
	FVector TossDir = FVector::ForwardVector;
	float TossPower = 1300.0f;
	/** Roster index of the player who is serving (for touch recording). */
	int32 ServerPlayerIndex = -1;

	/** Cooldown after a manual net tap so the ball isn't deflected every frame. */
	float NetTouchCooldown = 0.0f;
	/** M11c-3: after an AI block whiffs, this cools the block attempt so the
	 *  spike genuinely gets through to the back row (0 = block ready). */
	float BlockerMissCooldown = 0.0f;

	/** Previous-frame ball X, used to detect crossing the net plane. */
	float BallPrevX = 0.0f;
	/** Latched while the ball occupies the net collision slab. */
	bool bNetContactLatched = false;

	/** M10 rally state (possession, touches, last touch, settled flag). */
	FVolleyballRallyState RallyState;

	/** Banner text for the last rally result + remaining display time. */
	FString RallyResultText;
	float RallyResultDisplayTimer = 0.0f;

	/** True once the ball has legally crossed the net during the current serve. */
	bool bServeCrossedNet = false;

	/** True while a bot server is scheduled to serve automatically. */
	bool bAIServePending = false;
	float AIServeTimer = 0.0f;

	/** True when -QuickMatch is on the command line (1 set to 3, reuse real rules). */
	bool bQuickMatch = false;

	/** True when -devauto is on the command line (non-Shipping only). In normal
	 *  play a human server must press E; only under -devauto may the automation
	 *  path serve for the human after a 3 s timeout. See ShouldAutoServe. */
	bool bDevAuto = false;

	/** Throttle for dynamic scoreboard refresh (ball hint): ~6.7 Hz. Static fields
	 *  (score/set/phase/possession) still push immediately on change via direct
	 *  UpdateScoreboard() calls at the change sites. */
	float ScoreboardUpdateTimer = 0.0f;

	/** Signature of the last pushed scoreboard state; unchanged -> skip SetText. */
	FString LastScoreboardSignature;

	/** Throttle for AI tactical re-selection: ~12.5 Hz. Bot movement itself stays
	 *  per-frame inside ASpikeEliteCharacter::Tick. */
	float AIDirectiveTimer = 0.0f;

	/** Maximum distance (cm) at which a character may touch the ball. */
	static constexpr float TouchReach = 220.0f;

	/** Ball is hittable between these heights. */
	static constexpr float MinTouchZ = 120.0f;
	static constexpr float MaxTouchZ = 450.0f;

	/** Award a point to the given team and rotate serve. */
	void AwardPoint(EVolleyballTeam ScoringTeam);

	/** FIVB §7.4: on side-out, the serving team rotates clockwise. */
	void RotateTeam(EVolleyballTeam TeamToRotate);

	/** Teleport all players to their current home positions. */
	void RespawnPlayersToPositions();

	/** The 6 players on each side, in position order [1..6]. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Roster")
	TArray<TObjectPtr<ASpikeEliteCharacter>> TeamAPlayers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Roster")
	TArray<TObjectPtr<ASpikeEliteCharacter>> TeamBPlayers;

	/** On-screen scoreboard widget. */
	UPROPERTY()
	TObjectPtr<UScoreboardWidget> Scoreboard;

	/** FIVB court positions in cm (Team A half: X>0). */
	static TArray<FVector> GetPositionsA();

	/** Check whether the current set has been won; advance state. */
	void CheckSetWin();

	/** Reset per-set scores and bump CurrentSet. */
	void StartNextSet();

	/** Finish the toss: strike the ball, record the serve touch, enter Rally. */
	void ExecuteServe();

	/** M11f-5: Development-only accelerated best-of-five integration driver. */
	void DriveFiveSet();
	bool bFiveSetTest = false;
	bool bFiveSetReported = false;
	double FiveSetExitAt = -1.0;
	FTimerHandle FiveSetStartTimer;

	/** Single-settlement rally end shared by land / faults / serve faults. */
	void EndRally(ERallyEndReason Reason, EVolleyballTeam ScoringTeam);

	/**
	 * Shared core of TryTouchBall / ExecuteTacticalShot: phase gate, touch-armed,
	 * reach, EvaluateTouch and the strike + post-touch bookkeeping. The caller
	 * supplies the already-computed direction/power/spin.
	 */
	bool DoTouch(ASpikeEliteCharacter* Toucher, EBallTouchType Type, const FVector& Dir, float Power, float SpinRadS);

	/** Ball legally crossed the net plane above the net: switch possession. */
	void OnBallCrossedNet();

	/** Fill RallyResultText and start its display timer. */
	void SetRallyResult(ERallyEndReason Reason, EVolleyballTeam ScoringTeam);

	/** Generic banner (rally result / serve hint / info). */
	void ShowBanner(const FString& Text, float Seconds);

	/** Re-arm touch protection for characters once the ball leaves their reach. */
	void RearmTouchers();

	/** Push the latest match/rally info into the scoreboard widget. */
	void UpdateScoreboard();

	/** Notify the player controller that the match is over (result screen). */
	void NotifyMatchOver();

	/** Stable English label for a touch type (logs). */
	static const TCHAR* TypeStr(EBallTouchType Type);

	// ---- M10 AI coordination ----
	void UpdateAIDirectives(float DeltaSeconds);
	/** Choose the receiver: closest to the predicted landing point. */
	int32 SelectReceivePlayer(EVolleyballTeam Team, const FVector& Landing) const;
	/** Choose the setter: closest to the front-middle set zone. */
	int32 SelectSetterPlayer(EVolleyballTeam Team) const;
	/** Choose the attacker: closest to the front attack point near the net. */
	int32 SelectAttackerPlayer(EVolleyballTeam Team) const;

	/** M11c-5: pick the hitter whose rotation slot matches the active set play
	 *  (四号位 -> P4, 二号位 -> P2, 副攻 -> P3, 后排 -> nearest back-row slot). */
	int32 SelectAttackerForPlay(EVolleyballTeam Team) const;

	/** Current data-driven set play (set by the tactical UI on set confirmation). */
	int32 ActiveSetPlayId = -1;

	/** Defense plan chosen by the human player before the opponent's attack. */
	EVolleyballDefensePlan PlayerDefensePlan = EVolleyballDefensePlan::NoPlan;
	/** Predict where the ball will land (or where it would cross floor height). */
	FVector PredictBallLanding() const;
	/** Direction for an AI touch given the phase (receive/set/attack). */
	FVector ComputeAITouchDirection(const ASpikeEliteCharacter* Toucher, EBallTouchType Type) const;
	/** Roster index of Player in Team's roster, or -1. */
	int32 GetPlayerIndex(EVolleyballTeam Team, const ASpikeEliteCharacter* Player) const;
	/** Team of a character via TeamSide. */
	EVolleyballTeam TeamOf(const ASpikeEliteCharacter* Player) const;
};
