// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "Volleyball/SetPlay.h"
#include "Volleyball/VolleyballBall.h"
#include "SpikeEliteCharacter.h"
#include "SpikeElitePlayerController.h"
#include "UI/TacticalContactComponent.h"
#include "Volleyball/PracticeFlow.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballArena.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "UI/ScoreboardWidget.h"
#include "UI/RotationWidget.h"
#include "Volleyball/MatchOfficialManager.h"
#include "Blueprint/UserWidget.h"
#include "SpikeEliteGameModeHelpers.h"

// ---------------- M11h-4: team timeout ----------------

bool ASpikeEliteGameMode::CanRequestTimeout() const
{
	// Shared tested rule: dead ball before the service whistle only.
	return MatchModeConfig.Mode!=EGameModeChoice::Training && SEVolleyballRules::CanRequestTimeoutInPhase(MatchState);
}

bool ASpikeEliteGameMode::RequestTeamTimeout(EVolleyballTeam Team)
{
	if (Team == EVolleyballTeam::None) { return false; }
	if (!CanRequestTimeout())
	{
		const auto StateName = [](EMatchState S) -> const TCHAR*
		{
			switch (S)
			{
			case EMatchState::PreMatch: return TEXT("PreMatch");
			case EMatchState::BetweenRallies: return TEXT("BetweenRallies");
			case EMatchState::ResettingPositions: return TEXT("ResettingPositions");
			case EMatchState::AwaitingReady: return TEXT("AwaitingReady");
			case EMatchState::ServiceAuthorized: return TEXT("ServiceAuthorized");
			case EMatchState::ServingToss: return TEXT("ServingToss");
			case EMatchState::Rally: return TEXT("Rally");
			case EMatchState::Timeout: return TEXT("Timeout");
			case EMatchState::SetOver: return TEXT("SetOver");
			case EMatchState::MatchOver: return TEXT("MatchOver");
			default: return TEXT("?");
			}
		};
		UE_LOG(LogVolleyballRules, Log, TEXT("Timeout request REJECTED team=%s state=%s (not a dead-ball window)"),
			SpikeEliteGameModeHelpers::TeamStr(Team), StateName(MatchState));
		return false;
	}
	if (GetTimeoutLeft(Team) <= 0)
	{
		UE_LOG(LogVolleyballRules, Log, TEXT("Timeout request REJECTED team=%s (no timeouts left this set)"),
			SpikeEliteGameModeHelpers::TeamStr(Team));
		return false;
	}

	// Consume one timeout and enter the independent timeout phase. The world is
	// NOT paused: coach UI, player huddle and the 30 s clock keep running.
	if (Team == EVolleyballTeam::TeamA) { TimeoutLeftA = SEVolleyballRules::TimeoutLeftAfterRequest(TimeoutLeftA); }
	else { TimeoutLeftB = SEVolleyballRules::TimeoutLeftAfterRequest(TimeoutLeftB); }
	TimeoutTeam = Team;
	TimeoutTimer = SEVolleyballRules::TimeoutSeconds();
	TimeoutPausedSeconds = 0.f;
	MatchState = EMatchState::Timeout;
	bAIServePending = false; // no auto-serve can fire during the huddle

	// Huddle: move each team to its bench-side gathering zone (outside the free
	// zone, no collisions with the net / officials).
	for(int32 Side : {1,-1})
	{
		const auto& Roster=Side>0 ? TeamAPlayers:TeamBPlayers;
		for(int32 i=0;i<Roster.Num();++i) if(Roster[i])
		{
			Roster[i]->HomePosition=FVector(Side*(350.f+100.f*i),-1080.f,0.f);
			Roster[i]->bCeremonyWalking=true;
		}
	}
	if(auto* PC=Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this,0))) { PC->HideServeIntro(); if(PC->Tactical) PC->Tactical->CancelShot(); }
	UE_LOG(LogVolleyballRules, Log, TEXT("Team timeout STARTED team=%s leftA=%d leftB=%d 30s"),
		SpikeEliteGameModeHelpers::TeamStr(Team), TimeoutLeftA, TimeoutLeftB);
	UpdateScoreboard();
	RefreshRotationView();
	return true;
}

void ASpikeEliteGameMode::CancelTeamTimeout()
{
	if (MatchState != EMatchState::Timeout) { return; }
	UE_LOG(LogVolleyballRules, Log, TEXT("Team timeout CANCELLED team=%s"),
		SpikeEliteGameModeHelpers::TeamStr(TimeoutTeam));
	TimeoutTeam = EVolleyballTeam::None;
	TimeoutTimer = 0.f;
	TimeoutPausedSeconds = 0.f;
	RespawnPlayersToPositions();
	for(auto& C : TeamAPlayers) if(C) C->bCeremonyWalking=false;
	for(auto& C : TeamBPlayers) if(C) C->bCeremonyWalking=false;
	MatchState = EMatchState::ResettingPositions;
	PhaseTimer = ResetDelay;
	UpdateScoreboard();
}
