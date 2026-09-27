// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballCourt.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVolleyballCourt::AVolleyballCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(HalfCourtLength, HalfCourtWidth, 10.0f));
	Root->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Root;

	LinesRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LinesRoot"));
	LinesRoot->SetupAttachment(Root);

	// ---- Floor: 18m x 9m box, warm wood-tinted material ----
	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	FloorMesh->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FloorMesh->SetStaticMesh(CubeMesh.Object);
		FloorMesh->SetRelativeScale3D(FVector(HalfCourtLength * 2.0f / 100.0f, HalfCourtWidth * 2.0f / 100.0f, 0.1f));
		FloorMesh->SetRelativeLocation(FVector(0, 0, -5.0f));
	}
	// Warm maple court wood color (rgb ~ 200,160,110). Real wood_floor jpg sits
	// in Content/Textures/ and will be wired into a proper PBR material once the
	// editor imports it; for now a flat tint keeps the court readable.
	if (UMaterialInterface* BaseMat = FloorMesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.78f, 0.60f, 0.40f));
			FloorMesh->SetMaterial(0, MID);
		}
	}

	// ---- Net: thin semi-transparent cloth plane ----
	NetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Net"));
	NetMesh->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		// Plane is 100x100 cm, rotate so it spans the 9m width and stands vertical.
		NetMesh->SetStaticMesh(PlaneMesh.Object);
		NetMesh->SetRelativeScale3D(FVector(HalfCourtWidth * 2.0f / 100.0f, NetHeight / 100.0f, 1.0f));
		NetMesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		NetMesh->SetRelativeLocation(FVector(0, 0, NetHeight / 2.0f));
		NetMesh->SetCollisionProfileName(TEXT("BlockAll"));
	}
	// Fade the net to ~30% opacity so it reads as translucent cloth.
	if (UMaterialInterface* NetBase = NetMesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* NetMID = UMaterialInstanceDynamic::Create(NetBase, this))
		{
			NetMID->SetScalarParameterValue(TEXT("Opacity"), 0.35f);
			NetMesh->SetMaterial(0, NetMID);
		}
	}

	// ---- Posts ----
	auto MakePost = [&](const TCHAR* Name, float Y)
	{
		UStaticMeshComponent* Post = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Post->SetupAttachment(Root);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylMesh.Succeeded())
		{
			Post->SetStaticMesh(CylMesh.Object);
			Post->SetRelativeScale3D(FVector(0.15f, 0.15f, (NetHeight + 20.0f) / 100.0f));
			Post->SetRelativeLocation(FVector(0, Y, (NetHeight + 20.0f) / 2.0f));
		}
		return Post;
	};
	PostLeft = MakePost(TEXT("PostLeft"), HalfCourtWidth + 20.0f);
	PostRight = MakePost(TEXT("PostRight"), -HalfCourtWidth - 20.0f);

	// ---- Court lines (white, 5cm wide strips lying flat on the floor) ----
	// FIVB: lines are 5cm wide, counted as part of the court.
	const float LineW = 5.0f;
	// End lines (at the two 9m edges, along X)
	MakeLine(TEXT("Line_EndNear"),  HalfCourtLength, 0, LineW / 100.0f, (HalfCourtWidth * 2.0f) / 100.0f);
	MakeLine(TEXT("Line_EndFar"),  -HalfCourtLength, 0, LineW / 100.0f, (HalfCourtWidth * 2.0f) / 100.0f);
	// Sideline lines (along X at the two Y edges)
	MakeLine(TEXT("Line_SideLeft"), 0,  HalfCourtWidth, (HalfCourtLength * 2.0f) / 100.0f, LineW / 100.0f);
	MakeLine(TEXT("Line_SideRight"), 0, -HalfCourtWidth, (HalfCourtLength * 2.0f) / 100.0f, LineW / 100.0f);
	// Center line (under the net)
	MakeLine(TEXT("Line_Center"), 0, 0, LineW / 100.0f, (HalfCourtWidth * 2.0f) / 100.0f);
	// Attack lines (3m from net on each side, parallel to net)
	MakeLine(TEXT("Line_AttackNear"),  AttackLineOffset, 0, LineW / 100.0f, (HalfCourtWidth * 2.0f) / 100.0f);
	MakeLine(TEXT("Line_AttackFar"),  -AttackLineOffset, 0, LineW / 100.0f, (HalfCourtWidth * 2.0f) / 100.0f);
}

UStaticMeshComponent* AVolleyballCourt::MakeLine(const TCHAR* Name, float X, float Y, float ScaleX, float ScaleY)
{
	UStaticMeshComponent* Line = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Line->SetupAttachment(LinesRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Line->SetStaticMesh(CubeMesh.Object);
		Line->SetRelativeScale3D(FVector(ScaleX, ScaleY, 0.01f));
		Line->SetRelativeLocation(FVector(X, Y, 0.5f));  // 5mm above floor
		Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// White material
		if (UMaterialInterface* Base = Line->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
				Line->SetMaterial(0, MID);
			}
		}
	}
	return Line;
}

void AVolleyballCourt::BeginPlay()
{
	Super::BeginPlay();
	BuildCourt();
}

void AVolleyballCourt::BuildCourt()
{
	// Geometry is already built in constructor. Hook for M1 polish.
}
