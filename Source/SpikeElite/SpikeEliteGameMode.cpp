// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballBall.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolleyballRules, Log, All);

ASpikeEliteGameMode::ASpikeEliteGameMode()
{
	DefaultPawnClass = ASpikeEliteCharacter::StaticClass();
}

void ASpikeEliteGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	// Spawn a procedural court at origin.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Court = World->SpawnActor<AVolleyballCourt>(AVolleyballCourt::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// Spawn a ball near the net, slightly above it.
	if (Court)
	{
		FVector BallLoc = FVector(0.0f, 0.0f, Court->NetHeight + 50.0f);
		Ball = World->SpawnActor<AVolleyballBall>(AVolleyballBall::StaticClass(), BallLoc, FRotator::ZeroRotator, Params);
		if (Ball)
		{
			// First serve goes to Team A.
			Ball->Strike(FVector(0.6f, 0.0f, 0.25f), 600.0f, 0.0f);
		}
	}

	UE_LOG(LogVolleyballRules, Log, TEXT("SPIKE ELITE match start. Serve: Team A. First to %d takes the set."), PointsToWin);
}

void ASpikeEliteGameMode::OnBallLanded(const FVector& BallLocation)
{
	// Court halves: X>0 is Team A's side, X<0 is Team B's side.
	// If the ball bounces on Team A's floor, Team B wins the rally (and vice versa).
	EVolleyballTeam ScoringTeam = (BallLocation.X >= 0.0f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	AwardPoint(ScoringTeam);
}

void ASpikeEliteGameMode::AwardPoint(EVolleyballTeam ScoringTeam)
{
	if (ScoringTeam == EVolleyballTeam::TeamA)
	{
		TeamAScore++;
	}
	else
	{
		TeamBScore++;
	}

	// FIVB side-out: rally winner gets to serve.
	ServingTeam = ScoringTeam;

	UE_LOG(LogVolleyballRules, Log,
		TEXT("Rally point -> %s. Score A:%d  B:%d. Next serve: %s."),
		ScoringTeam == EVolleyballTeam::TeamA ? TEXT("Team A") : TEXT("Team B"),
		TeamAScore, TeamBScore,
		ServingTeam == EVolleyballTeam::TeamA ? TEXT("Team A") : TEXT("Team B"));

	// M1: no set-end detection yet (needs win-by-2, best-of-5). Just keep playing.
	// Reset ball to the serving team's end line for the next rally.
	if (Ball && Court)
	{
		float ServeX = (ServingTeam == EVolleyballTeam::TeamA) ? Court->HalfCourtLength - 50.0f : -(Court->HalfCourtLength - 50.0f);
		Ball->ResetBall(FVector(ServeX, 0.0f, Court->NetHeight + 80.0f));
		// Serve toward the opponent.
		FVector Dir = (ServingTeam == EVolleyballTeam::TeamA) ? FVector(-1.0f, 0.0f, 0.2f) : FVector(1.0f, 0.0f, 0.2f);
		Ball->Strike(Dir, 650.0f, 0.0f);
	}
}
