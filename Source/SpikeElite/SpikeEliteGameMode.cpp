// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "SpikeElitePlayerController.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballBall.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "UI/ScoreboardWidget.h"
#include "Blueprint/UserWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolleyballRules, Log, All);

TArray<FVector> ASpikeEliteGameMode::GetPositionsA()
{
	return {
		FVector(820.0f,   0.0f, 0.0f),
		FVector(550.0f, 300.0f, 0.0f),
		FVector(550.0f,   0.0f, 0.0f),
		FVector(550.0f,-300.0f, 0.0f),
		FVector(200.0f,-300.0f, 0.0f),
		FVector(200.0f,   0.0f, 0.0f),
	};
}

ASpikeEliteGameMode::ASpikeEliteGameMode()
{
	DefaultPawnClass = ASpikeEliteCharacter::StaticClass();
	PlayerControllerClass = ASpikeElitePlayerController::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ASpikeEliteGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Hide the auto-spawned pawn off-court until StartMatch.
	if (APawn* Human = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Human->SetActorLocation(FVector(0,0,-2000.f));
		Human->SetActorEnableCollision(false);
	}
}

void ASpikeEliteGameMode::StartMatch()
{
	if (bMatchActive) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// Reset scores/state.
	TeamAScore = TeamBScore = 0;
	TeamASetsWon = TeamBSetsWon = 0;
	CurrentSet = 1;
	PointsToWin = 25;
	MatchWinner = EVolleyballTeam::None;
	ServingTeam = EVolleyballTeam::TeamA;
	TeamAPlayers.Reset();
	TeamBPlayers.Reset();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Court = World->SpawnActor<AVolleyballCourt>(AVolleyballCourt::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Ball = World->SpawnActor<AVolleyballBall>(AVolleyballBall::StaticClass(), FVector(0,0,400), FRotator::ZeroRotator, Params);

	const TArray<FVector> PosA = GetPositionsA();
	TeamAPlayers.SetNum(6);
	TeamBPlayers.SetNum(6);

	if (APawn* Human = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ASpikeEliteCharacter* HC = Cast<ASpikeEliteCharacter>(Human))
		{
			HC->bIsBot = false;
			HC->TeamSide = 1;
			HC->HomePosition = PosA[0];
			HC->SetActorEnableCollision(true);
			HC->SetActorLocation(PosA[0] + FVector(0,0,100.0f));
			TeamAPlayers[0] = HC;
		}
	}
	for (int32 i = 1; i < 6; i++)
	{
		ASpikeEliteCharacter* Bot = World->SpawnActor<ASpikeEliteCharacter>(
			PosA[i] + FVector(0,0,100.0f), FRotator(0,-90,0), Params);
		if (Bot)
		{
			Bot->bIsBot = true; Bot->TeamSide = 1; Bot->HomePosition = PosA[i];
			TeamAPlayers[i] = Bot;
		}
	}
	for (int32 i = 0; i < 6; i++)
	{
		FVector BPos(-PosA[i].X, PosA[i].Y, 0.0f);
		ASpikeEliteCharacter* Bot = World->SpawnActor<ASpikeEliteCharacter>(
			BPos + FVector(0,0,100.0f), FRotator(0,90,0), Params);
		if (Bot)
		{
			Bot->bIsBot = true; Bot->TeamSide = -1; Bot->HomePosition = BPos;
			TeamBPlayers[i] = Bot;
		}
	}

	// Stadium light.
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(Params);
	if (Sun)
	{
		Sun->SetActorRotation(FRotator(-55.f,0.f,0.f));
		Sun->GetComponent()->SetIntensity(4.5f);
	}

	// Scoreboard.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		Scoreboard = CreateWidget<UScoreboardWidget>(PC, UScoreboardWidget::StaticClass());
		if (Scoreboard)
		{
			Scoreboard->AddToViewport(5);
			if (ASpikeElitePlayerController* SEPC = Cast<ASpikeElitePlayerController>(PC))
				SEPC->OnMatchStarted(Scoreboard);
		}
	}

	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 1.0f;
	bMatchActive = true;
	UE_LOG(LogVolleyballRules, Log, TEXT("=== Match started ==="));
}

void ASpikeEliteGameMode::CleanupMatch()
{
	if (Court) { Court->Destroy(); Court = nullptr; }
	if (Ball)  { Ball->Destroy(); Ball = nullptr; }
	for (auto& P : TeamAPlayers) if (P && P->bIsBot) P->Destroy();
	for (auto& P : TeamBPlayers) if (P) P->Destroy();
	TeamAPlayers.Reset();
	TeamBPlayers.Reset();
	if (Scoreboard) { Scoreboard->RemoveFromParent(); Scoreboard = nullptr; }
	// Remove leftover lights.
	TArray<AActor*> Lights;
	UGameplayStatics::GetAllActorsOfClass(this, ADirectionalLight::StaticClass(), Lights);
	for (AActor* L : Lights) L->Destroy();
	bMatchActive = false;
	MatchState = EMatchState::PreMatch;
}

void ASpikeEliteGameMode::ReturnToMainMenu()
{
	CleanupMatch();
	if (APawn* Human = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Human->SetActorEnableCollision(false);
		Human->SetActorLocation(FVector(0,0,-2000.f));
	}
}

void ASpikeEliteGameMode::RotateTeam(EVolleyballTeam TeamToRotate)
{
	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (TeamToRotate == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	if (Roster.Num() != 6) return;
	TObjectPtr<ASpikeEliteCharacter> OldP0 = Roster[0];
	for (int32 i = 0; i < 5; i++) Roster[i] = Roster[i+1];
	Roster[5] = OldP0;
	const TArray<FVector> PosA = GetPositionsA();
	for (int32 i = 0; i < 6; i++)
	{
		if (Roster[i])
		{
			FVector Home = (TeamToRotate == EVolleyballTeam::TeamA) ? PosA[i] : FVector(-PosA[i].X, PosA[i].Y, 0.f);
			Roster[i]->HomePosition = Home;
			if (!Roster[i]->bIsBot) Roster[i]->SetActorLocation(Home + FVector(0,0,100.f));
		}
	}
}

void ASpikeEliteGameMode::RespawnPlayersToPositions()
{
	const TArray<FVector> PosA = GetPositionsA();
	for (int32 i = 0; i < 6; i++)
	{
		if (TeamAPlayers[i]) { TeamAPlayers[i]->HomePosition = PosA[i]; TeamAPlayers[i]->SetActorLocation(PosA[i]+FVector(0,0,100.f)); }
		if (TeamBPlayers[i]) { FVector B(-PosA[i].X,PosA[i].Y,0); TeamBPlayers[i]->HomePosition = B; TeamBPlayers[i]->SetActorLocation(B+FVector(0,0,100.f)); }
	}
}

void ASpikeEliteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMatchActive) return;

	if (MatchState == EMatchState::BetweenRallies)
	{
		InterRallyTimer -= DeltaSeconds;
		if (InterRallyTimer <= 0.f) ServeNextBall();
	}

	if (bInToss && Ball)
	{
		TossTimer -= DeltaSeconds;
		if (TossTimer <= 0.f)
		{
			Ball->Strike(TossDir, TossPower, 0.f);
			bInToss = false;
			MatchState = EMatchState::Playing;
		}
	}

	// Net collision: only within the real net band (bottom 143cm to top 243cm) and width.
	if (MatchState == EMatchState::Playing && Ball && Court)
	{
		const FVector BL = Ball->GetActorLocation();
		const float NetTop = Court->NetHeight;          // 243
		const float NetBottom = NetTop - Court->NetBandHeight; // ~143
		const float HalfNetW = Court->HalfCourtWidth + Court->NetOverhang;
		if (FMath::Abs(BL.X) < 10.f && BL.Z < NetTop && BL.Z > NetBottom && FMath::Abs(BL.Y) < HalfNetW)
		{
			Ball->Strike(FVector(-Ball->GetVelocity().X, Ball->GetVelocity().Y*0.5f, 200.f).GetSafeNormal(), 400.f, 0.f);
		}
	}

	if (Scoreboard)
	{
		FString BallHint;
		if (Ball)
		{
			APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
			if (Player)
			{
				const FVector ToBall = Ball->GetActorLocation() - Player->GetActorLocation();
				const float Dist = ToBall.Size();
				if (Dist < 200.f) BallHint = TEXT("球在这里!");
				else
				{
					const float Yaw = FRotationMatrix::MakeFromX(ToBall).Rotator().Yaw - Player->GetControlRotation().Yaw;
					FString Dir;
					if      (Yaw > 45 && Yaw <= 135)  Dir = TEXT("球 << 左");
					else if (Yaw <= -45 && Yaw >= -135) Dir = TEXT("球 右 >>");
					else if (Yaw > 135 || Yaw < -135)   Dir = TEXT("球在身后");
					else                                   Dir = TEXT("球在前方");
					BallHint = FString::Printf(TEXT("%s  (%.0fm)"), *Dir, Dist/100.f);
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
	EVolleyballTeam ScoringTeam = (BallLocation.X >= 0.f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	AwardPoint(ScoringTeam);
}

void ASpikeEliteGameMode::AwardPoint(EVolleyballTeam ScoringTeam)
{
	if (ScoringTeam == EVolleyballTeam::TeamA) TeamAScore++; else TeamBScore++;
	const bool bWasServeWin = (ServingTeam == ScoringTeam);
	ServingTeam = ScoringTeam;
	if (!bWasServeWin) RotateTeam(ScoringTeam);
	UE_LOG(LogVolleyballRules, Log, TEXT("Point. A:%d B:%d"), TeamAScore, TeamBScore);
	CheckSetWin();
}

void ASpikeEliteGameMode::CheckSetWin()
{
	bool bA = TeamAScore >= PointsToWin && (TeamAScore-TeamBScore) >= 2;
	bool bB = TeamBScore >= PointsToWin && (TeamBScore-TeamAScore) >= 2;
	if (!bA && !bB) { MatchState = EMatchState::BetweenRallies; InterRallyTimer = 1.5f; return; }
	EVolleyballTeam W = bA ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	if (W == EVolleyballTeam::TeamA) TeamASetsWon++; else TeamBSetsWon++;
	if (TeamASetsWon >= 3 || TeamBSetsWon >= 3)
	{
		MatchWinner = (TeamASetsWon >= 3) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
		MatchState = EMatchState::MatchOver;
		return;
	}
	StartNextSet();
}

void ASpikeEliteGameMode::StartNextSet()
{
	CurrentSet++;
	TeamAScore = TeamBScore = 0;
	PointsToWin = (CurrentSet >= 5) ? 15 : 25;
	RespawnPlayersToPositions();
	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 3.5f;
}

void ASpikeEliteGameMode::ServeNextBall()
{
	if (!Ball || !Court) return;
	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (ServingTeam == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	FVector ServerPos = (Roster.Num() > 0 && Roster[0]) ? Roster[0]->GetActorLocation()
		: FVector(ServingTeam == EVolleyballTeam::TeamA ? 770.f : -770.f, 0.f, 0.f);
	Ball->ResetBall(ServerPos + FVector(0,0,180.f));
	TossDir = (ServingTeam == EVolleyballTeam::TeamA) ? FVector(-0.878f,0,0.479f) : FVector(0.878f,0,0.479f);
	TossPower = 1300.f;
	TossTimer = 0.6f;
	bInToss = true;
}
