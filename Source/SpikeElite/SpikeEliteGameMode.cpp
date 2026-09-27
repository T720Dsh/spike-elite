// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballBall.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

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

	// FIVB 6 positions per side (cm).
	struct FPos { float X; float Y; };
	TArray<FPos> Positions = {
		{ 820,   0},   // 1: server
		{ 550, 300},   // 2: front-right
		{ 550,   0},   // 3: front-middle
		{ 550,-300},   // 4: front-left
		{ 200,-300},   // 5: back-left
		{ 200,   0},   // 6: back-middle
	};

	// Reposition the human-controlled pawn (auto-spawned at origin) to Team A position 1.
	if (APawn* Human = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ASpikeEliteCharacter* HC = Cast<ASpikeEliteCharacter>(Human))
		{
			HC->bIsBot = false;
			HC->TeamSide = 1;
			HC->HomePosition = FVector(Positions[0].X, Positions[0].Y, 0);
			HC->SetActorLocation(FVector(Positions[0].X, Positions[0].Y, 100.0f));
		}
	}

	// Spawn the other 5 Team A bots (positions 2-6).
	for (int32 i = 1; i < 6; i++)
	{
		ASpikeEliteCharacter* Bot = World->SpawnActor<ASpikeEliteCharacter>(
			FVector(Positions[i].X, Positions[i].Y, 100.0f), FRotator(0, -90, 0), Params);
		if (Bot)
		{
			Bot->bIsBot = true;
			Bot->TeamSide = 1;
			Bot->HomePosition = FVector(Positions[i].X, Positions[i].Y, 0);
		}
	}

	// Spawn 6 Team B bots (mirror on X<0 side).
	for (int32 i = 0; i < 6; i++)
	{
		ASpikeEliteCharacter* Bot = World->SpawnActor<ASpikeEliteCharacter>(
			FVector(-Positions[i].X, Positions[i].Y, 100.0f), FRotator(0, 90, 0), Params);
		if (Bot)
		{
			Bot->bIsBot = true;
			Bot->TeamSide = -1;
			Bot->HomePosition = FVector(-Positions[i].X, Positions[i].Y, 0);
		}
	}

	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 1.0f;
	ServingTeam = EVolleyballTeam::TeamA;

	UE_LOG(LogVolleyballRules, Log, TEXT("=== SPIKE ELITE 6v6 match start. First to 3 sets wins. ==="));
}

void ASpikeEliteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MatchState == EMatchState::BetweenRallies)
	{
		InterRallyTimer -= DeltaSeconds;
		if (InterRallyTimer <= 0.0f) ServeNextBall();
	}

	// Net touch bounce (kept as safety net in case no bot is near the net).
	if (MatchState == EMatchState::Playing && Ball)
	{
		const FVector BL = Ball->GetActorLocation();
		const float NetH = Court ? Court->NetHeight : 243.0f;
		if (FMath::Abs(BL.X) < 12.0f && BL.Z < NetH && BL.Z > 20.0f)
		{
			Ball->Strike(FVector(-Ball->GetVelocity().X, Ball->GetVelocity().Y * 0.5f, 200.0f).GetSafeNormal(), 400.0f, 0.0f);
		}
	}

	// Scoreboard.
	if (GEngine)
	{
		FString Line1 = FString::Printf(TEXT("SET %d   SCORE  A %d : %d B     (to %d)"),
			CurrentSet, TeamAScore, TeamBScore, PointsToWin);
		FString Line2 = FString::Printf(TEXT("Sets: A %d - %d B   |   Serving: %s"),
			TeamASetsWon, TeamBSetsWon,
			ServingTeam == EVolleyballTeam::TeamA ? TEXT("A") : TEXT("B"));
		FString Line3 = (MatchState == EMatchState::MatchOver)
			? FString::Printf(TEXT("*** MATCH WINNER: %s ***"), MatchWinner == EVolleyballTeam::TeamA ? TEXT("TEAM A") : TEXT("TEAM B"))
			: FString(TEXT("WASD move  LMB hit  Space jump  V FP  E serve"));

		GEngine->AddOnScreenDebugMessage(101, 0.0f, FColor::Yellow, Line1);
		GEngine->AddOnScreenDebugMessage(102, 0.0f, FColor::Cyan, Line2);
		GEngine->AddOnScreenDebugMessage(103, 0.0f, FColor::Green, Line3);

		if (Ball)
		{
			APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
			if (Player)
			{
				const FVector ToBall = Ball->GetActorLocation() - Player->GetActorLocation();
				const float Dist = ToBall.Size();
				FString Dir;
				if (Dist < 200.0f) Dir = TEXT("HERE!");
				else
				{
					const float Yaw = FRotationMatrix::MakeFromX(ToBall).Rotator().Yaw - Player->GetControlRotation().Yaw;
					if      (Yaw > 45 && Yaw <= 135)  Dir = TEXT("<< LEFT");
					else if (Yaw <= -45 && Yaw >= -135) Dir = TEXT("RIGHT >>");
					else if (Yaw > 135 || Yaw < -135)   Dir = TEXT("BEHIND");
					else                                   Dir = TEXT("FRONT");
				}
				GEngine->AddOnScreenDebugMessage(104, 0.0f, FColor::Orange,
					FString::Printf(TEXT("Ball: %s  (%.0f m)"), *Dir, Dist / 100.0f));
			}
		}
	}
}

void ASpikeEliteGameMode::OnBallLanded(const FVector& BallLocation)
{
	if (MatchState != EMatchState::Playing) return;
	EVolleyballTeam ScoringTeam = (BallLocation.X >= 0.0f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	AwardPoint(ScoringTeam);
}

void ASpikeEliteGameMode::AwardPoint(EVolleyballTeam ScoringTeam)
{
	if (ScoringTeam == EVolleyballTeam::TeamA) TeamAScore++; else TeamBScore++;
	ServingTeam = ScoringTeam;
	UE_LOG(LogVolleyballRules, Log, TEXT("Rally point awarded. Score A:%d B:%d"), TeamAScore, TeamBScore);
	CheckSetWin();
}

void ASpikeEliteGameMode::CheckSetWin()
{
	bool bAHas = TeamAScore >= PointsToWin && (TeamAScore - TeamBScore) >= 2;
	bool bBHas = TeamBScore >= PointsToWin && (TeamBScore - TeamAScore) >= 2;
	if (!bAHas && !bBHas)
	{
		MatchState = EMatchState::BetweenRallies;
		InterRallyTimer = 1.5f;
		return;
	}
	EVolleyballTeam SetWinner = bAHas ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	if (SetWinner == EVolleyballTeam::TeamA) TeamASetsWon++; else TeamBSetsWon++;
	UE_LOG(LogVolleyballRules, Log, TEXT("=== Set %d won. Sets A:%d B:%d ==="), CurrentSet, TeamASetsWon, TeamBSetsWon);
	if (TeamASetsWon >= 3 || TeamBSetsWon >= 3)
	{
		MatchWinner = (TeamASetsWon >= 3) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
		MatchState = EMatchState::MatchOver;
		UE_LOG(LogVolleyballRules, Log, TEXT("*** MATCH OVER ***"));
		return;
	}
	StartNextSet();
}

void ASpikeEliteGameMode::StartNextSet()
{
	CurrentSet++;
	TeamAScore = 0;
	TeamBScore = 0;
	PointsToWin = (CurrentSet >= 5) ? 15 : 25;
	UE_LOG(LogVolleyballRules, Log, TEXT("--- Set %d starting, to %d ---"), CurrentSet, PointsToWin);
	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 3.5f;  // 1.5 + 2.0 between sets
}

void ASpikeEliteGameMode::ServeNextBall()
{
	if (!Ball || !Court) return;
	float ServeX = (ServingTeam == EVolleyballTeam::TeamA)
		? Court->HalfCourtLength - 50.0f : -(Court->HalfCourtLength - 50.0f);
	Ball->ResetBall(FVector(ServeX, 0.0f, Court->NetHeight + 80.0f));
	FVector Dir = (ServingTeam == EVolleyballTeam::TeamA)
		? FVector(-0.878f, 0.0f, 0.479f) : FVector(0.878f, 0.0f, 0.479f);
	Ball->Strike(Dir, 1300.0f, 0.0f);
	MatchState = EMatchState::Playing;
}
