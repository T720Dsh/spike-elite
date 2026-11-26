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

// ---------------- M11h-5: substitutions ----------------

void ASpikeEliteGameMode::ResetSubstitutionState()
{
	SubLedgerA = FSubstitutionLedger(); SubLedgerB = FSubstitutionLedger();
	SubstitutionsLeftA = SubstitutionsLeftB = SEVolleyballRules::SubstitutionsPerSet();
	auto Rebuild = [](const FTeamRosterState& RS, TArray<FString>& OutPool, TMap<FString, FString>& OutPairing)
	{
		OutPool.Reset();
		OutPairing.Reset();
		// Registered order i: starters are 0..5, bench 6..11. Default pairing maps
		// bench player to the starter with the same court index (coach-set pairing
		// is a later feature; this keeps substitution legal and symmetric).
		OutPool = RS.GetBench();
	};
	Rebuild(RosterA, SubPoolA, SubPairingA);
	Rebuild(RosterB, SubPoolB, SubPairingB);
	// The on-court lineup is always the authoritative court six.
	RosterA.OnCourtLineup = RosterA.StartingLineup;
	RosterB.OnCourtLineup = RosterB.StartingLineup;
	SubPoolA = RosterA.GetBench(); SubPoolB = RosterB.GetBench();
	SynchronizeCourtIdentities(); RefreshRegisteredBench();
}

bool ASpikeEliteGameMode::CanRequestSubstitution(EVolleyballTeam Team, int32 CourtIndex, const FString& SubId, FString& OutReason) const
{
	if (Team == EVolleyballTeam::None)
	{
		OutReason = TEXT("未知球队");
		return false;
	}
	if (MatchModeConfig.Mode==EGameModeChoice::Training || !SEVolleyballRules::CanSubstituteInPhase(MatchState))
	{
		OutReason = TEXT("仅死球且发球哨前可申请换人");
		return false;
	}
	const int32 Left = (Team == EVolleyballTeam::TeamA) ? SubstitutionsLeftA : SubstitutionsLeftB;
	if (Left <= 0)
	{
		OutReason = TEXT("本局换人次数已用完");
		return false;
	}
	const FTeamRosterState& RS = (Team == EVolleyballTeam::TeamA) ? RosterA : RosterB;
	if (!RS.OnCourtLineup.IsValidIndex(CourtIndex))
	{
		OutReason = TEXT("场上位置无效");
		return false;
	}
	const TArray<FString>& Pool = (Team == EVolleyballTeam::TeamA) ? SubPoolA : SubPoolB;
	if (!Pool.Contains(SubId))
	{
		OutReason = TEXT("该球员不在替补席");
		return false;
	}
	const FSubstitutionLedger& Ledger = Team == EVolleyballTeam::TeamA ? SubLedgerA : SubLedgerB;
	if (Ledger.LastRequestRally == CompletedRallies)
	{ OutReason = TEXT("同队两次独立换人请求之间须完成一个回合"); return false; }
	return Ledger.Validate(RS, CourtIndex, SubId, OutReason);
}

bool ASpikeEliteGameMode::RequestSubstitution(EVolleyballTeam Team, int32 CourtIndex, const FString& SubId)
{
	FString Reason;
	if (!CanRequestSubstitution(Team, CourtIndex, SubId, Reason))
	{
		UE_LOG(LogVolleyballRules, Log, TEXT("[SubstitutionRejected] team=%s court=%d sub=%s reason=%s"),
			SpikeEliteGameModeHelpers::TeamStr(Team), CourtIndex, *SubId, *Reason);
		return false;
	}

	FTeamRosterState& RS = (Team == EVolleyballTeam::TeamA) ? RosterA : RosterB;
	TArray<FString>& Pool = (Team == EVolleyballTeam::TeamA) ? SubPoolA : SubPoolB;
	int32& Left = (Team == EVolleyballTeam::TeamA) ? SubstitutionsLeftA : SubstitutionsLeftB;
	TArray<TObjectPtr<ASpikeEliteCharacter>>& CourtActors = (Team == EVolleyballTeam::TeamA) ? TeamAPlayers : TeamBPlayers;

	const FString OldId = RS.OnCourtLineup[CourtIndex];
	const FPlayerIdentity* SubIdentity = RS.FindById(SubId);

	// Atomic identity switch: OnCourtLineup, bench pool and allowance move
	// together; the court actor keeps its slot but takes the substitute's
	// number/name/role. No transient 7/5-man court, no duplicate PlayerId.
	FSubstitutionLedger& Ledger = Team == EVolleyballTeam::TeamA ? SubLedgerA : SubLedgerB;
	Ledger.Apply(RS, CourtIndex, SubId);
	Ledger.LastRequestRally = CompletedRallies;
	Pool.Remove(SubId);
	Pool.Add(OldId);
	Left = Ledger.Remaining;
	for (auto& C : BenchPlayers)
		if (C && C->RosterPlayerId == SubId) { C->RosterPlayerId = OldId; break; }
	SynchronizeCourtIdentities(); RefreshRegisteredBench();

	// Visual: update the court actor's jersey (same team colours).
	if (CourtActors.IsValidIndex(CourtIndex) && CourtActors[CourtIndex] && SubIdentity)
	{
		CourtActors[CourtIndex]->JerseyNumber = SubIdentity->JerseyNumber;
		CourtActors[CourtIndex]->RefreshJerseyNumberVisual();
		UE_LOG(LogVolleyballRules, Log,
			TEXT("[SubstitutionApproved] team=%s court=%d out=%s(%s #%d) in=%s(%s #%d) left=%d"),
			SpikeEliteGameModeHelpers::TeamStr(Team), CourtIndex,
			*OldId, RS.FindById(OldId) ? *RS.FindById(OldId)->DisplayName : TEXT("?"),
			RS.FindById(OldId) ? RS.FindById(OldId)->JerseyNumber : -1,
			*SubId, *SubIdentity->DisplayName, SubIdentity->JerseyNumber, Left);
	}
	else
	{
		UE_LOG(LogVolleyballRules, Log, TEXT("[SubstitutionApproved] team=%s court=%d out=%s in=%s left=%d"),
			SpikeEliteGameModeHelpers::TeamStr(Team), CourtIndex, *OldId, *SubId, Left);
	}

	// The P1 server slot follows the identity automatically (court index 0 is
	// the server; the new identity's number is what the HUD shows next serve).
	if (CourtIndex == 0 && MatchState == EMatchState::ServiceAuthorized)
	{
		ServerPlayerIndex = 0;
	}
	LastManagementMessage = FString::Printf(TEXT("换人完成：%s → %s（剩余 %d 次）"),
		*OldId, *SubId, Left);
	if (MatchState == EMatchState::ServePresentation)
	{
		if (auto* PC = Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this, 0))) { PC->HideServeIntro(); }
		MatchState = EMatchState::AwaitingReady; PhaseTimer = ReadyDelay;
	}
	UpdateScoreboard();
	RefreshRotationView();
	return true;
}
