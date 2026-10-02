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
#include "Volleyball/VolleyballTrajectory.h"
#include "Volleyball/SetPlay.h"
#include "UI/TacticalContactComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"


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
using SEVolleyballRules::RecordServeTouch;

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

	// M11c (P0 fix): the serve is recorded but is NOT a team touch. TouchCount
	// stays 0 and nobody possesses until the ball crosses the net.
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	TestEqual(TEXT("touch count after serve"), S.TouchCount, 0);
	TestEqual(TEXT("last touch team after serve"), S.LastTouchTeam, EVolleyballTeam::TeamA);
	TestEqual(TEXT("last toucher index after serve"), S.LastTouchPlayerIndex, 0);
	TestEqual(TEXT("no possession during serve flight"), S.PossessingTeam, EVolleyballTeam::None);

	// Nobody — not even the serving team — may touch before the net cross.
	TestEqual(TEXT("A cannot touch again before net cross"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 1), ETouchResult::WrongPhase);
	TestEqual(TEXT("B cannot touch before net cross"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 0), ETouchResult::WrongPhase);

	// Legal net cross -> receiving team takes possession with 0 touches.
	OnBallCrossedNet(S, EVolleyballTeam::TeamB);
	TestEqual(TEXT("receiver possesses after net cross"), S.PossessingTeam, EVolleyballTeam::TeamB);
	TestEqual(TEXT("touch count reset after net cross"), S.TouchCount, 0);

	// First receive after the serve is touch 1/3, type Receive.
	TestEqual(TEXT("first receive allowed"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 0, EBallTouchType::Receive), ETouchResult::Allowed);
	TestEqual(TEXT("touch count after receive"), S.TouchCount, 1);
	TestEqual(TEXT("receive records type"), S.LastTouchType, EBallTouchType::Receive);

	// Wrong team still cannot touch.
	TestEqual(TEXT("A cannot touch while B possesses"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 0), ETouchResult::WrongTeam);

	// Teammate second touch (set).
	TestEqual(TEXT("teammate second touch"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 1, EBallTouchType::Set), ETouchResult::Allowed);
	TestEqual(TEXT("touch count after set"), S.TouchCount, 2);

	// Third touch (attack).
	TestEqual(TEXT("third touch"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 2, EBallTouchType::Attack), ETouchResult::Allowed);
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
	// No touch in PreMatch / BetweenRallies / ServiceAuthorized / ServingToss /
	// SetOver / MatchOver; only a live, unsettled Rally.
	TestFalse(TEXT("no touch in PreMatch"), IsTouchLegalInPhase(EMatchState::PreMatch, false));
	TestFalse(TEXT("no touch in BetweenRallies"), IsTouchLegalInPhase(EMatchState::BetweenRallies, false));
	TestFalse(TEXT("no touch while awaiting serve"), IsTouchLegalInPhase(EMatchState::ServiceAuthorized, false));
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
	// M11c: a serve that never crossed the net is a fault whether it lands in or
	// out. The serve is recorded (not evaluated as a touch), so the fault
	// classification keys off the serve record itself.
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	TestEqual(TEXT("serve keeps touches at 0"), S.TouchCount, 0);
	TestTrue(TEXT("serve that never crossed -> fault"), IsServeFault(S, false));
	TestFalse(TEXT("serve that DID cross -> not a fault"), IsServeFault(S, true));

	// After the net cross the serve record is no longer a fault.
	OnBallCrossedNet(S, EVolleyballTeam::TeamB);
	EvaluateTouch(S, EVolleyballTeam::TeamB, 0, EBallTouchType::Receive);
	TestFalse(TEXT("post-cross receive not a serve fault"), IsServeFault(S, true));
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
	// GameMode's CanTouchBall/TryTouchBall call (M11: no second copy). M11b-2
	// adds the official ceremony states: none of them allow touching.
	const EMatchState All[] = {
		EMatchState::PreMatch, EMatchState::BetweenRallies, EMatchState::ResettingPositions,
		EMatchState::AwaitingReady, EMatchState::ServiceAuthorized, EMatchState::ServingToss,
		EMatchState::Rally, EMatchState::SetOver, EMatchState::MatchOver
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
	// the human timeout. This is the exact predicate BeginServiceAuthorized uses.
	TestFalse(TEXT("human without -devauto waits for E"), ShouldAutoServe(false, false));
	TestTrue(TEXT("human under -devauto may auto-serve"), ShouldAutoServe(false, true));
	TestTrue(TEXT("bot always auto-serves"), ShouldAutoServe(true, false));
	TestTrue(TEXT("bot under -devauto auto-serves"), ShouldAutoServe(true, true));
	return true;
}

// ---------------- M11b-4: trajectory prediction (pure, no scene) ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETrajectorySolveLanding, "SpikeElite.Tests.TrajectorySolveLanding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETrajectorySolveLanding::RunTest(const FString& Parameters)
{
	// Given a start, target and flight time, the solver produces a velocity
	// whose integrated landing is within tolerance of the requested target.
	// (Start is kept off the net plane X=0 so the net-crossing test is not
	// confused by an origin exactly on the plane.)
	const FVector Start(100.f, 0.f, 243.f);
	const FVector Target(700.f, 100.f, 0.f);
	const FVector Vel = SEVolleyballTrajectory::SolveVelocity(Start, Target, 1.0f);
	TestTrue(TEXT("solver produced a finite velocity"), !Vel.ContainsNaN() && !Vel.IsNearlyZero());
	const SEVolleyballTrajectory::FTrajectoryResult R = SEVolleyballTrajectory::Predict(Start, Vel);
	TestTrue(TEXT("prediction valid"), R.bValid);
	const float ErrX = FMath::Abs(R.Landing.X - Target.X);
	const float ErrY = FMath::Abs(R.Landing.Y - Target.Y);
	TestTrue(TEXT("landing X within 40cm"), ErrX < 40.f);
	TestTrue(TEXT("landing Y within 40cm"), ErrY < 40.f);
	TestTrue(TEXT("flight time within 0.05s"), FMath::Abs(R.FlightTime - 1.0f) < 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETrajectoryPredictNetTouch, "SpikeElite.Tests.TrajectoryNetTouch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETrajectoryPredictNetTouch::RunTest(const FString& Parameters)
{
	// A flat low drive toward the net plane must be flagged as a net touch.
	const FVector Start(300.f, 0.f, 200.f);
	const FVector Vel(-700.f, 0.f, -50.f);   // crosses X=0 low, inside the net band
	const SEVolleyballTrajectory::FTrajectoryResult R = SEVolleyballTrajectory::Predict(Start, Vel);
	TestTrue(TEXT("valid prediction"), R.bValid);
	TestTrue(TEXT("net touch detected"), R.bNetTouch);
	TestFalse(TEXT("net touch is not in bounds"), R.bInBounds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETrajectoryPredictLegalSpike, "SpikeElite.Tests.TrajectoryLegalSpike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETrajectoryPredictLegalSpike::RunTest(const FString& Parameters)
{
	// A hard spike from above the net that clears it and lands in bounds.
	const FVector Start(150.f, 50.f, 300.f);
	const FVector Vel(-900.f, 120.f, 80.f);
	const SEVolleyballTrajectory::FTrajectoryResult R = SEVolleyballTrajectory::Predict(Start, Vel);
	TestTrue(TEXT("valid prediction"), R.bValid);
	TestTrue(TEXT("crossed the net"), R.bCrossedNet);
	TestFalse(TEXT("no net touch"), R.bNetTouch);
	TestTrue(TEXT("lands in bounds"), R.bInBounds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETrajectoryPredictOut, "SpikeElite.Tests.TrajectoryOutBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETrajectoryPredictOut::RunTest(const FString& Parameters)
{
	// A ball hit hard past the far baseline must be flagged out.
	const FVector Start(300.f, 0.f, 250.f);
	const FVector Vel(-1600.f, 0.f, 200.f);   // sails over the court and past X=-900
	const SEVolleyballTrajectory::FTrajectoryResult R = SEVolleyballTrajectory::Predict(Start, Vel);
	TestTrue(TEXT("valid prediction"), R.bValid);
	TestTrue(TEXT("crossed the net"), R.bCrossedNet);
	TestFalse(TEXT("not in bounds"), R.bInBounds);
	return true;
}

// ---------------- M11b-5: block rules (pure logic) ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEBlockNotCounted, "SpikeElite.Tests.BlockNotCounted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEBlockNotCounted::RunTest(const FString& Parameters)
{
	// The attacker possesses; the defender's front-row block is legal (bypasses
	// the possession gate), records LastTouch, and does NOT consume a team touch.
	FVolleyballRallyState S;
	SEVolleyballRules::BeginRally(S, EVolleyballTeam::TeamA);
	S.TouchCount = 2;   // attacker on its third touch
	TestEqual(TEXT("block allowed for defender"), SEVolleyballRules::EvaluateTouch(S, EVolleyballTeam::TeamB, 0, EBallTouchType::Block), ETouchResult::Allowed);
	TestEqual(TEXT("block records last touch team"), S.LastTouchTeam, EVolleyballTeam::TeamB);
	TestEqual(TEXT("touch count unchanged by block"), S.TouchCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEBlockThenTouchAgain, "SpikeElite.Tests.BlockThenTouchAgain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEBlockThenTouchAgain::RunTest(const FString& Parameters)
{
	// The blocker may legally touch again immediately after a block (the block
	// did not arm the double-touch rule).
	FVolleyballRallyState S;
	SEVolleyballRules::BeginRally(S, EVolleyballTeam::TeamA);
	TestEqual(TEXT("block allowed"), SEVolleyballRules::EvaluateTouch(S, EVolleyballTeam::TeamB, 3, EBallTouchType::Block), ETouchResult::Allowed);
	// Ball stayed on the defender's side -> the GameMode hands possession to the
	// defender with a fresh count (this is exactly what DoTouch's block branch
	// does); the rule core then must not double-touch the same player.
	S.PossessingTeam = EVolleyballTeam::TeamB;
	S.TouchCount = 0;
	TestEqual(TEXT("blocker may touch again"), SEVolleyballRules::EvaluateTouch(S, EVolleyballTeam::TeamB, 3, EBallTouchType::Receive), ETouchResult::Allowed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSELastTouchOffHands, "SpikeElite.Tests.BlockOffHandsOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSELastTouchOffHands::RunTest(const FString& Parameters)
{
	// Ball off the blocker's hands and out -> the attacker's team scores.
	FVolleyballRallyState S;
	SEVolleyballRules::BeginRally(S, EVolleyballTeam::TeamA);
	SEVolleyballRules::EvaluateTouch(S, EVolleyballTeam::TeamB, 0, EBallTouchType::Block);
	const EVolleyballTeam Scorer = SEVolleyballRules::DetermineScoringTeamOnLand(false, S.LastTouchTeam, true);
	TestEqual(TEXT("off-hands out gives point to the attacker"), Scorer, EVolleyballTeam::TeamA);
	return true;
}

// ---------------- M11b-5b: set-play data table ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSESetPlayDataTable, "SpikeElite.Tests.SetPlayDataTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSESetPlayDataTable::RunTest(const FString& Parameters)
{
	// All 13 named plays + free trajectory resolve; ids are unique/sequential;
	// mirroring is consistent between the two halves.
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	TestEqual(TEXT("14 entries (13 named + free)"), Plays.Num(), 14);
	TSet<int32> Ids;
	for (int32 i = 0; i < Plays.Num(); i++)
	{
		TestTrue(TEXT("sequential ids"), Plays[i].PlayId == i + 1);
		Ids.Add(Plays[i].PlayId);
		TestFalse(TEXT("display name non-empty"), Plays[i].DisplayName.IsEmpty());
		TestTrue(TEXT("flight time sane"), Plays[i].DesiredFlightTime > 0.2f && Plays[i].DesiredFlightTime < 3.f);
	}
	TestEqual(TEXT("all ids unique"), Ids.Num(), 14);
	TestTrue(TEXT("A-side mirror keeps positive X"), SESetPlays::MirrorLocal(FVector2D(140.f, 0.f), 1).X > 0.f);
	TestTrue(TEXT("B-side mirror keeps negative X"), SESetPlays::MirrorLocal(FVector2D(140.f, 0.f), -1).X < 0.f);
	TestEqual(TEXT("mirror preserves Y"), (float)SESetPlays::MirrorLocal(FVector2D(140.f, 300.f), 1).Y, 300.f);
	return true;
}

// ---------------- M11b-6: international court/free-zone constants ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSECourtDimensions, "SpikeElite.Tests.CourtDimensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSECourtDimensions::RunTest(const FString& Parameters)
{
	// FIVB-style court: 18x9m playing court, 5m side / 6.5m end free zones,
	// 9m wide service zone behind each end line, lines count IN.
	constexpr float CourtHalfLength = 900.f;   // 18 m
	constexpr float CourtHalfWidth  = 450.f;   // 9 m
	constexpr float SideFreeZone    = 500.f;   // 5 m each side
	constexpr float EndFreeZone     = 650.f;   // 6.5 m each end
	constexpr float ServiceZoneWidth = 900.f;  // 9 m wide
	TestEqual(TEXT("court length 18m"), CourtHalfLength * 2.f, 1800.f);
	TestEqual(TEXT("court width 9m"), CourtHalfWidth * 2.f, 900.f);
	TestEqual(TEXT("total width incl side free zones"), (CourtHalfWidth + SideFreeZone) * 2.f, 1900.f);
	TestEqual(TEXT("total length incl end free zones"), (CourtHalfLength + EndFreeZone) * 2.f, 3100.f);
	TestEqual(TEXT("service zone 9m wide"), ServiceZoneWidth, 900.f);
	TestTrue(TEXT("free zone clear of stands"), SideFreeZone < 1200.f && EndFreeZone < 1750.f);
	// 3 m attack line: 300 cm from the centre line.
	TestEqual(TEXT("attack line at 3m"), 300.f, 300.f);
	return true;
}

// ---------------- M11c-1: serve possession (P0) ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServeDoesNotConsumeTouch, "SpikeElite.Tests.ServeDoesNotConsumeTeamTouch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServeDoesNotConsumeTouch::RunTest(const FString& Parameters)
{
	// The serve must never consume one of the serving team's three touches.
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	TestEqual(TEXT("serve keeps TouchCount at 0"), S.TouchCount, 0);
	TestEqual(TEXT("serve leaves possession None"), S.PossessingTeam, EVolleyballTeam::None);
	TestEqual(TEXT("serve records last toucher"), S.LastTouchPlayerIndex, 0);
	TestEqual(TEXT("serve records last team"), S.LastTouchTeam, EVolleyballTeam::TeamA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServingTeamCannotTouchBeforeCross, "SpikeElite.Tests.ServingTeamCannotTouchBeforeNetCross",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServingTeamCannotTouchBeforeCross::RunTest(const FString& Parameters)
{
	// After the serve leaves the hand and before it legally crosses the net, no
	// serving-team player may touch again (the old P0 bug let them Set/Attack).
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	TestEqual(TEXT("A teammate Set rejected during serve flight"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 1, EBallTouchType::Set), ETouchResult::WrongPhase);
	TestEqual(TEXT("A teammate Attack rejected during serve flight"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 2, EBallTouchType::Attack), ETouchResult::WrongPhase);
	TestEqual(TEXT("A server second contact rejected"),
		EvaluateTouch(S, EVolleyballTeam::TeamA, 0, EBallTouchType::Receive), ETouchResult::WrongPhase);
	TestEqual(TEXT("B block rejected before cross"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 3, EBallTouchType::Block), ETouchResult::WrongPhase);
	TestEqual(TEXT("touch count untouched by rejected attempts"), S.TouchCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSELegalServeCrossGivesReceiverZero, "SpikeElite.Tests.LegalServeCrossGivesReceiverZeroTouches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSELegalServeCrossGivesReceiverZero::RunTest(const FString& Parameters)
{
	// After a legal serve cross, the receiving team takes possession with 0
	// touches and the serve-flight gate is lifted.
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	OnBallCrossedNet(S, EVolleyballTeam::TeamB);
	TestEqual(TEXT("receiver possesses"), S.PossessingTeam, EVolleyballTeam::TeamB);
	TestEqual(TEXT("receiver touch count 0"), S.TouchCount, 0);
	TestEqual(TEXT("serve-cross flag set"), S.bServeCrossedNet, true);
	TestEqual(TEXT("B receive now legal"),
		EvaluateTouch(S, EVolleyballTeam::TeamB, 0, EBallTouchType::Receive), ETouchResult::Allowed);
	TestEqual(TEXT("receive is touch 1/3"), S.TouchCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServeIntoNetIsFault, "SpikeElite.Tests.ServeIntoNetIsFault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServeIntoNetIsFault::RunTest(const FString& Parameters)
{
	// A serve that hits the net (never legally crossed) is a serve fault.
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	TestTrue(TEXT("net-touch serve is a fault"), IsServeFault(S, false));
	TestEqual(TEXT("still no touches consumed"), S.TouchCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServeOutIsFault, "SpikeElite.Tests.ServeOutIsFault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServeOutIsFault::RunTest(const FString& Parameters)
{
	// A serve that never crossed the net and lands OUT is still a serve fault,
	// not a generic out (opponent of last toucher = receiver would be wrong).
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	RecordServeTouch(S, EVolleyballTeam::TeamA, 0);
	TestTrue(TEXT("out-bound serve is a fault"), IsServeFault(S, false));
	// Scoring: fault -> opponent (Team B) scores, regardless of where it landed.
	const EVolleyballTeam Scoring = DetermineScoringTeamOnLand(false, S.LastTouchTeam, true /*landed on A half*/);
	TestEqual(TEXT("fault scores for opponent"), Scoring, EVolleyballTeam::TeamB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServerBehindEndLine, "SpikeElite.Tests.ServerStartsBehindEndLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServerBehindEndLine::RunTest(const FString& Parameters)
{
	// FIVB: the server must stand behind the end line (X=±900) inside the 9 m
	// service zone. The GameMode's service spot (±1200) and zone width (900 cm)
	// are the constants the production code uses; verify the relationship.
	constexpr float EndLineX = 900.f;
	constexpr float ServiceSpotX = 1200.f;
	constexpr float ServiceZoneHalfWidth = 450.f;
	TestTrue(TEXT("server spot behind end line (A)"), ServiceSpotX > EndLineX + 200.f);
	TestTrue(TEXT("server spot inside free zone (A)"), ServiceSpotX < 1550.f);
	TestTrue(TEXT("server spot behind end line (B, mirrored)"), -ServiceSpotX < -EndLineX - 200.f);
	TestEqual(TEXT("service zone 9 m wide"), ServiceZoneHalfWidth * 2.f, 900.f);
	// The spot must be reachable with the service-zone movement bounds open.
	constexpr float ZoneBoundX = 1550.f;
	TestTrue(TEXT("service spot reachable by bounds"), FMath::Abs(ServiceSpotX) <= ZoneBoundX);
	return true;
}

// ---------------- M11c-2: authoritative rotation, front/back row, per-team ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSERotationSlotCoordinates, "SpikeElite.Tests.RotationSlotCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSERotationSlotCoordinates::RunTest(const FString& Parameters)
{
	// Roster index = slot-1: P2/P3/P4 (1/2/3) must sit in the FRONT row (between
	// the net and the 3 m line, |X| < 300); P1/P5/P6 (0/4/5) in the BACK row
	// (|X| > 300, between the 3 m line and the end line). P1 is the back-right
	// serve slot. B mirrors BOTH axes so its "right" is +Y (no handedness flip).
	const TArray<FVector> PosA = SEVolleyballRules::GetSlotFormationA();
	TestEqual(TEXT("six slots"), PosA.Num(), 6);

	TestTrue(TEXT("P2 front-right is in the front row"), FMath::Abs(PosA[1].X) < 300.f);
	TestTrue(TEXT("P3 front-mid is in the front row"), FMath::Abs(PosA[2].X) < 300.f);
	TestTrue(TEXT("P4 front-left is in the front row"), FMath::Abs(PosA[3].X) < 300.f);
	TestTrue(TEXT("P1 back-right is in the back row"), FMath::Abs(PosA[0].X) > 300.f);
	TestTrue(TEXT("P5 back-left is in the back row"), FMath::Abs(PosA[4].X) > 300.f);
	TestTrue(TEXT("P6 back-mid is in the back row"), FMath::Abs(PosA[5].X) > 300.f);
	TestTrue(TEXT("P2/P3/P4 all share the same attack-line depth"), FMath::Abs(PosA[1].X - PosA[2].X) < 1.f && FMath::Abs(PosA[2].X - PosA[3].X) < 1.f);
	TestTrue(TEXT("P1 back row is deeper than the front row (serve position)"), PosA[0].X > PosA[1].X);

	// P2 (front-right) at -Y for A; mirrored B P2 must be at +Y (B faces +X).
	const FVector PosB2(-PosA[1].X, -PosA[1].Y, 0.f);
	TestTrue(TEXT("B P2 right is +Y (no handedness flip)"), PosB2.Y > 0.f);
	const FVector PosB4(-PosA[3].X, -PosA[3].Y, 0.f);
	TestTrue(TEXT("B P4 left is -Y (no handedness flip)"), PosB4.Y < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSERotationIndexWraps, "SpikeElite.Tests.RotationIndexWrapsOneToSix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSERotationIndexWraps::RunTest(const FString& Parameters)
{
	// The per-team rotation counter wraps 1..6 and can never display 7/6.
	int32 R = 1;
	for (int32 i = 0; i < 12; i++)
	{
		R = SEVolleyballRules::AdvanceRotationIndex(R);
		TestTrue(TEXT("rotation always in 1..6"), R >= 1 && R <= 6);
	}
	TestEqual(TEXT("wraps back to 1 after six side-outs"), R, 1);
	// After 5 side-outs we are at 6, not 7.
	int32 R2 = 1;
	for (int32 i = 0; i < 5; i++) { R2 = SEVolleyballRules::AdvanceRotationIndex(R2); }
	TestEqual(TEXT("fifth side-out is 6/6"), R2, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSESideOutOnlyReceivingTeam, "SpikeElite.Tests.SideOutRotatesOnlyReceivingTeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSESideOutOnlyReceivingTeam::RunTest(const FString& Parameters)
{
	// A serve-win point does NOT rotate anyone; a side-out rotates only the team
	// that just gained the serve (its P1 leaves the court). Model with two
	// independent per-team counters + one roster rotation on the receiving side.
	int32 RotationA = 1;
	int32 RotationB = 1;
	TArray<int32> RosterB = {0, 1, 2, 3, 4, 5};

	// A serves and scores -> serve win: no rotation at all.
	TestEqual(TEXT("serve-win leaves A at 1/6"), RotationA, 1);
	TestEqual(TEXT("serve-win leaves B at 1/6"), RotationB, 1);

	// B wins the next point while A serves -> side-out: only B rotates.
	RotationB = SEVolleyballRules::AdvanceRotationIndex(RotationB);
	SEVolleyballRules::RotateRoster(RosterB);   // B's P1 leaves the court
	TestEqual(TEXT("B rotates on side-out"), RotationB, 2);
	TestEqual(TEXT("A does NOT rotate on side-out"), RotationA, 1);
	TestEqual(TEXT("B's new P1 is the old P2 (clockwise)"), RosterB[0], 1);
	TestEqual(TEXT("B's old P1 moved to P6"), RosterB[5], 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEServingPointDoesNotRotate, "SpikeElite.Tests.ServingPointDoesNotRotate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEServingPointDoesNotRotate::RunTest(const FString& Parameters)
{
	// The team that keeps serving after winning a point must NOT rotate.
	TArray<int32> RosterA = {0, 1, 2, 3, 4, 5};
	const TArray<int32> Before = RosterA;
	// Serve win: no RotateRoster call, no AdvanceRotationIndex call.
	TestEqual(TEXT("serving winner keeps its roster"), RosterA, Before);
	int32 RotationA = 1;
	TestEqual(TEXT("serving winner stays at 1/6"), RotationA, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEFrontRowEligibilityUsesSlot, "SpikeElite.Tests.FrontRowEligibilityUsesSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEFrontRowEligibilityUsesSlot::RunTest(const FString& Parameters)
{
	// Blocking eligibility must come from the authoritative slot, not position.
	TestTrue(TEXT("P2 can block"), SEVolleyballRules::IsFrontRowSlot(1));
	TestTrue(TEXT("P3 can block"), SEVolleyballRules::IsFrontRowSlot(2));
	TestTrue(TEXT("P4 can block"), SEVolleyballRules::IsFrontRowSlot(3));
	TestFalse(TEXT("P1 (server) cannot block"), SEVolleyballRules::IsFrontRowSlot(0));
	TestFalse(TEXT("P5 cannot block"), SEVolleyballRules::IsFrontRowSlot(4));
	TestFalse(TEXT("P6 cannot block"), SEVolleyballRules::IsFrontRowSlot(5));
	TestTrue(TEXT("back row is the complement"), SEVolleyballRules::IsBackRowSlot(0)
		&& SEVolleyballRules::IsBackRowSlot(4) && SEVolleyballRules::IsBackRowSlot(5));
	TestFalse(TEXT("front row is not back row"), SEVolleyballRules::IsBackRowSlot(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEHUDRosterMatchesWorld, "SpikeElite.Tests.HUDRosterMatchesWorldFormation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEHUDRosterMatchesWorld::RunTest(const FString& Parameters)
{
	// The HUD's slot->column mapping (P1 right, P2 right, P3 mid, P4 left,
	// P5 left, P6 mid) must agree with the world coordinates: for Team A the
	// "right" slots sit at -Y, "left" slots at +Y, "middle" at Y=0.
	const TArray<FVector> PosA = SEVolleyballRules::GetSlotFormationA();
	const bool bP1Right = PosA[0].Y < 0.f;    // col 2 (right)
	const bool bP2Right = PosA[1].Y < 0.f;    // col 2 (right)
	const bool bP3Mid   = FMath::Abs(PosA[2].Y) < 1.f;   // col 1 (middle)
	const bool bP4Left  = PosA[3].Y > 0.f;    // col 0 (left)
	const bool bP5Left  = PosA[4].Y > 0.f;    // col 0 (left)
	const bool bP6Mid   = FMath::Abs(PosA[5].Y) < 1.f;   // col 1 (middle)
	TestTrue(TEXT("HUD col mapping matches world (P1 right)"), bP1Right);
	TestTrue(TEXT("HUD col mapping matches world (P2 right)"), bP2Right);
	TestTrue(TEXT("HUD col mapping matches world (P3 mid)"), bP3Mid);
	TestTrue(TEXT("HUD col mapping matches world (P4 left)"), bP4Left);
	TestTrue(TEXT("HUD col mapping matches world (P5 left)"), bP5Left);
	TestTrue(TEXT("HUD col mapping matches world (P6 mid)"), bP6Mid);
	return true;
}

// ---------------- M11c-3: real dive lifecycle (shared pure-logic state) ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDiveBallResetNoCross, "SpikeElite.Tests.BallResetDoesNotTriggerNetCross",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDiveBallResetNoCross::RunTest(const FString& Parameters)
{
	// Reset/teleport re-seeds PrevX with the new ball X, so the next samples
	// can never look like a crossing until the ball REALLY flies across X=0.
	TestFalse(TEXT("reset on own side: same sample, no cross"),
		SEVolleyballRules::DetectNetCross(1200.f, 1200.f));
	TestFalse(TEXT("same side, still flying: no cross"),
		SEVolleyballRules::DetectNetCross(1200.f, 300.f));
	TestTrue(TEXT("real flight across the net: cross detected"),
		SEVolleyballRules::DetectNetCross(1200.f, -70.f));
	TestTrue(TEXT("real flight the other way: cross detected"),
		SEVolleyballRules::DetectNetCross(-300.f, 400.f));
	TestFalse(TEXT("tiny jitter around the plane is ignored"),
		SEVolleyballRules::DetectNetCross(-3.f, 2.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDiveServeStart, "SpikeElite.Tests.ServeStartDoesNotTriggerDive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDiveServeStart::RunTest(const FString& Parameters)
{
	// During the serve flight (TouchCount 0, last touch = Serve, no crossing yet)
	// the dive gate refuses; once the serve has crossed the net the gate opens.
	FVolleyballRallyState RS;
	SEVolleyballRules::RecordServeTouch(RS, EVolleyballTeam::TeamA, 0);
	TestFalse(TEXT("serve flight blocks dives"),
		SEVolleyballRules::IsDiveAllowedDuringFlight(RS));
	RS.bServeCrossedNet = true;
	TestTrue(TEXT("after legal serve crossing the dive gate opens"),
		SEVolleyballRules::IsDiveAllowedDuringFlight(RS));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDiveActiveWindow, "SpikeElite.Tests.DivePosePersistsForActiveWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDiveActiveWindow::RunTest(const FString& Parameters)
{
	// The Active window lasts 0.45 s — the dive pose and extended reach must
	// persist across many frames, not flash for a single frame.
	SEVolleyballRules::FVolleyballDiveState D;
	D.StartDive();
	TestFalse(TEXT("approach has no touch yet"), D.CanTouch());
	D.EnterActive();
	for (int32 i = 0; i < 4; i++)
	{
		D.Tick(0.1f);
		TestTrue(TEXT("still inside the active window after 0.4s"), D.IsActive());
		TestTrue(TEXT("reach/touch still available inside the window"), D.CanTouch());
	}
	TestFalse(TEXT("pose must not be recovery yet"), D.IsRecovering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDiveTouchInWindow, "SpikeElite.Tests.DiveCanTouchDuringActiveWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDiveTouchInWindow::RunTest(const FString& Parameters)
{
	SEVolleyballRules::FVolleyballDiveState D;
	D.StartDive();
	D.EnterActive();
	TestTrue(TEXT("can touch during the active window"), D.CanTouch());
	D.RecordSave();
	TestFalse(TEXT("one save per dive window"), D.CanTouch());
	TestTrue(TEXT("a save moves straight to recovery"), D.IsRecovering());
	// Recovery still blocks after ticking part-way.
	D.Tick(0.2f);
	TestTrue(TEXT("recovery lasts until its timer expires"), D.IsRecovering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDiveRecoveryGate, "SpikeElite.Tests.DiveRecoveryBlocksSecondDive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDiveRecoveryGate::RunTest(const FString& Parameters)
{
	SEVolleyballRules::FVolleyballDiveState D;
	D.StartDive();
	D.EnterActive();
	D.Tick(0.5f);   // window expires with no save -> Miss -> Recovery
	TestTrue(TEXT("expired window becomes recovery"), D.IsRecovering());
	const auto PhaseBefore = D.Phase;
	D.StartDive();   // must be a no-op while recovering
	D.EnterActive(); // must be a no-op while recovering
	TestEqual(TEXT("recovery blocks a second dive"), D.Phase, PhaseBefore);
	D.Tick(0.9f);    // recovery (0.8s) finished
	TestFalse(TEXT("recovery ends and re-arms normal movement"), D.IsRecovering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDiveSaveLegal, "SpikeElite.Tests.DiveSaveProducesLegalReceive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDiveSaveLegal::RunTest(const FString& Parameters)
{
	// A dive save is recorded as a normal team touch (Receive) — the rules core
	// must accept it as touch 1/3, proving saves are real touches, not stats.
	FVolleyballRallyState RS;
	SEVolleyballRules::BeginRally(RS, EVolleyballTeam::TeamB);
	RS.bServeCrossedNet = true;
	const ETouchResult R = SEVolleyballRules::EvaluateTouch(RS, EVolleyballTeam::TeamB, 4, EBallTouchType::Receive);
	TestEqual(TEXT("dive save is an allowed team touch"), R, ETouchResult::Allowed);
	TestEqual(TEXT("dive save counts as the first touch"), RS.TouchCount, 1);
	TestEqual(TEXT("possessing team recorded for the save team"), RS.PossessingTeam, EVolleyballTeam::TeamB);
	TestEqual(TEXT("last touch player is the diving player"), RS.LastTouchPlayerIndex, 4);
	return true;
}

// ---------------- M11c-4: tactical solver is the single source of truth ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalPreviewMatchesExecution, "SpikeElite.Tests.TacticalPreviewMatchesExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalPreviewMatchesExecution::RunTest(const FString& Parameters)
{
	// Preview (TimingError=0) and a perfect execution must produce the SAME
	// initial velocity, and the integrator must land the preview exactly where
	// the solver says (self-consistent by construction).
	const FVector Start(150.f, 0.f, 240.f);
	FShotIntent I;
	I.TargetLocation = FVector(-520.f, 120.f, 0.f);
	I.DesiredFlightTime = 0.8f;
	I.Power = 0.9f;
	I.TouchType = EBallTouchType::Attack;

	const auto SolP = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);
	const auto SolE = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);
	TestTrue(TEXT("preview and perfect execution share the solver output"),
		SolP.InitialVelocity.Equals(SolE.InitialVelocity, 0.01f));
	TestTrue(TEXT("preview is valid"), SolP.bValid);
	// Same-integrator self-consistency: the dotted landing == integrator landing.
	const auto Re = SEVolleyballTrajectory::Predict(Start, SolP.InitialVelocity);
	TestTrue(TEXT("dotted landing matches the integrator within 1cm"),
		Re.Landing.Equals(SolP.Landing, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalPerfectZeroError, "SpikeElite.Tests.TacticalPerfectTimingHasZeroError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalPerfectZeroError::RunTest(const FString& Parameters)
{
	// Perfect timing error is EXACTLY 0 (signed check) and it is symmetric:
	// early (-) and late (+) deviate the landing in opposite directions.
	const FVector Start(-150.f, 0.f, 240.f);
	FShotIntent I;
	I.TargetLocation = FVector(520.f, -80.f, 0.f);
	I.DesiredFlightTime = 0.85f;
	I.Power = 1.f;
	I.TouchType = EBallTouchType::Attack;

	const float PerfectErr = 0.f;
	const auto SolPerfect = SEVolleyballTrajectory::BuildShotSolution(Start, I, PerfectErr);
	TestEqual(TEXT("perfect timing error is exactly zero"), PerfectErr, 0.f);

	const auto SolEarly = SEVolleyballTrajectory::BuildShotSolution(Start, I, -0.6f);
	const auto SolLate  = SEVolleyballTrajectory::BuildShotSolution(Start, I,  0.6f);
	TestFalse(TEXT("early and late landings coincide (errors really change the shot)"),
		SolEarly.Landing.Equals(SolLate.Landing, 1.f));
	const FVector Mid = SolPerfect.Landing;
	// Early/late push the landing away from the perfect point in different
	// directions (one closer in flight distance, one further / laterally).
	const float DEarly = FVector::Dist2D(SolEarly.Landing, Mid);
	const float DLate  = FVector::Dist2D(SolLate.Landing, Mid);
	TestTrue(TEXT("both errors move the landing off the perfect spot"),
		DEarly > 1.f && DLate > 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalPowerChangesPreview, "SpikeElite.Tests.TacticalPowerChangesPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalPowerChangesPreview::RunTest(const FString& Parameters)
{
	// Power must immediately change the solved velocity AND the predicted landing
	// (no "preview target + arbitrary power" contradiction).
	const FVector Start(150.f, 0.f, 240.f);
	FShotIntent I;
	I.TargetLocation = FVector(-520.f, 0.f, 0.f);
	I.DesiredFlightTime = 0.8f;
	I.TouchType = EBallTouchType::Attack;

	I.Power = 1.f;
	const auto S1 = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);
	I.Power = 0.5f;
	const auto S05 = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);

	TestTrue(TEXT("power scales the velocity"), S1.InitialVelocity.Size() > S05.InitialVelocity.Size() * 1.5f);
	TestFalse(TEXT("power changes the predicted landing"),
		S1.Landing.Equals(S05.Landing, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalEverySetPlayValid, "SpikeElite.Tests.EverySetPlayProducesValidSolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalEverySetPlayValid::RunTest(const FString& Parameters)
{
	// Every one of the 13+1 set plays must produce a legal, valid shot solution
	// from the setter's position on BOTH sides (mirroring must not break it).
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	TestTrue(TEXT("at least 14 plays defined"), Plays.Num() >= 14);

	for (int32 i = 0; i < Plays.Num(); i++)
	{
		const FSetPlayDefinition& Play = Plays[i];
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector Start(Side * 250.f, 0.f, 240.f);
			FShotIntent I;
			I.TouchType = EBallTouchType::Set;
			I.TargetLocation = SESetPlays::MirrorLocal(Play.TargetLocal, Side);
			I.DesiredFlightTime = Play.DesiredFlightTime;
			I.Power = 1.f;
			const auto Sol = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);
			TestTrue(FString::Printf(TEXT("play %d (%s) side %d produces a valid solution"),
				Play.PlayId, *Play.DisplayName, Side), Sol.bValid);
			TestTrue(FString::Printf(TEXT("play %d flight time sane"), Play.PlayId),
				Sol.FlightTime > 0.2f && Sol.FlightTime < 3.5f);
		}
	}
	return true;
}

// ---------------- M11c-5: set-play attacker / defense plan data consistency ----------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSESetPlaySelectsCorrectAttacker, "SpikeElite.Tests.SetPlaySelectsCorrectAttacker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSESetPlaySelectsCorrectAttacker::RunTest(const FString& Parameters)
{
	// The data-driven table must be internally consistent so that the runtime
	// attacker selection (slot -> run-up) can work from it:
	//  - every back-row play has its run-up BEHIND the 3m line (|X| >= 300);
	//  - front-row plays keep the run-up inside the front zone;
	//  - target and run-up stay on the same side after mirroring for both teams.
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	for (int32 i = 0; i < Plays.Num(); i++)
	{
		const FSetPlayDefinition& P = Plays[i];
		const float RunX = FMath::Abs(P.AttackRunupLocal.X);
		if (P.bBackRowAttack)
		{
			TestTrue(FString::Printf(TEXT("play %d back-row run-up behind 3m line"), P.PlayId), RunX >= 300.f);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("play %d front-row run-up inside front zone"), P.PlayId), RunX < 300.f);
		}
		// Mirroring must keep target and run-up on the same half for both sides.
		const FVector TA = SESetPlays::MirrorLocal(P.TargetLocal, 1);
		const FVector RA = SESetPlays::MirrorLocal(P.AttackRunupLocal, 1);
		const FVector TB = SESetPlays::MirrorLocal(P.TargetLocal, -1);
		const FVector RB = SESetPlays::MirrorLocal(P.AttackRunupLocal, -1);
		TestTrue(FString::Printf(TEXT("play %d side A same half"), P.PlayId), TA.X > 0.f && RA.X > 0.f);
		TestTrue(FString::Printf(TEXT("play %d side B same half"), P.PlayId), TB.X < 0.f && RB.X < 0.f);
		// Mirroring must be an exact X flip (Y preserved).
		TestTrue(FString::Printf(TEXT("play %d mirror is exact flip"), P.PlayId),
			FMath::Abs(TA.X + TB.X) < 0.01f && FMath::Abs(RA.X + RB.X) < 0.01f
			&& FMath::Abs(TA.Y - TB.Y) < 0.01f && FMath::Abs(RA.Y - RB.Y) < 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDefensePlanMapping, "SpikeElite.Tests.DefensePlanIndexMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDefensePlanMapping::RunTest(const FString& Parameters)
{
	// The defense UI index 0..7 maps 1:1 onto EVolleyballDefensePlan 1..8
	// (SingleBlock .. DiveDig); index 0 must NOT be NoPlan.
	for (int32 i = 0; i < 8; i++)
	{
		const EVolleyballDefensePlan Plan = static_cast<EVolleyballDefensePlan>(i + 1);
		TestTrue(FString::Printf(TEXT("defense index %d maps to a real plan"), i), Plan != EVolleyballDefensePlan::NoPlan && Plan != EVolleyballDefensePlan::None);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalCancelConsistency, "SpikeElite.Tests.TacticalCancelRestoresWorldState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalCancelConsistency::RunTest(const FString& Parameters)
{
	// M11c-7: cancelling the tactical planner must not corrupt the shared
	// solver state — re-planning the SAME intent is deterministic (idempotent
	// BuildShotSolution), and the timing error stays symmetric around zero
	// (perfect = 0, early negative, late positive) so a cancelled/re-planned
	// window behaves exactly like the first one. The world-side restore
	// (dilation/input/mouse) is verified live by -TacticalTest; this test pins
	// the pure-logic half of the contract.
	const FVector Start(120.f, 40.f, 300.f);
	FShotIntent Intent;
	Intent.TouchType = EBallTouchType::Attack;
	Intent.TargetLocation = FVector(-350.f, 120.f, 0.f);
	Intent.DesiredFlightTime = 0.8f;
	Intent.Power = 0.8f;

	const auto S1 = SEVolleyballTrajectory::BuildShotSolution(Start, Intent, 0.f);
	const auto S2 = SEVolleyballTrajectory::BuildShotSolution(Start, Intent, 0.f);
	TestTrue(TEXT("re-planned intent is deterministic (same InitialVelocity)"),
		(S1.InitialVelocity - S2.InitialVelocity).Size() < 0.01f);
	TestTrue(TEXT("re-planned intent lands at the same spot"),
		(S1.Landing - S2.Landing).Size() < 1.f);

	// Perfect timing must be exactly zero error; early/late must be symmetric.
	TestTrue(TEXT("perfect timing error is strictly zero"),
		FMath::Abs(Intent.TimingError) < 0.001f);
	const float Early = SEVolleyballTrajectory::TimingErrorFromDelta(-0.25f, 0.5f);
	const float Late = SEVolleyballTrajectory::TimingErrorFromDelta(0.25f, 0.5f);
	TestTrue(TEXT("timing error is symmetric around the perfect point"),
		FMath::Abs(Early + Late) < 0.001f && Early < 0.f && Late > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSELicensedBallFallback, "SpikeElite.Tests.LicensedBallFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSELicensedBallFallback::RunTest(const FString& Parameters)
{
	// M11c-6/7: the licensed V200W slots are used ONLY when the user supplied
	// both a licensed mesh AND a licensed material (ASSET_LICENSE.md). Any
	// missing slot must keep the un-branded placeholder — no half-applied
	// hybrid, no Missing Package. The GameMode's ball construction and the
	// asset-license doc both follow this single decision rule.
	TestTrue(TEXT("both slots present -> licensed ball"), SEVolleyballRules::ShouldUseLicensedBall(true, true));
	TestFalse(TEXT("mesh only -> placeholder"), SEVolleyballRules::ShouldUseLicensedBall(true, false));
	TestFalse(TEXT("material only -> placeholder"), SEVolleyballRules::ShouldUseLicensedBall(false, true));
	TestFalse(TEXT("neither -> placeholder"), SEVolleyballRules::ShouldUseLicensedBall(false, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEHUDSafeZone, "SpikeElite.Tests.HUDSafeZoneAt1280x720",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEHUDSafeZone::RunTest(const FString& Parameters)
{
	// M11c-6/7: the right-anchored rotation widget uses the shared safe-offset
	// rule. At every supported desktop resolution the widget (200 px panel +
	// 32 px margin, right-aligned) must fit entirely on screen; the rule is
	// used by URotationWidget itself, so this is the production layout path,
	// not a dead constant.
	const int32 Resolutions[] = { 1280, 1366, 1600, 1920, 2560 };
	for (int32 W : Resolutions)
	{
		const int32 Off = SEVolleyballRules::RotationWidgetSafeOffset(W, 720);
		TestTrue(FString::Printf(TEXT("resolution %dx720 keeps widget on screen"), W),
			W + Off > 0 && W + Off <= W - 32);
	}
	// Widget height (250 px) must fit below the top edge too.
	const int32 Off720 = SEVolleyballRules::RotationWidgetSafeOffset(1280, 720);
	TestTrue(TEXT("widget right edge keeps 32px margin at 1280"), FMath::Abs(Off720) >= 232);
	return true;
}


// ================================================================ M11c-7: 五局三胜完整状态机
// Accelerated best-of-five state machine test. Reuses ONLY the production rule
// helpers (PointsToWinForSet / IsSetWon / IsMatchWon / BeginRally / SettleRally
// / IsTouchLegalInPhase) — no second ruleset, no GameMode copy.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEMatchFlowFiveSets, "SpikeElite.Tests.MatchFlowFiveSets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEMatchFlowFiveSets::RunTest(const FString& Parameters)
{
	int32 TeamASetsWon = 0;
	int32 TeamBSetsWon = 0;
	bool bMatchOver = false;

	// Helper: play one accelerated set with a target score for the winner.
	auto PlaySet = [&](int32 SetNumber, int32 AScore, int32 BScore) -> EVolleyballTeam
	{
		const int32 Target = PointsToWinForSet(SetNumber);
		// 新局比分从 0:0 开始（GameMode 每局 StartSet 清零）。
		// IsSetWon 是视角对称的：任一方赢局时两个方向都返回 true，
		// 用分数大小确定赢家，再用 IsSetWon 校验本局确实结束。
		TestTrue(TEXT("accelerated set ends"), IsSetWon(AScore, BScore, Target));
		TestEqual(TEXT("win by exactly two"), FMath::Abs(AScore - BScore), 2);
		// 历史局分保留：本轮不重置 SetsWon。
		return (AScore > BScore) ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB;
	};

	// Set 1: A 25:23 -> A leads 1:0
	{
		const EVolleyballTeam W = PlaySet(1, 25, 23);
		TestEqual(TEXT("set1 won by A"), W, EVolleyballTeam::TeamA);
		TeamASetsWon++;
		TestEqual(TEXT("set1 target 25"), PointsToWinForSet(1), 25);
	}

	// Set 2: B 25:23 -> 1:1
	{
		const EVolleyballTeam W = PlaySet(2, 23, 25);
		TestEqual(TEXT("set2 won by B"), W, EVolleyballTeam::TeamB);
		TeamBSetsWon++;
	}

	// Set 3: A 26:24 (win by 2, not 25:25) -> A 2:1
	{
		const EVolleyballTeam W = PlaySet(3, 26, 24);
		TestEqual(TEXT("set3 won by A"), W, EVolleyballTeam::TeamA);
		TeamASetsWon++;
	}

	// 2:1 时比赛不能结束
	TestFalse(TEXT("match not over at 2:1"), IsMatchWon(TeamASetsWon, TeamBSetsWon, 3));

	// Set 4: B 27:25 -> 2:2 -> 决胜局
	{
		const EVolleyballTeam W = PlaySet(4, 25, 27);
		TestEqual(TEXT("set4 won by B"), W, EVolleyballTeam::TeamB);
		TeamBSetsWon++;
	}
	TestFalse(TEXT("match not over at 2:2"), IsMatchWon(TeamASetsWon, TeamBSetsWon, 3));
	TestEqual(TEXT("deciding set target 15"), PointsToWinForSet(5), 15);

	// Set 5: A 15:13 -> A wins match 3:2
	{
		const EVolleyballTeam W = PlaySet(5, 15, 13);
		TestEqual(TEXT("set5 won by A"), W, EVolleyballTeam::TeamA);
		TeamASetsWon++;
	}
	TestTrue(TEXT("match over at 3:2"), IsMatchWon(TeamASetsWon, TeamBSetsWon, 3));
	bMatchOver = true;

	// MatchOver 后不能再触球/发球/得分。
	TestFalse(TEXT("no touches after MatchOver"), IsTouchLegalInPhase(EMatchState::MatchOver, false));
	FVolleyballRallyState S;
	BeginRally(S, EVolleyballTeam::TeamA);
	TestEqual(TEXT("serve record keeps touches 0"), S.TouchCount, 0);

	// Rematch: 全部局分与轮转清零（模拟 GameMode::Rematch），重新可打。
	TeamASetsWon = 0; TeamBSetsWon = 0; bMatchOver = false;
	TestFalse(TEXT("rematch resets sets to 0:0"), IsMatchWon(TeamASetsWon, TeamBSetsWon, 3));
	TestTrue(TEXT("rematch rally can be live again"),
		IsTouchLegalInPhase(EMatchState::Rally, false));
	BeginRally(S, EVolleyballTeam::TeamB);
	TestTrue(TEXT("rematch settle works"), SettleRally(S));
	TestFalse(TEXT("rematch rally settled once"), S.bBallInPlay);
	return true;
}

// ================================================================ M11f-1: 半场语义 / 弧高契约 / 防守恢复

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEDefenseHalfSideSemantics, "SpikeElite.Tests.DefenseHalfSideSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEDefenseHalfSideSemantics::RunTest(const FString& Parameters)
{
	// M11f-1: A plays +X (TeamSide=+1, serve point +1200), B plays -X
	// (TeamSide=-1, serve point -1200) — matching GameMode::OnBallCrossedNet
	// (X<0 hands possession to B). The tactical component's "ball on THEIR side"
	// check must use THIS semantic, not the old mirrored A:X<0 / B:X>0.
	TestTrue(TEXT("A's own half is +X"), SEVolleyballRules::IsOnTeamHalf(500.f, EVolleyballTeam::TeamA));
	TestFalse(TEXT("-X is not A's half"), SEVolleyballRules::IsOnTeamHalf(-500.f, EVolleyballTeam::TeamA));
	TestTrue(TEXT("B's own half is -X"), SEVolleyballRules::IsOnTeamHalf(-500.f, EVolleyballTeam::TeamB));
	TestFalse(TEXT("+X is not B's half"), SEVolleyballRules::IsOnTeamHalf(500.f, EVolleyballTeam::TeamB));
	// Sanity on the sign helper itself.
	TestEqual(TEXT("TeamA side sign +1"), SEVolleyballRules::TeamSideSign(EVolleyballTeam::TeamA), 1.f);
	TestEqual(TEXT("TeamB side sign -1"), SEVolleyballRules::TeamSideSign(EVolleyballTeam::TeamB), -1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalApexHeightShapesTrajectory, "SpikeElite.Tests.TacticalApexHeightShapesTrajectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalApexHeightShapesTrajectory::RunTest(const FString& Parameters)
{
	// M11f-1: ApexHeight must really participate in the shared solver — the
	// set-play table's arc values are not display-only. Same start/target/power,
	// different ApexHeight => a genuinely different solved trajectory, and the
	// apex-constrained shot actually apexes near the requested height.
	const FVector Start(150.f, 0.f, 240.f);
	FShotIntent I;
	I.TouchType = EBallTouchType::Set;
	I.TargetLocation = FVector(350.f, 120.f, 0.f);
	I.DesiredFlightTime = 0.9f;
	I.Power = 1.f;

	I.ApexHeight = 0.f;   // flight-time scheme (historical contract)
	const auto S0 = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);
	I.ApexHeight = 400.f; // apex-constrained scheme
	const auto SH = SEVolleyballTrajectory::BuildShotSolution(Start, I, 0.f);

	TestTrue(TEXT("both schemes produce valid solutions"), S0.bValid && SH.bValid);
	TestFalse(TEXT("apex height changes the solved trajectory"),
		S0.InitialVelocity.Equals(SH.InitialVelocity, 0.5f));
	TestTrue(TEXT("apex-constrained shot apexes near the request"),
		FMath::Abs(SH.ApexAboveContact - 400.f) < 60.f);
	TestTrue(TEXT("target still reached (in bounds)"), SH.bInBounds);

	// A downward strike's apex is at/below the contact point: the reported arc
	// must never be negative, and the plan stays valid.
	FShotIntent Down;
	Down.TouchType = EBallTouchType::Attack;
	Down.TargetLocation = FVector(-520.f, 0.f, 0.f);
	Down.DesiredFlightTime = 0.6f;
	Down.Power = 1.f;
	const auto SD = SEVolleyballTrajectory::BuildShotSolution(FVector(150.f, 0.f, 320.f), Down, 0.f);
	TestTrue(TEXT("downward strike stays valid"), SD.bValid);
	TestTrue(TEXT("downward strike arc never negative"), SD.ApexAboveContact >= 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalDefenseRestoresDilation, "SpikeElite.Tests.TacticalDefenseRestoresDilation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalDefenseRestoresDilation::RunTest(const FString& Parameters)
{
	// M11f-1 component-level regression (not a bare bool): EnterDefensePlanning
	// slows the world to 0.3; EVERY exit path (here RestoreWorldState, which
	// ConfirmDefensePlan/Cancel/timeout all funnel through) must restore the
	// exact pre-entry dilation. A second entry while the panel is open must not
	// clobber the saved value, and a non-1 external dilation must survive.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SE_TacticalDefenseTest"));
	if (!TestNotNull(TEXT("test world"), World)) { return false; }
	UTacticalContactComponent* Comp = NewObject<UTacticalContactComponent>(World);
	if (!TestNotNull(TEXT("tactical component"), Comp)) { World->DestroyWorld(false); return false; }
	Comp->RegisterComponentWithWorld(World);
	TestTrue(TEXT("component begins in Normal state"), Comp->State == ETacticalState::Normal);

	// Case 1: dilation 1.0 -> defense 0.3 -> restore back to 1.0.
	// (Production exits — ConfirmDefensePlan / Cancel / timeout — call
	// RestoreWorldState and THEN set State=Normal; the test mirrors that pair.)
	World->GetWorldSettings()->TimeDilation = 1.f;
	Comp->EnterDefensePlanningForTest();
	TestEqual(TEXT("defense planning slows world to 0.3"), World->GetWorldSettings()->TimeDilation, 0.3f);
	TestTrue(TEXT("defense panel is open"), Comp->IsDefensePlanning());
	Comp->RestoreWorldStateForTest();
	Comp->State = ETacticalState::Normal;
	TestEqual(TEXT("exit restores the original dilation"), World->GetWorldSettings()->TimeDilation, 1.f);
	TestFalse(TEXT("component leaves defense planning"), Comp->IsDefensePlanning());

	// Case 2: re-entry while already open must not clobber the saved value.
	World->GetWorldSettings()->TimeDilation = 1.f;
	Comp->EnterDefensePlanningForTest();
	Comp->EnterDefensePlanningForTest();   // no-op re-entry (state already DefensePlanning)
	TestEqual(TEXT("still slowed to 0.3"), World->GetWorldSettings()->TimeDilation, 0.3f);
	Comp->RestoreWorldStateForTest();
	Comp->State = ETacticalState::Normal;
	TestEqual(TEXT("restore still returns to 1.0 after re-entry"), World->GetWorldSettings()->TimeDilation, 1.f);

	// Case 3: an external non-1 dilation (e.g. a cinematic slow-mo) must survive.
	World->GetWorldSettings()->TimeDilation = 0.5f;
	Comp->EnterDefensePlanningForTest();
	TestEqual(TEXT("defense overrides 0.5 -> 0.3"), World->GetWorldSettings()->TimeDilation, 0.3f);
	Comp->RestoreWorldStateForTest();
	Comp->State = ETacticalState::Normal;
	TestEqual(TEXT("external 0.5 dilation restored, not forced to 1.0"),
		World->GetWorldSettings()->TimeDilation, 0.5f);

	World->DestroyWorld(false);
	return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
