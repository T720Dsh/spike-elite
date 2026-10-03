// SPDX-License-Identifier: MIT
#include "Misc/AutomationTest.h"
#include "Volleyball/PracticeFlow.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPracticeFeedTest,"SpikeElite.Tests.PracticeBallisticFeed",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPracticeFeedTest::RunTest(const FString& Parameters)
{
	for(float T:{.8f,1.1f,1.5f})
	{
		const FVector A(-400,0,500),B(600,0,210);
		const FVector V=SEPractice::FeedVelocity(A,B,T);
		TestTrue(TEXT("feed reaches intended contact under gravity"),(A+V*T-FVector(0,0,490.f*T*T)).Equals(B,.01f));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPracticeGoalsTest,"SpikeElite.Tests.PracticeGoalIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPracticeGoalsTest::RunTest(const FString& Parameters)
{
	const FVector Goal(200,0,0);
	TestFalse(TEXT("untouched receive feed cannot succeed"),SEPractice::IsSuccessfulLanding(ETrainingDrill::ReceiveTarget,Goal,Goal,0,false));
	TestTrue(TEXT("received ball in target succeeds"),SEPractice::IsSuccessfulLanding(ETrainingDrill::ReceiveTarget,Goal,Goal,1,false));
	TestFalse(TEXT("set alone is not set+attack success"),SEPractice::IsSuccessfulLanding(ETrainingDrill::SetAttack,Goal,Goal,1,false));
	TestTrue(TEXT("completed attack in target succeeds"),SEPractice::IsSuccessfulLanding(ETrainingDrill::SetAttack,Goal,Goal,1,true));
	TestFalse(TEXT("outside target fails"),SEPractice::IsSuccessfulLanding(ETrainingDrill::ServePlacement,Goal+FVector(250,0,0),Goal,0,false));
	return true;
}
#endif
