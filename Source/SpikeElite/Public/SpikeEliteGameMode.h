// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Volleyball/VolleyballEnums.h"
#include "Volleyball/VolleyballRules.h"
#include "SpikeEliteGameMode.generated.h"

class AVolleyballBall;
class AVolleyballCourt;
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
	 * Valid only in AwaitingServe, for the correct serving team, and for the
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

	/** Whether the given character is allowed to touch the ball right now. */
	bool CanTouchBall(const ASpikeEliteCharacter* Toucher) const;

	/** True while the ball may legally be touched (Rally). */
	bool IsRallyLive() const { return MatchState == EMatchState::Rally && !RallyState.bRallySettled; }

	// ---------------- M10: rally state access for UI / AI ----------------

	EVolleyballTeam GetPossessingTeam() const { return RallyState.PossessingTeam; }
	int32 GetTouchCount() const { return RallyState.TouchCount; }
	int32 GetLastTouchPlayerIndex() const { return RallyState.LastTouchPlayerIndex; }
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

	/** Place the ball at the server's hand and enter AwaitingServe. */
	void BeginAwaitingServe();

	/** Finish the toss: strike the ball, record the serve touch, enter Rally. */
	void ExecuteServe();

	/** Single-settlement rally end shared by land / faults / serve faults. */
	void EndRally(ERallyEndReason Reason, EVolleyballTeam ScoringTeam);

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
	/** Predict where the ball will land (or where it would cross floor height). */
	FVector PredictBallLanding() const;
	/** Direction for an AI touch given the phase (receive/set/attack). */
	FVector ComputeAITouchDirection(const ASpikeEliteCharacter* Toucher, EBallTouchType Type) const;
	/** Roster index of Player in Team's roster, or -1. */
	int32 GetPlayerIndex(EVolleyballTeam Team, const ASpikeEliteCharacter* Player) const;
	/** Team of a character via TeamSide. */
	EVolleyballTeam TeamOf(const ASpikeEliteCharacter* Player) const;
};
