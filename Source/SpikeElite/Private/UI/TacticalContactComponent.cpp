// SPDX-License-Identifier: MIT
#include "UI/TacticalContactComponent.h"
#include "UI/TrajectoryPreviewComponent.h"
#include "UI/TacticalHUDWidget.h"
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "SpikeElitePlayerController.h"
#include "Volleyball/VolleyballBall.h"
#include "Volleyball/SetPlay.h"
#include "Components/TextRenderComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/App.h"

namespace
{
	constexpr float TacticalWindowSeconds = 0.1f;   // M11c-4: fast balls only stay in reach ~0.26s
	constexpr float ArmedWindowSeconds = 0.8f;
	constexpr float SlowMotionDilation = 0.15f;
	constexpr float TouchReach2D = 220.f;
	constexpr float MinTouchZ = 120.f;
	constexpr float MaxTouchZ = 450.f;

	EBallTouchType TypeForTouchCount(int32 TouchCount)
	{
		if (TouchCount <= 0) return EBallTouchType::Receive;
		if (TouchCount == 1) return EBallTouchType::Set;
		return EBallTouchType::Attack;
	}
}

UTacticalContactComponent::UTacticalContactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTacticalContactComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = Cast<ASpikeElitePlayerController>(GetOwner());
	if (!OwnerPC.IsValid()) { return; }

	Pawn = Cast<ASpikeEliteCharacter>(OwnerPC->GetPawn());

	Preview = NewObject<UTrajectoryPreviewComponent>(GetOwner());
	Preview->SetupAttachment(GetOwner()->GetRootComponent());
	Preview->RegisterComponent();

	HintText = NewObject<UTextRenderComponent>(GetOwner());
	HintText->SetupAttachment(GetOwner()->GetRootComponent());
	HintText->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	HintText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	HintText->SetWorldSize(26.f);
	HintText->SetTextRenderColor(FColor(255, 255, 200));
	HintText->SetText(FText::FromString(TEXT("")));
	HintText->RegisterComponent();
	HintText->SetVisibility(false);

	// M11c-5: the real screen UMG (attack / set-list / defense panels).
	TacticalUI = CreateWidget<UTacticalHUDWidget>(GetWorld());
	if (TacticalUI)
	{
		TacticalUI->AddToViewport(50);
		TacticalUI->HideAll();
		TacticalUI->OnSetPlaySelected.AddUObject(this, &UTacticalContactComponent::HandleSetPlayPicked);
		TacticalUI->OnDefensePlanSelected.AddUObject(this, &UTacticalContactComponent::HandleDefensePicked);
	}
	else
	{
		UE_LOG(LogVolleyballRules, Warning, TEXT("[Tactical] CreateWidget failed - tactical UI disabled"));
	}
}

void UTacticalContactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// M11c-4: planning/armed run under TimeDilation != 1, so all UI input and
	// timing windows must use the REAL unscaled frame time (FApp::GetDeltaTime)
	// — otherwise W/S/Q/E and the timing bar stop responding when frozen.
	const float RealDt = FApp::GetDeltaTime();

	if (!GM.IsValid())
	{
		GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this));
	}
	if (!Pawn.IsValid())
	{
		if (OwnerPC.IsValid()) { Pawn = Cast<ASpikeEliteCharacter>(OwnerPC->GetPawn()); }
	}
	if (!GM.IsValid() || !Pawn.IsValid()) { return; }

	// Pause / menu / end-of-match must always drop out of any tactical phase.
	const bool bGameplayLocked = (GM->MatchState != EMatchState::Rally) || (OwnerPC.IsValid() && OwnerPC->IsMenuOpen());
	if (bGameplayLocked)
	{
		if (IsTacticalActive()) { CancelShot(); }
		return;
	}

	switch (State)
	{
	case ETacticalState::Normal:
	{
		if (!GM->GetBall()) { return; }
		const FVector PawnLoc = Pawn->GetActorLocation();
		const FVector BallLoc = GM->GetBall()->GetActorLocation();
		const float Dist2D = FVector::Dist2D(PawnLoc, BallLoc);

		// M11c-5: defense planning — the OPPONENT is about to attack (their
		// 2nd touch done, ball fast and close to the net on their half). The
		// human picks a defensive plan; AI uses defaults on timeout.
		const EVolleyballTeam MyTeam = (Pawn->TeamSide >= 0) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
		const EVolleyballTeam PossTeam = GM->GetPossessingTeam();
		if (PossTeam != EVolleyballTeam::None && PossTeam != MyTeam
			&& GM->GetTouchCount() >= 2)
		{
			const FVector BallVel = GM->GetBall()->GetVelocity();
			// M11f-1: "ball on THEIR side" = ball on the possessing team's own
			// half. A plays +X, B plays -X (GameMode TeamSide=+1/-1, serve
			// points ±1200). The old A:X<0 / B:X>0 check was mirrored backwards
			// and could fire only AFTER the ball had crossed into our half.
			const bool bBallOnTheirSide = SEVolleyballRules::IsOnTeamHalf(BallLoc.X, PossTeam);
			const bool bApproachingNet = (FMath::Abs(BallLoc.X) < 700.f);
			if (bBallOnTheirSide && bApproachingNet && BallVel.Size() > 350.f && BallLoc.Z < 420.f
				&& TacticalMode >= 1)
			{
				UE_LOG(LogVolleyballRules, Log,
					TEXT("[DefensePlanning] trigger possession=%s myTeamSide=%+d ballX=%.0f vel=%.0f touches=%d reason=opponent_attack_on_their_half"),
					(PossTeam == EVolleyballTeam::TeamA) ? TEXT("A") : TEXT("B"), Pawn->TeamSide, BallLoc.X, BallVel.Size(), GM->GetTouchCount());
				EnterDefensePlanning();
				break;
			}
		}

		if (Dist2D <= TouchReach2D && BallLoc.Z >= MinTouchZ && BallLoc.Z <= MaxTouchZ && Pawn->bTouchArmed)
		{
			if (TacticalMode == 0) { return; }
			const EBallTouchType Type = TypeForTouchCount(GM->GetTouchCount());
			const bool bAllowed = (TacticalMode >= 2) || (Type == EBallTouchType::Set || Type == EBallTouchType::Attack);
			if (bAllowed)
			{
				State = ETacticalState::ContactWindow;
				WindowTimer = TacticalWindowSeconds;
				PendingTouchType = Type;
			}
		}
		break;
	}
	case ETacticalState::DefensePlanning:
	{
		// M11c-5: slow motion while choosing; Esc/RMB cancels (AI default),
		// LMB/Enter confirms; timeout picks the AI default.
		DefenseTimer -= RealDt;
		if (OwnerPC->WasInputKeyJustPressed(EKeys::Up) || OwnerPC->WasInputKeyJustPressed(EKeys::W))
		{
			DefenseSelected = (DefenseSelected + 7) % 8;
			if (TacticalUI) { TacticalUI->UpdateDefenseList(DefenseSelected); }
		}
		if (OwnerPC->WasInputKeyJustPressed(EKeys::Down) || OwnerPC->WasInputKeyJustPressed(EKeys::S))
		{
			DefenseSelected = (DefenseSelected + 1) % 8;
			if (TacticalUI) { TacticalUI->UpdateDefenseList(DefenseSelected); }
		}
		if (OwnerPC->WasInputKeyJustPressed(EKeys::RightMouseButton) || OwnerPC->WasInputKeyJustPressed(EKeys::Escape))
		{
			DefensePlan = EVolleyballDefensePlan::NoPlan;
			if (GM.IsValid()) { GM->SetPlayerDefensePlan(EVolleyballDefensePlan::NoPlan); }
			RestoreWorldState();
			State = ETacticalState::Normal;
			if (TacticalUI) { TacticalUI->HideAll(); }
			break;
		}
		if ((OwnerPC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !(TacticalUI && TacticalUI->IsPointerOverPanel()))
			|| OwnerPC->WasInputKeyJustPressed(EKeys::Enter))
		{
			ConfirmDefensePlan();
			break;
		}
		if (DefenseTimer <= 0.f)
		{
			DefensePlan = EVolleyballDefensePlan::NoPlan;
			if (GM.IsValid()) { GM->SetPlayerDefensePlan(EVolleyballDefensePlan::NoPlan); }
			RestoreWorldState();
			State = ETacticalState::Normal;
			if (TacticalUI) { TacticalUI->HideAll(); }
		}
		break;
	}
	case ETacticalState::ContactWindow:
	{
		// Leave if the ball got out of reach or the player touched it normally.
		const FVector PawnLoc = Pawn->GetActorLocation();
		const FVector BallLoc = GM->GetBall()->GetActorLocation();
		const float Dist2D = FVector::Dist2D(PawnLoc, BallLoc);
		if (Dist2D > TouchReach2D + 20.f || !Pawn->bTouchArmed)
		{
			State = ETacticalState::Normal;
			break;
		}
		WindowTimer -= DeltaTime;
		if (WindowTimer <= 0.f)
		{
			EnterPlanning(PendingTouchType);
		}
		break;
	}
	case ETacticalState::TacticalPlanning:
		TickPlanning(DeltaTime);
		break;
	case ETacticalState::TacticalArmed:
	{
		ArmedTimer -= DeltaTime;
		// LMB executes with timing quality; timeout fires a conservative shot.
		if (OwnerPC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			const float Quality = FMath::Clamp(1.f - FMath::Abs(ArmedTimer - ArmedWindowSeconds * 0.45f) / (ArmedWindowSeconds * 0.45f), 0.f, 1.f);
			ExecuteTimedShot(0.5f - Quality);
		}
		else if (ArmedTimer <= 0.f)
		{
			ExecuteTimedShot(0.35f);  // late but still legal
		}
		break;
	}
	case ETacticalState::ContactResolved:
		RestoreWorldState();
		State = ETacticalState::Normal;
		break;
	}
}

void UTacticalContactComponent::EnterPlanning(EBallTouchType Type)
{
	State = ETacticalState::TacticalPlanning;
	PendingTouchType = Type;
	bWorldFrozen = true;
	// M11f-1: save the original dilation exactly once per override lifetime; a
	// later EnterArmed / re-plan must not overwrite the value we must restore.
	if (!bTimeOverrideActive)
	{
		SavedTimeDilation = GetWorld()->GetWorldSettings()->TimeDilation;
		bTimeOverrideActive = true;
	}
	GetWorld()->GetWorldSettings()->TimeDilation = 0.f;

	// Freeze gameplay input; the PC reads mouse directly in planning.
	if (Pawn.IsValid() && OwnerPC.IsValid())
	{
		Pawn->DisableInput(OwnerPC.Get());
		OwnerPC->SetShowMouseCursor(true);
		OwnerPC->SetInputMode(FInputModeGameAndUI());
	}
	if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("左键确认 · 右键取消 · 滚轮/W/S 力度 · Q/E 弧线")));
		HintText->SetVisibility(true);
	}

	Intent = FShotIntent();
	Intent.TouchType = Type;
	Intent.DesiredFlightTime = (Type == EBallTouchType::Attack) ? 0.75f : 0.9f;
	Intent.Power = 0.8f;
	// M11b-5b: a set opens the data-driven tactic picker (starts at 四号位高球).
	SelectedPlay = (Type == EBallTouchType::Set) ? 0 : -1;
	// M11f-1: the panel shows the FIRST tactic as selected, so the live Intent/
	// preview must match that selection immediately — applying the default play
	// here keeps "highlighted row" and "dotted preview" in sync from the start.
	if (Type == EBallTouchType::Set && SelectedPlay >= 0)
	{
		ApplySetPlayToIntent(SelectedPlay);
	}
	RebuildPreview();

	// M11c-5: real screen UMG — set list for a set, attack panel otherwise.
	if (TacticalUI)
	{
		if (Type == EBallTouchType::Set)
		{
			TacticalUI->ShowSetPanel();
			TacticalUI->UpdateSetList(SelectedPlay);
		}
		else
		{
			TacticalUI->ShowAttackPanel();
			TacticalUI->UpdateAttackInfo(Intent, SEVolleyballTrajectory::BuildShotSolution(
				GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector, Intent, 0.f));
		}
	}
}

void UTacticalContactComponent::TickPlanning(float DeltaTime)
{
	// M11c-4: DeltaTime here is the REAL frame time, so W/S/Q/E adjust the plan
	// even while the world is time-dilated/frozen.

	// M11f-1: the UMG panels/ScrollBox own the pointer while the cursor is over
	// them — the wheel there scrolls the list, never the power; a world LMB only
	// confirms outside the panels. Computed once so the wheel, the card clicks
	// and the confirm paths all agree on the same ownership.
	const bool bPointerOverPanel = (TacticalUI && TacticalUI->IsPointerOverPanel());

	// List-display state for a set (including free trajectory id 14). The free
	// trajectory keeps the mouse-picked target, so only the 13 named plays lock
	// the target to the mirrored play spot.
	const bool bSetTactics = (PendingTouchType == EBallTouchType::Set && SelectedPlay >= 0);
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	const bool bFreeTrajectory = bSetTactics && Plays.IsValidIndex(SelectedPlay) && Plays[SelectedPlay].PlayId == 14;
	const bool bSetTacticLocked = bSetTactics && !bFreeTrajectory;
	if (!bSetTacticLocked)
	{
		// Free aim: the mouse picks the landing spot (free trajectory and attack).
		const FVector Ground = PickGroundPoint();
		if (!Ground.IsZero())
		{
			Intent.TargetLocation = Ground;
		}
	}

	// Power: wheel (fast) or W/S (held). M11f-1: the wheel is owned by the list
	// while the pointer is over a panel — it must not also change the power.
	const float Wheel = OwnerPC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	if (!bPointerOverPanel && FMath::Abs(Wheel) > 0.01f)
	{
		Intent.Power = FMath::Clamp(Intent.Power + Wheel * 0.05f, 0.15f, 1.f);
	}
	if (OwnerPC->IsInputKeyDown(EKeys::W)) { Intent.Power = FMath::Clamp(Intent.Power + DeltaTime * 0.4f, 0.15f, 1.f); }
	if (OwnerPC->IsInputKeyDown(EKeys::S)) { Intent.Power = FMath::Clamp(Intent.Power - DeltaTime * 0.4f, 0.15f, 1.f); }

	// Arc / flight time: Q up (higher), E down.
	// M11f-1: Up/Down ALWAYS cycle the tactic cards (free trajectory included);
	// Q/E switch the list for the named plays, but adjust the arc height for the
	// free trajectory (id 14) and for plain attack aim — so "free trajectory"
	// really is free, and the manual-arc comment finally matches the behavior.
	bool bSelectionChanged = false;
	if (OwnerPC->WasInputKeyJustPressed(EKeys::Up))
	{
		SelectedPlay = (SelectedPlay + 1) % Plays.Num();
		bSelectionChanged = true;
	}
	if (OwnerPC->WasInputKeyJustPressed(EKeys::Down))
	{
		SelectedPlay = (SelectedPlay - 1 + Plays.Num()) % Plays.Num();
		bSelectionChanged = true;
	}
	if (bSetTactics)
	{
		if (!bFreeTrajectory)
		{
			if (OwnerPC->WasInputKeyJustPressed(EKeys::Q))
			{
				SelectedPlay = (SelectedPlay + 1) % Plays.Num();
				bSelectionChanged = true;
			}
			if (OwnerPC->WasInputKeyJustPressed(EKeys::E))
			{
				SelectedPlay = (SelectedPlay - 1 + Plays.Num()) % Plays.Num();
				bSelectionChanged = true;
			}
		}
		else
		{
			if (OwnerPC->IsInputKeyDown(EKeys::Q)) { Intent.DesiredFlightTime = FMath::Clamp(Intent.DesiredFlightTime + DeltaTime * 0.6f, 0.3f, 2.5f); }
			if (OwnerPC->IsInputKeyDown(EKeys::E)) { Intent.DesiredFlightTime = FMath::Clamp(Intent.DesiredFlightTime - DeltaTime * 0.6f, 0.3f, 2.5f); }
		}
		// Keyboard selection and mouse-card clicks share ApplySetPlayToIntent,
		// so the Intent/preview always follow the SAME SelectedPlay index.
		if (bSelectionChanged) { ApplySetPlayToIntent(SelectedPlay); }
		if (TacticalUI) { TacticalUI->UpdateSetList(SelectedPlay); }
		if (HintText)
		{
			const FString Name = Plays.IsValidIndex(SelectedPlay) ? Plays[SelectedPlay].DisplayName : TEXT("?");
			const FString ArcHint = bFreeTrajectory ? TEXT(" · Q/E 手动调弧线") : TEXT("");
			HintText->SetText(FText::FromString(FString::Printf(TEXT("二传战术：%s%s  Up/Down选择 · 空白区左键/Enter确认 · 右键取消"), *Name, *ArcHint)));
		}
	}
	else
	{
		if (OwnerPC->IsInputKeyDown(EKeys::Q)) { Intent.DesiredFlightTime = FMath::Clamp(Intent.DesiredFlightTime + DeltaTime * 0.6f, 0.3f, 2.5f); }
		if (OwnerPC->IsInputKeyDown(EKeys::E)) { Intent.DesiredFlightTime = FMath::Clamp(Intent.DesiredFlightTime - DeltaTime * 0.6f, 0.3f, 2.5f); }
	}

	RebuildPreview();

	// M11c-5: push the live attack info into the screen panel each frame.
	if (TacticalUI && !bSetTactics)
	{
		TacticalUI->UpdateAttackInfo(Intent, SEVolleyballTrajectory::BuildShotSolution(
			GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector, Intent, 0.f));
	}

	// Confirm / cancel. Card clicks select only: while the cursor is over a
	// tactical panel, LMB belongs to the UMG button (selection), never to the
	// world-space confirm. Enter confirms anywhere; a world LMB confirms too.
	if (OwnerPC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bPointerOverPanel)
	{
		if (PendingTouchType == EBallTouchType::Set && GM.IsValid())
		{
			GM->SetActiveSetPlay(SelectedPlay);   // M11c-5: attacker run-up follows the play
		}
		EnterArmed();
	}
	else if (OwnerPC->WasInputKeyJustPressed(EKeys::Enter))
	{
		if (PendingTouchType == EBallTouchType::Set && GM.IsValid())
		{
			GM->SetActiveSetPlay(SelectedPlay);
		}
		EnterArmed();
	}
	else if (OwnerPC->WasInputKeyJustPressed(EKeys::RightMouseButton) || OwnerPC->WasInputKeyJustPressed(EKeys::Escape))
	{
		CancelShot();
	}
}

FVector UTacticalContactComponent::PickGroundPoint()
{
	FVector Origin, Dir;
	if (!OwnerPC->DeprojectMousePositionToWorld(Origin, Dir)) { return FVector::ZeroVector; }
	if (FMath::Abs(Dir.Z) < 0.001f) { return FVector::ZeroVector; }
	const float T = -Origin.Z / Dir.Z;
	if (T < 0.f) { return FVector::ZeroVector; }
	FVector Point = Origin + Dir * T;
	Point.X = FMath::Clamp(Point.X, -1500.f, 1500.f);
	Point.Y = FMath::Clamp(Point.Y, -900.f, 900.f);
	Point.Z = 0.f;
	return Point;
}

void UTacticalContactComponent::RebuildPreview()
{
	if (!GM.IsValid() || !GM->GetBall() || !Pawn.IsValid()) { return; }

	const FVector Start = GM->GetBall()->GetActorLocation();
	// M11c-4: ONE solver for preview AND execution. The preview is the perfect
	// shot (TimingError = 0); the GameMode re-runs this same function with the
	// actual timing error, so a perfect hit reproduces the dotted line exactly.
	const SEVolleyballTrajectory::FShotSolution Sol =
		SEVolleyballTrajectory::BuildShotSolution(Start, Intent, 0.f);

	Intent.PredictedLanding = Sol.Landing;
	Intent.PredictedFlightTime = Sol.FlightTime;
	Intent.bPredictedCrossedNet = Sol.bCrossedNet;
	Intent.bPredictedNetTouch = Sol.Trajectory.bNetTouch;
	Intent.bPredictedInBounds = Sol.bInBounds;

	if (Preview)
	{
		Preview->ShowPreview(Start, Sol.InitialVelocity);
	}
}

void UTacticalContactComponent::EnterArmed()
{
	State = ETacticalState::TacticalArmed;
	ArmedTimer = ArmedWindowSeconds;
	// Slow motion for the timing window.
	GetWorld()->GetWorldSettings()->TimeDilation = SlowMotionDilation;
	if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("时机窗口：按左键击球（过早/过晚影响质量）")));
	}
	// M11c-5: the timing bar lives on the attack panel; a set keeps its list.
	if (TacticalUI)
	{
		TacticalUI->ShowAttackPanel();
		TacticalUI->ShowTiming(1.f, TEXT(""));
	}
}

void UTacticalContactComponent::ExecuteTimedShot(float TimingError)
{
	Intent.TimingError = TimingError;
	const bool bShot = GM.IsValid() && Pawn.IsValid() && GM->ExecuteTacticalShot(Pawn.Get(), Intent);
	if (bShot)
	{
		RestoreWorldState();
		State = ETacticalState::ContactResolved;
	}
	else
	{
		// The GameMode refused the shot (rules/phase/reach) — restore everything
		// and do NOT claim the shot was completed.
		CancelShot();
	}
}

void UTacticalContactComponent::CancelShot()
{
	RestoreWorldState();
	State = ETacticalState::Normal;
}

void UTacticalContactComponent::RestoreWorldState()
{
	// M11f-1: explicit paired restore — if this component still owns the
	// dilation override (planning 0 / armed 0.15 / defense 0.3), put the ORIGINAL
	// saved value back and clear the ownership. Never guess from the current
	// floating dilation, so a defense exit can no longer strand the world at 0.3.
	if (bTimeOverrideActive)
	{
		bTimeOverrideActive = false;
		bWorldFrozen = false;
		GetWorld()->GetWorldSettings()->TimeDilation = SavedTimeDilation;
	}

	if (Pawn.IsValid() && OwnerPC.IsValid())
	{
		Pawn->EnableInput(OwnerPC.Get());
		OwnerPC->SetShowMouseCursor(false);
		OwnerPC->SetInputMode(FInputModeGameOnly());
	}
	if (Preview) { Preview->HidePreview(); }
	if (HintText) { HintText->SetVisibility(false); }
	if (TacticalUI) { TacticalUI->HideAll(); }
}

// M11c-5: defense planning entry.
void UTacticalContactComponent::EnterDefensePlanning()
{
	if (State == ETacticalState::DefensePlanning) { return; }
	State = ETacticalState::DefensePlanning;
	DefenseTimer = 3.0f;
	DefenseSelected = 0;
	// M11f-1: the defense panel ALSO owns the time-scale override. Save once per
	// override lifetime (a second entry mid-panel must not clobber the original)
	// and mark ownership so every exit path (confirm/cancel/timeout/pause/round
	// end/match over) restores the saved value through the same paired restore.
	if (!bTimeOverrideActive)
	{
		SavedTimeDilation = GetWorld()->GetWorldSettings()->TimeDilation;
		bTimeOverrideActive = true;
	}
	bWorldFrozen = true;
	GetWorld()->GetWorldSettings()->TimeDilation = 0.3f;
	if (Pawn.IsValid() && OwnerPC.IsValid())
	{
		Pawn->DisableInput(OwnerPC.Get());
		OwnerPC->SetShowMouseCursor(true);
		OwnerPC->SetInputMode(FInputModeGameAndUI());
	}
	if (TacticalUI)
	{
		TacticalUI->ShowDefensePanel();
		TacticalUI->UpdateDefenseList(0);
	}
	UE_LOG(LogVolleyballRules, Log, TEXT("[DefensePlanning] opponent attack incoming - player chooses plan"));
}

void UTacticalContactComponent::ConfirmDefensePlan()
{
	// Index 0..7 maps onto EVolleyballDefensePlan (1 == SingleBlock .. 8 == DiveDig).
	const EVolleyballDefensePlan Plan = static_cast<EVolleyballDefensePlan>(DefenseSelected + 1);
	DefensePlan = Plan;
	if (GM.IsValid()) { GM->SetPlayerDefensePlan(Plan); }
	RestoreWorldState();
	State = ETacticalState::Normal;
	if (TacticalUI) { TacticalUI->HideAll(); }
	UE_LOG(LogVolleyballRules, Log, TEXT("[DefensePlan] player chose plan=%d"), DefenseSelected + 1);
}

void UTacticalContactComponent::ApplySetPlayToIntent(int32 Index)
{
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	if (!Plays.IsValidIndex(Index) || !Pawn.IsValid()) { return; }
	const FSetPlayDefinition& Play = Plays[Index];
	// Free trajectory (id 14) keeps the mouse-picked landing and manual arc.
	if (Play.PlayId == 14)
	{
		const FVector Ground = PickGroundPoint();
		if (!Ground.IsZero()) { Intent.TargetLocation = Ground; }
		return;
	}
	Intent.TargetLocation = SESetPlays::MirrorLocal(Play.TargetLocal, Pawn->TeamSide);
	Intent.DesiredFlightTime = Play.DesiredFlightTime;
	Intent.ApexHeight = Play.ApexHeight;
	Intent.TouchType = EBallTouchType::Set;
	UE_LOG(LogVolleyballRules, Log, TEXT("[SetPlayPick] index=%d name=%s target=%s flight=%.2f apex=%.0f"),
		Index, *Play.DisplayName, *Intent.TargetLocation.ToString(), Intent.DesiredFlightTime, Play.ApexHeight);
}

void UTacticalContactComponent::HandleSetPlayPicked(int32 Index)
{
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	if (!Plays.IsValidIndex(Index)) { return; }
	// M11f-1: stale card events (a click that lands after the panel closed, the
	// round ended or the match is over) must never retarget the intent of a NEW
	// round — the callbacks verify the live tactical phase before applying.
	if (State != ETacticalState::TacticalPlanning || PendingTouchType != EBallTouchType::Set) { return; }
	SelectedPlay = Index;
	// A card click selects AND rebuilds the Intent/preview through the same
	// function the keyboard uses, so the dotted line, verdict and the final
	// execution all read the same index/target/flight-time.
	ApplySetPlayToIntent(SelectedPlay);
	RebuildPreview();
	if (TacticalUI) { TacticalUI->UpdateSetList(SelectedPlay); }
}

void UTacticalContactComponent::HandleDefensePicked(int32 Index)
{
	// M11f-1: a card click from an old/closed panel must not arm a defense plan
	// for the next round; selection only applies while the defense panel is live.
	if (State != ETacticalState::DefensePlanning) { return; }
	// Card click SELECTS only — confirmation is a separate world-LMB or Enter
	// (or the AI default after the timer). Old cards must never auto-commit a
	// plan the player was still reading.
	DefenseSelected = FMath::Clamp(Index, 0, 7);
	if (TacticalUI) { TacticalUI->UpdateDefenseList(DefenseSelected); }
}

#if !UE_BUILD_SHIPPING
bool UTacticalContactComponent::DevTacticalStep(int32& PhaseOut, int32 SetPlayIndex,
	const FVector& TargetOverride, float Power, float FlightTime,
	bool bCancel, bool bConfirm, bool bExecute)
{
	PhaseOut = 0;
	if (State == ETacticalState::TacticalPlanning) { PhaseOut = 1; }
	else if (State == ETacticalState::TacticalArmed) { PhaseOut = 2; }
	else { return false; }

	if (PhaseOut == 1)
	{
		if (SetPlayIndex >= 0 && PendingTouchType == EBallTouchType::Set
			&& SESetPlays::GetPlays().IsValidIndex(SetPlayIndex))
		{
			// Go through the production card-click handler so the dev flow
			// exercises the SAME select->intent->preview->execute index chain
			// as a real mouse click (M11e-2).
			HandleSetPlayPicked(SetPlayIndex);
		}
		else if (!TargetOverride.IsZero())
		{
			Intent.TargetLocation = TargetOverride;
		}
		if (Power > 0.f) { Intent.Power = FMath::Clamp(Power, 0.15f, 1.f); }
		if (FlightTime > 0.f) { Intent.DesiredFlightTime = FMath::Clamp(FlightTime, 0.3f, 2.5f); }
		RebuildPreview();

		if (bCancel)
		{
			CancelShot();
		}
		if (bConfirm)
		{
			// Mirror the TickPlanning LMB path: a confirmed SET must register the
			// selected tactic with the GameMode so the attacker run-up follows the
			// play (M11c-5), otherwise the armed phase fires a "generic" set.
			if (PendingTouchType == EBallTouchType::Set && SelectedPlay >= 0 && GM.IsValid())
			{
				GM->SetActiveSetPlay(SelectedPlay);
			}
			EnterArmed();
		}
	}
	else if (PhaseOut == 2 && bCancel)
	{
		// Dev harnesses must be able to dismiss an armed shot after capturing
		// its UI.  Previously bCancel only worked during planning, which left
		// the tactical overlay visible in every later ShotSuite frame.
		CancelShot();
	}
	else if (PhaseOut == 2 && bExecute)
	{
		// Perfect timing: TimingError strictly zero (M11c-4 solver guarantees
		// the preview and the executed shot share the same InitialVelocity).
		ExecuteTimedShot(0.f);
	}
	return true;
}

void UTacticalContactComponent::DevForcePlanning(EBallTouchType Type)
{
	if (State == ETacticalState::Normal)
	{
		EnterPlanning(Type);
	}
}

void UTacticalContactComponent::DevForceDefensePlanning()
{
	if (State == ETacticalState::Normal)
	{
		EnterDefensePlanning();
	}
}

void UTacticalContactComponent::DevConfirmDefense()
{
	if (State == ETacticalState::DefensePlanning)
	{
		ConfirmDefensePlan();
		// The -ShotSuite only wants the panel captured; it must not leave a
		// player-chosen defense plan in place (that would override the AI's
		// natural dive/block decision for the rest of the match).
		if (GM.IsValid()) { GM->SetPlayerDefensePlan(EVolleyballDefensePlan::NoPlan); }
	}
}
#endif
