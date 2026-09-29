// SPDX-License-Identifier: MIT
#include "SpikeElitePlayerController.h"
#include "UI/MainMenuWidget.h"
#include "UI/PauseMenuWidget.h"
#include "UI/SettingsWidget.h"
#include "UI/ScoreboardWidget.h"
#include "SpikeEliteGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/SkeletalMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEMenu, Log, All);

ASpikeElitePlayerController::ASpikeElitePlayerController()
{
	// Pure C++ widgets 鈥?no Blueprint assets needed.
}

void ASpikeElitePlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Load persisted sensitivity from GameUserSettings ini.
	float Saved = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("MouseSensitivity"), Saved, GGameUserSettingsIni);
	}
	MouseSensitivity = FMath::Clamp(Saved, 0.1f, 5.0f);
	UE_LOG(LogSEMenu, Log, TEXT("PC BeginPlay, sensitivity=%.2f, showing main menu"), MouseSensitivity);
	ShowMainMenu();

	// Headless verification hook: -devauto starts a match after 2s, screenshots at 8s, quits at 10s.
	if (FParse::Param(FCommandLine::Get(), TEXT("devauto")))
	{
		GetWorldTimerManager().SetTimer(ShotTimer, this, &ASpikeElitePlayerController::DevAutoStart, 2.0f, false);
	}
}

void ASpikeElitePlayerController::DevAutoStart()
{
	UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: starting match"));
	StartMatch();
	// Screenshot the arena after it has spawned.
	FTimerHandle ShotH, QuitH;
	GetWorldTimerManager().SetTimer(ShotH, [this]()
	{
		ConsoleCommand(TEXT("shot"), true);
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: arena shot requested"));
	}, 6.0f, false);
	GetWorldTimerManager().SetTimer(QuitH, [this]()
	{
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: quitting"));
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}, 9.0f, false);
}

void ASpikeElitePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &ASpikeElitePlayerController::OnPausePressed);
}

void ASpikeElitePlayerController::OnPausePressed()
{
	if (MenuState == EMenuState::Playing) PauseGame();
	else if (MenuState == EMenuState::Paused) ResumeGame();
}

void ASpikeElitePlayerController::SetGameInputMode()
{
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
	bEnableMouseOverEvents = false;
	bEnableClickEvents = false;
	SetPause(false);
}

void ASpikeElitePlayerController::SetUIInputMode()
{
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(nullptr);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
	bEnableMouseOverEvents = true;
	bEnableClickEvents = true;
}

void ASpikeElitePlayerController::HideAllMenus()
{
	if (MainMenu) { MainMenu->RemoveFromParent(); MainMenu = nullptr; }
	if (PauseMenu) { PauseMenu->RemoveFromParent(); PauseMenu = nullptr; }
	if (SettingsMenu) { SettingsMenu->RemoveFromParent(); SettingsMenu = nullptr; }
}

void ASpikeElitePlayerController::ShowMainMenu()
{
	UE_LOG(LogSEMenu, Log, TEXT("ShowMainMenu"));
	HideAllMenus();
	MenuState = EMenuState::MainMenu;
	SetUIInputMode();
	SetPause(false);
	MainMenu = CreateWidget<UMainMenuWidget>(this);
	if (MainMenu)
	{
		MainMenu->OnStart.BindUObject(this, &ASpikeElitePlayerController::StartMatch);
		MainMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromMenu);
		MainMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::QuitToDesktop);
		MainMenu->AddToViewport(10);
		UE_LOG(LogSEMenu, Log, TEXT("MainMenu added. InViewport=%s CachedWidget=%s Vis=%d"),
			MainMenu->IsInViewport() ? TEXT("yes") : TEXT("no"),
			MainMenu->GetCachedWidget().IsValid() ? TEXT("yes") : TEXT("no"),
			(int32)MainMenu->GetVisibility());
	}
	else
	{
		UE_LOG(LogSEMenu, Error, TEXT("MainMenu CreateWidget returned null"));
	}
}

void ASpikeElitePlayerController::StartMatch()
{
	HideAllMenus();
	MenuState = EMenuState::Playing;
	SetGameInputMode();
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StartMatch();
	}
}

void ASpikeElitePlayerController::OnMatchStarted(UScoreboardWidget* InScoreboard)
{
	Scoreboard = InScoreboard;
}

void ASpikeElitePlayerController::PauseGame()
{
	if (MenuState != EMenuState::Playing) return;
	MenuState = EMenuState::Paused;
	SetPause(true);
	SetUIInputMode();	PauseMenu = CreateWidget<UPauseMenuWidget>(this);
	if (PauseMenu)
	{
		PauseMenu->OnResume.BindUObject(this, &ASpikeElitePlayerController::ResumeGame);
		PauseMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromPause);
		PauseMenu->OnMainMenu.BindUObject(this, &ASpikeElitePlayerController::ReturnToMainMenu);
		PauseMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::QuitToDesktop);
		PauseMenu->AddToViewport(20);
	}
}

void ASpikeElitePlayerController::ResumeGame()
{
	if (PauseMenu) { PauseMenu->RemoveFromParent(); PauseMenu = nullptr; }
	MenuState = EMenuState::Playing;
	SetPause(false);
	SetGameInputMode();
}

void ASpikeElitePlayerController::OpenSettingsFromMenu()
{
	if (MainMenu) MainMenu->RemoveFromParent();
	MenuState = EMenuState::SettingsFromMenu;
	SetUIInputMode();	SettingsMenu = CreateWidget<USettingsWidget>(this);
	if (SettingsMenu)
	{
		SettingsMenu->SetCurrentSensitivity(MouseSensitivity);
		SettingsMenu->OnBack.BindUObject(this, &ASpikeElitePlayerController::CloseSettings);
		SettingsMenu->OnSensitivityChanged.BindUObject(this, &ASpikeElitePlayerController::SetMouseSensitivity);
		SettingsMenu->AddToViewport(30);
	}
}

void ASpikeElitePlayerController::OpenSettingsFromPause()
{
	if (PauseMenu) PauseMenu->RemoveFromParent();
	MenuState = EMenuState::SettingsFromPause;
	SetUIInputMode();	SettingsMenu = CreateWidget<USettingsWidget>(this);
	if (SettingsMenu)
	{
		SettingsMenu->SetCurrentSensitivity(MouseSensitivity);
		SettingsMenu->OnBack.BindUObject(this, &ASpikeElitePlayerController::CloseSettings);
		SettingsMenu->OnSensitivityChanged.BindUObject(this, &ASpikeElitePlayerController::SetMouseSensitivity);
		SettingsMenu->AddToViewport(30);
	}
}

void ASpikeElitePlayerController::CloseSettings()
{
	if (SettingsMenu) { SettingsMenu->RemoveFromParent(); SettingsMenu = nullptr; }
	if (MenuState == EMenuState::SettingsFromMenu)
	{
		ShowMainMenu();
	}
	else if (MenuState == EMenuState::SettingsFromPause)
	{
		MenuState = EMenuState::Paused;
		PauseMenu = CreateWidget<UPauseMenuWidget>(this);
		if (PauseMenu)
		{
			PauseMenu->OnResume.BindUObject(this, &ASpikeElitePlayerController::ResumeGame);
			PauseMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromPause);
			PauseMenu->OnMainMenu.BindUObject(this, &ASpikeElitePlayerController::ReturnToMainMenu);
			PauseMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::QuitToDesktop);
			PauseMenu->AddToViewport(20);
		}
	}
}

void ASpikeElitePlayerController::ReturnToMainMenu()
{
	SetPause(false);
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ReturnToMainMenu();
	}
	ShowMainMenu();
}

void ASpikeElitePlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ASpikeElitePlayerController::SaveSettings()
{
	if (GConfig)
	{
		GConfig->SetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void ASpikeElitePlayerController::TakeDevShot()
{
	ConsoleCommand(TEXT("shot D:/projects/spike-elite/Saved/dev_menu.png"), true);
	UE_LOG(LogSEMenu, Log, TEXT("Dev screenshot requested"));
}


