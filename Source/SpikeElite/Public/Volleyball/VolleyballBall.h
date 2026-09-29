// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpikeEliteGameMode.h"
#include "VolleyballBall.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * The volleyball.
 *
 * Single-authority motion: UProjectileMovementComponent drives translation,
 * gravity and floor bounces. The static mesh does NOT simulate Chaos physics,
 * so the two systems can never fight each other.
 *
 *  - FIVB ball circumference 65-67 cm -> diameter ~21 cm.
 *  - LastHitTeam tracks the last team that touched the ball for IN/OUT calls.
 */
UCLASS()
class SPIKEELITE_API AVolleyballBall : public AActor
{
	GENERATED_BODY()

public:
	AVolleyballBall();

	virtual void BeginPlay() override;

	/**
	 * Strike the ball.
	 * @param Direction unit direction in world space (will be normalized).
	 * @param Power    initial speed in cm/s.
	 * @param SpinRadS back-spin / top-spin in rad/s (positive = top-spin).
	 */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Ball")
	void Strike(const FVector& Direction, float Power, float SpinRadS = 0.0f);

	/** Reset ball to a posed location with zero velocity. */
	UFUNCTION(BlueprintCallable, Category = "Volleyball|Ball")
	void ResetBall(const FVector& Location);

	/** Record which team last touched the ball (for in/out scoring). */
	void SetLastHitTeam(EVolleyballTeam Team) { LastHitTeam = Team; }
	EVolleyballTeam GetLastHitTeam() const { return LastHitTeam; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UProjectileMovementComponent> Projectile;

	/** Current spin state (rad/s). Reserved for Magnus integration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Ball|Physics")
	float CurrentSpin = 0.0f;

	/** Last team that touched the ball; None before the serve. */
	EVolleyballTeam LastHitTeam = EVolleyballTeam::None;

	/** Physics hit callback: detect floor contacts and notify the rules system. */
	UFUNCTION()
	void OnBallHit(class UPrimitiveComponent* HitComp, AActor* OtherActor, class UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/**
	 * ProjectileMovement bounce callback. Because the mesh is kinematic (no
	 * Chaos simulation), the mesh's OnComponentHit is not reliably broadcast;
	 * the projectile component owns collision, so landing is detected here.
	 */
	UFUNCTION()
	void HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);
};
