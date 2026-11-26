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

// ---------------- M11h-6: coach / team-management panel ----------------

void ASpikeElitePlayerController::ToggleCoachPanel()
{
	if(RosterMenu) return; // roster selection is the sole live management modal
	// The panel is a live-play tool: never open over the pause menu, confirm
	// dialog or result screen (they own the input mode).
	if (MenuState != EMenuState::Playing) { return; }
	if (CoachPanel)
	{
		CoachPanel->RemoveFromParent();
		CoachPanel = nullptr;
		if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
		{
			GM->bCoachPanelOpen = false;
		}
		SetGameInputMode();
		return;
	}

	CoachPanel = CreateWidget<UCoachPanelWidget>(this);
	if (!CoachPanel) { return; }
	CoachPanel->OnRequestTimeoutA.BindUObject(this, &ASpikeElitePlayerController::HandleCoachTimeoutA);
	CoachPanel->OnRequestTimeoutB.BindUObject(this, &ASpikeElitePlayerController::HandleCoachTimeoutB);
	CoachPanel->OnSubA.BindUObject(this, &ASpikeElitePlayerController::HandleCoachSubA);
	CoachPanel->OnSubB.BindUObject(this, &ASpikeElitePlayerController::HandleCoachSubB);
	CoachPanel->OnCycleServeZone.BindUObject(this, &ASpikeElitePlayerController::HandleCoachServeZone);
	CoachPanel->OnCycleBlock.BindUObject(this, &ASpikeElitePlayerController::HandleCoachBlock);
	CoachPanel->OnCycleDefense.BindUObject(this, &ASpikeElitePlayerController::HandleCoachDefense);
	CoachPanel->OnCycleSetter.BindUObject(this, &ASpikeElitePlayerController::HandleCoachSetter);
	CoachPanel->OnClosePanel.BindUObject(this, &ASpikeElitePlayerController::ToggleCoachPanel);
	CoachPanel->AddToViewport(30);
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		GM->bCoachPanelOpen = true;
	}
	// The panel is mouse-driven; keep the world running underneath (no pause).
	if (Tactical) { Tactical->CancelShot(); }
	SetUIInputMode(CoachPanel);
	CoachPanel->SetInitialFocus();
	RefreshCoachPanel();
}

void ASpikeElitePlayerController::RefreshCoachPanel()
{
	if (!CoachPanel) { return; }
	ASpikeEliteGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>() : nullptr;
	if (!GM) { return; }
	const FString Status = FString::Printf(
		TEXT("%s %d:%d %s · 暂停 A:%d B:%d · 换人 A:%d B:%d"),
		GM->ServingTeam == EVolleyballTeam::TeamA ? TEXT("A") :
			GM->ServingTeam == EVolleyballTeam::TeamB ? TEXT("B") : TEXT("-"),
		GM->TeamAScore, GM->TeamBScore,
		GM->GetPhaseLabel(), GM->TimeoutLeftA, GM->TimeoutLeftB,
		GM->SubstitutionsLeftA, GM->SubstitutionsLeftB);
	CoachPanel->SetStatus(Status);
	CoachPanel->SetPrefLabels(CoachZoneLabel(GM->GetCoach(EVolleyballTeam::TeamA)),
		CoachBlockLabel(GM->GetCoach(EVolleyballTeam::TeamA)),
		CoachDefenseLabel(GM->GetCoach(EVolleyballTeam::TeamA)),
		CoachSetterLabel(GM->GetCoach(EVolleyballTeam::TeamA)));
}

void ASpikeElitePlayerController::HandleCoachTimeoutA()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		GM->RequestTeamTimeout(EVolleyballTeam::TeamA);
		RefreshCoachPanel();
	}
}
void ASpikeElitePlayerController::HandleCoachTimeoutB()
{
	if(CoachPanel) CoachPanel->SetStatus(TEXT("你执教 A 队，不能替对手申请暂停。"));
}
void ASpikeElitePlayerController::HandleCoachSubA()
{
	ShowRoster(false);
}
void ASpikeElitePlayerController::HandleCoachSubB()
{
	// A human team manager cannot issue instructions to the opponent.
	if(CoachPanel) CoachPanel->SetStatus(TEXT("你执教 A 队，无法为对手申请换人。"));
}
void ASpikeElitePlayerController::HandleCoachServeZone()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		FCoachPreferences P = GM->GetCoach(EVolleyballTeam::TeamA);
		P.ServeZone = (P.ServeZone + 2) % 3 - 1; // -1 -> 0 -> 1 -> -1
		GM->SetCoachPreference(EVolleyballTeam::TeamA, P);
		RefreshCoachPanel();
	}
}
void ASpikeElitePlayerController::HandleCoachBlock()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		FCoachPreferences P = GM->GetCoach(EVolleyballTeam::TeamA);
		P.BlockPreference = (P.BlockPreference + 1) % 3; // 0/1/2 cycle
		GM->SetCoachPreference(EVolleyballTeam::TeamA, P);
		RefreshCoachPanel();
	}
}
void ASpikeElitePlayerController::HandleCoachDefense()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		FCoachPreferences P = GM->GetCoach(EVolleyballTeam::TeamA);
		P.DefenseDepth = (P.DefenseDepth + 2) % 3 - 1; // -1 -> 0 -> 1 -> -1
		GM->SetCoachPreference(EVolleyballTeam::TeamA, P);
		RefreshCoachPanel();
	}
}
void ASpikeElitePlayerController::HandleCoachSetter()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		FCoachPreferences P = GM->GetCoach(EVolleyballTeam::TeamA);
		P.SetterPreference = (P.SetterPreference + 1) % 5; // 0..4 cycle
		GM->SetCoachPreference(EVolleyballTeam::TeamA, P);
		RefreshCoachPanel();
	}
}

void ASpikeElitePlayerController::ShowServeIntro(const FString& PlayerId, int32 JerseyNumber,
	const FString& Name, EVolleyballTeam Team, const FString& RoleText, bool bShortBar)
{
	if (!ServeIntro)
	{
		ServeIntro = CreateWidget<UServeIntroWidget>(this);
	}
	if (!ServeIntro) { return; }
	const FString TeamLabel = (Team == EVolleyballTeam::TeamA) ? TEXT("TEAM A")
		: (Team == EVolleyballTeam::TeamB) ? TEXT("TEAM B") : TEXT("-");
	// Build the widget tree FIRST (AddToViewport triggers RebuildWidget), then
	// populate: packaged builds otherwise hit SetServer before the tree exists.
	if(!ServeIntro->IsInViewport()) ServeIntro->AddToViewport(25);
	ServeIntro->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ServeIntro->SetServer(JerseyNumber, Name, TeamLabel, RoleText, bShortBar);
}

void ASpikeElitePlayerController::HideServeIntro()
{
	if (ServeIntro)
	{
		ServeIntro->FadeOut();
	}
}

FString ASpikeElitePlayerController::CoachZoneLabel(const FCoachPreferences& P)
{
	return (P.ServeZone < 0) ? TEXT("左") : (P.ServeZone > 0) ? TEXT("右") : TEXT("中");
}
FString ASpikeElitePlayerController::CoachBlockLabel(const FCoachPreferences& P)
{
	return (P.BlockPreference == 0) ? TEXT("不拦") : (P.BlockPreference == 2) ? TEXT("双人") : TEXT("单人");
}
FString ASpikeElitePlayerController::CoachDefenseLabel(const FCoachPreferences& P)
{
	return (P.DefenseDepth < 0) ? TEXT("前压") : (P.DefenseDepth > 0) ? TEXT("后撤") : TEXT("标准");
}
FString ASpikeElitePlayerController::CoachSetterLabel(const FCoachPreferences& P)
{
	switch (P.SetterPreference)
	{
	case 1: return TEXT("主攻");
	case 2: return TEXT("副攻");
	case 3: return TEXT("接应");
	case 4: return TEXT("后排");
	default: return TEXT("默认");
	}
}
