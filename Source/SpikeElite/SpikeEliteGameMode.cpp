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
#include "Engine/DirectionalLight.h"
#include "Engine/SpotLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "UI/ScoreboardWidget.h"
#include "Blueprint/UserWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolleyballRules, Log, All);

TArray<FVector> ASpikeEliteGameMode::GetPositionsA()
{
	// FIVB positions 1..6 in cm on Team A's half (X>0).
	// 1 = server (back-right), 2 = front-right, 3 = front-middle, 4 = front-left,
	// 5 = back-left, 6 = back-middle.
	return {
		FVector(820.0f,   0.0f, 0.0f),   // 1
		FVector(550.0f, 300.0f, 0.0f),   // 2
		FVector(550.0f,   0.0f, 0.0f),   // 3
		FVector(550.0f,-300.0f, 0.0f),   // 4
		FVector(200.0f,-300.0f, 0.0f),   // 5
		FVector(200.0f,   0.0f, 0.0f),   // 6
	};
}

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

	const TArray<FVector> PosA = GetPositionsA();

	// --- Team A roster: index 0 = position 1 (server). ---
	TeamAPlayers.SetNum(6);
	TeamBPlayers.SetNum(6);

	// Human takes Team A position 1.
	if (APawn* Human = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ASpikeEliteCharacter* HC = Cast<ASpikeEliteCharacter>(Human))
		{
			HC->bIsBot = false;
			HC->TeamSide = 1;
			HC->HomePosition = PosA[0];
			HC->SetActorLocation(PosA[0] + FVector(0, 0, 100.0f));
			TeamAPlayers[0] = HC;
		}
	}
	for (int32 i = 1; i < 6; i++)
	{
		ASpikeEliteCharacter* Bot = World->SpawnActor<ASpikeEliteCharacter>(
			PosA[i] + FVector(0, 0, 100.0f), FRotator(0, -90, 0), Params);
		if (Bot)
		{
			Bot->bIsBot = true;
			Bot->TeamSide = 1;
			Bot->HomePosition = PosA[i];
			TeamAPlayers[i] = Bot;
		}
	}

	// Team B: mirror X<0.
	for (int32 i = 0; i < 6; i++)
	{
		FVector BPos(-PosA[i].X, PosA[i].Y, 0.0f);
		ASpikeEliteCharacter* Bot = World->SpawnActor<ASpikeEliteCharacter>(
			BPos + FVector(0, 0, 100.0f), FRotator(0, 90, 0), Params);
		if (Bot)
		{
			Bot->bIsBot = true;
			Bot->TeamSide = -1;
			Bot->HomePosition = BPos;
			TeamBPlayers[i] = Bot;
		}
	}

	// --- Lighting: overhead sports light so the court is readable. ---
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(Params);
	if (Sun)
	{
		Sun->SetActorRotation(FRotator(-55.0f, 0.0f, 0.0f));
		Sun->GetComponent()->SetIntensity(4.5f);
	}

	// --- Scoreboard UI. ---
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		Scoreboard = CreateWidget<UScoreboardWidget>(PC, UScoreboardWidget::StaticClass());
		if (Scoreboard) Scoreboard->AddToViewport();
	}

	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 1.0f;
	ServingTeam = EVolleyballTeam::TeamA;

	UE_LOG(LogVolleyballRules, Log, TEXT("=== SPIKE ELITE 6v6 match start. ==="));
}

void ASpikeEliteGameMode::RotateTeam(EVolleyballTeam TeamToRotate)
{
	// FIVB §7.4: on side-out, each player moves to the next clockwise position.
	// Position array [1..6]: new[0]=old[1], new[1]=old[2], ..., new[5]=old[0].
	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (TeamToRotate == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	if (Roster.Num() != 6) return;

	TObjectPtr<ASpikeEliteCharacter> OldP0 = Roster[0];
	for (int32 i = 0; i < 5; i++) Roster[i] = Roster[i+1];
	Roster[5] = OldP0;

	// Reassign home positions.
	const TArray<FVector> PosA = GetPositionsA();
	for (int32 i = 0; i < 6; i++)
	{
		if (Roster[i])
		{
			FVector Home = (TeamToRotate == EVolleyballTeam::TeamA) ? PosA[i] : FVector(-PosA[i].X, PosA[i].Y, 0.0f);
			Roster[i]->HomePosition = Home;
			// Slide the human gently; teleport bots (they were out of position anyway).
			if (!Roster[i]->bIsBot)
			{
				Roster[i]->SetActorLocation(Home + FVector(0, 0, 100.0f));
			}
		}
	}
	UE_LOG(LogVolleyballRules, Log, TEXT("Team rotated. New server is position 1."));
}

void ASpikeEliteGameMode::RespawnPlayersToPositions()
{
	const TArray<FVector> PosA = GetPositionsA();
	for (int32 i = 0; i < 6; i++)
	{
		if (TeamAPlayers[i])
		{
			TeamAPlayers[i]->HomePosition = PosA[i];
			TeamAPlayers[i]->SetActorLocation(PosA[i] + FVector(0,0,100.0f));
		}
		if (TeamBPlayers[i])
		{
			FVector BPos(-PosA[i].X, PosA[i].Y, 0.0f);
			TeamBPlayers[i]->HomePosition = BPos;
			TeamBPlayers[i]->SetActorLocation(BPos + FVector(0,0,100.0f));
		}
	}
}

void ASpikeEliteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MatchState == EMatchState::BetweenRallies)
	{
		InterRallyTimer -= DeltaSeconds;
		if (InterRallyTimer <= 0.0f) ServeNextBall();
	}

	// Serve toss: ball hangs in the air briefly, then gets struck.
	if (bInToss && Ball)
	{
		TossTimer -= DeltaSeconds;
		if (TossTimer <= 0.0f)
		{
			Ball->Strike(TossDir, TossPower, 0.0f);
			bInToss = false;
			MatchState = EMatchState::Playing;
		}
	}

	// Net touch bounce.
	if (MatchState == EMatchState::Playing && Ball)
	{
		const FVector BL = Ball->GetActorLocation();
		const float NetH = Court ? Court->NetHeight : 243.0f;
		if (FMath::Abs(BL.X) < 12.0f && BL.Z < NetH && BL.Z > 20.0f)
		{
			Ball->Strike(FVector(-Ball->GetVelocity().X, Ball->GetVelocity().Y * 0.5f, 200.0f).GetSafeNormal(), 400.0f, 0.0f);
		}
	}

	if (Scoreboard)
	{
		FString BallHint = TEXT("");
		if (Ball)
		{
			APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
			if (Player)
			{
				const FVector ToBall = Ball->GetActorLocation() - Player->GetActorLocation();
				const float Dist = ToBall.Size();
				if (Dist < 200.0f) BallHint = TEXT("Ball HERE!");
				else
				{
					const float Yaw = FRotationMatrix::MakeFromX(ToBall).Rotator().Yaw - Player->GetControlRotation().Yaw;
					FString Dir;
					if      (Yaw > 45 && Yaw <= 135)  Dir = TEXT("Ball << LEFT");
					else if (Yaw <= -45 && Yaw >= -135) Dir = TEXT("Ball RIGHT >>");
					else if (Yaw > 135 || Yaw < -135)   Dir = TEXT("Ball BEHIND");
					else                                   Dir = TEXT("Ball FRONT");
					BallHint = FString::Printf(TEXT("%s  (%.0fm)"), *Dir, Dist / 100.0f);
				}
			}
		}
		Scoreboard->UpdateScore(CurrentSet, TeamAScore, TeamBScore, TeamASetsWon, TeamBSetsWon,
			ServingTeam == EVolleyballTeam::TeamA, BallHint);
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
	// FIVB: the rally winner serves next. If the winner did NOT serve the previous rally
	// (i.e. side-out), the winning team rotates clockwise first.
	const bool bWasServeWin = (ServingTeam == ScoringTeam);
	ServingTeam = ScoringTeam;
	if (!bWasServeWin)
	{
		RotateTeam(ScoringTeam);
	}
	UE_LOG(LogVolleyballRules, Log, TEXT("Rally point. Score A:%d B:%d"), TeamAScore, TeamBScore);
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
	// Reset both teams to their starting positions for the new set.
	RespawnPlayersToPositions();
	UE_LOG(LogVolleyballRules, Log, TEXT("--- Set %d starting, to %d ---"), CurrentSet, PointsToWin);
	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 3.5f;
}

void ASpikeEliteGameMode::ServeNextBall()
{
	if (!Ball || !Court) return;
	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (ServingTeam == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	FVector ServerPos = (Roster.Num() > 0 && Roster[0]) ? Roster[0]->GetActorLocation()
		: FVector(ServingTeam == EVolleyballTeam::TeamA ? 770.0f : -770.0f, 0.0f, 0.0f);

	Ball->ResetBall(ServerPos + FVector(0.0f, 0.0f, 180.0f));
	TossDir = (ServingTeam == EVolleyballTeam::TeamA)
		? FVector(-0.878f, 0.0f, 0.479f) : FVector(0.878f, 0.0f, 0.479f);
	TossPower = 1300.0f;
	TossTimer = 0.6f;   // 0.6 s toss before the serve
	bInToss = true;
	// MatchState stays BetweenRallies until the ball is struck.
}
