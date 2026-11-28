// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * M12c: pure camera presentation math — framing assist and impact shake.
 *
 * No UObject/World access: the character applies the returned offsets to its
 * spring arm. Player look input always stays authoritative; the assist is a
 * bounded nudge, never a takeover.
 */
namespace SECameraFraming
{
	/**
	 * Bounded rotation nudge that pulls the view toward the ball.
	 * @param ViewRotation  current camera rotation (control rotation).
	 * @param CameraLocation camera world position.
	 * @param BallLocation  ball world position.
	 * @param MaxPitch/MaxYaw absolute degree limits of the assist.
	 * @param Weight 0..1 share of the angular error that is corrected.
	 */
	inline FRotator FramingAssist(const FRotator& ViewRotation, const FVector& CameraLocation,
		const FVector& BallLocation, float MaxPitch = 10.f, float MaxYaw = 8.f, float Weight = 0.35f)
	{
		const FVector ToBall = BallLocation - CameraLocation;
		if (ToBall.SizeSquared() < 1.f) { return FRotator::ZeroRotator; }
		const FRotator Desired = ToBall.Rotation();
		const FRotator Delta = (Desired - ViewRotation).GetNormalized();
		const float W = FMath::Clamp(Weight, 0.f, 1.f);
		return FRotator(
			FMath::Clamp(Delta.Pitch * W, -MaxPitch, MaxPitch),
			FMath::Clamp(Delta.Yaw * W, -MaxYaw, MaxYaw),
			0.f);
	}

	/** Arm length that opens up when the ball is far, so both stay in frame. */
	inline float FramingArmLength(float BaseLength, float BallDistance,
		float MaxExtra = 150.f, float FarDistance = 1400.f)
	{
		const float Ratio = FMath::Clamp(BallDistance / FMath::Max(FarDistance, 1.f), 0.f, 1.f);
		return BaseLength + MaxExtra * Ratio;
	}

	/** Shake envelope: 1 at impact, 0 at (and after) Duration, never negative. */
	inline float ShakeEnvelope(float Elapsed, float Duration)
	{
		if (Duration <= 0.f || Elapsed <= 0.f) { return Elapsed <= 0.f && Duration > 0.f ? 1.f : 0.f; }
		if (Elapsed >= Duration) { return 0.f; }
		const float T = 1.f - Elapsed / Duration;
		return T * T;
	}

	/**
	 * Deterministic impact shake offset (degrees). Strength 1.0 is the spike
	 * impact; smaller values are used for lighter contacts.
	 */
	inline FRotator ShakeOffset(float Elapsed, float Duration, float Strength)
	{
		const float Env = ShakeEnvelope(Elapsed, Duration) * FMath::Max(Strength, 0.f);
		if (Env <= 0.f) { return FRotator::ZeroRotator; }
		const float W = Elapsed * 46.f;
		return FRotator(FMath::Sin(W) * 1.5f * Env, FMath::Sin(W * 0.73f + 1.1f) * 1.1f * Env,
			FMath::Sin(W * 1.27f) * 1.8f * Env);
	}
}
