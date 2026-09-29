// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpikeEliteGameMode.generated.h"

class AVolleyballBall;
class AVolleyballCourt;
class ASpikeEliteCharacter;
class UScoreboardWidget;

/** Which side of the net a team defends. */
UENUM(BlueprintType)
enum class EVolleyballTeam : uint8
{
	TeamA       UMETA(DisplayName = "Team A (near / +X side)"),
	TeamB       UMETA(DisplayName = "Team B (far / -X side)"),
	None        UMETA(Hidden)
};

/** Match state machine. */
UENUM(BlueprintType)
enum class EMatchState : uint8
{
	PreMatch    UMETA(DisplayName = "Pre-match warmup"),
	Playing     UMETA(DisplayName = "Rally in progress"),
	BetweenRallies UMETA(DisplayName = "Between rallies (serve delay)"),
	SetOver     UMETA(DisplayName = "Set finished"),
	MatchOver   UMETA(DisplayName = "Match finished")
};

/**
 * Default game mode for SPIKE ELITE.
 *
 * FIVB rules modelled here (2025-2028 rulebook):
 *  - Rally point scoring: every dead ball awards a point (§12.2)
 *  - Side-out: the rally winner serves next (§12.4)
 *  - Set to 25, win by 2 (§6.1)
 *  - Best of 5 sets; 5th set goes to 15 (§6.2)
 *  - Team positions rotate clockwise on side-out (§7.4) — M2, data only here
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

	/** Points to win a set (25 for sets 1-4, 15 for set 5). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 PointsToWin = 25;

	/** Match winner, once MatchOver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	EVolleyballTeam MatchWinner = EVolleyballTeam::None;

	/** Current state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	EMatchState MatchState = EMatchState::PreMatch;

	/** True after StartMatch spawns the world; gates Tick logic. */
	bool bMatchActive = false;

protected:
	/** Tear down court/ball/players/scoreboard. */
	void CleanupMatch();
	UPROPERTY()
	TObjectPtr<AVolleyballCourt> Court;

	UPROPERTY()
	TObjectPtr<AVolleyballBall> Ball;

	/** Seconds of pause between rallies (let the crowd breathe). */
	UPROPERTY(EditAnywhere, Category = "Volleyball|Rules")
	float InterRallyDelay = 1.5f;

	/** Timer for the inter-rally pause. */
	float InterRallyTimer = 0.0f;

	/** Serving toss: ball is tossed up for a moment before being struck. */
	bool bInToss = false;
	float TossTimer = 0.0f;
	FVector TossDir = FVector::ForwardVector;
	float TossPower = 1300.0f;

	/** Cooldown so AI doesn't re-hit the same ball every frame. */
	float AIHitCooldown = 0.0f;

	/** Award a point to the given team and rotate serve. */
	void AwardPoint(EVolleyballTeam ScoringTeam);

	/** FIVB §7.4: on side-out, the serving team rotates clockwise (each player moves to the next lower position number). */
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

	/** Start the next serve after the pause. */
	void ServeNextBall();

	/** Reset per-set scores and bump CurrentSet. */
	void StartNextSet();
};
