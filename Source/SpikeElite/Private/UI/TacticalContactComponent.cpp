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
			const bool bBallOnTheirSide = (PossTeam == EVolleyballTeam::TeamA) ? (BallLoc.X < 0.f) : (BallLoc.X > 0.f);
			const float SideX = (MyTeam == EVolleyballTeam::TeamA) ? 1.f : -1.f;
			const bool bApproachingNet = (FMath::Abs(BallLoc.X) < 700.f);
			if (bBallOnTheirSide && bApproachingNet && BallVel.Size() > 350.f && BallLoc.Z < 420.f
				&& TacticalMode >= 1)
			{
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
		if (OwnerPC->WasInputKeyJustPressed(EKeys::LeftMouseButton) || OwnerPC->WasInputKeyJustPressed(EKeys::Enter))
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
	SavedTimeDilation = GetWorld()->GetWorldSettings()->TimeDilation;
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

	// Mouse -> landing spot (only for free aim; a selected set tactic locks the
	// target to the mirrored play target).
	const bool bSetTacticLocked = (PendingTouchType == EBallTouchType::Set && SelectedPlay >= 0
		&& SESetPlays::GetPlays().IsValidIndex(SelectedPlay)
		&& SESetPlays::GetPlays()[SelectedPlay].PlayId != 14);
	if (!bSetTacticLocked)
	{
		const FVector Ground = PickGroundPoint();
		if (!Ground.IsZero())
		{
			Intent.TargetLocation = Ground;
		}
	}

	// Power: wheel (fast) or W/S (held).
	const float Wheel = OwnerPC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	if (FMath::Abs(Wheel) > 0.01f)
	{
		Intent.Power = FMath::Clamp(Intent.Power + Wheel * 0.05f, 0.15f, 1.f);
	}
	if (OwnerPC->IsInputKeyDown(EKeys::W)) { Intent.Power = FMath::Clamp(Intent.Power + DeltaTime * 0.4f, 0.15f, 1.f); }
	if (OwnerPC->IsInputKeyDown(EKeys::S)) { Intent.Power = FMath::Clamp(Intent.Power - DeltaTime * 0.4f, 0.15f, 1.f); }

	// Arc / flight time: Q up (higher), E down.
	// M11b-5b: while setting, Q/E cycle the data-driven tactic list instead;
	// free trajectory (id 14) falls back to manual arc control. M11c-5 adds
	// Up/Down keyboard cycling and updates the screen list.
	const bool bSetTactics = (PendingTouchType == EBallTouchType::Set && SelectedPlay >= 0);
	if (bSetTactics)
	{
		const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
		if (OwnerPC->WasInputKeyJustPressed(EKeys::Q) || OwnerPC->WasInputKeyJustPressed(EKeys::Up))
		{
			SelectedPlay = (SelectedPlay + 1) % Plays.Num();
		}
		if (OwnerPC->WasInputKeyJustPressed(EKeys::E) || OwnerPC->WasInputKeyJustPressed(EKeys::Down))
		{
			SelectedPlay = (SelectedPlay - 1 + Plays.Num()) % Plays.Num();
		}
		if (TacticalUI) { TacticalUI->UpdateSetList(SelectedPlay); }
		// Mouse still picks the landing when free trajectory is selected.
		if (Plays.IsValidIndex(SelectedPlay) && Plays[SelectedPlay].PlayId != 14)
		{
			const FSetPlayDefinition& Play = Plays[SelectedPlay];
			const FVector Target = SESetPlays::MirrorLocal(Play.TargetLocal, Pawn->TeamSide);
			Intent.TargetLocation = Target;
			Intent.DesiredFlightTime = Play.DesiredFlightTime;
		}
		if (HintText)
		{
			const FString Name = Plays.IsValidIndex(SelectedPlay) ? Plays[SelectedPlay].DisplayName : TEXT("?");
			HintText->SetText(FText::FromString(FString::Printf(TEXT("二传战术：%s  Q/E切换 · 左键确认 · 右键取消"), *Name)));
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

	// Confirm / cancel.
	if (OwnerPC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (PendingTouchType == EBallTouchType::Set && GM.IsValid())
		{
			GM->SetActiveSetPlay(SelectedPlay);   // M11c-5: attacker run-up follows the play
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
	if (bWorldFrozen)
	{
		bWorldFrozen = false;
		GetWorld()->GetWorldSettings()->TimeDilation = SavedTimeDilation;
	}
	else if (GetWorld()->GetWorldSettings()->TimeDilation <= 0.2f)
	{
		// Armed phase: restore the pre-tactical dilation.
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
	SavedTimeDilation = GetWorld()->GetWorldSettings()->TimeDilation;
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

void UTacticalContactComponent::HandleSetPlayPicked(int32 Index)
{
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	if (Plays.IsValidIndex(Index))
	{
		SelectedPlay = Index;
		if (TacticalUI) { TacticalUI->UpdateSetList(SelectedPlay); }
	}
}

void UTacticalContactComponent::HandleDefensePicked(int32 Index)
{
	DefenseSelected = FMath::Clamp(Index, 0, 7);
	if (TacticalUI) { TacticalUI->UpdateDefenseList(DefenseSelected); }
	ConfirmDefensePlan();
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
			SelectedPlay = SetPlayIndex;
			const FSetPlayDefinition& Play = SESetPlays::GetPlays()[SetPlayIndex];
			Intent.TargetLocation = SESetPlays::MirrorLocal(Play.TargetLocal, Pawn->TeamSide);
			Intent.DesiredFlightTime = Play.DesiredFlightTime;
			if (TacticalUI) { TacticalUI->UpdateSetList(SelectedPlay); }
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
