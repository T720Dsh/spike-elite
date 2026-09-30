// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballBall.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "SpikeEliteGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AVolleyballBall::AVolleyballBall()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// Single-authority motion: the ProjectileMovementComponent moves the ball.
	// Chaos simulation stays OFF so it cannot fight the projectile integrator.
	Mesh->SetSimulatePhysics(false);
	Mesh->SetEnableGravity(false);          // gravity is applied by the projectile
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	// Players "hit" the ball via gameplay detection; do not let the capsule
	// physically kick the ball around.
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Mesh->SetNotifyRigidBodyCollision(false);

	// Visual: engine sphere. Base cube/sphere is 100 cm across; an FIVB ball is
	// ~21 cm in diameter, so scale 0.21.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.21f));
		// M11b-6: un-branded yellow/blue placeholder (UI label: 比赛用球).
		// No Mikasa/FIVB/Olympic logos. Swap in LicensedBallMesh/Material when the
		// user supplies a legally licensed V200W asset set (see ASSET_LICENSE.md).
		if (UMaterialInterface* Base = Mesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.62f, 0.10f));
			Mesh->SetMaterial(0, MID);
		}
	}

	// Yellow-blue center band: a thin cylinder around the equator reads as the
	// classic volleyball panel stripe without any trademarked art.
	BandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BandMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		BandMesh->SetStaticMesh(CylMesh.Object);
		BandMesh->SetRelativeScale3D(FVector(0.212f, 0.212f, 0.030f));
		if (UMaterialInterface* BBase = BandMesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* BMID = UMaterialInstanceDynamic::Create(BBase, this);
			BMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.22f, 0.55f));
			BandMesh->SetMaterial(0, BMID);
		}
	}
	BandMesh->SetupAttachment(Mesh);
	BandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BandMesh->SetCastShadow(false);

	Projectile = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile"));
	Projectile->SetUpdatedComponent(Mesh);
	Projectile->InitialSpeed = 0.0f;
	Projectile->MaxSpeed = 4000.0f;
	Projectile->bRotationFollowsVelocity = false;
	Projectile->bShouldBounce = true;
	Projectile->Bounciness = 0.78f;          // FIVB ball bounce on wood floor
	Projectile->Friction = 0.2f;
	Projectile->ProjectileGravityScale = 1.0f;

	// Kinematic projectile: the movement component (not Chaos) owns impact, so
	// bind its bounce delegate for reliable floor detection.
	Projectile->OnProjectileBounce.AddDynamic(this, &AVolleyballBall::HandleProjectileBounce);
}

void AVolleyballBall::BeginPlay()
{
	Super::BeginPlay();
}

void AVolleyballBall::HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// Same floor test as OnBallHit but driven by the projectile component, which
	// is the authority for this kinematic ball. Up-facing normal = floor or the
	// top of a stand step (the latter counts as landing out).
	if (!bLandingReported && ImpactResult.Normal.Z > 0.7f)
	{
		bLandingReported = true;
		if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->OnBallLanded(ImpactResult.ImpactPoint);
		}
	}
}

void AVolleyballBall::Strike(const FVector& Direction, float Power, float SpinRadS)
{
	CurrentSpin = SpinRadS;
	const FVector Dir = Direction.GetSafeNormal();
	if (Projectile)
	{
		Projectile->Activate(true);
		Projectile->Velocity = Dir * Power;
		Projectile->UpdateComponentVelocity();
	}
}

void AVolleyballBall::ResetBall(const FVector& Location)
{
	SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
	if (Projectile)
	{
		Projectile->StopMovementImmediately();
		Projectile->Velocity = FVector::ZeroVector;
		Projectile->Deactivate();
	}
	CurrentSpin = 0.0f;
	bLandingReported = false;
}
