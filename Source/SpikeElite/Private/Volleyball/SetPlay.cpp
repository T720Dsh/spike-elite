// SPDX-License-Identifier: MIT
#include "Volleyball/SetPlay.h"

namespace SESetPlays
{
	const TArray<FSetPlayDefinition>& GetPlays()
	{
		static const TArray<FSetPlayDefinition> Plays = {
			{ 1,  TEXT("四号位高球"), TEXT("四号位"), FVector2D(140.f, -300.f), 350.f, 1.20f, false, 0 },
			{ 2,  TEXT("平拉开"),     TEXT("四号位"), FVector2D(140.f, -300.f), 200.f, 0.55f, false, 1 },
			{ 3,  TEXT("副攻近体快"), TEXT("副攻"),   FVector2D(120.f,  -80.f), 160.f, 0.35f, false, 2 },
			{ 4,  TEXT("副攻短平快"), TEXT("副攻"),   FVector2D(120.f, -150.f), 180.f, 0.45f, false, 2 },
			{ 5,  TEXT("副攻拉三"),   TEXT("副攻"),   FVector2D(140.f, -120.f), 220.f, 0.60f, false, 1 },
			{ 6,  TEXT("副攻背快"),   TEXT("副攻"),   FVector2D(120.f,   80.f), 160.f, 0.35f, false, 2 },
			{ 7,  TEXT("副攻半高球"), TEXT("副攻"),   FVector2D(140.f,    0.f), 280.f, 0.70f, false, 1 },
			{ 8,  TEXT("后三"),       TEXT("后排"),   FVector2D(280.f,    0.f), 260.f, 0.75f, true,  1 },
			{ 9,  TEXT("后二进攻"),   TEXT("后排"),   FVector2D(280.f,  300.f), 260.f, 0.75f, true,  1 },
			{ 10, TEXT("二号位冲进"), TEXT("二号位"), FVector2D(120.f,  300.f), 180.f, 0.50f, false, 2 },
			{ 11, TEXT("四号位冲进"), TEXT("四号位"), FVector2D(120.f, -300.f), 180.f, 0.50f, false, 2 },
			{ 12, TEXT("二号位进攻"), TEXT("二号位"), FVector2D(140.f,  300.f), 350.f, 1.20f, false, 0 },
			{ 13, TEXT("背飞"),       TEXT("组合"),   FVector2D(150.f,  200.f), 240.f, 0.65f, false, 2 },
			{ 14, TEXT("自由轨迹"),   TEXT("自由"),   FVector2D(150.f,    0.f),   0.f, 0.80f, false, 0 }
		};
		return Plays;
	}

	FVector MirrorLocal(const FVector2D& Local, int32 TeamSide)
	{
		// TeamSide: +1 = Team A (own half X>0), -1 = Team B (own half X<0).
		const int32 S = (TeamSide >= 0) ? 1 : -1;
		return FVector(Local.X * S, Local.Y, 0.f);
	}
}
