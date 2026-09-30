// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "VolleyballEnums.h"
#include "VolleyballRules.h"
#include "VolleyballTrajectory.generated.h"

/**
 * Data-driven shot intent produced by the tactical planning UI and validated
 * by the GameMode before execution. All values are in the serving team's local
 * court space (mirrored by the GameMode per side).
 */
USTRUCT(BlueprintType)
struct FShotIntent
{
	GENERATED_BODY()

	/** World-space target point (the predicted landing spot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TargetLocation = FVector::ZeroVector;

	/** Desired total flight time (seconds). Controls arc height indirectly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.3", ClampMax = "3.0"))
	float DesiredFlightTime = 0.8f;

	/** Explicit apex height above the contact point (cm). 0 = derive from flight time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "600.0"))
	float ApexHeight = 0.f;

	/** Power scale 0..1 (maps to the actual strike impulse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float Power = 1.f;

	/** Touch type this intent replaces (Receive/Set/Attack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBallTouchType TouchType = EBallTouchType::Attack;

	/** Spin in rad/s (visual only; the projectile model is drag-free). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpinRadS = 0.f;

	/** Timing error from the player (0 = perfect, +-1 = early/late). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float TimingError = 0.f;

	/** Predicted landing point (filled by the preview, verified by the GameMode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PredictedLanding = FVector::ZeroVector;

	/** Predicted flight time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PredictedFlightTime = 0.f;

	/** True if the prediction crossed the net legally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPredictedCrossedNet = false;

	/** True if the prediction ended in a net touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPredictedNetTouch = false;

	/** True if the predicted landing is in bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPredictedInBounds = false;
};

/**
 * Pure deterministic trajectory math shared by the tactical preview and the
 * GameMode validation. Uses the same gravity/step/collision parameters as the
 * live ball so the dotted preview matches the real flight.
 */
namespace SEVolleyballTrajectory
{
	/** Ground level of the court floor (cm, actor Z). */
	constexpr float GroundZ = 0.f;
	/** Ball radius used for net/ground intersection tests (cm). */
	constexpr float BallRadius = 10.5f;
	/** Projectile gravity used by the live ball (cm/s^2). */
	constexpr float BallGravity = 980.f;
	/** Court half-length / half-width (match lines, in-bounds). */
	constexpr float CourtHalfLength = 900.f;
	constexpr float CourtHalfWidth = 450.f;
	/** Net plane X, top/bottom (cm). */
	constexpr float NetX = 0.f;
	constexpr float NetTop = 243.f;
	constexpr float NetBottom = 143.f;
	/** Free-zone limits (out of court, still playable area). */
	constexpr float FreeHalfLength = 1550.f;
	constexpr float FreeHalfWidth = 950.f;

	/** One sampled point of a predicted flight. */
	struct FTrajectoryPoint
	{
		FVector Location = FVector::ZeroVector;
		float Time = 0.f;
	};

	/** Full prediction result. */
	struct FTrajectoryResult
	{
		bool bValid = false;
		FVector Landing = FVector::ZeroVector;
		float FlightTime = 0.f;
		bool bCrossedNet = false;
		bool bNetTouch = false;
		bool bInBounds = false;
		FVector Apex = FVector::ZeroVector;
		float ApexTime = 0.f;
		TArray<FTrajectoryPoint> Points;
	};

	/**
	 * Integrate a ballistic flight from Start with InitialVelocity using the
	 * same gravity and collision parameters as the live ball. Detects the net
	 * plane, the ground and the free-zone limits.
	 */
	SPIKEELITE_API FTrajectoryResult Predict(const FVector& Start, const FVector& InitialVelocity,
		float Gravity = BallGravity, float Step = 0.02f, int32 MaxSteps = 250);

	/** Solve the initial velocity needed to reach Target in FlightTime. */
	SPIKEELITE_API FVector SolveVelocity(const FVector& Start, const FVector& Target, float FlightTime, float Gravity = BallGravity);
}
