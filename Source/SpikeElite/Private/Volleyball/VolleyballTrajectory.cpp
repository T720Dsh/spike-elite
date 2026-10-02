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

SEVolleyballTrajectory::FShotSolution SEVolleyballTrajectory::BuildShotSolution(
	const FVector& Start, const FShotIntent& Intent, float TimingError)
{
	FShotSolution S;
	const float FlightTime = FMath::Clamp(Intent.DesiredFlightTime, 0.3f, 3.0f);

	// M11f-1: TWO consistent schemes, chosen by ApexHeight, both shared by the
	// dotted preview and the GameMode's final strike (no UI/exec split):
	//  - ApexHeight <= 0 : Target + DesiredFlightTime solve the base velocity
	//    (the historical contract — power scales it, timing rotates it).
	//  - ApexHeight > 0  : the apex height ABOVE the contact point really shapes
	//    the trajectory. Rise time = sqrt(2H/g); the fall time = apex->target.
	//    The set-play table's arc values are therefore NOT display-only: they
	//    are part of the solved initial velocity. Target stays the landing spot,
	//    DesiredFlightTime remains the UI reference and the resulting flight
	//    time is reported back. Power scales the whole vector exactly like the
	//    flight-time scheme, so power still re-shapes the dotted line instantly.
	FVector Vel;
	if (Intent.ApexHeight > 0.5f)
	{
		const float H = FMath::Max(Intent.ApexHeight, 10.f);
		const float RiseTime = FMath::Sqrt(2.f * H / BallGravity);
		const float FallH = Start.Z + H - Intent.TargetLocation.Z;
		if (FallH >= 0.f)
		{
			const float FallTime = FMath::Sqrt(2.f * FallH / BallGravity);
			const float TotalT = RiseTime + FallTime;
			if (TotalT > 0.001f)
			{
				const FVector Delta = Intent.TargetLocation - Start;
				Vel.X = Delta.X / TotalT;
				Vel.Y = Delta.Y / TotalT;
				Vel.Z = BallGravity * RiseTime;
			}
		}
		if (Vel.IsNearlyZero())
		{
			// Target above the requested apex (or degenerate input): fall back to
			// the flight-time solve so the plan stays valid and predictable.
			Vel = SolveVelocity(Start, Intent.TargetLocation, FlightTime);
		}
	}
	else
	{
		// Target + flight time solve the base velocity; Power scales it so the
		// dotted preview and PredictedLanding react to power changes immediately.
		Vel = SolveVelocity(Start, Intent.TargetLocation, FlightTime);
	}
	FVector VelScaled = Vel * FMath::Clamp(Intent.Power, 0.1f, 1.0f);

	// Timing error is applied INSIDE the shared solver: early = negative,
	// late = positive, symmetric around 0, and Perfect = exactly 0.
	const float Err = FMath::Clamp(TimingError, -1.f, 1.f);
	if (FMath::Abs(Err) > 0.0001f)
	{
		FRotator Rot = VelScaled.Rotation();
		Rot.Yaw += Err * 14.f;   // early/late -> lateral bias
		Rot.Pitch -= Err * 6.f;  // early/late -> flatter/lofted
		VelScaled = Rot.Vector() * VelScaled.Size();
	}

	S.InitialVelocity = VelScaled;
	S.Trajectory = Predict(Start, VelScaled);
	S.Landing = S.Trajectory.Landing;
	S.FlightTime = S.Trajectory.FlightTime;
	S.Apex = S.Trajectory.Apex;
	// M11f-1: a downward strike's apex is at (or below) the contact point — the
	// reported arc-above-contact must never go negative for a legal shot.
	S.ApexAboveContact = FMath::Max(S.Trajectory.Apex.Z - Start.Z, 0.f);
	S.bCrossedNet = S.Trajectory.bCrossedNet;
	S.bInBounds = S.Trajectory.bInBounds;
	S.bValid = S.Trajectory.bValid;
	return S;
}

