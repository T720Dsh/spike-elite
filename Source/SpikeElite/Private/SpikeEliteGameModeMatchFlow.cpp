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

void ASpikeEliteGameMode::AwardPoint(EVolleyballTeam ScoringTeam)
{
	const auto& TouchActors = GetTeamPlayers(RallyState.LastTouchTeam);
	const FPlayerIdentity* TouchIdentity = TouchActors.IsValidIndex(RallyState.LastTouchPlayerIndex)
		? FindIdentity(RallyState.LastTouchTeam, TouchActors[RallyState.LastTouchPlayerIndex]) : nullptr;
	const int32 TouchIdentityIndex = TouchIdentity ?
		(RallyState.LastTouchTeam == EVolleyballTeam::TeamA ? RosterA : RosterB).RegisteredIndex(TouchIdentity->PlayerId) : INDEX_NONE;
	if (ScoringTeam == EVolleyballTeam::TeamA) { TeamAScore++; }
	else if (ScoringTeam == EVolleyballTeam::TeamB) { TeamBScore++; }
	else { return; }

	const bool bWasServeWin = (ServingTeam == ScoringTeam);
	const EVolleyballTeam ServingTeamBeforePoint = ServingTeam;
	ServingTeam = ScoringTeam;
	if (!bWasServeWin)
	{
		// Side-out: ONLY the team that just gained the serve rotates (clockwise,
		// its P1 leaves the court after serving... here the P1 slot moves to P6).
		// The serving winner's rotation stays untouched (no rotation on a point).
		RotateTeam(ScoringTeam);
		if (ScoringTeam == EVolleyballTeam::TeamA) { TeamARotation = SEVolleyballRules::AdvanceRotationIndex(TeamARotation); }
		else { TeamBRotation = SEVolleyballRules::AdvanceRotationIndex(TeamBRotation); }
	}
	// M11h-8: attribute the rally outcome to player stats (server ace/error,
	// attack win/error, block) from the real last-touch data. Indexed by court
	// slot; the last touch index lives in RallyState.
	if (RallyState.LastTouchPlayerIndex >= 0 && RallyState.LastTouchTeam != EVolleyballTeam::None)
	{
		SEVolleyballRules::AttachStatsForRally(
			StatsFor(RallyState.LastTouchTeam), RallyState.LastTouchType,
			TouchIdentityIndex, RallyState.TouchCount,
			ScoringTeam, ServingTeamBeforePoint, RallyState.LastTouchTeam);
	}
	RefreshRotationView();
	++CompletedRallies;

	// Record the current set's running score.
	if (SetScoresA.Num() >= CurrentSet) { SetScoresA[CurrentSet-1] = TeamAScore; }
	if (SetScoresB.Num() >= CurrentSet) { SetScoresB[CurrentSet-1] = TeamBScore; }

	UE_LOG(LogVolleyballRules, Log, TEXT("Point. A:%d B:%d (serve=%s)"), TeamAScore, TeamBScore, SpikeEliteGameModeHelpers::TeamStr(ServingTeam));
	CheckSetWin();
}

void ASpikeEliteGameMode::CheckSetWin()
{
	if (!SEVolleyballRules::IsSetWon(TeamAScore, TeamBScore, PointsToWin))
	{
		MatchState = EMatchState::BetweenRallies;
		InterRallyTimer = InterRallyDelay;
		bInToss = false;
		UpdateScoreboard();
		return;
	}

	const EVolleyballTeam W = (TeamAScore > TeamBScore) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	if (W == EVolleyballTeam::TeamA) { TeamASetsWon++; } else { TeamBSetsWon++; }

	if (SEVolleyballRules::IsMatchWon(TeamASetsWon, TeamBSetsWon, MatchWinsNeeded))
	{
		MatchWinner = W;
		MatchState = EMatchState::MatchOver;
		// The end-of-match screen shows the result itself; drop the last rally
		// banner so the scoreboard reads clean under MatchOver.
		RallyResultText.Empty();
		RallyResultDisplayTimer = 0.0f;
		UpdateScoreboard();
#if !UE_BUILD_SHIPPING
		// M11f-5: accelerated best-of-five acceptance — report the full set
		// history and exit BEFORE NotifyMatchOver pauses the world (a paused
		// world stops this Tick, which would strand the driver).
		if (bFiveSetTest)
		{
			const bool bOk = (TeamASetsWon == 3 && TeamBSetsWon == 2 && CurrentSet == 5);
			UE_LOG(LogVolleyballRules, Log, TEXT("FIVE SET TEST: MatchOver winner=%s setsA=%d setsB=%d"),
				SpikeEliteGameModeHelpers::TeamStr(MatchWinner), TeamASetsWon, TeamBSetsWon);
			for (int32 i = 0; i < SetScoresA.Num(); ++i)
			{
				UE_LOG(LogVolleyballRules, Log, TEXT("FIVE SET TEST: set %d -> A %d : %d B"),
					i + 1, SetScoresA[i], SetScoresB.Num() > i ? SetScoresB[i] : -1);
			}
			UE_LOG(LogVolleyballRules, Log, TEXT("FIVE SET TEST: RESULT=%s"), bOk ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExit(0);
			return;
		}
#endif
		// M11h-2b: challenge progress — a Team A win advances the stage.
		if (MatchModeConfig.Mode == EGameModeChoice::Challenge12 && MatchWinner == EVolleyballTeam::TeamA)
		{
			LoadChallengeState();
			ChallengeSave.Won++;
			if (ChallengeSave.Stage + 1 >= SEChallenge::StageCount) { ChallengeSave.bCompleted = true; ChallengeSave.Stage = SEChallenge::StageCount; }
			else { ChallengeSave.Stage++; }
			SaveChallengeState();
			UE_LOG(LogVolleyballRules, Log, TEXT("Challenge advanced: stage=%d won=%d completed=%d"),
				ChallengeSave.Stage, ChallengeSave.Won, ChallengeSave.bCompleted ? 1 : 0);
		}
		NotifyMatchOver();
		return;
	}

	// Set over: show banner, then start the next set after the pause.
	MatchState = EMatchState::SetOver;
	InterRallyTimer = SetOverDelay;
	const FString Text = FString::Printf(TEXT("第 %d 局结束：%s 队获胜  %d : %d"),
		CurrentSet, SpikeEliteGameModeHelpers::TeamStr(W), TeamAScore, TeamBScore);
	ShowBanner(Text, SetOverDelay);
	UpdateScoreboard();
}

void ASpikeEliteGameMode::StartNextSet()
{
	CurrentSet++;
	TeamARotation=TeamBRotation=1;
	TeamAScore = TeamBScore = 0;
	PointsToWin = bQuickMatch ? 3 : SEVolleyballRules::PointsToWinForSet(CurrentSet);
	SetScoresA.Add(0);
	SetScoresB.Add(0);
	// M11h-4: timeout allowances reset at the start of every set (FIVB 15.2).
	TimeoutLeftA = TimeoutLeftB = 2;
	TimeoutTeam = EVolleyballTeam::None;
	TimeoutTimer = 0.f;
	TimeoutPausedSeconds = 0.f;
	// M11h-5: substitution allowances and bench pool reset each set; the
	// starting six return (FIVB 15.2 — starters may be used again in the next
	// set; on-court lineup returns to this set's starting lineup).
	ResetSubstitutionState();
	// M11h-3: a new set always shows the full server intro card again.
	LastPresentedServerId = TEXT("");
	RespawnPlayersToPositions();
	SEVolleyballRules::BeginRally(RallyState, ServingTeam);
	MatchState = EMatchState::BetweenRallies;
	InterRallyTimer = SetOverDelay * 0.6f;
	UpdateScoreboard();
	UE_LOG(LogVolleyballRules, Log, TEXT("Set %d begins (to %d)"), CurrentSet, PointsToWin);
}

void ASpikeEliteGameMode::DriveFiveSet()
{
	// M11f-5: accelerated best-of-five acceptance. Reuses the PRODUCTION
	// scoring path (AwardPoint -> CheckSetWin -> IsSetWon/IsMatchWon) and the
	// production state transitions (SetOver -> StartNextSet). No result arrays
	// are faked; every point goes through AwardPoint and every set through
	// CheckSetWin. Plan: A wins sets 1/3/5, B wins 2/4 -> 3:2, with sets
	// ending 26:24 (first four) and 16:14 (fifth) — both prove "lead by 2".
	// MatchOver reporting/exit happens inside CheckSetWin before the world
	// pauses for the result screen.
	if (MatchState == EMatchState::MatchOver || MatchState == EMatchState::SetOver) { return; }
	// Only drive from BetweenRallies (points land after a settled rally).
	if (MatchState != EMatchState::BetweenRallies) { return; }

	// Production target points: 25 for sets 1-4, 15 for the deciding set.
	const int32 PlanPts = SEVolleyballRules::PointsToWinForSet(CurrentSet);
	const EVolleyballTeam Winner = (CurrentSet % 2 == 1) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	const int32 AS = TeamAScore;
	const int32 BS = TeamBScore;

	// Alternate up to 24:24 (14:14 in set 5) so no early "lead by 2" set end.
	if (AS < PlanPts - 1 && BS < PlanPts - 1)
	{
		AwardPoint(AS <= BS ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB);
		return;
	}
	// Tied at 24:24 / 14:14 -> winner takes two straight.
	if (AS == PlanPts - 1 && BS == PlanPts - 1) { AwardPoint(Winner); return; }
	// Winner leads by one (25:24 / 15:14) -> one more ends the set 26:24 / 16:14.
	if ((Winner == EVolleyballTeam::TeamA && AS == PlanPts && BS == PlanPts - 1) ||
		(Winner == EVolleyballTeam::TeamB && BS == PlanPts && AS == PlanPts - 1))
	{
		AwardPoint(Winner);
		return;
	}
	// Safety: alternate if anything unexpected lands here.
	AwardPoint(AS <= BS ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB);
}
