// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballBall.h"

#if !UE_BUILD_SHIPPING
bool ASpikeEliteGameMode::DevManagementChecks(FString& Failures)
{
	// Explicit deterministic state fixtures, not a human full-match test.
	auto Check=[&](bool Good,const TCHAR* Label) { UE_LOG(LogVolleyballRules,Log,TEXT("[ManagementCheck] %s=%s"),Label,Good?TEXT("PASS"):TEXT("FAIL")); if(!Good) Failures+=FString(Label)+TEXT("; "); };
	FString Why; Check(ValidateRuntimeRoster(Why),TEXT("24-unique-identities"));
	const auto* B=FindIdentity(EVolleyballTeam::TeamB,TeamBPlayers[0]);
	Check(B && B->PlayerId==RosterB.OnCourtLineup[0],TEXT("team-B-identity"));
	ResetMatchStats();
	const FString Original=RosterA.OnCourtLineup[0];
	const int32 StatIndex=RosterA.RegisteredIndex(Original);
	RecordTouchStat(EVolleyballTeam::TeamA,0,EBallTouchType::Set,false);
	RotateTeam(EVolleyballTeam::TeamA);
	const int32 NewSlot=RosterA.OnCourtIndex(Original);
	RecordTouchStat(EVolleyballTeam::TeamA,NewSlot,EBallTouchType::Set,false);
	Check(StatsA[StatIndex].Sets==2 && ValidateRuntimeRoster(Why),TEXT("rotation-stats-owner"));
	ResetSubstitutionState();
	const FString Starter=RosterA.OnCourtLineup[0], Bench=RosterA.GetBench()[0];
	MatchState=EMatchState::ServiceAuthorized;
	Check(!RequestTeamTimeout(EVolleyballTeam::TeamA) && !CanRequestSubstitution(EVolleyballTeam::TeamA,0,Bench,Why),TEXT("post-whistle-management-rejected"));
	MatchState=EMatchState::ServePresentation;
	Check(RequestSubstitution(EVolleyballTeam::TeamA,0,Bench),TEXT("substitute-enters"));
	Check(ValidateRuntimeRoster(Why),TEXT("substitution-identities"));
	Check(!CanRequestSubstitution(EVolleyballTeam::TeamA,0,Starter,Why),TEXT("same-interval-second-request-rejected"));
	++CompletedRallies; MatchState=EMatchState::AwaitingReady;
	Check(RequestSubstitution(EVolleyballTeam::TeamA,0,Starter),TEXT("starter-returns"));
	++CompletedRallies;
	Check(!CanRequestSubstitution(EVolleyballTeam::TeamA,0,Bench,Why),TEXT("starter-second-exit-rejected"));
	ResetSubstitutionState(); MatchState=EMatchState::AwaitingReady;
	Check(RequestSubstitution(EVolleyballTeam::TeamA,0,Bench),TEXT("new-set-sub-rights"));
	ResetSubstitutionState();
	Check(RosterA.OnCourtLineup==RosterA.StartingLineup && ValidateRuntimeRoster(Why),TEXT("new-set-court-and-bench-reset"));
	TimeoutLeftA=TimeoutLeftB=2; MatchState=EMatchState::ServePresentation;
	Check(RequestTeamTimeout(EVolleyballTeam::TeamA) && TimeoutLeftA==1 && TimeoutLeftB==2 && FMath::IsNearlyEqual(TimeoutTimer,30.f),TEXT("timeout-30s-authority"));
	Check(RequestSubstitution(EVolleyballTeam::TeamA,1,RosterA.GetBench()[0]),TEXT("substitution-during-timeout"));
	CancelTeamTimeout(); Check(ValidateRuntimeRoster(Why),TEXT("timeout-return-identities"));
	ResetSubstitutionState(); ResetMatchStats(); TimeoutLeftA=TimeoutLeftB=2;
	MatchState=EMatchState::BetweenRallies;
	return Failures.IsEmpty();
}
#endif
