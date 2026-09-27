// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballBall.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "SpikeEliteGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

AVolleyballBall::AVolleyballBall()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetLinearDamping(0.1f);
	Mesh->SetAngularDamping(0.5f);
	Mesh->OnComponentHit.AddDynamic(this, &AVolleyballBall::OnBallHit);

	// Visual: sphere mesh, orange volleyball color.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		// Sphere is 100cm diameter; FIVB ball is ~20cm radius (40cm diameter).
		Mesh->SetWorldScale3D(FVector(0.4f, 0.4f, 0.4f));
		if (UMaterialInterface* Base = Mesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.45f, 0.1f));  // orange
			Mesh->SetMaterial(0, MID);
		}
	}
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

void AVolleyballBall::OnBallHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// FIVB rally ends when the ball contacts the floor. We treat any hit with
	// a strongly-upward normal (floor bounce) as a floor contact.
	if (Hit.Normal.Z > 0.7f)
	{
		if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->OnBallLanded(Hit.ImpactPoint);
		}
	}
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
