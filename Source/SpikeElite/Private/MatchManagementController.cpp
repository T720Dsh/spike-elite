// SPDX-License-Identifier: MIT
#include "SpikeElitePlayerController.h"
#include "SpikeEliteCharacter.h"
#include "UI/TeamRosterWidget.h"
#include "UI/CoachPanelWidget.h"
#include "UI/TacticalContactComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

void ASpikeElitePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ManagementRefreshTimer+=DeltaSeconds;
	if(ManagementRefreshTimer>.25f) { ManagementRefreshTimer=0; RefreshCoachPanel(); }
}

void ASpikeElitePlayerController::ShowRoster(bool bPreMatch)
{
	auto* GM=GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>(); if(!GM) return;
	if(!bPreMatch && !SEVolleyballRules::CanSubstituteInPhase(GM->MatchState))
	{ if(CoachPanel) CoachPanel->SetStatus(TEXT("只能死球哨前或球队暂停期间申请换人。")); return; }
	if(Tactical) Tactical->CancelShot();
	if(bPreMatch) HideAllMenus();
	else if(CoachPanel) { CoachPanel->RemoveFromParent(); CoachPanel=nullptr; GM->bCoachPanelOpen=false; }
	if(bPreMatch)
	{
		SEVolleyballRoster::BuildDefaultRoster(EVolleyballTeam::TeamA,GM->RosterA);
		SEVolleyballRoster::BuildDefaultRoster(EVolleyballTeam::TeamB,GM->RosterB);
		GM->LoadLineups();
		FString Why;
		if(GM->SavedLineupA.Num()==6) GM->SetStartingLineup(EVolleyballTeam::TeamA,GM->SavedLineupA,Why);
		if(GM->SavedLineupB.Num()==6) GM->SetStartingLineup(EVolleyballTeam::TeamB,GM->SavedLineupB,Why);
		MenuState=EMenuState::MainMenu;
	}
	RosterMenu=CreateWidget<UTeamRosterWidget>(this); if(!RosterMenu) return;
	RosterMenu->AddToViewport(30); RosterMenu->Configure(GM,bPreMatch);
	if(bPreMatch)
	{
		RosterMenu->OnDone.BindUObject(this,&ASpikeElitePlayerController::StartPreparedMatch);
		RosterMenu->OnBack.BindUObject(this,&ASpikeElitePlayerController::ShowModeSelect);
	}
	else
	{
		SetPause(true);
		RosterMenu->OnDone.BindUObject(this,&ASpikeElitePlayerController::CloseRoster);
		RosterMenu->OnBack.BindUObject(this,&ASpikeElitePlayerController::CloseRoster);
	}
	SetUIInputMode(RosterMenu); RosterMenu->SetKeyboardFocus();
}
void ASpikeElitePlayerController::CloseRoster()
{
	if(!RosterMenu) return;
	RosterMenu->RemoveFromParent(); RosterMenu=nullptr;
	if(MenuState==EMenuState::MainMenu) ShowModeSelect();
	else { SetPause(false); SetGameInputMode(); ToggleCoachPanel(); }
}
void ASpikeElitePlayerController::StartPreparedMatch()
{
	HideAllMenus(); MenuState=EMenuState::Playing;
	auto* GM=GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>(); if(!GM) return;
	GM->SetMatchMode(PendingMode); SetGameInputMode(); GM->StartMatch();
	if(PendingMode.Mode==EGameModeChoice::Coach && GM->MatchState!=EMatchState::Entrance)
	{
		if(!DevCam) DevCam=GetWorld()->SpawnActor<ACameraActor>();
		if(auto* Camera=Cast<ACameraActor>(DevCam))
		{ const FVector P(1450,-1750,720); Camera->SetActorLocationAndRotation(P,(FVector(0,0,160)-P).Rotation()); Camera->GetCameraComponent()->SetFieldOfView(75); SetViewTarget(Camera); }
		ToggleCoachPanel();
	}
}
void ASpikeElitePlayerController::SwitchControlledPlayer()
{
	if(IsMenuOpen()) return;
	auto* GM=GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>();
	if(!GM || GM->GetMatchMode()==EGameModeChoice::Coach) return;
	if(GM->GetMatchMode()==EGameModeChoice::Training) { GM->StartTrainingAttempt(); return; }
	const auto& Team=GM->GetTeamPlayers(EVolleyballTeam::TeamA);
	int32 Current=Team.IndexOfByPredicate([this](const auto& C){return C==GetPawn();});
	if(Current==INDEX_NONE || Team.Num()!=6) return;
	if(Tactical) Tactical->CancelShot();
	Team[Current]->bIsBot=true;
	auto* Next=Team[(Current+1)%6].Get(); if(!Next) return;
	Next->bIsBot=false; Possess(Next); SetViewTarget(Next); SetControlRotation(FRotator(-12,180,0));
	GM->RefreshRotationView();
}
