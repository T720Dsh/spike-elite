// SPDX-License-Identifier: MIT
#include "SpikeElitePlayerController.h"
#include "UI/MatchActionWidget.h"
#include "UI/TrajectoryPreviewComponent.h"
#include "Volleyball/VolleyballBall.h"
#include "UI/MainMenuWidget.h"
#include "UI/ModeSelectWidget.h"
#include "UI/PauseMenuWidget.h"
#include "UI/SettingsWidget.h"
#include "UI/ScoreboardWidget.h"
#include "UI/MatchEndWidget.h"
#include "UI/ConfirmWidget.h"
#include "UI/CoachPanelWidget.h"
#include "UI/ServeIntroWidget.h"
#include "UI/TeamRosterWidget.h"
#include "UI/MatchEventWidget.h"
#include "UI/RotationWidget.h"
#include "UI/SEUiStyle.h"
#include "Engine/UserInterfaceSettings.h"
#include "UI/TacticalContactComponent.h"
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "SpikeElitePlayerControllerLog.h"

void ASpikeElitePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Esc must keep working while the game is paused, so enable it there too.
	FInputActionBinding& PauseBind =
		InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &ASpikeElitePlayerController::OnPausePressed);
	PauseBind.bExecuteWhenPaused = true;

	// H toggles the right-top rotation HUD (gameplay only).
	InputComponent->BindAction(TEXT("ToggleRotation"), IE_Pressed, this, &ASpikeElitePlayerController::OnToggleRotation);

	// M11h-6: Tab opens/closes the coach / team-management panel.
	FInputActionBinding& CoachBind =
		InputComponent->BindAction(TEXT("ToggleCoach"), IE_Pressed, this, &ASpikeElitePlayerController::ToggleCoachPanel);
	CoachBind.bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::R,IE_Pressed,this,&ASpikeElitePlayerController::SwitchControlledPlayer);
	InputComponent->BindKey(EKeys::LeftAlt,IE_Pressed,this,&ASpikeElitePlayerController::ToggleMatchMouse);
	InputComponent->BindKey(EKeys::RightAlt,IE_Pressed,this,&ASpikeElitePlayerController::ToggleMatchMouse);
}

void ASpikeElitePlayerController::OnToggleRotation()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		if (GM->GetRotationWidget()) { GM->GetRotationWidget()->ToggleVisible(); }
	}
}

void ASpikeElitePlayerController::OnPausePressed()
{
	if (RosterMenu) { CloseRoster(); return; }
	if (CoachPanel) { ToggleCoachPanel(); return; }
	// M11b-4: while the tactical planning UI is open, Esc cancels the tactical
	// shot instead of opening the pause menu.
	if (Tactical && Tactical->State == ETacticalState::TacticalPlanning)
	{
		Tactical->CancelShot();
		return;
	}

	switch (MenuState)
	{
	case EMenuState::Playing:
		PauseGame();
		break;
	case EMenuState::Paused:
		ResumeGame();
		break;
	case EMenuState::SettingsFromPause:
		CloseSettings();   // returns to the pause menu
		break;
	case EMenuState::SettingsFromMenu:
		CloseSettings();   // returns to the main menu
		break;
	case EMenuState::Confirm:
		CancelConfirm();   // Esc backs out of the confirm dialog
		break;
	case EMenuState::MatchOver:
	default:
		// M10: on the result screen Esc deliberately does NOTHING — it must
		// never revive a finished match. Use the on-screen buttons.
		break;
	}
}

void ASpikeElitePlayerController::SetGameInputMode()
{
	if (auto* GM=GetWorld()?GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>():nullptr)
		if (GM->GetMatchMode()==EGameModeChoice::Coach)
		{
			FInputModeGameAndUI Mode; Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); bShowMouseCursor=true; SetPause(false); return;
		}
	// GameOnly defaults to capturing the mouse permanently for look control.
	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	bShowMouseCursor = false;
	bEnableMouseOverEvents = false;
	bEnableClickEvents = false;

	if (UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		VC->SetMouseLockMode(EMouseLockMode::LockOnCapture);
	}
	SetPause(false);
}

void ASpikeElitePlayerController::SetUIInputMode(UUserWidget* FocusWidget)
{
	// GameAndUI keeps the Esc action routed to this controller (UIOnly does not
	// reliably deliver keyboard input) while the game world is paused so that
	// movement / hit actions cannot leak through.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	if (FocusWidget)
	{
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(Mode);

	bShowMouseCursor = true;
	bEnableMouseOverEvents = true;
	bEnableClickEvents = true;

	if (UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		VC->SetMouseLockMode(EMouseLockMode::DoNotLock);
	}

	// Release any held capture so the cursor can leave the window immediately.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		Slate.ReleaseAllPointerCapture();
	}
}
