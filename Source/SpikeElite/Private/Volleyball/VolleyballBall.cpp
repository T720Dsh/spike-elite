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
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace
{
	/** M11c-6: tint basic geometry with the project-authored M_Tint material.
	 *  Engine BasicShapes use a default material without a writable Color
	 *  parameter, so creating a MID from that material leaves the ball grey. */
	void ApplyTint(UMeshComponent* Comp, const FLinearColor& Color)
	{
		SEMaterials::TintMesh(Comp, Comp, Color);
	}

	/**
	 * M11f-4: original multi-panel volleyball texture (no trademarks).
	 * Classic 3-panel look: yellow base, white panel seams (equator + two
	 * meridians + two diagonals), blue side blocks. Rendered straight into a
	 * transient UTexture2D (512x256) at runtime so the seam pattern rotates with
	 * the ball; replaces the old cylinder-band "equator ring" which did not form
	 * real panels and visually floated on the sphere.
	 */
	constexpr int32 kBallTexW = 512;
	constexpr int32 kBallTexH = 256;

	UTexture2D* BuildPanelTexture()
	{
		UTexture2D* Tex = UTexture2D::CreateTransient(kBallTexW, kBallTexH, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->SRGB = true;
		FTexture2DMipMap* Mip = &Tex->GetPlatformData()->Mips[0];
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		void* Raw = Mip->BulkData.Realloc(kBallTexW * kBallTexH * sizeof(FColor));
		FColor* Data = static_cast<FColor*>(Raw);
		if (!Data)
		{
			Mip->BulkData.Unlock();
			return nullptr;
		}

		const FColor Base   (242, 199,  84, 255); // volleyball yellow
		const FColor White  (246, 248, 250, 255); // seam white
		const FColor Blue   ( 40,  96, 180, 255); // panel blue
		const FColor Shade  (212, 168,  62, 255); // soft shading band (fake curvature)

		for (int32 y = 0; y < kBallTexH; ++y)
		{
			// V runs 0..1 top->bottom; the sphere UV maps V=0 to the north pole.
			const float V = static_cast<float>(y) / static_cast<float>(kBallTexH - 1);
			for (int32 x = 0; x < kBallTexW; ++x)
			{
				const float U = static_cast<float>(x) / static_cast<float>(kBallTexW - 1);
				FColor C = Base;

				// Equator seam.
				if (FMath::Abs(V - 0.5f) < 0.012f) { C = White; }
				// Two meridians.
				if (FMath::Abs(U - 0.25f) < 0.012f || FMath::Abs(U - 0.75f) < 0.012f) { C = White; }
				// Two diagonal panel seams (V-shaped classic volleyball seam).
				if (FMath::Abs(U + V - 0.72f) < 0.016f || FMath::Abs(U - V - 0.28f) < 0.016f
					|| FMath::Abs(U + V - 1.28f) < 0.016f || FMath::Abs(U - V + 0.28f) < 0.016f) { C = White; }

				// Blue side panels: upper-left and lower-right quadrants.
				if ((V < 0.42f && U > 0.58f) || (V > 0.58f && U < 0.42f))
				{
					C = Blue;
				}
				// Fake vertical shading (slight darkening near the poles) so the
				// ball reads as a sphere at distance, not a flat yellow disc.
				const float Polar = FMath::Clamp((V < 0.5f ? V : 1.f - V) * 3.0f, 0.f, 1.f);
				if (Polar < 0.85f)
				{
					C.R = static_cast<uint8>(FMath::Lerp(static_cast<float>(C.R), 0.f, (0.85f - Polar) * 0.35f));
					C.G = static_cast<uint8>(FMath::Lerp(static_cast<float>(C.G), 0.f, (0.85f - Polar) * 0.35f));
					C.B = static_cast<uint8>(FMath::Lerp(static_cast<float>(C.B), 0.f, (0.85f - Polar) * 0.35f));
				}

				Data[y * kBallTexW + x] = C;
			}
		}
		Mip->BulkData.Unlock();
		Tex->UpdateResource();
		return Tex;
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
	// ~21 cm in diameter, so the sphere itself is scaled 0.21. The multi-panel
	// texture material is applied in BeginPlay (runtime texture creation needs a
	// live world); until then the plain yellow tint is the constructor fallback.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.21f));
		ApplyTint(Mesh, FLinearColor(0.85f, 0.62f, 0.10f));
	}

	// M11f-4: the equator cylinder band is retired — the panel seams now come
	// from the UV texture, so these old rings stay hidden (kept as components
	// for ABI safety, invisible; removed from gameplay).
	BandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BandMesh"));
	BandMesh->SetupAttachment(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		BandMesh->SetStaticMesh(CylMesh.Object);
		BandMesh->SetRelativeScale3D(FVector(0.212f / 0.21f, 0.212f / 0.21f, 0.030f / 0.21f));
		ApplyTint(BandMesh, FLinearColor(0.10f, 0.22f, 0.55f));
	}
	BandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BandMesh->SetCastShadow(false);
	BandMesh->SetVisibility(false);   // M11f-4: replaced by texture panels

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
	BandMesh2->SetVisibility(false);   // M11f-4: replaced by texture panels

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

	// M11f-4: original multi-panel surface applied at BeginPlay — runtime
	// texture (yellow base, white panel seams, blue side panels) on the
	// project-authored M_TintBall material. No Mikasa/FIVB/Olympic logos; the
	// pattern rotates with the ball. LicensedBallMesh/Material slots still apply
	// when a legally licensed V200W set is provided (checked below).
	UMaterialInterface* BallBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_TintBall.M_TintBall"));
	if (BallBase)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BallBase, this);
		if (MID)
		{
			MID->SetTextureParameterValue(TEXT("BallTexture"), BuildPanelTexture());
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			Mesh->SetMaterial(0, MID);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("M_TintBall missing; ball keeps plain yellow tint"));
	}

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
