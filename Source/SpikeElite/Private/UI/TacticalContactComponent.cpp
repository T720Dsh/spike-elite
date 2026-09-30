// SPDX-License-Identifier: MIT
#include "UI/TacticalContactComponent.h"
#include "UI/TrajectoryPreviewComponent.h"
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "SpikeElitePlayerController.h"
#include "Volleyball/VolleyballBall.h"
#include "Volleyball/SetPlay.h"
#include "Components/TextRenderComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
	constexpr float TacticalWindowSeconds = 0.35f;
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
}

void UTacticalContactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
}

void UTacticalContactComponent::TickPlanning(float DeltaTime)
{
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
	// free trajectory (id 14) falls back to manual arc control.
	const bool bSetTactics = (PendingTouchType == EBallTouchType::Set && SelectedPlay >= 0);
	if (bSetTactics)
	{
		const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
		if (OwnerPC->WasInputKeyJustPressed(EKeys::Q))
		{
			SelectedPlay = (SelectedPlay + 1) % Plays.Num();
		}
		if (OwnerPC->WasInputKeyJustPressed(EKeys::E))
		{
			SelectedPlay = (SelectedPlay - 1 + Plays.Num()) % Plays.Num();
		}
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

	// Confirm / cancel.
	if (OwnerPC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
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
	// Solve the initial velocity for the current target/flight-time.
	FVector Vel = SEVolleyballTrajectory::SolveVelocity(Start, Intent.TargetLocation, Intent.DesiredFlightTime);
	const SEVolleyballTrajectory::FTrajectoryResult R = SEVolleyballTrajectory::Predict(Start, Vel);

	Intent.PredictedLanding = R.Landing;
	Intent.PredictedFlightTime = R.FlightTime;
	Intent.bPredictedCrossedNet = R.bCrossedNet;
	Intent.bPredictedNetTouch = R.bNetTouch;
	Intent.bPredictedInBounds = R.bInBounds;

	if (Preview)
	{
		Preview->ShowPreview(Start, Vel);
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
}

void UTacticalContactComponent::ExecuteTimedShot(float TimingError)
{
	if (GM.IsValid() && Pawn.IsValid())
	{
		Intent.TimingError = TimingError;
		GM->ExecuteTacticalShot(Pawn.Get(), Intent);
	}
	RestoreWorldState();
	State = ETacticalState::ContactResolved;
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
}



