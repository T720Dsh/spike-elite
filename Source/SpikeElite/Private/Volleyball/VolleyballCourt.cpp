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
#include "SEMaterials.h"

static UMaterialInstanceDynamic* CourtMakeMID(UObject* Owner, const FLinearColor& Color)
{
	return SEMaterials::MakeTint(Owner, Color);
}

AVolleyballCourt::AVolleyballCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(GetFreeZoneHalfLength(), GetFreeZoneHalfWidth(), 10.f));
	Root->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Root;

	LinesRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LinesRoot"));
	LinesRoot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cyl = CylFinder.Succeeded() ? CylFinder.Object : nullptr;

	BuildFloor(Cube);
	BuildNet(Cube, Cyl);
	BuildServeShortLines(Cube);

	// ---- Lines (5cm wide). Thin (1cm) and sunk 0.05cm into the floor so they
	// read as paint rather than a raised ridge. Collision disabled so they
	// never affect ball or players. ----
	const float LW = 5.f;
	const float SX = LW / 100.f;
	const float SY = LW / 100.f;
	const float FullLenX = (HalfCourtLength * 2.f) / 100.f;  // 18
	const float FullLenY = (HalfCourtWidth  * 2.f) / 100.f;  // 9
	MakeLine(TEXT("L_EndA"),  HalfCourtLength, 0, SX, FullLenY);
	MakeLine(TEXT("L_EndB"), -HalfCourtLength, 0, SX, FullLenY);
	MakeLine(TEXT("L_SideA"), 0,  HalfCourtWidth, FullLenX, SY);
	MakeLine(TEXT("L_SideB"), 0, -HalfCourtWidth, FullLenX, SY);
	MakeLine(TEXT("L_Center"), 0, 0, SX, FullLenY);
	MakeLine(TEXT("L_AtkA"),  AttackLineOffset, 0, SX, FullLenY);
	MakeLine(TEXT("L_AtkB"), -AttackLineOffset, 0, SX, FullLenY);
}

void AVolleyballCourt::ApplyFloorMaterial(UStaticMeshComponent* Comp, const FLinearColor& Fallback)
{
	if (!Comp) return;
	static UMaterialInterface* WoodMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_WoodFloor.M_WoodFloor"));
	static UMaterialInterface* SportMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SportFloor.M_SportFloor"));
	UMaterialInterface* Chosen = nullptr;
	if (Comp == CourtFloor && WoodMat)       Chosen = WoodMat;
	else if (Comp == FreeZoneFloor && SportMat) Chosen = SportMat;
	if (Chosen)
	{
		Comp->SetMaterial(0, Chosen);
	}
	else if (UMaterialInstanceDynamic* M = CourtMakeMID(Comp, Fallback))
	{
		Comp->SetMaterial(0, M);
	}
}

void AVolleyballCourt::BuildFloor(UStaticMesh* Cube)
{
	if (!Cube) return;

	// Free zone (distinct sport surround colour), 31 x 19 m: 5 m past the
	// sidelines, 6.5 m past the end lines. No stands/boards/objects inside.
	FreeZoneFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FreeZone"));
	FreeZoneFloor->SetupAttachment(Root);
	FreeZoneFloor->SetStaticMesh(Cube);
	FreeZoneFloor->SetRelativeScale3D(FVector(
		GetFreeZoneHalfLength()*2/100.f,
		GetFreeZoneHalfWidth()*2/100.f, 0.1f));
	FreeZoneFloor->SetRelativeLocation(FVector(0, 0, -5.5f));
	ApplyFloorMaterial(FreeZoneFloor, FLinearColor(0.16f, 0.35f, 0.42f)); // blue sport surround

	// Court wood (warm maple), 18m x 9m, top surface at Z = 0.
	CourtFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CourtFloor"));
	CourtFloor->SetupAttachment(Root);
	CourtFloor->SetStaticMesh(Cube);
	CourtFloor->SetRelativeScale3D(FVector(HalfCourtLength*2/100.f, HalfCourtWidth*2/100.f, 0.1f));
	CourtFloor->SetRelativeLocation(FVector(0, 0, -5.f));
	ApplyFloorMaterial(CourtFloor, FLinearColor(0.78f, 0.60f, 0.40f));
}

void AVolleyballCourt::BuildNet(UStaticMesh* Cube, UStaticMesh* Cyl)
{
	const float NetW = HalfCourtWidth*2 + NetOverhang*2;  // total net width 1060
	const float NetBottom = NetHeight - NetBandHeight;    // 143
	const float NetCenter = (NetHeight + NetBottom) / 2.f; // 193

	NetGrid = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("NetGrid"));
	NetGrid->SetupAttachment(Root);
	if (Cube) NetGrid->SetStaticMesh(Cube);
	NetGrid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// M11e-4: finer, darker net strands so the net reads as mesh, not a thick
	// white fence hiding the far side.
	if (auto* M = CourtMakeMID(NetGrid, FLinearColor(0.38f, 0.40f, 0.44f)))
		NetGrid->SetMaterial(0, M);

	NetTopBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetTopBand"));
	NetTopBand->SetupAttachment(Root);
	if (Cube)
	{
		NetTopBand->SetStaticMesh(Cube);
		NetTopBand->SetRelativeScale3D(FVector(0.07f, NetW/100.f, 0.07f));
		NetTopBand->SetRelativeLocation(FVector(0, 0, NetHeight - 3.5f));
		if (auto* M = CourtMakeMID(NetTopBand, FLinearColor(0.95f, 0.95f, 0.95f)))
			NetTopBand->SetMaterial(0, M);
	}
	NetTopBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	NetBottomBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetBottomBand"));
	NetBottomBand->SetupAttachment(Root);
	if (Cube)
	{
		NetBottomBand->SetStaticMesh(Cube);
		NetBottomBand->SetRelativeScale3D(FVector(0.05f, NetW/100.f, 0.05f));
		NetBottomBand->SetRelativeLocation(FVector(0, 0, NetBottom + 2.5f));
		if (auto* M = CourtMakeMID(NetBottomBand, FLinearColor(0.95f, 0.95f, 0.95f)))
			NetBottomBand->SetMaterial(0, M);
	}
	NetBottomBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	auto MakePost = [&](const TCHAR* N, float Y)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(N);
		P->SetupAttachment(Root);
		if (Cyl)
		{
			P->SetStaticMesh(Cyl);
			P->SetRelativeScale3D(FVector(0.12f, 0.12f, 2.6f));
			P->SetRelativeLocation(FVector(0, Y, 130.f));
			if (auto* M = CourtMakeMID(P, FLinearColor(0.75f, 0.75f, 0.8f)))
				P->SetMaterial(0, M);
		}
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return P;
	};
	PostLeft  = MakePost(TEXT("PostL"),  HalfCourtWidth + NetOverhang + 20.f);
	PostRight = MakePost(TEXT("PostR"), -(HalfCourtWidth + NetOverhang + 20.f));
}

void AVolleyballCourt::BuildServeShortLines(UStaticMesh* Cube)
{
	// FIVB short service lines: two 15 cm long, 5 cm wide marks, 20 cm behind
	// each end line, at the inner edge of each sideline (perpendicular to the
	// end line, i.e. running along X).
	const float SX = 5.f / 100.f;
	const float SY = 5.f / 100.f;
	const float Offset = 20.f;
	const float Len = 15.f;
	MakeLine(TEXT("L_SSA_P"),  HalfCourtLength + Offset + Len/2.f,  HalfCourtWidth - 7.5f, Len/100.f, SX);
	MakeLine(TEXT("L_SSA_N"),  HalfCourtLength + Offset + Len/2.f, -HalfCourtWidth + 7.5f, Len/100.f, SX);
	MakeLine(TEXT("L_SSB_P"), -HalfCourtLength - Offset - Len/2.f,  HalfCourtWidth - 7.5f, Len/100.f, SX);
	MakeLine(TEXT("L_SSB_N"), -HalfCourtLength - Offset - Len/2.f, -HalfCourtWidth + 7.5f, Len/100.f, SX);
}

void AVolleyballCourt::BeginPlay()
{
	Super::BeginPlay();
	PopulateNetGrid();
}

void AVolleyballCourt::PopulateNetGrid()
{
	if (!NetGrid) return;
	NetGrid->ClearInstances();

	const float NetW = HalfCourtWidth*2 + NetOverhang*2;
	const float HalfNetW = NetW / 2.f;
	const float NetBottom = NetHeight - NetBandHeight;
	const float Spacing = 8.f;

	const int32 HCount = FMath::Max(2, FMath::RoundToInt(NetBandHeight / Spacing));
	for (int32 i = 0; i <= HCount; ++i)
	{
		const float Z = NetBottom + (NetBandHeight * i / HCount);
		FTransform T(FRotator::ZeroRotator, FVector(0, 0, Z), FVector(0.009f, NetW/100.f, 0.009f));
		NetGrid->AddInstance(T);
	}
	for (float Y = -HalfNetW; Y <= HalfNetW + 0.1f; Y += Spacing)
	{
		FTransform T(FRotator::ZeroRotator, FVector(0, Y, (NetBottom+NetHeight)/2.f),
			FVector(0.012f, 0.012f, NetBandHeight/100.f));
		NetGrid->AddInstance(T);
	}
}

UStaticMeshComponent* AVolleyballCourt::MakeLine(const TCHAR* Name, float X, float Y, float ScaleX, float ScaleY)
{
	UStaticMeshComponent* Line = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Line->SetupAttachment(LinesRoot);
	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		Line->SetStaticMesh(CubeMesh);
		Line->SetRelativeScale3D(FVector(ScaleX, ScaleY, 0.01f));
		Line->SetRelativeLocation(FVector(X, Y, 0.45f));
		Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (auto* M = CourtMakeMID(Line, FLinearColor(0.95f, 0.95f, 0.95f)))
			Line->SetMaterial(0, M);
	}
	return Line;
}