#pragma once
#include "CoreMinimal.h"
#include "VolleyballEnums.h"

namespace SEContactAnimation
{
	constexpr float Duration=.72f;
	// 0 = preparation, .35 = actual contact, .65 = follow-through, 1 = recovered.
	inline float Curve(float P,float Ready,float Impact,float Follow,float Rest)
	{
		P=FMath::Clamp(P,0.f,1.f);
		const float A=P<.35f?0.f:P<.65f?.35f:.65f;
		const float B=P<.35f?.35f:P<.65f?.65f:1.f;
		const float X=(P-A)/(B-A), Smooth=X*X*(3.f-2.f*X);
		return FMath::Lerp(P<.35f?Ready:P<.65f?Impact:Follow,P<.35f?Impact:P<.65f?Follow:Rest,Smooth);
	}
	inline float ProgressAfterContact(float Remaining)
	{
		return .35f+.65f*FMath::Clamp(1.f-Remaining/Duration,0.f,1.f);
	}
}
