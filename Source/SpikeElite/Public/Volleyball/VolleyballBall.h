// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolleyballBall.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * The volleyball.
 *
 * M0:
 *  - Static mesh sphere driven by ProjectileMovementComponent
 *  - Exposes a simple "Hit(Position, Direction, Power, Spin)" helper so
 *    pawns / AI can serve, set, spike.
 *
 * M1 TODO:
 *  - Replace ProjectileMovement with a custom integrator that models:
 *      * Magnus lift from spin (floater vs top-spin serves)
 *      * Air drag (F = -c * v|v|)
 *      * Net-tap / cord interaction
 *      * Bounce restitution tuned to FIVB ball (0.75-0.8)
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UProjectileMovementComponent> Projectile;

	/** Current spin state (rad/s). TODO: integrate into custom physics in M1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Ball|Physics")
	float CurrentSpin = 0.0f;

	/** Physics hit callback: detect floor bounces and notify the rules system. */
	UFUNCTION()
	void OnBallHit(class UPrimitiveComponent* HitComp, AActor* OtherActor, class UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
