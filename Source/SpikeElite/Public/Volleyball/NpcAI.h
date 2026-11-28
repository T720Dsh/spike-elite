// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "VolleyballEnums.h"

/**
 * M12c: pure-logic NPC decision core.
 *
 * Everything here is a plain function over plain values: no UObject, no World,
 * no ticking, no randomness. The GameMode coordinator calls these to decide
 * WHO reacts and WHERE they stand; the authoritative rules core
 * (SEVolleyballRules) still decides whether a touch is legal, and the GameMode
 * still owns ball physics and scoring. Automation tests exercise this file
 * directly (SpikeElite.Tests.Npc*).
 *
 * Court space: Team A defends +X, Team B defends -X. Own half spans
 * |X| in [30, 900] for play (950 clamp incl. free zone), |Y| <= 500.
 */
namespace SENpcAI
{
	constexpr float Gravity = 980.f;
	constexpr float HalfCourtX = 900.f;
	constexpr float HalfCourtY = 450.f;
	constexpr float MaxPlayX = 950.f;
	constexpr float MaxPlayY = 500.f;

	/** Ballistic time from Pos/Vel down to FloorZ (<=0 when it never falls there). */
	inline float TimeToFloor(const FVector& Pos, const FVector& Vel, float FloorZ = 0.f, float G = Gravity)
	{
		const float A = -0.5f * G;
		const float B = Vel.Z;
		const float C = Pos.Z - FloorZ;
		const float Disc = B * B - 4.f * A * C;
		if (Disc < 0.f) { return 0.f; }
		const float SqrtD = FMath::Sqrt(Disc);
		const float T1 = (-B - SqrtD) / (2.f * A);
		const float T2 = (-B + SqrtD) / (2.f * A);
		float T = 0.f;
		if (T1 > 0.f && T2 > 0.f) { T = FMath::Min(T1, T2); }
		else if (T1 > 0.f) { T = T1; }
		else if (T2 > 0.f) { T = T2; }
		return T;
	}

	/** Predicted floor contact point of a free flight (Z forced to FloorZ). */
	inline FVector PredictLanding(const FVector& Pos, const FVector& Vel, float FloorZ = 0.f, float G = Gravity)
	{
		const float T = TimeToFloor(Pos, Vel, FloorZ, G);
		if (T <= 0.f) { return FVector(Pos.X, Pos.Y, FloorZ); }
		FVector L = Pos + Vel * T + FVector(0.f, 0.f, -0.5f * G * T * T);
		L.Z = FloorZ;
		return L;
	}

	/** Keep a target inside the team's own half (never across the net, never out
	 *  of the free zone). TeamSide: +1 = Team A (+X), -1 = Team B (-X). */
	inline FVector ClampToOwnHalf(const FVector& P, int32 TeamSide, float LimitX = MaxPlayX, float LimitY = MaxPlayY)
	{
		FVector Out = P;
		Out.X = (TeamSide > 0) ? FMath::Clamp(Out.X, 30.f, LimitX) : FMath::Clamp(Out.X, -LimitX, -30.f);
		Out.Y = FMath::Clamp(Out.Y, -LimitY, LimitY);
		return Out;
	}

	/** True when the predicted landing is inside the team's own half of the court. */
	inline bool LandsOnOwnSide(const FVector& Landing, int32 TeamSide)
	{
		return (TeamSide > 0) ? (Landing.X > 0.f) : (Landing.X < 0.f);
	}

	/**
	 * Nearest candidate to the landing point. Positions are roster-ordered;
	 * entries flagged unavailable (bAvailable=false) and ExcludeIndex are
	 * skipped. Returns -1 when nobody can take it.
	 */
	inline int32 SelectNearest(const TArray<FVector>& Positions, const TArray<bool>& Available,
		const FVector& Target, int32 ExcludeIndex = -1)
	{
		int32 Best = -1;
		float BestDist = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			if (i == ExcludeIndex) { continue; }
			if (Available.IsValidIndex(i) && !Available[i]) { continue; }
			const float D = FVector::Dist2D(Positions[i], Target);
			if (D < BestDist) { BestDist = D; Best = i; }
		}
		return Best;
	}

	/** Second nearest (the cover/backup player) to the same target. */
	inline int32 SelectBackup(const TArray<FVector>& Positions, const TArray<bool>& Available,
		const FVector& Target, int32 PrimaryIndex)
	{
		return SelectNearest(Positions, Available, Target, PrimaryIndex);
	}

	/**
	 * A human-controlled primary handler may simply not react. A nearby NPC
	 * takes over the touch right only when the ball really is about to drop on
	 * our side and the human has not already touched it. Deliberately strict so
	 * NPCs never steal a ball the player is playing.
	 */
	inline bool ShouldBackupTakeOver(float BallZ, float BallSpeed2D, float BackupDist2D,
		bool bPrimaryIsHuman, bool bPrimaryStillArmed)
	{
		if (!bPrimaryIsHuman || !bPrimaryStillArmed) { return false; }
		if (BallZ > 260.f) { return false; }              // still high: leave it to the player
		if (BackupDist2D > 420.f) { return false; }       // too far to be a real help
		return BallZ < 200.f || BallSpeed2D > 700.f;      // falling or a fast dig
	}

	/** Phase of our own possession, used to pick the support formation. */
	enum class EPhase : uint8 { ServeReceive, Defend, Build, Attack };

	inline EPhase PhaseFromState(bool bPossessing, int32 TouchCount, bool bServeInFlight)
	{
		if (bServeInFlight) { return EPhase::ServeReceive; }
		if (!bPossessing) { return EPhase::Defend; }
		if (TouchCount >= 2) { return EPhase::Attack; }
		return EPhase::Build;
	}

	/** Front-row rotation slots are P2/P3/P4 (roster indices 1,2,3). */
	inline bool IsFrontRow(int32 SlotIndex) { return SlotIndex >= 1 && SlotIndex <= 3; }

	/**
	 * Support position for a player who is NOT handling the ball. This is what
	 * makes a full six react instead of standing on their home spot:
	 *  - ServeReceive: back row forms the W receive line, front row holds at
	 *    the attack line.
	 *  - Defend: front row steps to the net (block-ready), back row spreads
	 *    behind the 3 m line toward the opponent's hitting lane.
	 *  - Build: front row runs to the approach lanes, back row covers behind.
	 *  - Attack: everybody tightens around the attacker (spike coverage).
	 */
	inline FVector SupportPosition(int32 SlotIndex, int32 TeamSide, EPhase Phase,
		const FVector& Home, const FVector& BallLocation, const FVector& FocusPoint)
	{
		const float S = (TeamSide > 0) ? 1.f : -1.f;
		const bool bFront = IsFrontRow(SlotIndex);
		FVector P = Home;
		switch (Phase)
		{
		case EPhase::ServeReceive:
			P.X = bFront ? S * 330.f : FMath::Clamp(FMath::Abs(Home.X), 480.f, 780.f) * S;
			P.Y = FMath::Clamp(Home.Y * 1.05f, -380.f, 380.f);
			break;
		case EPhase::Defend:
			P.X = bFront ? S * 150.f : S * FMath::Clamp(FMath::Abs(Home.X), 420.f, 760.f);
			// Back row shades toward the side the ball is on (hitting lane).
			P.Y = bFront ? FMath::Clamp(BallLocation.Y * 0.55f + Home.Y * 0.45f, -330.f, 330.f)
			             : FMath::Clamp(Home.Y + BallLocation.Y * 0.25f, -430.f, 430.f);
			break;
		case EPhase::Build:
			P.X = bFront ? S * 210.f : S * FMath::Clamp(FMath::Abs(Home.X), 380.f, 700.f);
			P.Y = FMath::Clamp(Home.Y * 0.9f, -400.f, 400.f);
			break;
		case EPhase::Attack:
		default:
		{
			// Spike coverage: a tight arc 180-320 cm behind the attacker.
			const FVector Focus = FocusPoint.IsNearlyZero() ? Home : FocusPoint;
			const float Ring = bFront ? 190.f : 330.f;
			const float Lane = (SlotIndex % 2 == 0) ? 1.f : -1.f;
			P.X = Focus.X + S * Ring;
			P.Y = FMath::Clamp(Focus.Y + Lane * (bFront ? 170.f : 300.f), -420.f, 420.f);
			break;
		}
		}
		P.Z = Home.Z;
		return ClampToOwnHalf(P, TeamSide);
	}

	/**
	 * Which roster slot should set (second touch): nearest to the setter zone,
	 * never the player who just touched. Positions/Available are roster-ordered.
	 */
	inline int32 SelectSetter(const TArray<FVector>& Positions, const TArray<bool>& Available,
		int32 TeamSide, int32 LastToucherIndex)
	{
		const FVector Zone((TeamSide > 0) ? 250.f : -250.f, 0.f, 0.f);
		return SelectNearest(Positions, Available, Zone, LastToucherIndex);
	}

	/** Target the setter should deliver to for a given attacker slot. */
	inline FVector AttackTarget(int32 AttackerSlot, int32 TeamSide, float BallY)
	{
		const float S = (TeamSide > 0) ? 1.f : -1.f;
		const float X = IsFrontRow(AttackerSlot) ? S * 130.f : S * 380.f;
		float Y = 0.f;
		if (AttackerSlot == 3) { Y = -300.f * S; }       // P4 outside
		else if (AttackerSlot == 1) { Y = 300.f * S; }   // P2 opposite
		else { Y = FMath::Clamp(BallY, -220.f, 220.f); } // middle / back row
		return FVector(X, FMath::Clamp(Y, -350.f, 350.f), 230.f);
	}
}
