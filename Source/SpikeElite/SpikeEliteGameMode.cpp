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

	// Count leftovers BEFORE spawning so a failed cleanup (duplicate court/ball/
	// bots from a previous match) is obvious in the log.
	LogActorCounts(TEXT("BeforeStart"));

	// Reset scores/state.
	TeamAScore = TeamBScore = 0;
	TeamASetsWon = TeamBSetsWon = 0;
	CurrentSet = 1;
	PointsToWin = 25;
	MatchWinner = EVolleyballTeam::None;
	ServingTeam = EVolleyballTeam::TeamA;
	TeamAPlayers.Reset();
	TeamBPlayers.Reset();

	// Reset rally timers / flags so a stale timer from a previous match cannot
	// fire the moment the new match starts.
	bInToss = false;
	TossTimer = 0.0f;
	InterRallyTimer = 0.0f;
	AIHitCooldown = 0.0f;
	NetTouchCooldown = 0.0f;
	BallPrevX = 0.0f;

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
	// Bots are spawned deferred so TeamSide/bIsBot/HomePosition are set BEFORE
	// BeginPlay runs ApplyJerseyColor(); otherwise every bot would keep the
	// default Team A jersey colour.
	auto SpawnBot = [&](const FVector& Loc, const FRotator& Rot, int32 Side, const FVector& Home) -> ASpikeEliteCharacter*
	{
		ASpikeEliteCharacter* Bot = World->SpawnActorDeferred<ASpikeEliteCharacter>(
			ASpikeEliteCharacter::StaticClass(),
			FTransform(Rot.Quaternion(), Loc, FVector(1.f)),
			nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Bot)
		{
			Bot->bIsBot = true;
			Bot->TeamSide = Side;
			Bot->HomePosition = Home;
			UGameplayStatics::FinishSpawningActor(Bot, FTransform(Rot.Quaternion(), Loc, FVector(1.f)));
		}
		return Bot;
	};

	for (int32 i = 1; i < 6; i++)
	{
		TeamAPlayers[i] = SpawnBot(PosA[i] + FVector(0,0,100.0f), FRotator(0,-90,0), 1, PosA[i]);
	}
	for (int32 i = 0; i < 6; i++)
	{
		const FVector BPos(-PosA[i].X, PosA[i].Y, 0.0f);
		TeamBPlayers[i] = SpawnBot(BPos + FVector(0,0,100.0f), FRotator(0,90,0), -1, BPos);
	}

	// Indoor arena lighting is owned by the Court actor (and destroyed with it),
	// so we never spawn or delete level lights here.

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
	LogActorCounts(TEXT("StartMatch"));
}

void ASpikeEliteGameMode::LogActorCounts(const TCHAR* Tag) const
{
	auto Count = [this](UClass* Class) -> int32
	{
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), Class, Actors);
		return Actors.Num();
	};
	UE_LOG(LogVolleyballRules, Log,
		TEXT("[%s] Court=%d Ball=%d Characters=%d"),
		Tag, Count(AVolleyballCourt::StaticClass()), Count(AVolleyballBall::StaticClass()),
		Count(ASpikeEliteCharacter::StaticClass()));
}

void ASpikeEliteGameMode::CleanupMatch()
{
	// Human player (roster slot 0 of Team A) is kept and parked; only bots die.
	if (Court) { Court->Destroy(); Court = nullptr; }
	if (Ball)  { Ball->Destroy(); Ball = nullptr; }
	for (auto& P : TeamAPlayers) if (P && P->bIsBot) P->Destroy();
	for (auto& P : TeamBPlayers) if (P) P->Destroy();
	TeamAPlayers.Reset();
	TeamBPlayers.Reset();
	if (Scoreboard) { Scoreboard->RemoveFromParent(); Scoreboard = nullptr; }

	// IMPORTANT: do NOT delete all DirectionalLights / SkyLights — that would
	// remove lights owned by the loaded level. Match lighting lives on the Court
	// and was destroyed above.

	// Reset every piece of transient rally/match state.
	bInToss = false;
	TossTimer = 0.0f;
	InterRallyTimer = 0.0f;
	AIHitCooldown = 0.0f;
	NetTouchCooldown = 0.0f;
	BallPrevX = 0.0f;
	bMatchActive = false;
	MatchState = EMatchState::PreMatch;

	// Clear any pending timers (serve toss / inter-rally) from this match.
	GetWorldTimerManager().ClearAllTimersForObject(this);

	UE_LOG(LogVolleyballRules, Log, TEXT("Match cleaned up; level lights untouched"));
	LogActorCounts(TEXT("Cleanup"));
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

	// ---- Single-authority net collision (the visual net has NO physics) ----
	// Detect the ball crossing the net plane while inside the real band
	// (bottom 143 to top 243) and within net width; deflect it back once with a
	// cooldown so it cannot be struck every frame. Ball below 143 or outside the
	// net width passes freely under/around the net.
	if (NetTouchCooldown > 0.f) NetTouchCooldown -= DeltaSeconds;
	if (MatchState == EMatchState::Playing && Ball && Court)
	{
		const FVector BL = Ball->GetActorLocation();
		const FVector BV = Ball->GetVelocity();
		const float NetTop = Court->NetHeight;                 // 243
		const float NetBottom = NetTop - Court->NetBandHeight; // 143
		const float HalfNetW = Court->HalfCourtWidth + Court->NetOverhang; // ~530

		const bool bCrossedPlane = (BallPrevX * BL.X < 0.f) || FMath::Abs(BL.X) < 12.f;
		const bool bInBand = (BL.Z < NetTop && BL.Z > NetBottom);
		const bool bInWidth = FMath::Abs(BL.Y) < HalfNetW;
		const bool bMovingAcross = FMath::Abs(BV.X) > 20.f;

		if (bCrossedPlane && bInBand && bInWidth && bMovingAcross && NetTouchCooldown <= 0.f)
		{
			// Rebound back toward the side it came from, damped, with a little rise.
			const float ReboundSpeed = FMath::Clamp(BV.Size() * 0.55f, 260.f, 720.f);
			FVector Rebound(-BV.X * 0.6f, BV.Y * 0.4f, FMath::Max(BV.Z * 0.3f, 0.f) + 170.f);
			Ball->Strike(Rebound.GetSafeNormal(), ReboundSpeed, 0.f);
			NetTouchCooldown = 0.45f;
			// A net tap does NOT change the last touching team.
			UE_LOG(LogVolleyballRules, Log, TEXT("Net touch at Z=%.0f -> rebound (last touch unchanged)"), BL.Z);
		}
		BallPrevX = BL.X;
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

	// FIVB court: |X| <= 900 (end lines), |Y| <= 450 (side lines). Lines are in.
	const bool bIn = FMath::Abs(BallLocation.X) <= 900.f
	              && FMath::Abs(BallLocation.Y) <= 450.f;

	const EVolleyballTeam Last = Ball ? Ball->GetLastHitTeam() : EVolleyballTeam::None;
	EVolleyballTeam ScoringTeam;

	if (bIn)
	{
		// Landed in a court: the side defending that half loses the rally.
		// X > 0 is Team A's half, so Team B scores (and vice versa).
		ScoringTeam = (BallLocation.X >= 0.f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	}
	else
	{
		// Landed out: the opponent of the last touching team scores.
		if (Last == EVolleyballTeam::TeamA)      ScoringTeam = EVolleyballTeam::TeamB;
		else if (Last == EVolleyballTeam::TeamB) ScoringTeam = EVolleyballTeam::TeamA;
		else                                     ScoringTeam = (BallLocation.X >= 0.f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	}

	UE_LOG(LogVolleyballRules, Log,
		TEXT("Ball landed %s at (%.0f, %.0f, %.0f) | LastTouch=%s | Scoring=%s"),
		bIn ? TEXT("IN") : TEXT("OUT"),
		BallLocation.X, BallLocation.Y, BallLocation.Z,
		Last == EVolleyballTeam::TeamA ? TEXT("A") : Last == EVolleyballTeam::TeamB ? TEXT("B") : TEXT("None"),
		ScoringTeam == EVolleyballTeam::TeamA ? TEXT("A") : TEXT("B"));

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
	Ball->SetLastHitTeam(ServingTeam);   // serve counts as the serving team's touch
	TossDir = (ServingTeam == EVolleyballTeam::TeamA) ? FVector(-0.878f,0,0.479f) : FVector(0.878f,0,0.479f);
	TossPower = 1300.f;
	TossTimer = 0.6f;
	bInToss = true;
	// Leave BetweenRallies immediately: otherwise the Tick's BetweenRallies
	// branch re-calls ServeNextBall every frame, resetting the toss timer so the
	// ball is frozen at the serve spot and is never struck.
	MatchState = EMatchState::Playing;
}
