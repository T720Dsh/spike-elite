// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballBall.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolleyballRules, Log, All);

ASpikeEliteGameMode::ASpikeEliteGameMode()
{
	DefaultPawnClass = ASpikeEliteCharacter::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ASpikeEliteGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Court = World->SpawnActor<AVolleyballCourt>(AVolleyballCourt::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Ball = World->SpawnActor<AVolleyballBall>(AVolleyballBall::StaticClass(), FVector(0,0,400), FRotator::ZeroRotator, Params);

	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 1.0f;  // short opening delay
	ServingTeam = EVolleyballTeam::TeamA;

	UE_LOG(LogVolleyballRules, Log, TEXT("=== SPIKE ELITE match start. First to 3 sets wins. Set 1: to %d ==="), PointsToWin);
}

void ASpikeEliteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MatchState == EMatchState::BetweenRallies)
	{
		InterRallyTimer -= DeltaSeconds;
		if (InterRallyTimer <= 0.0f)
		{
			ServeNextBall();
		}
	}

	// On-screen scoreboard (M1: built-in debug text; replace with UMG later).
	if (GEngine)
	{
		FString Line1 = FString::Printf(TEXT("SET %d   SCORE  A %d : %d B     (to %d)"),
			CurrentSet, TeamAScore, TeamBScore, PointsToWin);
		FString Line2 = FString::Printf(TEXT("Sets: A %d - %d B   |   Serving: %s"),
			TeamASetsWon, TeamBSetsWon,
			ServingTeam == EVolleyballTeam::TeamA ? TEXT("A") : TEXT("B"));
		FString Line3 = (MatchState == EMatchState::MatchOver)
			? FString::Printf(TEXT("*** MATCH WINNER: %s ***"), MatchWinner == EVolleyballTeam::TeamA ? TEXT("TEAM A") : TEXT("TEAM B"))
			: FString(TEXT("WASD move  Mouse look  Space jump  V toggle FP"));

		GEngine->AddOnScreenDebugMessage(101, 0.0f, FColor::Yellow, Line1);
		GEngine->AddOnScreenDebugMessage(102, 0.0f, FColor::Cyan, Line2);
		GEngine->AddOnScreenDebugMessage(103, 0.0f, FColor::Green, Line3);
	}
}

void ASpikeEliteGameMode::OnBallLanded(const FVector& BallLocation)
{
	if (MatchState != EMatchState::Playing) return;

	// X>0 is Team A's floor -> Team B wins the rally (and vice versa).
	EVolleyballTeam ScoringTeam = (BallLocation.X >= 0.0f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	AwardPoint(ScoringTeam);
}

void ASpikeEliteGameMode::AwardPoint(EVolleyballTeam ScoringTeam)
{
	if (ScoringTeam == EVolleyballTeam::TeamA) TeamAScore++;
	else TeamBScore++;

	ServingTeam = ScoringTeam;

	UE_LOG(LogVolleyballRules, Log,
		TEXT("Rally -> %s. Set %d score:  A %d : %d B.  Next serve: %s"),
		ScoringTeam == EVolleyballTeam::TeamA ? TEXT("A") : TEXT("B"),
		CurrentSet, TeamAScore, TeamBScore,
		ServingTeam == EVolleyballTeam::TeamA ? TEXT("A") : TEXT("B"));

	CheckSetWin();
}

void ASpikeEliteGameMode::CheckSetWin()
{
	// FIVB: need >= PointsToWin AND lead by at least 2.
	bool bAHas = TeamAScore >= PointsToWin && (TeamAScore - TeamBScore) >= 2;
	bool bBHas = TeamBScore >= PointsToWin && (TeamBScore - TeamAScore) >= 2;

	if (!bAHas && !bBHas)
	{
		// Not over yet -> pause then re-serve.
		MatchState = EMatchState::BetweenRallies;
		InterRallyTimer = InterRallyDelay;
		return;
	}

	EVolleyballTeam SetWinner = bAHas ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	if (SetWinner == EVolleyballTeam::TeamA) TeamASetsWon++; else TeamBSetsWon++;

	UE_LOG(LogVolleyballRules, Log,
		TEXT("=== Set %d won by %s. Sets: A %d - %d B ==="),
		CurrentSet, SetWinner == EVolleyballTeam::TeamA ? TEXT("A") : TEXT("B"),
		TeamASetsWon, TeamBSetsWon);

	// Best of 5: first to 3 sets.
	if (TeamASetsWon >= 3 || TeamBSetsWon >= 3)
	{
		MatchWinner = (TeamASetsWon >= 3) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
		MatchState = EMatchState::MatchOver;
		UE_LOG(LogVolleyballRules, Log, TEXT("*** MATCH WON BY %s ***"), MatchWinner == EVolleyballTeam::TeamA ? TEXT("TEAM A") : TEXT("TEAM B"));
		return;
	}

	StartNextSet();
}

void ASpikeEliteGameMode::StartNextSet()
{
	CurrentSet++;
	TeamAScore = 0;
	TeamBScore = 0;
	// Set 5 (decider) goes to 15, not 25.
	PointsToWin = (CurrentSet >= 5) ? 15 : 25;

	UE_LOG(LogVolleyballRules, Log, TEXT("--- Set %d starting, to %d ---"), CurrentSet, PointsToWin);

	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = InterRallyDelay + 2.0f;  // longer break between sets
}

void ASpikeEliteGameMode::ServeNextBall()
{
	if (!Ball || !Court) return;

	float ServeX = (ServingTeam == EVolleyballTeam::TeamA)
		? Court->HalfCourtLength - 50.0f
		: -(Court->HalfCourtLength - 50.0f);
	Ball->ResetBall(FVector(ServeX, 0.0f, Court->NetHeight + 80.0f));

	FVector Dir = (ServingTeam == EVolleyballTeam::TeamA)
		? FVector(-0.878f, 0.0f, 0.479f)
		: FVector(0.878f, 0.0f, 0.479f);
	Ball->Strike(Dir, 1300.0f, 0.0f);

	MatchState = EMatchState::Playing;
}
