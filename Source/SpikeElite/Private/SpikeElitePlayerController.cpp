// SPDX-License-Identifier: MIT
#include "SpikeElitePlayerController.h"
#include "UI/MainMenuWidget.h"
#include "UI/PauseMenuWidget.h"
#include "UI/SettingsWidget.h"
#include "UI/ScoreboardWidget.h"
#include "UI/MatchEndWidget.h"
#include "UI/ConfirmWidget.h"
#include "SpikeEliteGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEMenu, Log, All);

ASpikeElitePlayerController::ASpikeElitePlayerController()
{
	// All menus are built in pure C++ UMG; no Blueprint assets are required.
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
	MouseSensitivity = FMath::Clamp(Saved, MinSensitivity(), MaxSensitivity());
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

#if !UE_BUILD_SHIPPING
void ASpikeElitePlayerController::DevShot(const FString& Name)
{
	// Plain Screenshot defaults to bShowUI=false (3D only). "SHOWUI" makes it
	// composite Slate/UMG (menus) into the captured backbuffer.
	ConsoleCommand(FString::Printf(TEXT("Screenshot SHOWUI filename=%s"), *Name), true);
	UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: shot requested: %s"), *Name);
}

void ASpikeElitePlayerController::DevView(const FVector& Loc, const FRotator& Rot)
{
	if (!DevCam)
	{
		DevCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform());
		if (ACameraActor* Cam = Cast<ACameraActor>(DevCam))
		{
			Cam->GetCameraComponent()->SetFieldOfView(78.f);
		}
	}
	if (DevCam)
	{
		DevCam->SetActorLocationAndRotation(Loc, Rot);
		SetViewTarget(DevCam);
	}
}

void ASpikeElitePlayerController::DevViewPlayer()
{
	if (APawn* P = GetPawn())
	{
		// Returning from a dev camera can leave the view pitched straight down;
		// restore a natural slightly-elevated third-person aim.
		SetControlRotation(FRotator(-12.f, GetControlRotation().Yaw, 0.f));
		SetViewTarget(P);
	}
}

void ASpikeElitePlayerController::DevAutoStart()
{
	// -QuickMatch: drive a full unattended quick match (menu -> match -> AI plays
	// until MatchOver -> screenshot the result screen -> quit).
	if (FParse::Param(FCommandLine::Get(), TEXT("QuickMatch")))
	{
		DevQuickMatch();
		return;
	}

	// UE 5.8 removed per-timer bPauseable, and a paused game world no longer
	// ticks its timer manager. Drive the whole sequence from the core ticker,
	// which keeps running while the game is paused, so the pause-menu shot and
	// the final quit still fire.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };

	// --- Front end (world is on the main menu) ---
	At(1.5f,  [this]() { DevShot(TEXT("shot_01_menu")); });
	At(2.4f,  [this]() { OpenSettingsFromMenu(); });
	At(3.4f,  [this]() { DevShot(TEXT("shot_02_settings")); });
	At(4.2f,  [this]() { CloseSettings(); });
	At(4.8f,  [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: starting match")); StartMatch(); });

	// --- Third-person style broadcast view of the full court ---
	At(9.6f,  [this]()
	{
		const FVector Loc(1250.f, 0.f, 430.f);
		const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(-120.f, 0.f, 170.f));
		DevView(Loc, Rot);
	});
	At(10.5f, [this]() { DevShot(TEXT("shot_03_court")); });

	// --- Side/oblique close-up of the net (shows top/bottom tapes & gap) ---
	At(11.4f, [this]()
	{
		const FVector Loc(360.f, -470.f, 200.f);
		const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(0.f, 0.f, 195.f));
		DevView(Loc, Rot);
	});
	At(12.3f, [this]() { DevShot(TEXT("shot_04_net")); });

	// --- High corner: stands, seating aisles and the roof ---
	At(13.2f, [this]()
	{
		const FVector Loc(1650.f, 1150.f, 880.f);
		const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(0.f, 0.f, 220.f));
		DevView(Loc, Rot);
	});
	At(14.1f, [this]() { DevShot(TEXT("shot_05_stands_roof")); });

	// --- Back to player, open the pause menu and shoot it ---
	At(15.0f, [this]() { DevViewPlayer(); });
	At(15.4f, [this]() { PauseGame(); });
	At(16.3f, [this]() { DevShot(TEXT("shot_06_pause")); });
	At(17.6f, [this]() { ResumeGame(); });

	// --- Lifecycle cycle test: menu -> match -> menu -> match (twice) to prove
	// cleanup leaves no duplicate court/ball/players and level lights survive.
	At(20.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: return to menu #1")); ReturnToMainMenu(); });
	At(22.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: start match #2")); StartMatch(); });
	At(26.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: return to menu #2")); ReturnToMainMenu(); });
	At(28.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: start match #3")); StartMatch(); });

	At(32.0f, [this]()
	{
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: quitting"));
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	});

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double StartSeconds = FPlatformTime::Seconds();
	int32 Index = 0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, StartSeconds, Index, Events = MoveTemp(Events)](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}
		return Index < Events.Num();
	}));
}

void ASpikeElitePlayerController::DevQuickMatch()
{
	// Unattended -QuickMatch: menu shot, start the match, then poll until
	// MatchOver, screenshot the result screen and quit. Serve timeouts let the
	// AI finish a full match without keyboard input.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };

	At(1.5f, [this]() { DevShot(TEXT("shot_qm_01_menu")); });
	At(4.0f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: starting")); StartMatch(); });

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double StartSeconds = FPlatformTime::Seconds();
	int32 Index = 0;
	float ShotAt = -1.0f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, StartSeconds, Index, Events = MoveTemp(Events), ShotAt](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}

		if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(PC)))
		{
			if (GM->MatchState == EMatchState::MatchOver && ShotAt < 0.0f)
			{
				ShotAt = static_cast<float>(Elapsed);
				UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: MatchOver reached (winner A=%d B=%d), shooting result screen"),
					GM->TeamASetsWon, GM->TeamBSetsWon);
				PC->DevShot(TEXT("shot_qm_02_matchover"));
			}
			if (ShotAt > 0.0f && (Elapsed - ShotAt) > 3.0f)
			{
				UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: quitting"));
				UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
				return false;
			}
		}

		if (Elapsed > 300.0)
		{
			UE_LOG(LogSEMenu, Error, TEXT("DEV QUICK MATCH: timed out waiting for MatchOver"));
			UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
			return false;
		}
		return true;
	}));
}
#endif

void ASpikeElitePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Esc must keep working while the game is paused, so enable it there too.
	FInputActionBinding& PauseBind =
		InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &ASpikeElitePlayerController::OnPausePressed);
	PauseBind.bExecuteWhenPaused = true;
}

void ASpikeElitePlayerController::OnPausePressed()
{
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

void ASpikeElitePlayerController::HideAllMenus()
{
	if (MainMenu)     { MainMenu->RemoveFromParent();     MainMenu = nullptr; }
	if (PauseMenu)    { PauseMenu->RemoveFromParent();    PauseMenu = nullptr; }
	if (SettingsMenu) { SettingsMenu->RemoveFromParent(); SettingsMenu = nullptr; }
	if (MatchEnd)     { MatchEnd->RemoveFromParent();     MatchEnd = nullptr; }
	if (Confirm)      { Confirm->RemoveFromParent();      Confirm = nullptr; }
	PendingConfirmAction = nullptr;
}

void ASpikeElitePlayerController::ShowMainMenu()
{
	UE_LOG(LogSEMenu, Log, TEXT("ShowMainMenu"));
	HideAllMenus();
	MenuState = EMenuState::MainMenu;
	SetPause(false);

	MainMenu = CreateWidget<UMainMenuWidget>(this);
	if (MainMenu)
	{
		MainMenu->OnStart.BindUObject(this, &ASpikeElitePlayerController::StartMatch);
		MainMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromMenu);
		MainMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::QuitToDesktop);
		MainMenu->AddToViewport(10);
		SetUIInputMode(MainMenu);
		UE_LOG(LogSEMenu, Log, TEXT("MainMenu added. InViewport=%s"),
			MainMenu->IsInViewport() ? TEXT("yes") : TEXT("no"));
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

void ASpikeElitePlayerController::BuildPauseMenu()
{
	PauseMenu = CreateWidget<UPauseMenuWidget>(this);
	if (PauseMenu)
	{
		PauseMenu->OnResume.BindUObject(this, &ASpikeElitePlayerController::ResumeGame);
		PauseMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromPause);
		PauseMenu->OnMainMenu.BindUObject(this, &ASpikeElitePlayerController::AskReturnToMainMenu);
		PauseMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::AskQuitToDesktop);
		PauseMenu->AddToViewport(20);
	}
}

void ASpikeElitePlayerController::PauseGame()
{
	if (MenuState != EMenuState::Playing) return;
	MenuState = EMenuState::Paused;
	SetPause(true);
	BuildPauseMenu();
	SetUIInputMode(PauseMenu);
}

void ASpikeElitePlayerController::ResumeGame()
{
	if (PauseMenu) { PauseMenu->RemoveFromParent(); PauseMenu = nullptr; }
	MenuState = EMenuState::Playing;
	SetPause(false);
	SetGameInputMode();
}

void ASpikeElitePlayerController::BuildMatchEnd()
{
	MatchEnd = CreateWidget<UMatchEndWidget>(this);
	if (MatchEnd)
	{
		MatchEnd->OnRematch.BindUObject(this, &ASpikeElitePlayerController::Rematch);
		MatchEnd->OnMainMenu.BindUObject(this, &ASpikeElitePlayerController::AskReturnToMainMenu);
		MatchEnd->OnQuit.BindUObject(this, &ASpikeElitePlayerController::AskQuitToDesktop);
		MatchEnd->AddToViewport(20);
	}
}

void ASpikeElitePlayerController::OnMatchOver(const TArray<int32>& ScoresA, const TArray<int32>& ScoresB, EVolleyballTeam Winner)
{
	UE_LOG(LogSEMenu, Log, TEXT("Match over: winner=%s"), Winner == EVolleyballTeam::TeamA ? TEXT("A") : Winner == EVolleyballTeam::TeamB ? TEXT("B") : TEXT("-"));
	// The match is finished; the world may stay unpaused (GameMode stops
	// updating at MatchOver), but we must release the mouse.
	SetPause(false);
	MenuState = EMenuState::MatchOver;
	BuildMatchEnd();
	if (MatchEnd) { MatchEnd->SetResult(Winner, ScoresA, ScoresB); }
	SetUIInputMode(MatchEnd);
}

void ASpikeElitePlayerController::Rematch()
{
	UE_LOG(LogSEMenu, Log, TEXT("Rematch requested"));
	if (MatchEnd) { MatchEnd->RemoveFromParent(); MatchEnd = nullptr; }
	HideAllMenus();
	MenuState = EMenuState::Playing;
	SetGameInputMode();
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StartMatch();   // StartMatch cleans up the finished match first.
	}
}

void ASpikeElitePlayerController::ShowConfirm(const FString& Message, EMenuState RestoreState, TFunction<void()> Action)
{
	StateBeforeConfirm = RestoreState;
	PendingConfirmAction = MoveTemp(Action);

	Confirm = CreateWidget<UConfirmWidget>(this);
	if (Confirm)
	{
		Confirm->SetMessage(Message);
		Confirm->OnConfirm.BindUObject(this, &ASpikeElitePlayerController::AcceptConfirm);
		Confirm->OnCancel.BindUObject(this, &ASpikeElitePlayerController::CancelConfirm);
		Confirm->AddToViewport(40);
		SetUIInputMode(Confirm);
	}
}

void ASpikeElitePlayerController::AcceptConfirm()
{
	if (Confirm) { Confirm->RemoveFromParent(); Confirm = nullptr; }
	TFunction<void()> Action = MoveTemp(PendingConfirmAction);
	PendingConfirmAction = nullptr;
	if (Action) { Action(); }
}

void ASpikeElitePlayerController::CancelConfirm()
{
	if (Confirm) { Confirm->RemoveFromParent(); Confirm = nullptr; }
	PendingConfirmAction = nullptr;

	if (StateBeforeConfirm == EMenuState::Paused && PauseMenu)
	{
		MenuState = EMenuState::Paused;
		SetPause(true);
		SetUIInputMode(PauseMenu);
	}
	else if (StateBeforeConfirm == EMenuState::MatchOver && MatchEnd)
	{
		MenuState = EMenuState::MatchOver;
		SetPause(false);
		SetUIInputMode(MatchEnd);
	}
	else
	{
		MenuState = EMenuState::MainMenu;
		SetPause(false);
		SetUIInputMode(MainMenu);
	}
}

void ASpikeElitePlayerController::AskReturnToMainMenu()
{
	ShowConfirm(TEXT("返回主菜单？未结束的比赛进度将丢失。"), MenuState,
		[this]() { LeaveToMainMenu(); });
}

void ASpikeElitePlayerController::AskQuitToDesktop()
{
	ShowConfirm(TEXT("确定退出游戏？"), MenuState,
		[this]() { QuitNow(); });
}

void ASpikeElitePlayerController::LeaveToMainMenu()
{
	SetPause(false);
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ReturnToMainMenu();
	}
	Scoreboard = nullptr;   // GameMode destroyed its scoreboard widget.
	if (MatchEnd) { MatchEnd->RemoveFromParent(); MatchEnd = nullptr; }
	ShowMainMenu();
}

void ASpikeElitePlayerController::ReturnToMainMenu()
{
	// Direct version used by dev automation and lifecycle tests (no dialog).
	LeaveToMainMenu();
}

void ASpikeElitePlayerController::QuitToDesktop()
{
	UE_LOG(LogSEMenu, Log, TEXT("Quit to desktop requested"));
	QuitNow();
}

void ASpikeElitePlayerController::QuitNow()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ASpikeElitePlayerController::OpenSettingsFromMenu()
{
	if (MainMenu) MainMenu->RemoveFromParent();
	MainMenu = nullptr;
	MenuState = EMenuState::SettingsFromMenu;

	SettingsMenu = CreateWidget<USettingsWidget>(this);
	if (SettingsMenu)
	{
		SettingsMenu->InitFromCurrentSettings();
		SettingsMenu->SetCurrentSensitivity(MouseSensitivity);
		SettingsMenu->OnBack.BindUObject(this, &ASpikeElitePlayerController::CloseSettings);
		SettingsMenu->OnApply.BindUObject(this, &ASpikeElitePlayerController::SaveSettings);
		SettingsMenu->OnSensitivityChanged.BindUObject(this, &ASpikeElitePlayerController::SetMouseSensitivity);
		SettingsMenu->AddToViewport(30);
		SetUIInputMode(SettingsMenu);
	}
}

void ASpikeElitePlayerController::OpenSettingsFromPause()
{
	if (PauseMenu) PauseMenu->RemoveFromParent();
	PauseMenu = nullptr;
	MenuState = EMenuState::SettingsFromPause;

	SettingsMenu = CreateWidget<USettingsWidget>(this);
	if (SettingsMenu)
	{
		SettingsMenu->InitFromCurrentSettings();
		SettingsMenu->SetCurrentSensitivity(MouseSensitivity);
		SettingsMenu->OnBack.BindUObject(this, &ASpikeElitePlayerController::CloseSettings);
		SettingsMenu->OnApply.BindUObject(this, &ASpikeElitePlayerController::SaveSettings);
		SettingsMenu->OnSensitivityChanged.BindUObject(this, &ASpikeElitePlayerController::SetMouseSensitivity);
		SettingsMenu->AddToViewport(30);
		SetUIInputMode(SettingsMenu);
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
		SetPause(true);
		BuildPauseMenu();
		SetUIInputMode(PauseMenu);
	}
}

void ASpikeElitePlayerController::SaveSettings()
{
	if (GConfig)
	{
		GConfig->SetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
