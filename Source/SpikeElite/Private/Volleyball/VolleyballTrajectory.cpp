// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballTrajectory.h"

SEVolleyballTrajectory::FTrajectoryResult SEVolleyballTrajectory::Predict(
	const FVector& Start, const FVector& InitialVelocity, float Gravity, float Step, int32 MaxSteps)
{
	FTrajectoryResult Result;
	Result.Points.Reserve(MaxSteps);

	FVector Pos = Start;
	FVector Vel = InitialVelocity;
	Result.Points.Add({ Pos, 0.f });

	bool bPrevCrossedNet = false;

	for (int32 i = 1; i <= MaxSteps; ++i)
	{
		const float T = static_cast<float>(i) * Step;
		Vel.Z -= Gravity * Step;
		Pos += Vel * Step;

		// Net plane crossing: track whether X crossed 0 while within net width.
		const bool bNearNetWidth = FMath::Abs(Pos.Y) <= CourtHalfWidth + 60.f;
		const bool bNowCrossed = (Pos.X > NetX) != (Start.X > NetX);
		if (bNearNetWidth && bNowCrossed)
		{
			// Interpolate the crossing point.
			const float PrevX = Pos.X - Vel.X * Step;
			const float Frac = (NetX - PrevX) / (Pos.X - PrevX);
			const float CrossZ = Pos.Z - Vel.Z * Step + Vel.Z * Step * Frac;
			// A crossing is only legal above the net tape: any part of the ball
			// below the net top is a net touch (FIVB: the ball must pass entirely
			// above the net).
			if (CrossZ - BallRadius < NetTop)
			{
				Result.bNetTouch = true;
				Result.Landing = FVector(NetX, Pos.Y, CrossZ);
				Result.FlightTime = T;
				Result.bValid = true;
				break;
			}
			Result.bCrossedNet = true;
		}

		// Ground contact (ball centre reaches the floor, Z=0).
		if (Pos.Z <= 0.f)
		{
			Result.Landing = FVector(Pos.X, Pos.Y, 0.f);
			Result.FlightTime = T;
			Result.bInBounds = (FMath::Abs(Pos.X) <= CourtHalfLength) && (FMath::Abs(Pos.Y) <= CourtHalfWidth);
			Result.bValid = true;
			break;
		}

		// Out of the free zone horizontally (fully out — stop early).
		if (FMath::Abs(Pos.X) > FreeHalfLength || FMath::Abs(Pos.Y) > FreeHalfWidth)
		{
			Result.Landing = FVector(Pos.X, Pos.Y, Pos.Z);
			Result.FlightTime = T;
			Result.bInBounds = false;
			Result.bValid = true;
			break;
		}

		Result.Points.Add({ Pos, T });
		if (i == MaxSteps)
		{
			Result.Landing = Pos;
			Result.FlightTime = T;
			Result.bValid = false;  // fell off the integrator horizon
		}
	}

	// Apex.
	for (int32 i = 1; i < Result.Points.Num(); ++i)
	{
		if (Result.Points[i].Location.Z > Result.Points[i - 1].Location.Z)
		{
			if (Result.Points[i].Location.Z >= Result.Apex.Z)
			{
				Result.Apex = Result.Points[i].Location;
				Result.ApexTime = Result.Points[i].Time;
			}
		}
	}

	return Result;
}

FVector SEVolleyballTrajectory::SolveVelocity(const FVector& Start, const FVector& Target, float FlightTime, float Gravity)
{
	if (FlightTime <= 0.f) { return FVector::ZeroVector; }
	const FVector Delta = Target - Start;
	FVector V;
	V.X = Delta.X / FlightTime;
	V.Y = Delta.Y / FlightTime;
	V.Z = (Delta.Z + 0.5f * Gravity * FlightTime * FlightTime) / FlightTime;
	return V;
}

