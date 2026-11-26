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

void ASpikeEliteGameMode::ResetMatchStats()
{
	StatsA.Reset();
	StatsB.Reset();
	for (int32 i = 0; i < 12; ++i)
	{
		StatsA.Add(SEVolleyballRules::FPlayerMatchStats());
		StatsB.Add(SEVolleyballRules::FPlayerMatchStats());
	}
}

void ASpikeEliteGameMode::RecordTouchStat(EVolleyballTeam Team, int32 CourtIndex, EBallTouchType Type, bool bWasDiveSave)
{
	if (MatchModeConfig.Mode == EGameModeChoice::Training) { return; }
	TArray<SEVolleyballRules::FPlayerMatchStats>& Stats = StatsFor(Team);
	const auto& Actors = GetTeamPlayers(Team);
	const FPlayerIdentity* Identity = Actors.IsValidIndex(CourtIndex) ? FindIdentity(Team, Actors[CourtIndex]) : nullptr;
	CourtIndex = Identity ? (Team == EVolleyballTeam::TeamA ? RosterA : RosterB).RegisteredIndex(Identity->PlayerId) : INDEX_NONE;
	if (!Stats.IsValidIndex(CourtIndex)) { return; }
	SEVolleyballRules::FPlayerMatchStats& S = Stats[CourtIndex];
	switch (Type)
	{
	case EBallTouchType::Receive: S.Receives++; break;
	case EBallTouchType::Set:     S.Sets++;     break;
	case EBallTouchType::Attack:  S.Attacks++;  break;
	case EBallTouchType::Block:   S.Blocks++;   break;
	case EBallTouchType::Serve:   S.ServeAttempts++; break;
	default: break;
	}
	if (bWasDiveSave) { S.Digs++; }
}
