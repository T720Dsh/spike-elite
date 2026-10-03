#include "Misc/AutomationTest.h"
#include "Volleyball/ContactAnimation.h"
#include "Volleyball/VolleyballTrajectory.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContactCurveTest,"SpikeElite.Tests.ContactAnimationContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FContactCurveTest::RunTest(const FString&)
{
	TestEqual(TEXT("touch starts at impact, not windup"),SEContactAnimation::ProgressAfterContact(SEContactAnimation::Duration),.35f);
	TestEqual(TEXT("ends at rest"),SEContactAnimation::ProgressAfterContact(0),1.f);
	for(float Boundary:{.35f,.65f}) TestTrue(TEXT("keyframe boundary continuous"),FMath::Abs(SEContactAnimation::Curve(Boundary-.00001f,225,170,70,5)-SEContactAnimation::Curve(Boundary+.00001f,225,170,70,5))<.02f);
	float Previous=225;
	for(int32 I=0;I<=1000;++I) { const float Value=SEContactAnimation::Curve(I/1000.f,225,170,70,5); TestTrue(TEXT("swing smooth and finite"),FMath::IsFinite(Value) && FMath::Abs(Value-Previous)<1.f); Previous=Value; }
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FServeArcTest,"SpikeElite.Tests.MouseServeArcSolver",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FServeArcTest::RunTest(const FString&)
{
	for(int32 Side:{-1,1})
	{
		FShotIntent I; I.TargetLocation=FVector(-Side*600,180,0); I.ApexHeight=220;
		const FVector Start(Side*1200,0,280);
		const auto Low=SEVolleyballTrajectory::BuildShotSolution(Start,I,0);
		I.ApexHeight=450; const auto High=SEVolleyballTrajectory::BuildShotSolution(Start,I,0);
		TestTrue(TEXT("arc changes flight, not landing target"),High.FlightTime>Low.FlightTime && High.InitialVelocity.Z>Low.InitialVelocity.Z);
		TestTrue(TEXT("both sides legally cross net"),Low.bCrossedNet && High.bCrossedNet);
		TestTrue(TEXT("landing matches target within integration tolerance"),FVector::Dist2D(High.Landing,I.TargetLocation)<35);
	}
	return true;
}
#endif
