// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpikeEliteGameMode.h"
#include "VolleyballBall.generated.h"

class UStaticMeshComponent;
class USceneComponent;
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
	/** M11c-6: unscaled root — the sphere and the band are SIBLINGS under it so
	 *  the band never inherits the sphere's 0.21 scale and disappears into it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Yellow-blue center band (no brand; engine basic cylinder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> BandMesh;

	/** M11d-6: white meridian stripe — original un-branded multi-panel look
	 *  (yellow sphere, blue equator, white pole-to-pole stripe). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> BandMesh2;

	/**
	 * Licensed V200W ball asset slots (M11b-6). Empty until the user provides a
	 * legally licensed Mikasa V200W model + textures with ASSET_LICENSE.md next
	 * to it. Until then the game shows the un-branded yellow/blue placeholder;
	 * the UI labels it 比赛用球, never claiming an official license.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ball|Licensed")
	TObjectPtr<UStaticMesh> LicensedBallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ball|Licensed")
	TObjectPtr<UMaterialInterface> LicensedBallMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
	TObjectPtr<UProjectileMovementComponent> Projectile;

	/** Current spin state (rad/s). Reserved for Magnus integration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Ball|Physics")
	float CurrentSpin = 0.0f;

	/** Last team that touched the ball; None before the serve. */
	EVolleyballTeam LastHitTeam = EVolleyballTeam::None;
	bool bLandingReported = false;

	/**
	 * ProjectileMovement bounce callback. Because the mesh is kinematic (no
	 * Chaos simulation), the mesh's OnComponentHit is not reliably broadcast;
	 * the projectile component owns collision, so landing is detected here.
	 */
	UFUNCTION()
	void HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);
};
