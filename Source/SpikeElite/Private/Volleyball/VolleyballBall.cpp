// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballBall.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "SpikeEliteGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Volleyball/VolleyballRules.h"
#include "SEMaterials.h"

namespace
{
	/** M11c-6: tint basic geometry with the project-authored M_Tint material.
	 *  Engine BasicShapes use a default material without a writable Color
	 *  parameter, so creating a MID from that material leaves the ball grey. */
	void ApplyTint(UMeshComponent* Comp, const FLinearColor& Color)
	{
		SEMaterials::TintMesh(Comp, Comp, Color);
	}
}

AVolleyballBall::AVolleyballBall()
{
	PrimaryActorTick.bCanEverTick = true;

	// M11c-6: the sphere mesh is the actor root (ProjectileMovement MUST sweep a
	// root component — a scaled child mesh as the updated component has no
	// reliable collision and the ball would fall through the floor). The
	// yellow-blue band is a child with a COMPENSATED scale (divided by the
	// parent 0.21) so its WORLD scale stays 0.212 x 0.03: the band no longer
	// inherits the sphere's 0.21 and vanishes into it, which is the visual goal
	// the unscaled-root design was after. The unscaled "Root" scene node still
	// exists as a decorative attach point hanging off the mesh.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetupAttachment(Mesh);

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
	// ~21 cm in diameter, so the sphere itself is scaled 0.21.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.21f));
		// M11b-6 / M11c-6: un-branded yellow placeholder (UI label: 比赛用球).
		// No Mikasa/FIVB/Olympic logos. Swap in LicensedBallMesh/Material when the
		// user supplies a legally licensed V200W asset set (see ASSET_LICENSE.md).
		// Tinted with the project's own M_Tint (parameter introspection, no fake
		// "Color" on engine defaults).
		ApplyTint(Mesh, FLinearColor(0.85f, 0.62f, 0.10f));
	}

	// Yellow-blue center band: a thin cylinder around the equator reads as the
	// classic volleyball panel stripe without any trademarked art.
	BandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BandMesh"));
	BandMesh->SetupAttachment(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		BandMesh->SetStaticMesh(CylMesh.Object);
		// Compensated scale: parent sphere is 0.21, so the band needs
		// 0.212/0.21 (radius) x 0.030/0.21 (height) to read 0.212 x 0.03 in
		// world space and hug the equator without sinking in.
		BandMesh->SetRelativeScale3D(FVector(0.212f / 0.21f, 0.212f / 0.21f, 0.030f / 0.21f));
		ApplyTint(BandMesh, FLinearColor(0.10f, 0.22f, 0.55f));
	}
	BandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BandMesh->SetCastShadow(false);

	// M11d-6: white meridian stripe (pole-to-pole) crosses the blue equator into
	// a simple four-panel look. Same compensated scale / tint approach as above;
	// no trademarked art anywhere.
	BandMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BandMesh2"));
	BandMesh2->SetupAttachment(Mesh);
	if (CylMesh.Succeeded())
	{
		BandMesh2->SetStaticMesh(CylMesh.Object);
		BandMesh2->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		BandMesh2->SetRelativeScale3D(FVector(0.213f / 0.21f, 0.213f / 0.21f, 0.012f / 0.21f));
		ApplyTint(BandMesh2, FLinearColor(0.92f, 0.94f, 0.96f));
	}
	BandMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BandMesh2->SetCastShadow(false);

	Projectile = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile"));
	Projectile->SetUpdatedComponent(Mesh);
	Projectile->InitialSpeed = 0.0f;
	Projectile->MaxSpeed = 4000.0f;
	// M11c-6: let the ball tumble with its velocity so the yellow/blue pattern
	// is visibly rotating (rotation is purely visual; motion stays the single
	// ProjectileMovement authority).
	Projectile->bRotationFollowsVelocity = true;   // tumble so the yellow/blue pattern visibly rotates
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

	// M11c-6: the licensed slots are applied ONLY when the user supplied a fully
	// licensed V200W mesh AND material (see ASSET_LICENSE.md). Otherwise the
	// un-branded yellow/blue placeholder stays — no Missing Package, no fake
	// official claims. Missing slots are silently normal.
	if (SEVolleyballRules::ShouldUseLicensedBall(LicensedBallMesh != nullptr, LicensedBallMaterial != nullptr))
	{
		Mesh->SetStaticMesh(LicensedBallMesh);
		Mesh->SetRelativeScale3D(FVector(1.f));
		Mesh->SetMaterial(0, LicensedBallMaterial);
		if (BandMesh) { BandMesh->SetVisibility(false); }
		if (BandMesh2) { BandMesh2->SetVisibility(false); }
	}
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
