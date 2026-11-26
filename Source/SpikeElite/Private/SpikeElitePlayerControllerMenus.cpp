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

void ASpikeElitePlayerController::HideAllMenus()
{
	ReleaseServeCamera();
	bMatchMouse=false; bServeAiming=false; bServeMouseHidden=false;
	ServeTarget=FVector(-600,0,0); ServeArc=220.f;
	if(MatchActions) { MatchActions->RemoveFromParent(); MatchActions=nullptr; }
	if(ServePreview) ServePreview->HidePreview();
	if(EventHUD) { EventHUD->RemoveFromParent(); EventHUD=nullptr; }
	if(RosterMenu) { RosterMenu->RemoveFromParent(); RosterMenu=nullptr; }
	if(CoachPanel) { CoachPanel->RemoveFromParent(); CoachPanel=nullptr; }
	if(ServeIntro) { ServeIntro->RemoveFromParent(); ServeIntro=nullptr; }
	if(auto* GM=GetWorld()?GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>():nullptr) GM->bCoachPanelOpen=false;
	if (MainMenu)     { MainMenu->RemoveFromParent();     MainMenu = nullptr; }
	if (ModeSelect)   { ModeSelect->RemoveFromParent();   ModeSelect = nullptr; }
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
		MainMenu->OnStart.BindUObject(this, &ASpikeElitePlayerController::ShowModeSelect);
		MainMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromMenu);
		// M11: the main menu "退出游戏" button also goes through the confirm dialog
		// so a stray click cannot kill the process while a match is in progress.
		MainMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::AskQuitToDesktop);
		MainMenu->AddToViewport(10);
		SetUIInputMode(MainMenu);
		MainMenu->SetInitialFocus();   // M11f-3: focus 开始比赛 after the menu is live
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

void ASpikeElitePlayerController::ShowModeSelect()
{
	UE_LOG(LogSEMenu, Log, TEXT("ShowModeSelect"));
	HideAllMenus();
	MenuState = EMenuState::MainMenu;
	SetPause(false);

	ModeSelect = CreateWidget<UModeSelectWidget>(this);
	if (ModeSelect)
	{
		ModeSelect->OnPickQuick.BindLambda([this](bool bShort) { StartMatchAs(EGameModeChoice::QuickMatch, bShort, 0); });
		ModeSelect->OnPickChallenge.BindLambda([this]() { StartMatchAs(EGameModeChoice::Challenge12, true, 0); });
		ModeSelect->OnPickCoach.BindLambda([this]() { StartMatchAs(EGameModeChoice::Coach, false, 0); });
		ModeSelect->OnPickTraining.BindLambda([this]() { StartMatchAs(EGameModeChoice::Training, false, 0); });
		ModeSelect->OnPickDrill.BindUObject(this,&ASpikeElitePlayerController::ShowTrainingDrill);
		ModeSelect->OnBack.BindUObject(this, &ASpikeElitePlayerController::ShowMainMenu);
		ModeSelect->AddToViewport(10);
		SetUIInputMode(ModeSelect);
		ModeSelect->SetInitialFocus();
	}
}

void ASpikeElitePlayerController::StartMatchAs(EGameModeChoice Mode, bool bShortSets, int32 ChallengeStage)
{
	UE_LOG(LogSEMenu, Log, TEXT("StartMatchAs mode=%d short=%d stage=%d"), (int32)Mode, bShortSets ? 1 : 0, ChallengeStage);
	PendingMode.Mode=Mode; PendingMode.bShortSets=bShortSets; PendingMode.ChallengeStage=ChallengeStage;
	PendingMode.Drill=Mode==EGameModeChoice::Training?static_cast<ETrainingDrill>(FMath::Clamp(ChallengeStage,0,2)):ETrainingDrill::None;
	if(Mode==EGameModeChoice::Training) StartPreparedMatch();
	else ShowRoster(true);
}

void ASpikeElitePlayerController::ShowTrainingDrill(int32 Drill)
{
	UE_LOG(LogSEMenu, Log, TEXT("ShowTrainingDrill %d"), Drill);
	StartMatchAs(EGameModeChoice::Training, false, Drill);
}

void ASpikeElitePlayerController::OnMatchStarted(UScoreboardWidget* InScoreboard)
{
	Scoreboard = InScoreboard;
	if(!EventHUD) { EventHUD=CreateWidget<UMatchEventWidget>(this); if(EventHUD) EventHUD->AddToViewport(22); }
	if(auto* GM=GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>(); GM && GM->GetMatchMode()==EGameModeChoice::Coach)
	{
		if(!DevCam) DevCam=GetWorld()->SpawnActor<ACameraActor>();
		if(auto* Camera=Cast<ACameraActor>(DevCam))
		{ const FVector P(1450,-1750,720); Camera->SetActorLocationAndRotation(P,(FVector(0,0,160)-P).Rotation()); Camera->GetCameraComponent()->SetFieldOfView(75); SetViewTarget(Camera); }
		SetGameInputMode(); return;
	}
	// The map's PlayerStart can carry a top-down rotation from the menu scene.
	// Reset the actual controller (not just the spring arm) on EVERY new match.
	if (ASpikeEliteCharacter* C = Cast<ASpikeEliteCharacter>(GetPawn()))
	{
		SetViewTarget(C);
		SetControlRotation(FRotator(-12.f, C->TeamSide > 0 ? 180.f : 0.f, 0.f));
	}
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
	RefreshMatchInteraction();
	SetPause(true);
	BuildPauseMenu();
	SetUIInputMode(PauseMenu);
	PauseMenu->SetInitialFocus();   // M11f-3: focus 继续游戏
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
	HideServeIntro(); // M11h-3: the result screen never keeps the server card.
	// M11f-2: stable broadcast end-of-match view. Point the player camera at the
	// court centre from a slight elevation instead of leaving the last rally's
	// tight framing (player back or scorer text filling the result background).
	if (!DevCam)
	{
		DevCam = GetWorld()->SpawnActor<ACameraActor>();
	}
	if (ACameraActor* ResultCamera = Cast<ACameraActor>(DevCam))
	{
		const FVector Location(1500.f,900.f,650.f);
		ResultCamera->SetActorLocationAndRotation(Location,(FVector(0,0,120)-Location).Rotation());
		ResultCamera->GetCameraComponent()->SetFieldOfView(70.f);
		SetViewTarget(ResultCamera);
	}
	// M11 input gate: FREEZE the world. Without this, character input, bot Tick
	// directives and the ball's projectile all kept running behind the result
	// screen even though MatchState was MatchOver. UI stays fully interactive
	// (buttons/mouse/keyboard are driven by this controller, not the world tick).
	SetPause(true);
	MenuState = EMenuState::MatchOver;
	RefreshMatchInteraction();
	// The result card is a final broadcast state, not another modal stacked on
	// top of live match instrumentation.  Hiding these also prevents a stale
	// score/rotation readout from competing with the final per-set result.
	if (Scoreboard) { Scoreboard->SetVisibility(ESlateVisibility::Collapsed); }
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (GM->GetRotationWidget()) { GM->GetRotationWidget()->SetVisibility(ESlateVisibility::Collapsed); }
	}
	BuildMatchEnd();
	if (MatchEnd)
	{
		ASpikeEliteGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>() : nullptr;
		// M11h-8: per-player stats (attributed from real events) feed the result
		// screen; identities come from the authoritative rosters.
		if (GM)
		{
			MatchEnd->SetResult(Winner, ScoresA, ScoresB,
				GM->StatsA, GM->StatsB, GM->RosterA.Registered, GM->RosterB.Registered);
		}
		else
		{
			MatchEnd->SetResult(Winner, ScoresA, ScoresB,
				TArray<SEVolleyballRules::FPlayerMatchStats>(), TArray<SEVolleyballRules::FPlayerMatchStats>(),
				TArray<FPlayerIdentity>(), TArray<FPlayerIdentity>());
		}
	}
	SetUIInputMode(MatchEnd);
	MatchEnd->SetInitialFocus();   // M11f-3
}

void ASpikeElitePlayerController::Rematch()
{
	UE_LOG(LogSEMenu, Log, TEXT("Rematch requested"));
	if (MatchEnd) { MatchEnd->RemoveFromParent(); MatchEnd = nullptr; }
	HideAllMenus();
	HideServeIntro(); // M11h-3: never carry the server card into a fresh match
	MenuState = EMenuState::Playing;
	SetGameInputMode();
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StartMatch();   // StartMatch cleans up the finished match first.
	}
}

void ASpikeElitePlayerController::ShowConfirm(const FString& Message, EMenuState RestoreState, TFunction<void()> Action)
{
	// M11: never stack a second dialog. If one is already up, just refocus it
	// (the caller was a duplicate "返回主菜单"/"退出" press).
	if (Confirm)
	{
		SetUIInputMode(Confirm);
		Confirm->SetInitialFocus();   // M11f-3: duplicate press just refocuses 取消
		return;
	}

	StateBeforeConfirm = RestoreState;
	PendingConfirmAction = MoveTemp(Action);

	Confirm = CreateWidget<UConfirmWidget>(this);
	if (Confirm)
	{
		Confirm->SetMessage(Message);
		Confirm->OnConfirm.BindUObject(this, &ASpikeElitePlayerController::AcceptConfirm);
		Confirm->OnCancel.BindUObject(this, &ASpikeElitePlayerController::CancelConfirm);
		Confirm->AddToViewport(40);
		// M11: the confirm dialog BECOMES the active state. Esc then routes to
		// CancelConfirm instead of accidentally resuming the game, and the pause/
		// result screens underneath can never be operated behind the dialog.
		MenuState = EMenuState::Confirm;
		SetUIInputMode(Confirm);
		Confirm->SetInitialFocus();   // M11f-3: safe default = 取消
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

	// M11: restore exactly the state that opened the dialog — including Playing
	// (a confirm opened mid-match and cancelled must NOT kick the player back to
	// the main menu) — and rebuild the target widget if it was lost somehow.
	if (StateBeforeConfirm == EMenuState::Paused && PauseMenu)
	{
		MenuState = EMenuState::Paused;
		SetPause(true);
		SetUIInputMode(PauseMenu);
		PauseMenu->SetInitialFocus();   // M11f-3
	}
	else if (StateBeforeConfirm == EMenuState::MatchOver && MatchEnd)
	{
		MenuState = EMenuState::MatchOver;
		SetPause(true);
		SetUIInputMode(MatchEnd);
		MatchEnd->SetInitialFocus();   // M11f-3
	}
	else if (StateBeforeConfirm == EMenuState::Playing)
	{
		MenuState = EMenuState::Playing;
		SetPause(false);
		SetGameInputMode();
	}
	else
	{
		MenuState = EMenuState::MainMenu;
		SetPause(false);
		if (MainMenu) { SetUIInputMode(MainMenu); MainMenu->SetInitialFocus(); }   // M11f-3
		else { ShowMainMenu(); }
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
		SettingsMenu->SetInitialFocus();   // M11f-3: safe default = 返回
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
		SettingsMenu->SetInitialFocus();   // M11f-3: safe default = 返回
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
		PauseMenu->SetInitialFocus();   // M11f-3
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
