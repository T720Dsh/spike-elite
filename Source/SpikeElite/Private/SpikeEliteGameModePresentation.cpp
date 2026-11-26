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

// ---------------- M11h-3: server introduction ----------------

void ASpikeEliteGameMode::EnterServePresentation()
{
	if (MatchState != EMatchState::AwaitingReady) { return; }
	TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = (ServingTeam == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;
	ASpikeEliteCharacter* Server = (Roster.Num() > 0) ? Roster[0].Get() : nullptr;
	const FPlayerIdentity* Id = FindIdentity(ServingTeam, Server);
	// Identity PlayerId is a stable FString ("A01"); the actor's int PlayerId is
	// only the spawn index. The presentation tracks the IDENTITY, so a
	// substitution keeps the comparison meaningful.
	const FString ServerId = Id ? Id->PlayerId : TEXT("");

	// Same player serving again -> short name/number bar only; a new server gets
	// the full card. The intro runs at UI/real time (world is NOT paused here).
	const bool bSameServer = (!ServerId.IsEmpty() && ServerId == LastPresentedServerId);
	PresentationTimer = bSameServer ? 1.2f : 2.2f;
	MatchState = EMatchState::ServePresentation;
	UpdateScoreboard();

	const FPlayerIdentity* ServerIdentity = FindIdentity(ServingTeam, Server);
	if (ASpikeElitePlayerController* SEPC = Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		SEPC->ShowServeIntro(ServerId, ServerIdentity ? ServerIdentity->JerseyNumber : 0,
			ServerIdentity ? ServerIdentity->DisplayName : TEXT("发球员"),
			ServingTeam, ServerIdentity ? SEVolleyballRoster::RoleDisplayName(ServerIdentity->Role) : TEXT(""),
			bSameServer);
	}
	UE_LOG(LogVolleyballRules, Log, TEXT("Serve presentation: server=%s same=%d"),
		*ServerId, bSameServer ? 1 : 0);
}

void ASpikeEliteGameMode::SkipServePresentation()
{
	if (MatchState != EMatchState::ServePresentation) { return; }
	if (ASpikeElitePlayerController* SEPC = Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		SEPC->HideServeIntro();
	}
	BeginServiceAuthorized();
}
