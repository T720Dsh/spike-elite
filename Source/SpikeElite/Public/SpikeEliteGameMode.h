// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpikeEliteGameMode.generated.h"

class AVolleyballBall;
class AVolleyballCourt;

/** Which side of the net a team defends. */
UENUM(BlueprintType)
enum class EVolleyballTeam : uint8
{
	TeamA       UMETA(DisplayName = "Team A (near / +X side)"),
	TeamB       UMETA(DisplayName = "Team B (far / -X side)"),
	None        UMETA(Hidden)
};

/**
 * Default game mode for SPIKE ELITE.
 *
 * M1: FIVB rally-point rules in their simplest form:
 *  - Every dead ball awards a point (rally point scoring, FIVB §12.2)
 *  - The team that wins the rally also gets to serve next (side-out, §12.4)
 *  - A set goes to 25, win by 2 (§6.1) — M1 only tracks the count, no set win yet
 *  - Court halves are split by X: Team A defends X>0, Team B defends X<0
 *
 * Ball-ground detection lives here for now; later it moves to the ball / a
 * dedicated rules component once AI and a proper UI are in.
 */
UCLASS()
class SPIKEELITE_API ASpikeEliteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpikeEliteGameMode();

	virtual void BeginPlay() override;

	/** Call when the ball hits the floor. Location.X decides which side's court. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Rules")
	void OnBallLanded(const FVector& BallLocation);

	/** Current score, Team A vs Team B. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 TeamAScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	int32 TeamBScore = 0;

	/** Who serves next. FIVB: the rally winner serves. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volleyball|Score")
	EVolleyballTeam ServingTeam = EVolleyballTeam::TeamA;

	/** Points needed to win a set (FIVB: 25). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volleyball|Rules")
	int32 PointsToWin = 25;

protected:
	UPROPERTY()
	TObjectPtr<AVolleyballCourt> Court;

	UPROPERTY()
	TObjectPtr<AVolleyballBall> Ball;

	/** Award a point to the given team and rotate serve. */
	void AwardPoint(EVolleyballTeam ScoringTeam);
};
