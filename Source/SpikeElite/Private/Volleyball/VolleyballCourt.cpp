// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballCourt.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

static UMaterialInstanceDynamic* MakeMID(UMaterialInterface* Base, UObject* Owner, const FLinearColor& Color)
{
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Owner);
	if (M) M->SetVectorParameterValue(TEXT("Color"), Color);
	return M;
}

AVolleyballCourt::AVolleyballCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(HalfCourtLength + FreeZone + 1500.f, HalfCourtWidth + FreeZone + 1500.f, 10.f));
	Root->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Root;

	LinesRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LinesRoot"));
	LinesRoot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Plane = PlaneFinder.Succeeded() ? PlaneFinder.Object : nullptr;
	UStaticMesh* Cyl = CylFinder.Succeeded() ? CylFinder.Object : nullptr;

	BuildFloor(Cube);
	BuildNet(Cube, Plane, Cyl);
	BuildStands(Cube);

	// ---- Lines (5cm wide, raised 1cm to avoid z-fighting, no collision) ----
	const float LW = 5.f;
	const float SX = LW / 100.f;          // 5cm along X
	const float SY = LW / 100.f;          // 5cm along Y
	const float FullLenX = (HalfCourtLength * 2.f) / 100.f;  // 18
	const float FullLenY = (HalfCourtWidth  * 2.f) / 100.f;  // 9
	// End lines
	MakeLine(TEXT("L_EndA"),  HalfCourtLength, 0, SX, FullLenY);
	MakeLine(TEXT("L_EndB"), -HalfCourtLength, 0, SX, FullLenY);
	// Side lines
	MakeLine(TEXT("L_SideA"), 0,  HalfCourtWidth, FullLenX, SY);
	MakeLine(TEXT("L_SideB"), 0, -HalfCourtWidth, FullLenX, SY);
	// Center
	MakeLine(TEXT("L_Center"), 0, 0, SX, FullLenY);
	// Attack lines
	MakeLine(TEXT("L_AtkA"),  AttackLineOffset, 0, SX, FullLenY);
	MakeLine(TEXT("L_AtkB"), -AttackLineOffset, 0, SX, FullLenY);
}

void AVolleyballCourt::BuildFloor(UStaticMesh* Cube)
{
	if (!Cube) return;

	// Arena outer floor (dark concrete), 60m x 40m.
	ArenaFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArenaFloor"));
	ArenaFloor->SetupAttachment(Root);
	ArenaFloor->SetStaticMesh(Cube);
	ArenaFloor->SetRelativeScale3D(FVector(30.f, 20.f, 0.1f));
	ArenaFloor->SetRelativeLocation(FVector(0,0,-6.f));
	if (auto* M = MakeMID(ArenaFloor->GetMaterial(0), this, FLinearColor(0.12f,0.12f,0.14f)))
		ArenaFloor->SetMaterial(0, M);

	// Free zone (dark sport floor), 24m x 15m (3m around court).
	FreeZoneFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FreeZone"));
	FreeZoneFloor->SetupAttachment(Root);
	FreeZoneFloor->SetStaticMesh(Cube);
	FreeZoneFloor->SetRelativeScale3D(FVector(
		(HalfCourtLength*2 + FreeZone*2)/100.f,
		(HalfCourtWidth*2  + FreeZone*2)/100.f, 0.1f));
	FreeZoneFloor->SetRelativeLocation(FVector(0,0,-5.5f));
	if (auto* M = MakeMID(FreeZoneFloor->GetMaterial(0), this, FLinearColor(0.35f,0.22f,0.13f)))
		FreeZoneFloor->SetMaterial(0, M);

	// Court wood (warm maple), 18m x 9m.
	CourtFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CourtFloor"));
	CourtFloor->SetupAttachment(Root);
	CourtFloor->SetStaticMesh(Cube);
	CourtFloor->SetRelativeScale3D(FVector(HalfCourtLength*2/100.f, HalfCourtWidth*2/100.f, 0.1f));
	CourtFloor->SetRelativeLocation(FVector(0,0,-5.f));
	if (auto* M = MakeMID(CourtFloor->GetMaterial(0), this, FLinearColor(0.78f,0.60f,0.40f)))
		CourtFloor->SetMaterial(0, M);
}

void AVolleyballCourt::BuildNet(UStaticMesh* Cube, UStaticMesh* Plane, UStaticMesh* Cyl)
{
	const float NetW = HalfCourtWidth*2 + NetOverhang*2;  // total net width
	const float NetBottom = NetHeight - NetBandHeight;    // 143
	const float NetCenter = (NetHeight + NetBottom) / 2.f; // 193

	// Cloth: vertical plane facing X, width NetW, height NetBandHeight.
	NetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetCloth"));
	NetMesh->SetupAttachment(Root);
	if (Plane)
	{
		NetMesh->SetStaticMesh(Plane);
		NetMesh->SetRelativeScale3D(FVector(NetW/100.f, NetBandHeight/100.f, 1.f));
		NetMesh->SetRelativeRotation(FRotator(0,90,0));
		NetMesh->SetRelativeLocation(FVector(0,0,NetCenter));
		// Translucent dark cloth.
		if (auto* M = MakeMID(NetMesh->GetMaterial(0), this, FLinearColor(0.05f,0.05f,0.06f)))
		{
			M->SetScalarParameterValue(TEXT("Opacity"), 0.55f);
			NetMesh->SetMaterial(0, M);
		}
	}
	NetMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	NetMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Top white band: 7cm tall.
	NetTopBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetTopBand"));
	NetTopBand->SetupAttachment(Root);
	if (Cube)
	{
		NetTopBand->SetStaticMesh(Cube);
		NetTopBand->SetRelativeScale3D(FVector(0.07f, NetW/100.f, 0.07f));
		NetTopBand->SetRelativeLocation(FVector(0,0,NetHeight - 3.5f));
		if (auto* M = MakeMID(NetTopBand->GetMaterial(0), this, FLinearColor::White))
			NetTopBand->SetMaterial(0, M);
	}
	NetTopBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Bottom white band: 5cm tall.
	NetBottomBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetBottomBand"));
	NetBottomBand->SetupAttachment(Root);
	if (Cube)
	{
		NetBottomBand->SetStaticMesh(Cube);
		NetBottomBand->SetRelativeScale3D(FVector(0.05f, NetW/100.f, 0.05f));
		NetBottomBand->SetRelativeLocation(FVector(0,0,NetBottom + 2.5f));
		if (auto* M = MakeMID(NetBottomBand->GetMaterial(0), this, FLinearColor::White))
			NetBottomBand->SetMaterial(0, M);
	}
	NetBottomBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Posts (cylinders, 2.6m tall, just outside the net overhang).
	auto MakePost = [&](const TCHAR* N, float Y)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(N);
		P->SetupAttachment(Root);
		if (Cyl)
		{
			P->SetStaticMesh(Cyl);
			P->SetRelativeScale3D(FVector(0.12f,0.12f,2.6f));
			P->SetRelativeLocation(FVector(0,Y,130.f));
			if (auto* M = MakeMID(P->GetMaterial(0), this, FLinearColor(0.7f,0.7f,0.75f)))
				P->SetMaterial(0, M);
		}
		return P;
	};
	PostLeft  = MakePost(TEXT("PostL"),  HalfCourtWidth + NetOverhang + 20.f);
	PostRight = MakePost(TEXT("PostR"), -(HalfCourtWidth + NetOverhang + 20.f));
}

void AVolleyballCourt::BuildStands(UStaticMesh* Cube)
{
	if (!Cube) return;

	StandSteps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StandSteps"));
	StandSteps->SetupAttachment(Root);
	StandSteps->SetStaticMesh(Cube);
	StandSteps->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (auto* M = MakeMID(StandSteps->GetMaterial(0), this, FLinearColor(0.22f,0.23f,0.27f)))
		StandSteps->SetMaterial(0, M);

	Crowd = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Crowd"));
	Crowd->SetupAttachment(Root);
	Crowd->SetStaticMesh(Cube);
	Crowd->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = MakeMID(Crowd->GetMaterial(0), this, FLinearColor(0.6f,0.5f,0.45f)))
		Crowd->SetMaterial(0, M);
}

void AVolleyballCourt::BeginPlay()
{
	Super::BeginPlay();
	PopulateStands();
}

void AVolleyballCourt::PopulateStands()
{
	if (!StandSteps || !Crowd) return;
	StandSteps->ClearInstances();
	Crowd->ClearInstances();

	const float SideY0 = HalfCourtWidth + FreeZone;
	const float EndX0  = HalfCourtLength + FreeZone;

	auto AddStep = [&](const FVector& Loc, const FVector& Scale)
	{
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, Loc, Scale));
	};

	for (int32 Row = 0; Row < StandRows; Row++)
	{
		float Y = SideY0 + Row * StandStepDepth;
		float Z = Row * StandStepHeight;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			AddStep(FVector(0, Side*Y, Z + 5.f), FVector(11.f, StandStepDepth/100.f, 0.1f));
			for (int32 C = -12; C <= 12; C++)
			{
				FVector Loc(C*80.f, Side*(Y - 20.f), Z + 45.f);
				Crowd->AddInstance(FTransform(FRotator::ZeroRotator, Loc, FVector(0.35f,0.35f,0.85f)));
			}
		}
	}
	for (int32 Row = 0; Row < StandRows; Row++)
	{
		float X = EndX0 + Row * StandStepDepth;
		float Z = Row * StandStepHeight;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			AddStep(FVector(Side*X, 0, Z + 5.f), FVector(StandStepDepth/100.f, 8.f, 0.1f));
			for (int32 C = -7; C <= 7; C++)
			{
				FVector Loc(Side*(X - 20.f), C*80.f, Z + 45.f);
				Crowd->AddInstance(FTransform(FRotator::ZeroRotator, Loc, FVector(0.35f,0.35f,0.85f)));
			}
		}
	}
}

UStaticMeshComponent* AVolleyballCourt::MakeLine(const TCHAR* Name, float X, float Y, float ScaleX, float ScaleY)
{
	UStaticMeshComponent* Line = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Line->SetupAttachment(LinesRoot);
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		Line->SetStaticMesh(CubeMesh);
		Line->SetRelativeScale3D(FVector(ScaleX, ScaleY, 0.02f));
		Line->SetRelativeLocation(FVector(X, Y, 1.0f));
		Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (auto* M = MakeMID(Line->GetMaterial(0), this, FLinearColor(0.95f,0.95f,0.95f)))
			Line->SetMaterial(0, M);
	}
	return Line;
}
