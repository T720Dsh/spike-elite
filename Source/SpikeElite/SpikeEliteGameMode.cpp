// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"

// M11b-5c dive-window state (GameMode-owned; reset in StartMatch).
static float GM_NetCrossWindow = 0.f;
static float GM_NetCrossSpeed = 0.f;
static float GM_PrevBallX = 0.f;
#include "SpikeEliteCharacter.h"
#include "SpikeElitePlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballArena.h"
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
#include "UI/RotationWidget.h"
#include "Volleyball/MatchOfficialManager.h"
#include "Blueprint/UserWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolleyballRules, Log, All);

namespace
{
	/** Map the current touch count to its phase label (Receive -> Set -> Attack). */
	EBallTouchType ResolveTouchType(int32 TouchCount)
	{
		switch (TouchCount)
		{
		case 0:  return EBallTouchType::Receive;
		case 1:  return EBallTouchType::Set;
		case 2:  return EBallTouchType::Attack;
		default: return EBallTouchType::Unknown;
		}
	}

	const TCHAR* TeamStr(EVolleyballTeam Team)
	{
		if (Team == EVolleyballTeam::TeamA) return TEXT("A");
		if (Team == EVolleyballTeam::TeamB) return TEXT("B");
		return TEXT("-");
	}
}

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

	// -QuickMatch: one set to 3 points, but reuses the real rule code.
	bQuickMatch = FParse::Param(FCommandLine::Get(), TEXT("QuickMatch"));

	// M11: -devauto (non-Shipping) lets the automation path serve for a human
	// after a timeout; without it a human server always waits for the E key.
#if !UE_BUILD_SHIPPING
	bDevAuto = FParse::Param(FCommandLine::Get(), TEXT("devauto"));
#endif
	UE_LOG(LogVolleyballRules, Log, TEXT("devauto=%d QuickMatch=%d"), bDevAuto ? 1 : 0, bQuickMatch ? 1 : 0);

	// M11b-2: -FastFlow shortens the post-rally ceremony (result display,
	// readiness check and serve deadline). Automated QuickMatch runs use it;
	// normal play keeps the official pacing.
	bFastFlow = FParse::Param(FCommandLine::Get(), TEXT("FastFlow"));
	if (bFastFlow)
	{
		InterRallyDelay = 0.35f;
		SetOverDelay = 0.8f;
		ResetDelay = 0.06f;
		ReadyDelay = 0.12f;
		ServeDeadline = 2.0f;
		UE_LOG(LogVolleyballRules, Log, TEXT("FastFlow=1 ceremony timings shortened"));
	}

	// Seeded AI randomness: -Seed=N makes automated runs reproducible.
	int32 Seed = FDateTime::Now().GetTicks() % 1000000;
	int32 SeedArg = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SEED="), SeedArg)) { Seed = SeedArg; }
	AIStream.Initialize(Seed);
	UE_LOG(LogVolleyballRules, Log, TEXT("AIStream seed=%d QuickMatch=%d"), Seed, bQuickMatch ? 1 : 0);
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
	// M10: "再来一场" re-enters here while a match is active -> full cleanup first.
	if (bMatchActive) { CleanupMatch(); }
	UWorld* World = GetWorld();
	if (!World) return;

	// Count leftovers BEFORE spawning so a failed cleanup (duplicate court/ball/
	// bots from a previous match) is obvious in the log.
	LogActorCounts(TEXT("BeforeStart"));

	// Reset scores/state.
	TeamAScore = TeamBScore = 0;
	TeamASetsWon = TeamBSetsWon = 0;
	CurrentSet = 1;
	PointsToWin = bQuickMatch ? 3 : SEVolleyballRules::PointsToWinForSet(CurrentSet);
	MatchWinsNeeded = bQuickMatch ? 1 : 3;
	MatchWinner = EVolleyballTeam::None;
	ServingTeam = EVolleyballTeam::TeamA;
	TeamAPlayers.Reset();
	TeamBPlayers.Reset();
	SetScoresA.Reset();
	SetScoresB.Reset();
	SetScoresA.Add(0);
	SetScoresB.Add(0);

	// Reset rally timers / flags so a stale timer from a previous match cannot
	// fire the moment the new match starts.
	bInToss = false;
	TossTimer = 0.0f;
	InterRallyTimer = 0.0f;
	NetTouchCooldown = 0.0f;
	BallPrevX = 0.0f;
	bNetContactLatched = false;
	RallyResultText.Empty();
	RallyResultDisplayTimer = 0.0f;
	SEVolleyballRules::BeginRally(RallyState, ServingTeam);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// M11b-1: arena shell + court are persistent. They are created once and are
	// never destroyed by CleanupMatch, so Rematch cannot duplicate them.
	if (!Arena)
	{
		Arena = World->SpawnActor<AVolleyballArena>(AVolleyballArena::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (!Court)
	{
		Court = World->SpawnActor<AVolleyballCourt>(AVolleyballCourt::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (!Ball)
	{
		Ball = World->SpawnActor<AVolleyballBall>(AVolleyballBall::StaticClass(), FVector(0,0,400), FRotator::ZeroRotator, Params);
	}
	else
	{
		// Reuse the persistent ball: park it until the serve toss places it.
		Ball->ResetBall(FVector(0, 0, 400));
	}

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
			HC->PlayerId = 0;
			HC->JerseyNumber = 1;
			HC->SetActorEnableCollision(true);
			HC->SetActorLocation(PosA[0] + FVector(0,0,100.0f));
			HC->bTouchArmed = true;
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
			Bot->bTouchArmed = true;
			UGameplayStatics::FinishSpawningActor(Bot, FTransform(Rot.Quaternion(), Loc, FVector(1.f)));
			// Deferred-spawned Characters can come up with MovementMode=None
			// (no simulation). Force walking so the GameMode's directives move them.
			if (UCharacterMovementComponent* MC = Bot->GetCharacterMovement())
			{
				MC->SetMovementMode(MOVE_Walking);
			}
		}
		return Bot;
	};

	for (int32 i = 1; i < 6; i++)
	{
		TeamAPlayers[i] = SpawnBot(PosA[i] + FVector(0,0,100.0f), FRotator(0,-90,0), 1, PosA[i]);
		if (TeamAPlayers[i]) { TeamAPlayers[i]->PlayerId = i; TeamAPlayers[i]->JerseyNumber = i + 1; }
	}
	for (int32 i = 0; i < 6; i++)
	{
		const FVector BPos(-PosA[i].X, PosA[i].Y, 0.0f);
		TeamBPlayers[i] = SpawnBot(BPos + FVector(0,0,100.0f), FRotator(0,90,0), -1, BPos);
		if (TeamBPlayers[i]) { TeamBPlayers[i]->PlayerId = 6 + i; TeamBPlayers[i]->JerseyNumber = 7 + i; }
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

		// M11b-2: persistent match officials + rotation HUD (created once; a
		// Rematch reuses them instead of respawning referee stands or benches).
		if (!Officials)
		{
			Officials = World->SpawnActor<AMatchOfficialManager>(
				AMatchOfficialManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		}
		if (!RotationWidget)
		{
			RotationWidget = CreateWidget<URotationWidget>(PC, URotationWidget::StaticClass());
			if (RotationWidget) { RotationWidget->AddToViewport(20); }
		}
	}

	RotationCount = 1;
	RefreshRotationView();
	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = 1.0f;
	bMatchActive = true;
	UE_LOG(LogVolleyballRules, Log, TEXT("=== Match started (QuickMatch=%d, PointsToWin=%d, MatchWinsNeeded=%d) ==="),
		bQuickMatch ? 1 : 0, PointsToWin, MatchWinsNeeded);
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
	// M11b-1: arena shell, court and ball are persistent across matches and are
	// deliberately NOT destroyed here (Rematch reuses them; counts must stay
	// Court=1 Ball=1 Arena=1). Only bots, the scoreboard and transient state go.
	if (Ball) { Ball->ResetBall(FVector(0, 0, 400)); }
	for (auto& P : TeamAPlayers) if (P && P->bIsBot) P->Destroy();
	for (auto& P : TeamBPlayers) if (P) P->Destroy();
	TeamAPlayers.Reset();
	TeamBPlayers.Reset();
	if (Scoreboard) { Scoreboard->RemoveFromParent(); Scoreboard = nullptr; }

	// M11: any un-settled rally at cleanup is explicitly cancelled (no point).
	// This keeps ERallyEndReason::Cancelled a real, exercised state instead of a
	// dead enum value.
	if (!RallyState.bRallySettled)
	{
		SEVolleyballRules::SettleRally(RallyState);
		UE_LOG(LogVolleyballRules, Log,
			TEXT("[RallyEnd] reason=Cancelled scoring=- lastTouch=%s touchCount=%d A:%d B:%d set=%d"),
			TeamStr(RallyState.LastTouchTeam), RallyState.TouchCount,
			TeamAScore, TeamBScore, CurrentSet);
	}

	// Reset every piece of transient rally/match state.
	bInToss = false;
	TossTimer = 0.0f;
	InterRallyTimer = 0.0f;
	NetTouchCooldown = 0.0f;
	BallPrevX = 0.0f;
	bNetContactLatched = false;
	bMatchActive = false;
	MatchState = EMatchState::PreMatch;
	RallyResultText.Empty();
	RallyResultDisplayTimer = 0.0f;
	SEVolleyballRules::BeginRally(RallyState, EVolleyballTeam::TeamA);
	SetScoresA.Reset();
	SetScoresB.Reset();

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

	// M11: reuse the ONE tested core rotation (RotateRoster). Production and the
	// automation tests can no longer diverge on side-out order.
	TArray<int32> Order;
	for (int32 i = 0; i < Roster.Num(); i++) { Order.Add(i); }
	SEVolleyballRules::RotateRoster(Order);
	TArray<TObjectPtr<ASpikeEliteCharacter>> NewRoster;
	NewRoster.SetNum(6);
	for (int32 i = 0; i < 6; i++) { NewRoster[i] = Roster[Order[i]]; }
	Roster = MoveTemp(NewRoster);

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

void ASpikeEliteGameMode::ShowBanner(const FString& Text, float Seconds)
{
	RallyResultText = Text;
	RallyResultDisplayTimer = Seconds;
}

void ASpikeEliteGameMode::SetRallyResult(ERallyEndReason Reason, EVolleyballTeam ScoringTeam)
{
	const FString Text = FString::Printf(TEXT("%s · %s 队得分"),
		*SEVolleyballRules::RallyReasonLabel(Reason), TeamStr(ScoringTeam));
	ShowBanner(Text, 1.5f);
}

void ASpikeEliteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMatchActive || !Ball || !Court) return;

	// M11b-5c: detect a fast ball crossing the net into the opposing side and
	// keep a short dive window for the defending team (resets on any touch by
	// TryTouchBall/DoTouch via EndRally... touch itself is handled elsewhere).
	if (MatchState == EMatchState::Rally)
	{
		const float BallX = Ball->GetActorLocation().X;
		if (GM_PrevBallX * BallX < 0.f && FMath::Abs(BallX) > 60.f)
		{
			GM_NetCrossWindow = 0.6f;
			GM_NetCrossSpeed = Ball->GetVelocity().Size2D();
			// M11b-5c: a fast ball that just entered our half triggers one dive
			// by the closest receiver (the same coordinator as regular receive).
			if (GM_NetCrossSpeed > 600.f)
			{
				const EVolleyballTeam DefTeam = (BallX < 0.f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
				const int32 Recv = SelectReceivePlayer(DefTeam, PredictBallLanding());
				ASpikeEliteCharacter* R = nullptr;
				if (DefTeam == EVolleyballTeam::TeamA) { R = TeamAPlayers.IsValidIndex(Recv) ? TeamAPlayers[Recv].Get() : nullptr; }
				else { R = TeamBPlayers.IsValidIndex(Recv) ? TeamBPlayers[Recv].Get() : nullptr; }
				if (R && R->bIsBot && !R->bDiveRecovering)
				{
					SetAIDirective(R, EAIBehavior::Dive, PredictBallLanding(), true);
					UE_LOG(LogVolleyballRules, Log, TEXT("[Dive] %s dives (net cross %.0f cm/s)"), *R->GetName(), GM_NetCrossSpeed);
				}
			}
		}
		GM_PrevBallX = BallX;
		if (GM_NetCrossWindow > 0.f)
		{
			GM_NetCrossWindow -= DeltaSeconds;
		}
	}

	switch (MatchState)
	{
	case EMatchState::BetweenRallies:
	{
		InterRallyTimer -= DeltaSeconds;
		if (InterRallyTimer <= 0.f) { AdvanceRallyPhase(); }
		break;
	}
	case EMatchState::ResettingPositions:
	{
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			MatchState = EMatchState::AwaitingReady;
			PhaseTimer = ReadyDelay;
			UE_LOG(LogVolleyballRules, Log, TEXT("Readiness check: 2nd referee verifying formation"));
			UpdateScoreboard();
		}
		break;
	}
	case EMatchState::AwaitingReady:
	{
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f) { BeginServiceAuthorized(); }
		break;
	}
	case EMatchState::ServiceAuthorized:
	{
		// 8 s serve window (FIVB); running out is a serve fault.
		ServeDeadlineTimer -= DeltaSeconds;
		if (ServeDeadlineTimer <= 0.f) { HandleServeDeadline(); break; }

		// A bot server (or Team A's bot when the human rotated away) serves
		// automatically after a short pause; the human waits for the E key
		// (or -devauto serves for them after its own short delay).
		if (bAIServePending)
		{
			AIServeTimer -= DeltaSeconds;
			if (AIServeTimer <= 0.f)
			{
				bAIServePending = false;
				TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (ServingTeam == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
				if (Roster.Num() > 0) { RequestServe(Roster[0].Get()); }
			}
		}
		break;
	}
	case EMatchState::ServingToss:
	{
		if (bInToss)
		{
			TossTimer -= DeltaSeconds;
			if (TossTimer <= 0.f) { ExecuteServe(); }
		}
		break;
	}
	case EMatchState::Rally:
	{
		// ---- Single-authority net collision (the visual net has NO physics) ----
		// Detect the ball crossing the net plane while inside the real band
		// (bottom 143 to top 243) and within net width; deflect it back once with a
		// cooldown so it cannot be struck every frame.
		if (NetTouchCooldown > 0.f) { NetTouchCooldown -= DeltaSeconds; }

		const FVector BL = Ball->GetActorLocation();
		const FVector BV = Ball->GetVelocity();
		const float NetTop = Court->NetHeight;                 // 243
		const float NetBottom = NetTop - Court->NetBandHeight; // 143
		const float HalfNetW = Court->HalfCourtWidth + Court->NetOverhang; // ~530

		constexpr float NetSlabHalfDepth = 14.f;
		const bool bEnteredFromPositive = BallPrevX > NetSlabHalfDepth && BL.X <= NetSlabHalfDepth;
		const bool bEnteredFromNegative = BallPrevX < -NetSlabHalfDepth && BL.X >= -NetSlabHalfDepth;
		const bool bEnteredSlab = bEnteredFromPositive || bEnteredFromNegative;
		const bool bInBand = (BL.Z < NetTop && BL.Z > NetBottom);
		const bool bInWidth = FMath::Abs(BL.Y) < HalfNetW;
		const bool bMovingAcross = FMath::Abs(BV.X) > 20.f;

		if (FMath::Abs(BL.X) > NetSlabHalfDepth + 8.f)
		{
			bNetContactLatched = false;
		}

		if (bEnteredSlab && !bNetContactLatched && bInBand && bInWidth && bMovingAcross && NetTouchCooldown <= 0.f)
		{
			bNetContactLatched = true;
			// Rebound back toward the side it came from, damped, with a little rise.
			const float ReboundSpeed = FMath::Clamp(BV.Size() * 0.55f, 260.f, 720.f);
			FVector Rebound(-BV.X * 0.6f, BV.Y * 0.4f, FMath::Max(BV.Z * 0.3f, 0.f) + 170.f);
			Ball->Strike(Rebound.GetSafeNormal(), ReboundSpeed, 0.f);
			NetTouchCooldown = 0.45f;
			// A net tap does NOT change the last touching team.
			UE_LOG(LogVolleyballRules, Log, TEXT("Net touch at Z=%.0f -> rebound (last touch unchanged)"), BL.Z);
		}

		// ---- Legal net crossing (above the net): switch possession ----
		const bool bCrossedToNegative = BallPrevX > 0.f && BL.X <= 0.f;
		const bool bCrossedToPositive = BallPrevX < 0.f && BL.X >= 0.f;
		if ((bCrossedToNegative || bCrossedToPositive) && BL.Z > NetTop)
		{
			OnBallCrossedNet();
		}
		BallPrevX = BL.X;

		// ---- Rearm touch protection once the ball leaves reach ----
		RearmTouchers();

		break;
	}
	case EMatchState::SetOver:
	{
		InterRallyTimer -= DeltaSeconds;
		if (InterRallyTimer <= 0.f) { StartNextSet(); }
		break;
	}
	default:
		break;
	}

	// Rally-result banner fades after its window (unpaused timer is fine: the
	// world is not paused during rallies; banners also expire between rallies).
	if (RallyResultDisplayTimer > 0.f)
	{
		RallyResultDisplayTimer -= DeltaSeconds;
		if (RallyResultDisplayTimer <= 0.f) { RallyResultText.Empty(); }
	}

	// ---- M11 frequency control ----
	// AI tactical re-selection at ~12.5 Hz; bot movement stays per-frame in the
	// characters' own Tick. Scoreboard dynamic refresh (ball hint) at ~6.7 Hz;
	// static fields (score/set/phase/possession/touches) still push immediately
	// from their change sites via direct UpdateScoreboard() calls.
	AIDirectiveTimer -= DeltaSeconds;
	if (AIDirectiveTimer <= 0.f)
	{
		AIDirectiveTimer = 0.08f;
		UpdateAIDirectives(DeltaSeconds);
	}

	ScoreboardUpdateTimer -= DeltaSeconds;
	if (ScoreboardUpdateTimer <= 0.f)
	{
		ScoreboardUpdateTimer = 0.15f;
		UpdateScoreboard();
	}
}

void ASpikeEliteGameMode::RearmTouchers()
{
	if (!Ball) return;
	const FVector BL = Ball->GetActorLocation();
	auto Rearm = [&](TObjectPtr<ASpikeEliteCharacter>& C)
	{
		if (C && !C->bTouchArmed && FVector::Dist(C->GetActorLocation(), BL) > TouchReach + 40.f)
		{
			C->bTouchArmed = true;
		}
	};
	for (auto& C : TeamAPlayers) Rearm(C);
	for (auto& C : TeamBPlayers) Rearm(C);
}

void ASpikeEliteGameMode::UpdateScoreboard()
{
	if (!Scoreboard) return;

	// Ball direction/distance is dynamic: recompute on every call (throttled to
	// ~6.7 Hz from Tick; immediate at state-change sites). Cheap FString only.
	FString BallHint;
	if (Ball)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player)
		{
			const FVector ToBall = Ball->GetActorLocation() - Player->GetActorLocation();
			const float Dist = ToBall.Size();
			if (Dist < TouchReach)
			{
				BallHint = TEXT("球在触球范围！");
			}
			else
			{
				// FMath::FindDeltaAngleDegrees avoids the ±180° wrap bug.
				const float Yaw = FMath::FindDeltaAngleDegrees(
					FRotationMatrix::MakeFromX(ToBall).Rotator().Yaw, Player->GetControlRotation().Yaw);
				FString Dir;
				if      (Yaw > 45 && Yaw <= 135)       Dir = TEXT("球 << 左");
				else if (Yaw <= -45 && Yaw >= -135)    Dir = TEXT("球 右 >>");
				else if (Yaw > 135 || Yaw < -135)      Dir = TEXT("球在身后");
				else                                   Dir = TEXT("球在前方");
				BallHint = FString::Printf(TEXT("%s  (%.0fm)"), *Dir, Dist/100.f);
			}
		}
	}

	// Phase line (Chinese label for the current state machine node).
	FString Phase;
	switch (MatchState)
	{
	case EMatchState::PreMatch:           Phase = TEXT("赛前"); break;
	case EMatchState::BetweenRallies:     Phase = TEXT("回合间"); break;
	case EMatchState::ResettingPositions: Phase = TEXT("球员就位"); break;
	case EMatchState::AwaitingReady:      Phase = TEXT("裁判确认准备"); break;
	case EMatchState::ServiceAuthorized:  Phase = TEXT("允许发球"); break;
	case EMatchState::ServingToss:        Phase = TEXT("发球抛球"); break;
	case EMatchState::Rally:              Phase = TEXT("回合进行"); break;
	case EMatchState::SetOver:            Phase = TEXT("局间休息"); break;
	case EMatchState::MatchOver:          Phase = TEXT("比赛结束"); break;
	default:                              Phase = TEXT("-"); break;
	}

	// Serve hint ONLY in the legal state, for the legal server.
	FString ServeHint;
	if (MatchState == EMatchState::ServiceAuthorized && ServingTeam == EVolleyballTeam::TeamA)
	{
		if (ASpikeEliteCharacter* Player = Cast<ASpikeEliteCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (TeamAPlayers.Num() > 0 && TeamAPlayers[0] == Player)
			{
				ServeHint = FString::Printf(TEXT("按 E 发球（%.0f 秒）"), FMath::CeilToFloat(ServeDeadlineTimer));
			}
		}
	}

	// Possession / touch counter, e.g. "A 2/3".
	const FString Possession = SEVolleyballRules::TouchLabel(RallyState);

	// M11 dirty-check: build ONE compact signature (cheap ints + a few FStrings)
	// and skip every SetText unless something actually changed. Before M11 this
	// function rebuilt every FString/FText and called SetText unconditionally at
	// 60+ Hz; now the widget is only touched when its content really differs.
	const FString Sig = FString::Printf(TEXT("%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s|%s"),
		CurrentSet, TeamAScore, TeamBScore, TeamASetsWon, TeamBSetsWon,
		(int32)ServingTeam, (int32)MatchState, RallyState.TouchCount,
		*Phase, *Possession, *ServeHint, *RallyResultText, *BallHint);
	if (Sig == LastScoreboardSignature) { return; }
	LastScoreboardSignature = Sig;

	Scoreboard->UpdateScore(CurrentSet, TeamAScore, TeamBScore, TeamASetsWon, TeamBSetsWon,
		ServingTeam == EVolleyballTeam::TeamA, BallHint, Phase, Possession, ServeHint, RallyResultText);

	// M11b-2: push the authoritative score to the scorer-table scoreboard
	// (event-driven; the officials own no score of their own).
	if (Officials)
	{
		Officials->SetScorerText(
			FString::Printf(TEXT("SET %d"), CurrentSet),
			FString::Printf(TEXT("A %d : %d B"), TeamAScore, TeamBScore),
			FString::Printf(TEXT("Serve %s  Sets A%d:%d B"), TeamStr(ServingTeam), TeamASetsWon, TeamBSetsWon));
	}
}

void ASpikeEliteGameMode::OnBallLanded(const FVector& BallLocation)
{
	if (MatchState != EMatchState::Rally) return;
	if (RallyState.bRallySettled) return;

	const bool bIn = SEVolleyballRules::IsInBounds(BallLocation, Court->HalfCourtLength, Court->HalfCourtWidth);
	const EVolleyballTeam Last = RallyState.LastTouchTeam;
	const EVolleyballTeam Scoring = SEVolleyballRules::DetermineScoringTeamOnLand(bIn, Last, BallLocation.X >= 0.f);

	ERallyEndReason Reason = bIn ? ERallyEndReason::BallIn : ERallyEndReason::BallOut;

	// M11: a serve that never legally crossed the net is a serve fault whether it
	// lands IN (own side, impossible to be in-bounds there but keep the rule) or
	// OUT — classification comes from the shared rule core, not a BallIn-only case.
	if (SEVolleyballRules::IsServeFault(RallyState, bServeCrossedNet))
	{
		Reason = ERallyEndReason::ServeFault;
	}

	UE_LOG(LogVolleyballRules, Log,
		TEXT("Ball landed %s at (%.0f, %.0f, %.0f) | LastTouch=%s | TouchCount=%d | Scoring=%s"),
		bIn ? TEXT("IN") : TEXT("OUT"),
		BallLocation.X, BallLocation.Y, BallLocation.Z,
		TeamStr(Last),
		RallyState.TouchCount,
		TeamStr(Scoring));

	EndRally(Reason, Scoring);
}

void ASpikeEliteGameMode::EndRally(ERallyEndReason Reason, EVolleyballTeam ScoringTeam)
{
	if (!SEVolleyballRules::SettleRally(RallyState)) return;  // single settlement

	// M11b-2: the 1st referee blows the end-of-rally whistle.
	if (Officials) { Officials->Whistle(); }

	SetRallyResult(Reason, ScoringTeam);

	// M11: award FIRST, then log with explicit before -> after so [RallyEnd] never
	// dresses up a stale score as the final one.
	const int32 BeforeA = TeamAScore;
	const int32 BeforeB = TeamBScore;
	AwardPoint(ScoringTeam);

	UE_LOG(LogVolleyballRules, Log,
		TEXT("[RallyEnd] reason=%s scoring=%s lastTouch=%s touchCount=%d A:%d->%d B:%d->%d set=%d"),
		*SEVolleyballRules::RallyReasonLabel(Reason), TeamStr(ScoringTeam),
		TeamStr(RallyState.LastTouchTeam), RallyState.TouchCount,
		BeforeA, TeamAScore, BeforeB, TeamBScore, CurrentSet);
}

void ASpikeEliteGameMode::AwardPoint(EVolleyballTeam ScoringTeam)
{
	if (ScoringTeam == EVolleyballTeam::TeamA) { TeamAScore++; }
	else if (ScoringTeam == EVolleyballTeam::TeamB) { TeamBScore++; }
	else { return; }

	const bool bWasServeWin = (ServingTeam == ScoringTeam);
	ServingTeam = ScoringTeam;
	if (!bWasServeWin) { RotateTeam(ScoringTeam); RotationCount++; }
	RefreshRotationView();

	// Record the current set's running score.
	if (SetScoresA.Num() >= CurrentSet) { SetScoresA[CurrentSet-1] = TeamAScore; }
	if (SetScoresB.Num() >= CurrentSet) { SetScoresB[CurrentSet-1] = TeamBScore; }

	UE_LOG(LogVolleyballRules, Log, TEXT("Point. A:%d B:%d (serve=%s)"), TeamAScore, TeamBScore, TeamStr(ServingTeam));
	CheckSetWin();
}

void ASpikeEliteGameMode::CheckSetWin()
{
	if (!SEVolleyballRules::IsSetWon(TeamAScore, TeamBScore, PointsToWin))
	{
		MatchState = EMatchState::BetweenRallies;
		InterRallyTimer = InterRallyDelay;
		bInToss = false;
		UpdateScoreboard();
		return;
	}

	const EVolleyballTeam W = (TeamAScore > TeamBScore) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	if (W == EVolleyballTeam::TeamA) { TeamASetsWon++; } else { TeamBSetsWon++; }

	if (SEVolleyballRules::IsMatchWon(TeamASetsWon, TeamBSetsWon, MatchWinsNeeded))
	{
		MatchWinner = W;
		MatchState = EMatchState::MatchOver;
		// The end-of-match screen shows the result itself; drop the last rally
		// banner so the scoreboard reads clean under MatchOver.
		RallyResultText.Empty();
		RallyResultDisplayTimer = 0.0f;
		UpdateScoreboard();
		NotifyMatchOver();
		return;
	}

	// Set over: show banner, then start the next set after the pause.
	MatchState = EMatchState::SetOver;
	InterRallyTimer = SetOverDelay;
	const FString Text = FString::Printf(TEXT("第 %d 局结束：%s 队获胜  %d : %d"),
		CurrentSet, TeamStr(W), TeamAScore, TeamBScore);
	ShowBanner(Text, SetOverDelay);
	UpdateScoreboard();
}

void ASpikeEliteGameMode::StartNextSet()
{
	CurrentSet++;
	TeamAScore = TeamBScore = 0;
	PointsToWin = bQuickMatch ? 3 : SEVolleyballRules::PointsToWinForSet(CurrentSet);
	SetScoresA.Add(0);
	SetScoresB.Add(0);
	RespawnPlayersToPositions();
	SEVolleyballRules::BeginRally(RallyState, ServingTeam);
	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = SetOverDelay * 0.6f;
	UpdateScoreboard();
	UE_LOG(LogVolleyballRules, Log, TEXT("Set %d begins (to %d)"), CurrentSet, PointsToWin);
}

void ASpikeEliteGameMode::BeginServiceAuthorized()
{
	if (!Ball) return;

	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (ServingTeam == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	ASpikeEliteCharacter* Server = (Roster.Num() > 0) ? Roster[0].Get() : nullptr;
	const FVector ServerPos = Server ? Server->GetActorLocation()
		: FVector(ServingTeam == EVolleyballTeam::TeamA ? 770.f : -770.f, 0.f, 0.f);

	Ball->ResetBall(ServerPos + FVector(0,0,180.f));
	BallPrevX = Ball->GetActorLocation().X;
	bNetContactLatched = false;
	bServeCrossedNet = false;
	SEVolleyballRules::BeginRally(RallyState, ServingTeam);
	ServerPlayerIndex = (Server && !Server->bIsBot) ? 0 : -1;

	MatchState = EMatchState::ServiceAuthorized;
	bInToss = false;

	// M11b-2: the 1st referee blows the service whistle and the 8 s window opens.
	// Before this whistle the E key is refused (RequestServe state gate).
	ServeDeadlineTimer = ServeDeadline;
	if (Officials) { Officials->Whistle(); }
	UE_LOG(LogVolleyballRules, Log, TEXT("Whistle: service authorized for team=%s (%.0fs window)"), TeamStr(ServingTeam), ServeDeadline);

	// M11 auto-serve policy: bots always auto-serve after a short pause; a human
	// server serves ONLY when the automation flag (-devauto) is present. Normal
	// play never substitutes keyboard input, so "按 E 发球" is honest.
	bAIServePending = false;
	AIServeTimer = 0.0f;
	if (Server)
	{
		const bool bAuto = SEVolleyballRules::ShouldAutoServe(Server->bIsBot, bDevAuto);
		bAIServePending = bAuto;
		AIServeTimer = (Server->bIsBot ? 1.0f : 0.5f) * (bFastFlow ? 0.3f : 1.0f);
	}

	UE_LOG(LogVolleyballRules, Log, TEXT("Service authorized: team=%s server=%s auto=%d"), TeamStr(ServingTeam),
		(Server && !Server->bIsBot) ? TEXT("human") : TEXT("bot"), bAIServePending ? 1 : 0);
	RefreshRotationView();
	UpdateScoreboard();
}

void ASpikeEliteGameMode::AdvanceRallyPhase()
{
	// Post-rally ceremony: players return to their formation, then the 2nd
	// referee confirms readiness, then the 1st referee authorizes service.
	if (!Ball) return;
	RespawnPlayersToPositions();
	MatchState = EMatchState::ResettingPositions;
	PhaseTimer = ResetDelay;
	UE_LOG(LogVolleyballRules, Log, TEXT("Rally ceremony: players resetting positions (side-out rotation already applied)"));
	UpdateScoreboard();
}

void ASpikeEliteGameMode::HandleServeDeadline()
{
	// 8 s elapsed with no legal serve: serve delay fault, opponent scores.
	if (Officials) { Officials->Whistle(); }
	UE_LOG(LogVolleyballRules, Log, TEXT("Serve delay: %s failed to serve within %.0fs -> fault"),
		TeamStr(ServingTeam), ServeDeadline);
	EndRally(ERallyEndReason::ServeFault, (ServingTeam == EVolleyballTeam::TeamA) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA);
}

bool ASpikeEliteGameMode::RequestServe(ASpikeEliteCharacter* Server)
{
	// M11b-2: the service whistle (ServiceAuthorized) is the ONLY legal entry.
	// Pressing E while players are resetting or the 2nd referee is checking is
	// refused with a hint; every other state refuses silently.
	if (MatchState != EMatchState::ServiceAuthorized)
	{
		if (MatchState == EMatchState::ResettingPositions || MatchState == EMatchState::AwaitingReady)
		{
			ShowBanner(TEXT("裁判尚未鸣哨"), 1.0f);
			UpdateScoreboard();
		}
		return false;
	}
	if (!Server) { return false; }

	const EVolleyballTeam Team = TeamOf(Server);
	if (Team != ServingTeam)
	{
		ShowBanner(TEXT("现在不是你的发球"), 1.2f);
		UpdateScoreboard();
		return false;
	}

	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	if (Roster.Num() == 0 || Roster[0] != Server)
	{
		ShowBanner(TEXT("当前发球员不是你"), 1.2f);
		UpdateScoreboard();
		return false;
	}

	// Legal: enter the toss.
	TossDir = (Team == EVolleyballTeam::TeamA) ? FVector(-0.878f, 0.f, 0.479f) : FVector(0.878f, 0.f, 0.479f);
	TossPower = 1300.f;
	TossTimer = 0.6f;
	bInToss = true;
	ServerPlayerIndex = GetPlayerIndex(Team, Server);
	MatchState = EMatchState::ServingToss;
	bAIServePending = false;

	UE_LOG(LogVolleyballRules, Log, TEXT("Serve toss by team=%s player=%d"), TeamStr(Team), ServerPlayerIndex);
	UpdateScoreboard();
	return true;
}

void ASpikeEliteGameMode::ExecuteServe()
{
	if (!Ball) return;
	bInToss = false;

	Ball->Strike(TossDir, TossPower, 0.f);
	bServeCrossedNet = false;

	// The serve counts as the serving team's first touch.
	if (ServerPlayerIndex >= 0)
	{
		const ETouchResult R = SEVolleyballRules::EvaluateTouch(RallyState, ServingTeam, ServerPlayerIndex);
		if (R != ETouchResult::Allowed)
		{
			UE_LOG(LogVolleyballRules, Warning, TEXT("Serve touch unexpectedly rejected: %d"), (int32)R);
		}
	}

	MatchState = EMatchState::Rally;
	// M11: the serve is actually out -> the ball is now in play. This is the one
	// place that sets bBallInPlay true; cleanup/end-of-rally set it back to false.
	SEVolleyballRules::StartPlay(RallyState);
	// M11b-3: serve pose for the server.
	{
		const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (ServingTeam == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
		if (Roster.IsValidIndex(ServerPlayerIndex) && Roster[ServerPlayerIndex])
		{
			Roster[ServerPlayerIndex]->NotifyContact(EBallTouchType::Serve);
		}
	}
	UE_LOG(LogVolleyballRules, Log, TEXT("[Serve] team=%s player=%d power=%.0f dir=(%.2f,%.2f,%.2f) ballInPlay=1"),
		TeamStr(ServingTeam), ServerPlayerIndex, TossPower, TossDir.X, TossDir.Y, TossDir.Z);
	UpdateScoreboard();
}

void ASpikeEliteGameMode::OnBallCrossedNet()
{
	if (MatchState != EMatchState::Rally || RallyState.bRallySettled) return;

	const EVolleyballTeam NewPossessor = (Ball && Ball->GetActorLocation().X < 0.f) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	SEVolleyballRules::OnBallCrossedNet(RallyState, NewPossessor);
	bServeCrossedNet = true;

	ShowBanner(FString::Printf(TEXT("球过网 → %s 队控球"), TeamStr(NewPossessor)), 0.8f);
	UE_LOG(LogVolleyballRules, Log, TEXT("[NetCross] possession -> %s, touches reset"), TeamStr(NewPossessor));
	UpdateScoreboard();
}

bool ASpikeEliteGameMode::CanTouchBall(const ASpikeEliteCharacter* Toucher) const
{
	if (!Toucher || !bMatchActive) return false;
	// M11: the SAME phase gate the tests exercise (Rally && !settled); no second
	// copy of the rule in production code.
	if (!SEVolleyballRules::IsTouchLegalInPhase(MatchState, RallyState.bRallySettled)) return false;
	if (!Toucher->bTouchArmed) return false;
	const EVolleyballTeam Team = TeamOf(Toucher);
	if (RallyState.PossessingTeam != EVolleyballTeam::None && RallyState.PossessingTeam != Team) return false;
	return true;
}

bool ASpikeEliteGameMode::TryTouchBall(ASpikeEliteCharacter* Toucher, EBallTouchType Type)
{
	if (!Toucher || !Ball || !bMatchActive) return false;
	// M11: shared phase gate (Rally && !settled) — same as CanTouchBall and tests.
	if (!SEVolleyballRules::IsTouchLegalInPhase(MatchState, RallyState.bRallySettled)) return false;
	if (!Toucher->bTouchArmed) return false;
	// M11b-5c: a player in the dive recovery can neither re-dive nor touch.
	if (Toucher->bDiveRecovering) return false;

	const EVolleyballTeam Team = TeamOf(Toucher);
	const int32 Index = GetPlayerIndex(Team, Toucher);
	if (Index < 0) return false;

	// Physical reach check: horizontal distance (players "jump" for a ball that is
	// up to MaxTouchZ overhead) plus a vertical contact window. Using 3D distance
	// made high sets/attacks unreachable even when the player stood right under
	// the ball (3D dist ballooned with Z). A dive lunges further and can touch a
	// lower ball, but only within the lunge window.
	const EBallTouchType EffectiveType = (Type != EBallTouchType::Unknown) ? Type : ResolveTouchType(RallyState.TouchCount);
	const FVector MyLoc = Toucher->GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float Dist2D = FVector::Dist2D(MyLoc, BallLoc);
	const float Reach = Toucher->bDiving ? (TouchReach + 90.f) : TouchReach;
	const float MinZ = Toucher->bDiving ? 60.f : MinTouchZ;
	if (Dist2D > Reach || BallLoc.Z < MinZ || BallLoc.Z > MaxTouchZ)
	{
		return false;
	}
	// An AI attack must contact the ball at a height that clears the net plane
	// (243cm); a low contact either clips the net or produces a hopeless lob.
	// The attacker waits for the high point instead of chasing a falling ball.
	if (Toucher->bIsBot && EffectiveType == EBallTouchType::Attack && BallLoc.Z < 240.f)
	{
		return false;
	}

	// Direction: AI obeys phase rules; the human uses their view direction.
	FVector Dir;
	float Power;
	if (Toucher->bIsBot)
	{
		Dir = ComputeAITouchDirection(Toucher, EffectiveType);
		// A dive save pops the ball high and slow back toward our own depth so
		// teammates can set up the counter.
		Power = Toucher->bDiving ? 620.f
			: (EffectiveType == EBallTouchType::Receive) ? 780.f
			: (EffectiveType == EBallTouchType::Set) ? 550.f
			: 850.f;
	}
	else
	{
		const FVector LookDir = Toucher->GetControlRotation().Vector();
		Dir = LookDir;
		Dir.Z = FMath::Max(Dir.Z, 0.15f);
		Dir.Normalize();
		const bool bSpiking = Toucher->GetCharacterMovement() && !Toucher->GetCharacterMovement()->IsMovingOnGround();
		Power = bSpiking ? 1200.f : 850.f;
	}

	return DoTouch(Toucher, EffectiveType, Dir, Power, 0.f);
}

bool ASpikeEliteGameMode::ExecuteTacticalShot(ASpikeEliteCharacter* Toucher, const FShotIntent& Intent)
{
	if (!Toucher || !Ball || !bMatchActive) return false;
	if (!SEVolleyballRules::IsTouchLegalInPhase(MatchState, RallyState.bRallySettled)) return false;
	if (!Toucher->bTouchArmed) return false;

	const EVolleyballTeam Team = TeamOf(Toucher);
	const int32 Index = GetPlayerIndex(Team, Toucher);
	if (Index < 0) return false;

	const FVector MyLoc = Toucher->GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float Dist2D = FVector::Dist2D(MyLoc, BallLoc);
	if (Dist2D > TouchReach + 30.f || BallLoc.Z < MinTouchZ || BallLoc.Z > MaxTouchZ)
	{
		return false;
	}

	// Re-derive the intended velocity from the validated intent and predict.
	const FVector Start = BallLoc;
	const float T = FMath::Clamp(Intent.DesiredFlightTime, 0.3f, 3.0f);
	FVector Vel = SEVolleyballTrajectory::SolveVelocity(Start, Intent.TargetLocation, T);
	// Timing error biases direction (lateral) and power.
	const float Err = FMath::Clamp(Intent.TimingError, -1.f, 1.f);
	{
		FRotator Rot = Vel.Rotation();
		Rot.Yaw += Err * 14.f;   // early/late -> lateral bias
		Rot.Pitch -= Err * 6.f;  // early/late -> flatter/lofted
		Vel = Rot.Vector() * Vel.Size();
	}
	const float PowerScale = FMath::Clamp(1.f + Err * 0.25f, 0.6f, 1.4f);

	FVector Dir = Vel.GetSafeNormal();
	Dir.Z = FMath::Max(Dir.Z, 0.05f);
	Dir.Normalize();
	const float Power = (Intent.Power * 1100.f) * PowerScale;

	UE_LOG(LogVolleyballRules, Log, TEXT("[TacticalShot] team=%s player=%d target=(%.0f,%.0f) flight=%.2f power=%.2f err=%.2f net=%d in=%d"),
		TeamStr(Team), Index, Intent.TargetLocation.X, Intent.TargetLocation.Y, T, Intent.Power, Err,
		Intent.bPredictedCrossedNet ? 1 : 0, Intent.bPredictedInBounds ? 1 : 0);

	return DoTouch(Toucher, (Intent.TouchType != EBallTouchType::Unknown) ? Intent.TouchType : EBallTouchType::Attack,
		Dir, Power, Intent.SpinRadS);
}

bool ASpikeEliteGameMode::DoTouch(ASpikeEliteCharacter* Toucher, EBallTouchType Type, const FVector& Dir, float Power, float SpinRadS)
{
	if (!Toucher || !Ball || !bMatchActive) return false;
	if (!SEVolleyballRules::IsTouchLegalInPhase(MatchState, RallyState.bRallySettled)) return false;
	if (!Toucher->bTouchArmed) return false;

	const EVolleyballTeam Team = TeamOf(Toucher);
	const int32 Index = GetPlayerIndex(Team, Toucher);
	if (Index < 0) return false;

	const EBallTouchType EffectiveType = (Type != EBallTouchType::Unknown) ? Type : ResolveTouchType(RallyState.TouchCount);
	const FVector MyLoc = Toucher->GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float Dist2D = FVector::Dist2D(MyLoc, BallLoc);
	if (Dist2D > TouchReach || BallLoc.Z < MinTouchZ || BallLoc.Z > MaxTouchZ)
	{
		return false;
	}
	if (Toucher->bIsBot && EffectiveType == EBallTouchType::Attack && BallLoc.Z < 240.f)
	{
		return false;
	}

	const ETouchResult Result = SEVolleyballRules::EvaluateTouch(RallyState, Team, Index, EffectiveType);

	switch (Result)
	{
	case ETouchResult::Allowed:
	{
		Ball->Strike(Dir, Power, SpinRadS);
		Toucher->bTouchArmed = false;   // single-touch protection until rearmed
		// M11b-2: a successful touch must immediately retire the player from the
		// primary-handler role. Without this the setter (who is still flagged
		// Primary on the frame after the set) could touch the ball again before
		// UpdateAIDirectives picks the attacker, producing a DoubleTouch fault.
		Toucher->bIsPrimaryHandler = false;
		// M11b-3: drive the short contact pose (receive/set/spike/serve).
		Toucher->NotifyContact(EffectiveType);

		// M11b-5 block: the touch did not consume a team touch, but if the ball
		// stayed on the blocker's side of the net (soft block into the block
		// coverage), the blocking team now takes possession with a fresh count
		// — the classic "block then still have three touches" rule.
		if (EffectiveType == EBallTouchType::Block)
		{
			const bool bOwnSide = (Team == EVolleyballTeam::TeamA)
				? (Ball->GetActorLocation().X > 0.f)
				: (Ball->GetActorLocation().X < 0.f);
			if (bOwnSide)
			{
				RallyState.PossessingTeam = Team;
				RallyState.TouchCount = 0;
			}
		}

		UE_LOG(LogVolleyballRules, Log, TEXT("[Touch] team=%s player=%d touch=%d/%d type=%s"),
			TeamStr(Team), Index, RallyState.TouchCount, 3, TypeStr(EffectiveType));
		UpdateScoreboard();
		return true;
	}
	case ETouchResult::FourTouchesFault:
	{
		const EVolleyballTeam Opp = (Team == EVolleyballTeam::TeamA) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
		UE_LOG(LogVolleyballRules, Log, TEXT("Fault: 4th touch by team=%s -> %s scores"), TeamStr(Team), TeamStr(Opp));
		EndRally(ERallyEndReason::FourTouches, Opp);
		return false;
	}
	case ETouchResult::DoubleTouchFault:
	{
		const EVolleyballTeam Opp = (Team == EVolleyballTeam::TeamA) ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
		UE_LOG(LogVolleyballRules, Log, TEXT("Fault: double touch by team=%s player=%d -> %s scores"), TeamStr(Team), Index, TeamStr(Opp));
		EndRally(ERallyEndReason::DoubleTouch, Opp);
		return false;
	}
	default:
		return false;
	}
}

bool ASpikeEliteGameMode::TryBlockBall(ASpikeEliteCharacter* Toucher)
{
	if (!Toucher || !Ball || !bMatchActive) return false;
	if (!SEVolleyballRules::IsTouchLegalInPhase(MatchState, RallyState.bRallySettled)) return false;
	if (!Toucher->bTouchArmed) return false;

	const EVolleyballTeam Team = TeamOf(Toucher);
	const int32 Index = GetPlayerIndex(Team, Toucher);
	if (Index < 0) return false;

	// M11b-5: only front-row players may block. Front row is judged by the
	// home/defensive position X (|X| < 400 near the net), the same rule the AI
	// directive uses, so rotation order cannot wrongly demote a front-row player.
	if (FMath::Abs(Toucher->HomePosition.X) >= 400.f)
	{
		return false;
	}

	const FVector MyLoc = Toucher->GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float Dist2D = FVector::Dist2D(MyLoc, BallLoc);
	if (Dist2D > TouchReach || BallLoc.Z < 150.f || BallLoc.Z > 480.f)
	{
		return false;
	}

	// A block pushes the ball back over the net plane at medium-low power (the
	// live net collision absorbs anything below the tape). If it stays on our
	// side the DoTouch block branch hands us possession with a fresh count.
	FVector Dir = -MyLoc;
	Dir.Z = FMath::Max(Dir.Z, 0.1f);
	Dir.Normalize();
	const float Power = 420.f;

	const ETouchResult Result = SEVolleyballRules::EvaluateTouch(RallyState, Team, Index, EBallTouchType::Block);
	if (Result != ETouchResult::Allowed)
	{
		return false;
	}
	Ball->Strike(Dir, Power, 0.f);
	Toucher->bTouchArmed = false;
	Toucher->bIsPrimaryHandler = false;
	Toucher->NotifyContact(EBallTouchType::Block);

	const bool bOwnSide = (Team == EVolleyballTeam::TeamA)
		? (Ball->GetActorLocation().X > 0.f)
		: (Ball->GetActorLocation().X < 0.f);
	if (bOwnSide)
	{
		RallyState.PossessingTeam = Team;
		RallyState.TouchCount = 0;
	}

	UE_LOG(LogVolleyballRules, Log, TEXT("[Block] team=%s player=%d (front-row) hand-contact, ball %s"),
		TeamStr(Team), Index, bOwnSide ? TEXT("stayed own side") : TEXT("over the net"));
	UpdateScoreboard();
	return true;
}

// ---------------- M10: AI coordination ----------------

void ASpikeEliteGameMode::SetAIDirective(ASpikeEliteCharacter* Bot, EAIBehavior Behavior, const FVector& Target, bool bPrimary)
{
	if (!Bot) return;
	Bot->AIBehavior = Behavior;
	Bot->AITargetLocation = Target;
	Bot->bIsPrimaryHandler = bPrimary;
}

EVolleyballTeam ASpikeEliteGameMode::TeamOf(const ASpikeEliteCharacter* Player) const
{
	if (!Player) return EVolleyballTeam::None;
	return (Player->TeamSide > 0) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
}

int32 ASpikeEliteGameMode::GetPlayerIndex(EVolleyballTeam Team, const ASpikeEliteCharacter* Player) const
{
	const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	for (int32 i = 0; i < Roster.Num(); i++)
	{
		if (Roster[i] == Player) return i;
	}
	return -1;
}

FVector ASpikeEliteGameMode::PredictBallLanding() const
{
	if (!Ball) return FVector::ZeroVector;
	const FVector Pos = Ball->GetActorLocation();
	const FVector Vel = Ball->GetVelocity();

	// Solve 0.5*g*t^2 + vz*t + z0 = 0 for the floor (z = 0), projectile gravity = -980.
	constexpr float G = -980.f;
	const float A = 0.5f * G;
	const float B = Vel.Z;
	const float C = Pos.Z;
	float T = -1.f;
	const float Disc = B*B - 4.f*A*C;
	if (Disc >= 0.f)
	{
		const float SqrtD = FMath::Sqrt(Disc);
		const float T1 = (-B - SqrtD) / (2.f*A);
		const float T2 = (-B + SqrtD) / (2.f*A);
		if (T1 > 0.f && T2 > 0.f) { T = FMath::Min(T1, T2); }
		else if (T1 > 0.f) { T = T1; }
		else if (T2 > 0.f) { T = T2; }
	}
	if (T < 0.f) { T = 1.0f; }

	FVector Landing = Pos + FVector(Vel.X, Vel.Y, Vel.Z) * T + 0.5f * FVector(0.f, 0.f, G) * T * T;
	Landing.Z = 0.f;
	return Landing;
}

int32 ASpikeEliteGameMode::SelectReceivePlayer(EVolleyballTeam Team, const FVector& Landing) const
{
	const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	int32 Best = -1;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Roster.Num(); i++)
	{
		if (!Roster[i]) continue;
		const float D = FVector::Dist2D(Roster[i]->GetActorLocation(), Landing);
		if (D < BestDist) { BestDist = D; Best = i; }
	}
	return Best;
}

int32 ASpikeEliteGameMode::SelectSetterPlayer(EVolleyballTeam Team) const
{
	const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	const float SideX = (Team == EVolleyballTeam::TeamA) ? 250.f : -250.f;
	const FVector SetZone(SideX, 0.f, 0.f);
	int32 Best = -1;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Roster.Num(); i++)
	{
		if (!Roster[i]) continue;
		// M11b-2: the player who just made the first touch can never be picked as
		// the setter (consecutive-touch fault). Without this the receiver, who is
		// often the closest player to the setter zone right after the pass, was
		// selected again and every rally ended in a DoubleTouch fault.
		if (i == RallyState.LastTouchPlayerIndex && RallyState.LastTouchTeam == Team) continue;
		const float D = FVector::Dist2D(Roster[i]->GetActorLocation(), SetZone);
		if (D < BestDist) { BestDist = D; Best = i; }
	}
	return Best;
}

int32 ASpikeEliteGameMode::SelectAttackerPlayer(EVolleyballTeam Team) const
{
	const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	FVector AttackPoint;
	if (Ball)
	{
		const float SideX = (Team == EVolleyballTeam::TeamA) ? 150.f : -150.f;
		AttackPoint = FVector(SideX, FMath::Clamp(Ball->GetActorLocation().Y, -300.f, 300.f), 220.f);
	}
	else
	{
		AttackPoint = FVector((Team == EVolleyballTeam::TeamA) ? 150.f : -150.f, 0.f, 220.f);
	}
	int32 Best = -1;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Roster.Num(); i++)
	{
		if (!Roster[i]) continue;
		// The player who just set cannot attack (no consecutive touches).
		if (i == RallyState.LastTouchPlayerIndex && RallyState.LastTouchTeam == Team) continue;
		const float D = FVector::Dist2D(Roster[i]->GetActorLocation(), AttackPoint);
		if (D < BestDist) { BestDist = D; Best = i; }
	}
	return Best;
}

FVector ASpikeEliteGameMode::ComputeAITouchDirection(const ASpikeEliteCharacter* Toucher, EBallTouchType Type) const
{
	const int32 Side = Toucher ? Toucher->TeamSide : 1;
	const FVector BallLoc = Ball ? Ball->GetActorLocation() : FVector::ZeroVector;
	const FVector MyLoc = Toucher ? Toucher->GetActorLocation() : FVector::ZeroVector;

	FVector Target;
	float MinZ = 0.15f;

	switch (Type)
	{
	case EBallTouchType::Receive:
		// First touch must go to the setter zone (front-middle), NEVER straight
		// back over the net.
		Target = FVector(Side * 250.f, FMath::Clamp(BallLoc.Y * 0.35f, -120.f, 120.f), 0.f);
		break;

	case EBallTouchType::Set:
		// Second touch: loft a slow, high ball to the front attack point so the
		// attacker (already moving there) has time to arrive. Power 550 with a
		// ~0.5 min elevation keeps the ball above the net's 243cm plane and
		// hangs for roughly half a second around the attack point.
		Target = FVector(Side * 130.f, FMath::Clamp(BallLoc.Y, -280.f, 280.f), 350.f);
		MinZ = 0.5f;
		break;

	case EBallTouchType::Attack:
	default:
	{
		// Third touch: over the net. A seeded error rate keeps rallies finite:
		// ~6% wide (out), ~9% long/into the net. The min elevation 0.45 with
		// Attack power 850 clears the 243cm net from a ~190cm contact and lands
		// inside the far court (~854cm flight, court half is 900cm).
		const float Roll = AIStream.FRand();
		if (Roll < 0.06f)
		{
			const float SideY = (AIStream.RandBool() ? 1.f : -1.f) * AIStream.FRandRange(560.f, 720.f);
			Target = FVector(-Side * 650.f, SideY, BallLoc.Z);
			MinZ = 0.2f;
		}
		else if (Roll < 0.15f)
		{
			Target = FVector(-Side * AIStream.FRandRange(700.f, 1050.f),
				AIStream.FRandRange(-200.f, 200.f), BallLoc.Z);
			MinZ = AIStream.FRandRange(0.05f, 0.2f);
		}
		else
		{
			Target = FVector(-Side * AIStream.FRandRange(480.f, 720.f),
				AIStream.FRandRange(-220.f, 220.f), BallLoc.Z);
			MinZ = 0.5f;
		}
		break;
	}
	}

	FVector Dir = (Target - BallLoc).GetSafeNormal();
	Dir.Z = FMath::Max(Dir.Z, MinZ);
	Dir.Normalize();
	return Dir;
}

void ASpikeEliteGameMode::UpdateAIDirectives(float DeltaSeconds)
{
	if (MatchState != EMatchState::Rally)
	{
		// Not a live rally: everyone returns to their home / defensive slot.
		for (auto& C : TeamAPlayers)
		{
			if (C && C->bIsBot) { SetAIDirective(C, EAIBehavior::ReturnHome, C->HomePosition, false); }
		}
		for (auto& C : TeamBPlayers)
		{
			if (C && C->bIsBot) { SetAIDirective(C, EAIBehavior::ReturnHome, C->HomePosition, false); }
		}
		return;
	}

	const FVector Landing = PredictBallLanding();

	auto DirectTeam = [&](EVolleyballTeam Team, TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster)
	{
		const bool bPossess = (RallyState.PossessingTeam == Team);
		int32 Primary = -1;
		EAIBehavior PrimaryBehavior = EAIBehavior::ReturnHome;
		FVector PrimaryTarget = FVector::ZeroVector;

		if (bPossess)
		{
			switch (RallyState.TouchCount)
			{
			case 0: // Receive: closest to the predicted landing.
				Primary = SelectReceivePlayer(Team, Landing);
				PrimaryBehavior = EAIBehavior::MoveToReceive;
				PrimaryTarget = Landing;
				break;
			case 1: // Set: designated setter zone.
				Primary = SelectSetterPlayer(Team);
				PrimaryBehavior = EAIBehavior::Set;
				PrimaryTarget = FVector((Team == EVolleyballTeam::TeamA) ? 250.f : -250.f,
					FMath::Clamp(Landing.Y, -150.f, 150.f), 0.f);
				// Pre-select the attacker while the setter is still handling, so the
				// attacker gets the whole flight time of the set to reach the front
				// point and can contact the ball at a high Z (no net clips).
				{
					const float SideX = (Team == EVolleyballTeam::TeamA) ? 130.f : -130.f;
					const FVector AttackPt = FVector(SideX,
						FMath::Clamp(Ball ? Ball->GetActorLocation().Y : 0.f, -300.f, 300.f), 220.f);
					int32 BestA = -1;
					float BestDA = TNumericLimits<float>::Max();
					for (int32 j = 0; j < Roster.Num(); j++)
					{
						if (!Roster[j]) continue;
						if (j == Primary) continue;
						if (j == RallyState.LastTouchPlayerIndex && RallyState.LastTouchTeam == Team) continue;
						const float D = FVector::Dist2D(Roster[j]->GetActorLocation(), AttackPt);
						if (D < BestDA) { BestDA = D; BestA = j; }
					}
					if (BestA >= 0)
					{
						SetAIDirective(Roster[BestA].Get(), EAIBehavior::Attack, AttackPt, false);
					}
				}
				break;
			case 2: // Attack: closest to the front attack point.
				Primary = SelectAttackerPlayer(Team);
				PrimaryBehavior = EAIBehavior::Attack;
				PrimaryTarget = FVector((Team == EVolleyballTeam::TeamA) ? 150.f : -150.f,
					FMath::Clamp(Ball ? Ball->GetActorLocation().Y : 0.f, -300.f, 300.f), 220.f);
				break;
			default:
				break;
			}
		}

		for (int32 i = 0; i < Roster.Num(); i++)
		{
			ASpikeEliteCharacter* C = Roster[i].Get();
			if (!C || !C->bIsBot) continue;

			if (i == Primary)
			{
				SetAIDirective(C, PrimaryBehavior, PrimaryTarget, true);
			}
			else if (bPossess)
			{
				// Front-row players (except the primary handler) run to the attack
				// point as soon as the team gains possession, so the attacker is
				// already in position when the set arrives; keep them there through
				// the third touch so the attacker never chases a falling ball.
				if (FMath::Abs(C->HomePosition.X) < 400.f && RallyState.TouchCount <= 2)
				{
					const FVector AttackPt = FVector((Team == EVolleyballTeam::TeamA) ? 150.f : -150.f,
						FMath::Clamp(Ball ? Ball->GetActorLocation().Y : 0.f, -300.f, 300.f), 220.f);
					SetAIDirective(C, EAIBehavior::Attack, AttackPt, false);
				}
				else
				{
					// Non-handlers hold their role position (don't all chase the ball).
					SetAIDirective(C, EAIBehavior::ReturnHome, C->HomePosition, false);
				}
			}
			else
			{
				// Defending (opponent possesses).
				const FVector Home = C->HomePosition;
				const bool bFrontRow = FMath::Abs(Home.X) < 400.f;
				const bool bBallOnOwnSide = Ball && ((Team == EVolleyballTeam::TeamA)
					? (Ball->GetActorLocation().X > 0.f) : (Ball->GetActorLocation().X < 0.f));
				const float BallZ = Ball ? Ball->GetActorLocation().Z : 0.f;
				// Low or fast ball on our side -> the closest receiver digs it.
				// M11b-5c: a fast low ball beyond normal reach is a dive lunge
				// (fast sprint + extended touch window); recovery blocks re-dives.
				const FVector BallVel = Ball ? Ball->GetVelocity() : FVector::ZeroVector;
				const bool bDefensiveBall = bBallOnOwnSide && (BallZ < 130.f || BallVel.Size2D() > 500.f);
				const int32 RecvIdx = bDefensiveBall ? SelectReceivePlayer(Team, Landing) : -1;
				// M11b-5c: a ball that just crossed the net into our side at speed is
				// a dive situation - the receiver lunges (extended touch window).
				const bool bDiveSituation = (bBallOnOwnSide && GM_NetCrossWindow > 0.f
					&& GM_NetCrossSpeed > 450.f && !C->bDiveRecovering) ? true : false;
				if (i == RecvIdx)
				{
					if (bDiveSituation)
					{
						UE_LOG(LogVolleyballRules, Log, TEXT("[Dive] %s dives (net cross %.0f cm/s)"), *C->GetName(), GM_NetCrossSpeed);
						SetAIDirective(C, EAIBehavior::Dive, Landing, true);
					}
					else
					{
						SetAIDirective(C, EAIBehavior::MoveToReceive, Landing, true);
					}
				}
				// M11b-5 block: when the opponent is about to attack (2 touches done)
				// — or a high ball is on our side — the front row slides to the
				// front point at the ball's Y and attempts a block (primary handler
				// asks TryBlockBall, which re-checks the front-row gate and reach).
				else if (bFrontRow
					&& (RallyState.TouchCount >= 2 || (bBallOnOwnSide && BallZ > 130.f)))
				{
					const float SideX = (Team == EVolleyballTeam::TeamA) ? 120.f : -120.f;
					const FVector BlockPt = FVector(SideX,
						FMath::Clamp(Ball ? Ball->GetActorLocation().Y : 0.f, -350.f, 350.f), Home.Z);
					SetAIDirective(C, EAIBehavior::MoveToBlock, BlockPt, true);
				}
				else
				{
					FVector Defensive = Home;
					if (bFrontRow) { Defensive = FVector((Team == EVolleyballTeam::TeamA) ? 230.f : -230.f, Home.Y, Home.Z); }
					SetAIDirective(C, EAIBehavior::Wait, Defensive, false);
				}
			}
		}
	};

	DirectTeam(EVolleyballTeam::TeamA, TeamAPlayers);
	DirectTeam(EVolleyballTeam::TeamB, TeamBPlayers);
}

const TCHAR* ASpikeEliteGameMode::TypeStr(EBallTouchType Type)
{
	switch (Type)
	{
	case EBallTouchType::Serve:   return TEXT("Serve");
	case EBallTouchType::Receive: return TEXT("Receive");
	case EBallTouchType::Set:     return TEXT("Set");
	case EBallTouchType::Attack:  return TEXT("Attack");
	case EBallTouchType::Block:   return TEXT("Block");
	default:                  return TEXT("Unknown");
	}
}

void ASpikeEliteGameMode::NotifyMatchOver()
{
	if (ASpikeElitePlayerController* SEPC = Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		SEPC->OnMatchOver(SetScoresA, SetScoresB, MatchWinner);
	}
}

void ASpikeEliteGameMode::BuildRotationView(FRotationViewState& Out) const
{
	Out.RotationIndex = RotationCount;
	Out.ServingTeam = ServingTeam;

	// Roster order IS the authoritative rotation: index 0 = P1 (back-right,
	// the server), then P2/P3/P4 (front row), P5/P6.
	auto Fill = [this](const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster, TArray<FRotationSlotView>& OutArr)
	{
		OutArr.Reset();
		for (int32 i = 0; i < Roster.Num(); ++i)
		{
			FRotationSlotView V;
			V.SlotIndex = i;
			if (Roster[i])
			{
				V.PlayerId = Roster[i]->PlayerId;
				V.Jersey = FString::Printf(TEXT("#%d"), Roster[i]->JerseyNumber);
				V.bControlled = !Roster[i]->bIsBot;
				V.bServer = (i == 0);
			}
			V.bFrontRow = (i == 1 || i == 2 || i == 3);
			OutArr.Add(V);
		}
	};
	Fill(TeamAPlayers, Out.TeamA);
	Fill(TeamBPlayers, Out.TeamB);
}

void ASpikeEliteGameMode::RefreshRotationView()
{
	if (!RotationWidget) return;
	FRotationViewState State;
	BuildRotationView(State);
	RotationWidget->Refresh(State);
}