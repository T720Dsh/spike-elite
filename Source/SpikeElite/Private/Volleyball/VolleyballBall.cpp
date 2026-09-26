// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballBall.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AVolleyballBall::AVolleyballBall()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetLinearDamping(0.1f);
	Mesh->SetAngularDamping(0.5f);
	// FIVB ball: circumference 65-67 cm -> radius ~10.5 cm.
	// The visual mesh and collision shape are assigned in the Blueprint
	// child class (BP_VolleyballBall) once we have a ball static mesh.

	Projectile = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile"));
	Projectile->SetUpdatedComponent(Mesh);
	Projectile->InitialSpeed = 0.0f;
	Projectile->MaxSpeed = 3000.0f;
	Projectile->bRotationFollowsVelocity = false;
	Projectile->bShouldBounce = true;
	Projectile->Bounciness = 0.78f;           // FIVB ball bounce on wood floor
	Projectile->ProjectileGravityScale = 1.0f;
}

void AVolleyballBall::BeginPlay()
{
	Super::BeginPlay();
}

void AVolleyballBall::Strike(const FVector& Direction, float Power, float SpinRadS)
{
	CurrentSpin = SpinRadS;
	const FVector Dir = Direction.GetSafeNormal();
	Projectile->Velocity = Dir * Power;
	// Spin will apply Magnus force in M1; for now we just record it.
}

void AVolleyballBall::ResetBall(const FVector& Location)
{
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	if (Projectile)
	{
		Projectile->Velocity = FVector::ZeroVector;
	}
	CurrentSpin = 0.0f;
}
