// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Volleyball/MatchMode.h"
namespace SEPractice
{
	inline FVector FeedVelocityForServe(FVector Start,FVector Target)
	{ constexpr float T=1.65f; FVector V=(Target-Start)/T; V.Z+=490.f*T; return V; }
	inline FVector ServeVelocity(FVector Start,FRotator Aim)
	{
		const float Offset=FMath::Clamp(FMath::FindDeltaAngleDegrees(180.f,Aim.Yaw),-20.f,20.f);
		const float Depth=FMath::Clamp(-620.f+(Aim.Pitch+12.f)*12.f,-880.f,-240.f);
		const FVector Target(Depth,Start.Y-FMath::Tan(FMath::DegreesToRadians(Offset))*(Start.X-Depth),10.5f);
		return FeedVelocityForServe(Start,Target);
	}
	inline FVector FeedVelocity(FVector Start, FVector Contact, float Seconds)
	{ Seconds=FMath::Max(.2f,Seconds); FVector V=(Contact-Start)/Seconds; V.Z+=490.f*Seconds; return V; }
	inline bool IsSuccessfulLanding(ETrainingDrill Drill,FVector Landing,FVector Goal,int32 HumanTouches,bool bAttackFinished)
	{ return FVector::Dist2D(Landing,Goal)<=240.f && (Drill==ETrainingDrill::ServePlacement || HumanTouches>0)
		&& (Drill!=ETrainingDrill::SetAttack || bAttackFinished); }
}
