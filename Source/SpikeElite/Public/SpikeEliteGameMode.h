// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Volleyball/VolleyballEnums.h"
#include "Volleyball/VolleyballIdentity.h"
#include "Volleyball/MatchMode.h"
#include "Volleyball/ChallengeSave.h"
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
class UInstancedStaticMeshComponent;

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

	/** M11h-1: authoritative rosters (12 registered, 6 starting / on court).
	 *  Rotation/substitution change slots, never PlayerId/identity/stats owner. */
	FTeamRosterState RosterA;
	FTeamRosterState RosterB;
	FSubstitutionLedger SubLedgerA, SubLedgerB;
	int32 CompletedRallies = 0;
	TArray<FString> SavedLineupA, SavedLineupB;
	UPROPERTY() TArray<TObjectPtr<ASpikeEliteCharacter>> BenchPlayers;
	FString LastManagementMessage;
	void SynchronizeCourtIdentities();
	void RefreshRegisteredBench();
	bool SetStartingLineup(EVolleyballTeam Team, const TArray<FString>& Lineup, FString& Reason);
	void LoadLineups();
	bool SaveLineups() const;
	float EntranceRemaining=0.f;
	void BeginEntrance();
	void FinishEntrance();
	void TickEntrance(float DeltaSeconds);
	int32 TrainingAttempts=0, TrainingSuccesses=0, TrainingPlayerTouches=0;
	float TrainingTimer=0.f;
	bool bTrainingWaiting=false;
	FVector TrainingGoal=FVector::ZeroVector;
	FString TrainingFeedback;
	UPROPERTY() TObjectPtr<AActor> TrainingMarker;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TrainingPreview;
	void StartTrainingAttempt();
	void FinishTrainingAttempt(bool bSuccess, const FString& Reason);
	void TickTraining(float DeltaSeconds);
	FString GetPracticeStatus() const;
	bool ValidateRuntimeRoster(FString& Reason) const;
	#if !UE_BUILD_SHIPPING
	bool DevManagementChecks(FString& Failures);
	#endif
	void ResetFlightTracking();

	/** M11h-1: identity of the character's court player (or nullptr for bench/
	 *  non-roster actors). Team decided from the character's TeamSide. */
	const FPlayerIdentity* FindIdentity(EVolleyballTeam Team, const ASpikeEliteCharacter* C) const;

	/** M11h-2: current production mode configuration (menu-picked). */
	FMatchModeConfig MatchModeConfig;

	/** M11h-2: apply the menu-selected mode before StartMatch. QuickMatch with
	 *  bShortSets=false runs the official five-set rules; the dev -QuickMatch
	 *  command line only sets the same flag in the constructor. */
	void SetMatchMode(const FMatchModeConfig& Cfg) { MatchModeConfig = Cfg; }
	EGameModeChoice GetMatchMode() const { return MatchModeConfig.Mode; }

	/** M11h-2b: challenge save state (loaded on demand, saved at MatchOver). */
	SEChallenge::FSaveData ChallengeSave;
	void LoadChallengeState();
	void SaveChallengeState();
	/** M11h-2b: apply stage difficulty to the opponent (Team B) roster. */
	void ApplyChallengeDifficulty();

	// ---------------- M11h-4: team timeouts (FIVB subset) ----------------
	/** Per-team timeouts remaining this set (FIVB: 2 per set, 30 s). Esc
	 *  system pause does NOT consume these. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Flow")
	int32 TimeoutLeftA = 2;
	int32 TimeoutLeftB = 2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Flow")
	EVolleyballTeam TimeoutTeam = EVolleyballTeam::None;
	/** Seconds remaining in the current timeout (0 when not in a timeout). */
	float TimeoutTimer = 0.f;
	/** Seconds of system-pause time accumulated during a timeout (so the 30 s
	 *  clock never jumps forward after Esc resume). */
	float TimeoutPausedSeconds = 0.f;

	/** Request a team timeout. Legal only on a dead ball BEFORE the service
	 *  whistle (BetweenRallies/ResettingPositions/AwaitingReady/ServePresentation),
	 *  never during ServingToss/Rally/SetOver/MatchOver, with >=1 remaining. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Flow")
	bool RequestTeamTimeout(EVolleyballTeam Team);

	/** Abort/cancel the current timeout (dev / menu path). */
	void CancelTeamTimeout();
	int32 GetTimeoutLeft(EVolleyballTeam Team) const
	{
		return (Team == EVolleyballTeam::TeamA) ? TimeoutLeftA : TimeoutLeftB;
	}
	float GetTimeoutRemaining() const { return TimeoutTimer; }
	bool IsInTimeout() const { return MatchState == EMatchState::Timeout; }
	/** Dead-ball window in which a team timeout may legally be requested. */
	bool CanRequestTimeout() const;

	// ---------------- M11h-3: server introduction (ServePresentation) ----------------
	/** Seconds the server intro card stays up (long for a new server, short bar
	 *  when the same player serves again consecutively). */
	float PresentationTimer = 0.f;
	/** PlayerId of the last presented server (to pick short vs long intro). */
	FString LastPresentedServerId;
	/** Skip the intro early (E / Enter) and move to the whistle. */
	void SkipServePresentation();
	/** Enter the server-intro phase (called when AwaitingReady elapses). */
	void EnterServePresentation();

	// ---------------- M11h-5: substitutions (FIVB 15.1/15.2/15.6 subset) ----------------
	/** Substitutions remaining this set (FIVB: 6 per set). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Flow")
	int32 SubstitutionsLeftA = 6;
	int32 SubstitutionsLeftB = 6;
	/** Bench PlayerIds currently off court. */
	TArray<FString> SubPoolA;
	TArray<FString> SubPoolB;
	/** Bench player -> the one starter they may replace (and who may replace
	 *  them back). Built per set from the registered roster pairing. */
	TMap<FString, FString> SubPairingA;
	TMap<FString, FString> SubPairingB;

	/** Legal substitution request (dead ball, allowance, pairing, on-court index
	 *  validity). Applies atomically: identity switch + jersey refresh, no
	 *  transient 7/5-man court, no duplicate PlayerId. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Flow")
	bool RequestSubstitution(EVolleyballTeam Team, int32 CourtIndex, const FString& SubId);

	/** Same gates as RequestSubstitution, with a human-readable rejection reason. */
	bool CanRequestSubstitution(EVolleyballTeam Team, int32 CourtIndex, const FString& SubId, FString& OutReason) const;

	/** Rebuild bench/pairing/allowances for a new set or match. */
	void ResetSubstitutionState();

	// ---------------- M11h-6: coach / team-management preferences ----------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Coach")
	FCoachPreferences CoachA;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Coach")
	FCoachPreferences CoachB;
	/** True while the coach panel is open (coach mode / normal team panel). */
	bool bCoachPanelOpen = false;

	/** Set a team's coach preference; validated and clamped. */
	void SetCoachPreference(EVolleyballTeam Team, const FCoachPreferences& Pref)
	{
		FCoachPreferences& Dst = (Team == EVolleyballTeam::TeamA) ? CoachA : CoachB;
		Dst.ServeZone = FMath::Clamp(Pref.ServeZone, -1, 1);
		Dst.BlockPreference = FMath::Clamp(Pref.BlockPreference, 0, 2);
		Dst.DefenseDepth = FMath::Clamp(Pref.DefenseDepth, -1, 1);
		Dst.SetterPreference = FMath::Clamp(Pref.SetterPreference, 0, 4);
		Dst.RiskTolerance = FMath::Clamp(Pref.RiskTolerance, 0.f, 1.f);
	}
	const FCoachPreferences& GetCoach(EVolleyballTeam Team) const
	{
		return (Team == EVolleyballTeam::TeamA) ? CoachA : CoachB;
	}

	// ---------------- M11h-8: per-player match stats ----------------
	/** Per-registered-player stats (stable Registered index, 0..11). Reset per match,
	 *  accumulate across sets. Attributed from real touch/point events only.
	 *  (Plain C++ members: the stat struct is a non-reflected namespace type.) */
	TArray<SEVolleyballRules::FPlayerMatchStats> StatsA;
	TArray<SEVolleyballRules::FPlayerMatchStats> StatsB;
	/** Reset StatsA/StatsB to 12 empty entries (match start / rematch). */
	void ResetMatchStats();
	/** The stats array for a team (indexed by stable registered identity). */
	TArray<SEVolleyballRules::FPlayerMatchStats>& StatsFor(EVolleyballTeam Team)
	{
		return (Team == EVolleyballTeam::TeamA) ? StatsA : StatsB;
	}
	/** A real touch happened: bump the type counters (dive saves included). */
	void RecordTouchStat(EVolleyballTeam Team, int32 CourtIndex, EBallTouchType Type, bool bWasDiveSave);

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

	/** Chinese label of the current match-state node (coach panel status line). */
	const TCHAR* GetPhaseLabel() const
	{
		switch (MatchState)
		{
		case EMatchState::PreMatch:            return TEXT("赛前");
		case EMatchState::BetweenRallies:      return TEXT("回合间");
		case EMatchState::ResettingPositions:  return TEXT("复位");
		case EMatchState::AwaitingReady:       return TEXT("等待就绪");
		case EMatchState::ServiceAuthorized:   return TEXT("允许发球");
		case EMatchState::ServingToss:         return TEXT("发球抛球");
		case EMatchState::Rally:               return TEXT("回合进行");
		case EMatchState::Timeout:             return TEXT("球队暂停");
		case EMatchState::SetOver:             return TEXT("局间休息");
		case EMatchState::MatchOver:           return TEXT("比赛结束");
		default:                               return TEXT("-");
		}
	}

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
