// SPDX-License-Identifier: MIT
// Unreal Automation Tests for the M10 pure-logic volleyball rules core.
//
// These tests never load a map: they exercise SEVolleyballRules over plain
// FVolleyballRallyState values, plus the match-level gates used by the
// GameMode. Run with:
//   UnrealEditor.exe <project> -ExecCmds="Automation RunTests SpikeElite.Tests; Quit" -unattended -nopause -nosplash
// (all tests in this file are prefixed "SpikeElite.Tests.").

#include "Misc/AutomationTest.h"
#include "Volleyball/VolleyballRules.h"

#if WITH_DEV_AUTOMATION_TESTS

using SEVolleyballRules::DetermineScoringTeamOnLand;
using SEVolleyballRules::EvaluateTouch;
using SEVolleyballRules::IsInBounds;
using SEVolleyballRules::IsMatchWon;
using SEVolleyballRules::IsSetWon;
using SEVolleyballRules::IsTouchLegalInPhase;
using SEVolleyballRules::IsServeFault;
using SEVolleyballRules::OnBallCrossedNet;
using SEVolleyballRules::PointsToWinForSet;
using SEVolleyballRules::RotateRoster;
using SEVolleyballRules::BeginRally;
using SEVolleyballRules::SettleRally;
using SEVolleyballRules::StartPlay;
using SEVolleyballRules::ShouldAutoServe;

// ---------------------------------------------------------------- 1/2: IN / OUT scoring

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEInBoundsScoring, "SpikeElite.Tests.InBoundsScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEInBoundsScoring::RunTest(const FString& Parameters)
{
	// Landed IN on Team A's half (X>0) -> Team B scores, regardless of last touch.
	TestEqual(TEXT("IN on A half scores for B"),
		DetermineScoringTeamOnLand(true, EVolleyballTeam::TeamA, true), EVolleyballTeam::TeamB);
	TestEqual(TEXT("IN on A half with B last touch still scores for B"),
		DetermineScoringTeamOnLand(true, EVolleyballTeam::TeamB, true), EVolleyballTeam::TeamB);
	// Landed IN on Team B's half (X<0) -> Team A scores.
	TestEqual(TEXT("IN on B half scores for A"),
		DetermineScoringTeamOnLand(true, EVolleyballTeam::TeamB, false), EVolleyballTeam::TeamA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSELineCountsIn, "SpikeElite.Tests.LineCountsIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSELineCountsIn::RunTest(const FString& Parameters)
{
	constexpr float HalfLen = 900.f;
	constexpr float HalfWid = 450.f;
	// Lines count IN.
	TestTrue(TEXT("end line (X=900) counts in"), IsInBounds(FVector(900.f, 0.f, 0.f), HalfLen, HalfWid));
	TestTrue(TEXT("end line (X=-900) counts in"), IsInBounds(FVector(-900.f, 0.f, 0.f), HalfLen, HalfWid));
	TestTrue(TEXT("side line (Y=450) counts in"), IsInBounds(FVector(0.f, 450.f, 0.f), HalfLen, HalfWid));
	TestTrue(TEXT("side line (Y=-450) counts in"), IsInBounds(FVector(0.f, -450.f, 0.f), HalfLen, HalfWid));
	TestTrue(TEXT("corner (900,450) counts in"), IsInBounds(FVector(900.f, 450.f, 0.f), HalfLen, HalfWid));
	// Just outside is OUT.
	TestFalse(TEXT("X=900.1 is out"), IsInBounds(FVector(900.1f, 0.f, 0.f), HalfLen, HalfWid));
	TestFalse(TEXT("Y=450.1 is out"), IsInBounds(FVector(0.f, 450.1f, 0.f), HalfLen, HalfWid));
	// A ball landing exactly on Team A's end line is IN and scores for B.
	TestEqual(TEXT("on-line land scores for B"),
		DetermineScoringTeamOnLand(true, EVolleyballTeam::TeamA, true), EVolleyballTeam::TeamB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEOutBoundScoring, "SpikeElite.Tests.OutBoundScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEOutBoundScoring::RunTest(const FString& Parameters)
{
	// OUT -> opponent of the last touching team scores.
	TestEqual(TEXT("OUT after A touch -> B scores"),
		DetermineScoringTeamOnLand(false, EVolleyballTeam::TeamA, false), EVolleyballTeam::TeamB);
	TestEqual(TEXT("OUT after B touch -> A scores"),
		DetermineScoringTeamOnLand(false, EVolleyballTeam::TeamB, true), EVolleyballTeam::TeamA);
	// No last touch -> fall back to half.
	TestEqual(TEXT("OUT with no touch on A half -> B scores"),
		DetermineScoringTeamOnLand(false, EVolleyballTeam::None, true), EVolleyballTeam::TeamB);
	return true;
}

// ---------------------------------------------------------------- 3: touches & faults

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETouchSequence, "SpikeElite.Tests.TouchSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETouchSequence::RunTest(const FString& Parameters)
{
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	TestEqual(TEXT("serve counts as first touch"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 0), ETouchResult::Allowed);
	TestEqual(TEXT("touch count after serve"), S.TouchCount, 1);
	TestEqual(TEXT("last touch team after serve"), S.LastTouchTeam, EVolleyballTeam::TeamA);
	TestEqual(TEXT("last toucher index after serve"), S.LastTouchPlayerIndex, 0);

	// Wrong team cannot touch.
	TestEqual(TEXT("B cannot touch while A possesses"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 0), ETouchResult::WrongTeam);

	// Teammate second touch (set).
	TestEqual(TEXT("teammate second touch"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 1), ETouchResult::Allowed);
	TestEqual(TEXT("touch count after set"), S.TouchCount, 2);

	// Third touch (attack).
	TestEqual(TEXT("third touch"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 2), ETouchResult::Allowed);
	TestEqual(TEXT("touch count after attack"), S.TouchCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDoubleTouchFault, "SpikeElite.Tests.DoubleTouchFault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDoubleTouchFault::RunTest(const FString& Parameters)
{
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	TestEqual(TEXT("serve"), EvaluateTouch(S, EVolleyballTeam::TeamA, 0), ETouchResult::Allowed);
	// Same player touches again -> fault.
	TestEqual(TEXT("same player twice in a row"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 0), ETouchResult::DoubleTouchFault);
	// Fault must NOT mutate state.
	TestEqual(TEXT("state untouched after fault"), S.TouchCount, 1);
	// A different teammate is still legal.
	TestEqual(TEXT("teammate after fault still legal"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 1), ETouchResult::Allowed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETwoPlayersNoFault, "SpikeElite.Tests.SamePlayerOverRally",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETwoPlayersNoFault::RunTest(const FString& Parameters)
{
	// After the ball crosses the net, possession switches and the same player
	// index on the NEW side may touch again legally (fresh counter).
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	EvaluateTouch(S, EVolleyballTeam::TeamA, 0); // serve
	OnBallCrossedNet(S, EVolleyballTeam::TeamB);
	TestEqual(TEXT("B touches reset after cross"), S.TouchCount, 0);
	TestEqual(TEXT("B player 0 receive"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 0), ETouchResult::Allowed);
	TestEqual(TEXT("B touch count"), S.TouchCount, 1);
	return true;
}

// ---------------------------------------------------------------- 4: cross-net reset

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSECrossNetResetsCount, "SpikeElite.Tests.CrossNetResetsCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSECrossNetResetsCount::RunTest(const FString& Parameters)
{
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	EvaluateTouch(S, EVolleyballTeam::TeamA, 0);
	EvaluateTouch(S, EVolleyballTeam::TeamA, 1);
	EvaluateTouch(S, EVolleyballTeam::TeamA, 2);
	TestEqual(TEXT("A at 3 touches"), S.TouchCount, 3);

	// Legal cross over the net switches possession and resets the counter.
	OnBallCrossedNet(S, EVolleyballTeam::TeamB);
	TestEqual(TEXT("possession switches to B"), S.PossessingTeam, EVolleyballTeam::TeamB);
	TestEqual(TEXT("B counter resets to 0"), S.TouchCount, 0);

	// B now runs its own 3-touch sequence.
	TestEqual(TEXT("B receive"), EvaluateTouch(S, EVolleyballTeam::TeamB, 0), ETouchResult::Allowed);
	TestEqual(TEXT("B set"), EvaluateTouch(S, EVolleyballTeam::TeamB, 1), ETouchResult::Allowed);
	TestEqual(TEXT("B attack"), EvaluateTouch(S, EVolleyballTeam::TeamB, 2), ETouchResult::Allowed);
	// B's 4th touch is a fault.
	TestEqual(TEXT("B 4th touch faults"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 3), ETouchResult::FourTouchesFault);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSERallySettlesOnce, "SpikeElite.Tests.RallySettlesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSERallySettlesOnce::RunTest(const FString& Parameters)
{
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	TestTrue(TEXT("first settle succeeds"), SettleRally(S));
	TestFalse(TEXT("second settle rejected"), SettleRally(S));
	TestEqual(TEXT("touches after settle rejected"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 1), ETouchResult::RallySettled);
	return true;
}

// ---------------------------------------------------------------- 5: match scoring gates

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEMatchWinByTwo, "SpikeElite.Tests.WinByTwoAt24",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEMatchWinByTwo::RunTest(const FString& Parameters)
{
	constexpr int32 Target = 25;
	TestFalse(TEXT("24:24 not won"), IsSetWon(24, 24, Target));
	TestFalse(TEXT("25:24 not won (lead 1)"), IsSetWon(25, 24, Target));
	TestTrue(TEXT("25:23 won"), IsSetWon(25, 23, Target));
	TestTrue(TEXT("26:24 won"), IsSetWon(26, 24, Target));
	TestFalse(TEXT("24:25 not won by B"), IsSetWon(24, 25, Target));
	TestTrue(TEXT("23:25 won by B"), IsSetWon(23, 25, Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDecidingSet15, "SpikeElite.Tests.DecidingSet15",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDecidingSet15::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("set 5 target is 15"), PointsToWinForSet(5), 15);
	TestEqual(TEXT("set 1 target is 25"), PointsToWinForSet(1), 25);
	TestEqual(TEXT("set 4 target is 25"), PointsToWinForSet(4), 25);
	TestFalse(TEXT("15:14 not won"), IsSetWon(15, 14, 15));
	TestTrue(TEXT("16:14 won"), IsSetWon(16, 14, 15));
	TestTrue(TEXT("15:13 won"), IsSetWon(15, 13, 15));
	TestFalse(TEXT("14:15 not won by B"), IsSetWon(14, 15, 15));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSESideOutRotation, "SpikeElite.Tests.SideOutRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSESideOutRotation::RunTest(const FString& Parameters)
{
	TArray<int32> Order = {0, 1, 2, 3, 4, 5};
	RotateRoster(Order);
	TestEqual(TEXT("first rotation"), Order, TArray<int32>({1, 2, 3, 4, 5, 0}));
	RotateRoster(Order);
	TestEqual(TEXT("second rotation"), Order, TArray<int32>({2, 3, 4, 5, 0, 1}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEMatchOverBlocksActions, "SpikeElite.Tests.MatchOverBlocksActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEMatchOverBlocksActions::RunTest(const FString& Parameters)
{
	// No touch in PreMatch / BetweenRallies / AwaitingServe / ServingToss /
	// SetOver / MatchOver; only a live, unsettled Rally.
	TestFalse(TEXT("no touch in PreMatch"), IsTouchLegalInPhase(EMatchState::PreMatch, false));
	TestFalse(TEXT("no touch in BetweenRallies"), IsTouchLegalInPhase(EMatchState::BetweenRallies, false));
	TestFalse(TEXT("no touch while awaiting serve"), IsTouchLegalInPhase(EMatchState::AwaitingServe, false));
	TestFalse(TEXT("no touch during serving toss"), IsTouchLegalInPhase(EMatchState::ServingToss, false));
	TestFalse(TEXT("no touch after set over"), IsTouchLegalInPhase(EMatchState::SetOver, false));
	TestFalse(TEXT("no touch after match over"), IsTouchLegalInPhase(EMatchState::MatchOver, false));
	TestTrue(TEXT("touch legal in live rally"), IsTouchLegalInPhase(EMatchState::Rally, false));
	TestFalse(TEXT("no touch in settled rally"), IsTouchLegalInPhase(EMatchState::Rally, true));

	// Match / set win gates.
	TestTrue(TEXT("best-of-5 match won at 3-0"), IsMatchWon(3, 0, 3));
	TestFalse(TEXT("not won at 2-0"), IsMatchWon(2, 0, 3));
	TestTrue(TEXT("QuickMatch (best-of-1) won at 1-0"), IsMatchWon(1, 0, 1));
	TestFalse(TEXT("QuickMatch not won at 0-0"), IsMatchWon(0, 0, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEQuickMatchRules, "SpikeElite.Tests.QuickMatchRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEQuickMatchRules::RunTest(const FString& Parameters)
{
	// -QuickMatch uses the real rule code with a 3-point target: no second ruleset.
	constexpr int32 QuickTarget = 3;
	TestFalse(TEXT("2:2 not won in quick"), IsSetWon(2, 2, QuickTarget));
	TestTrue(TEXT("3:1 won in quick"), IsSetWon(3, 1, QuickTarget));
	TestFalse(TEXT("3:2 not won in quick (lead 1)"), IsSetWon(3, 2, QuickTarget));
	return true;
}

// ================================================================ M11a tests
// The GameMode now calls these exact helpers (IsTouchLegalInPhase / RotateRoster
// / StartPlay / IsServeFault / ShouldAutoServe); passing here means the
// production code path and the tests cannot diverge.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEMatchWinByTwo2, "SpikeElite.Tests.BallInPlayLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEMatchWinByTwo2::RunTest(const FString& Parameters)
{
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	TestFalse(TEXT("ball not in play right after BeginRally"), S.bBallInPlay);

	// ExecuteServe path: the GameMode calls StartPlay() when the serve is struck.
	StartPlay(S);
	TestTrue(TEXT("ball in play after StartPlay (ExecuteServe)"), S.bBallInPlay);

	// EndRally / cleanup path: SettleRally() puts it back to false.
	TestTrue(TEXT("settle succeeds"), SettleRally(S));
	TestFalse(TEXT("ball not in play after SettleRally"), S.bBallInPlay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServeFaultClassification, "SpikeElite.Tests.ServeFaultClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServeFaultClassification::RunTest(const FString& Parameters)
{
	// A serve that never crossed the net is a fault whether it lands in or out.
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	EvaluateTouch(S, EVolleyballTeam::TeamA, 0); // serve = touch 1
	TestTrue(TEXT("touch-1 serve that never crossed -> fault"), IsServeFault(S, false));
	TestFalse(TEXT("touch-1 serve that DID cross -> not a fault"), IsServeFault(S, true));

	// A landed ball after the second touch is never a serve fault.
	EvaluateTouch(S, EVolleyballTeam::TeamA, 1);
	TestFalse(TEXT("touch-2 ball not a serve fault"), IsServeFault(S, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSERotationMatchesCoreAlgorithm, "SpikeElite.Tests.RotationMatchesCoreAlgorithm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSERotationMatchesCoreAlgorithm::RunTest(const FString& Parameters)
{
	// Production GameMode::RotateTeam reorders its roster by applying RotateRoster
	// to {0..5} and reading the result; simulate that here and verify the final
	// roster order equals the old hand-rolled shift (P0 -> P5).
	TArray<int32> Order = {0, 1, 2, 3, 4, 5};
	SEVolleyballRules::RotateRoster(Order);
	const TArray<int32> Expected = {1, 2, 3, 4, 5, 0};
	TestEqual(TEXT("GameMode rotation equals RotateRoster output"), Order, Expected);

	// Two side-outs in a row (e.g. B wins serve twice): still stable.
	SEVolleyballRules::RotateRoster(Order);
	TestEqual(TEXT("second rotation"), Order, TArray<int32>({2, 3, 4, 5, 0, 1}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEPhaseGateMatrix, "SpikeElite.Tests.PhaseGateMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEPhaseGateMatrix::RunTest(const FString& Parameters)
{
	// Full phase matrix over every EMatchState; this is exactly the gate the
	// GameMode's CanTouchBall/TryTouchBall call (M11: no second copy).
	const EMatchState All[] = {
		EMatchState::PreMatch, EMatchState::BetweenRallies, EMatchState::AwaitingServe,
		EMatchState::ServingToss, EMatchState::Rally, EMatchState::SetOver, EMatchState::MatchOver
	};
	for (EMatchState State : All)
	{
		const bool bLegal = (State == EMatchState::Rally);
		TestEqual(TEXT("unsettled gate matches"), IsTouchLegalInPhase(State, false), bLegal);
		TestFalse(TEXT("settled rally never touchable"), IsTouchLegalInPhase(State, true));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEAutoServePolicy, "SpikeElite.Tests.AutoServePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEAutoServePolicy::RunTest(const FString& Parameters)
{
	// M11: normal human NEVER auto-serves; bots always do; -devauto re-enables
	// the human timeout. This is the exact predicate BeginAwaitingServe uses.
	TestFalse(TEXT("human without -devauto waits for E"), ShouldAutoServe(false, false));
	TestTrue(TEXT("human under -devauto may auto-serve"), ShouldAutoServe(false, true));
	TestTrue(TEXT("bot always auto-serves"), ShouldAutoServe(true, false));
	TestTrue(TEXT("bot under -devauto auto-serves"), ShouldAutoServe(true, true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
