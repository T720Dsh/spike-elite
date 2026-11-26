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

DEFINE_LOG_CATEGORY(LogSEMenu);

ASpikeElitePlayerController::ASpikeElitePlayerController()
{
	// All menus are built in pure C++ UMG; no Blueprint assets are required.
}

void ASpikeElitePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// M11b-4: tactical slow-motion shot planner (human only, runtime component).
	Tactical = NewObject<UTacticalContactComponent>(this);
	Tactical->RegisterComponent();
	// Under -devauto the automation flow is unattended; the tactical planning
	// UI must not freeze the world waiting for a human to pick a target.
	// -TacticalTest is the exception: it drives the REAL tactical UMG and
	// needs the planning windows ENABLED (mode 2 = all touches).
	if (FParse::Param(FCommandLine::Get(), TEXT("devauto")))
	{
		Tactical->TacticalMode = FParse::Param(FCommandLine::Get(), TEXT("TacticalTest")) ? 2 : 0;
	}

	// Load persisted sensitivity from GameUserSettings ini.
	float Saved = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("MouseSensitivity"), Saved, GGameUserSettingsIni);
	}
	MouseSensitivity = FMath::Clamp(Saved, MinSensitivity(), MaxSensitivity());
	float SavedUIScale = 1.f;
	bool bSavedReducedMotion = false;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("UIScale"), SavedUIScale, GGameUserSettingsIni);
		GConfig->GetBool(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("ReducedMotion"), bSavedReducedMotion, GGameUserSettingsIni);
	}
	GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = FMath::Clamp(SavedUIScale, 0.8f, 1.4f);
	SEUiStyle::SetReducedMotion(bSavedReducedMotion);
#if !UE_BUILD_SHIPPING
	float DevScale=0.f;
	if(FParse::Value(FCommandLine::Get(),TEXT("UIScale="),DevScale)) GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale=FMath::Clamp(DevScale,.8f,1.4f);
	if(FParse::Param(FCommandLine::Get(),TEXT("ReducedMotion"))) SEUiStyle::SetReducedMotion(true);
#endif
	UE_LOG(LogSEMenu, Log, TEXT("PC BeginPlay, sensitivity=%.2f, showing main menu"), MouseSensitivity);
	ShowMainMenu();

#if !UE_BUILD_SHIPPING
	// Headless verification hook: -devauto walks every menu and viewpoint,
	// screenshots each, then quits. Timers are unpausable so the pause-menu
	// shot and the final quit still fire while the world is paused.
	if (FParse::Param(FCommandLine::Get(), TEXT("devauto")))
	{
		GetWorldTimerManager().SetTimer(DevTimer, this, &ASpikeElitePlayerController::DevAutoStart, 1.2f, false);
	}
#endif
}
