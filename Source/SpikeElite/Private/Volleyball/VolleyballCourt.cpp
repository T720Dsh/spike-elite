// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballCourt.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "SEMaterials.h"

// All structural/crowd/net geometry is tinted through the project M_Tint
// material (guaranteed "Color" parameter). The Base argument is ignored; it is
// kept in the signature so call sites read naturally with their fallback colour.
static UMaterialInstanceDynamic* MakeMID(UMaterialInterface* /*Base*/, UObject* Owner, const FLinearColor& Color)
{
	return SEMaterials::MakeTint(Owner, Color);
}

AVolleyballCourt::AVolleyballCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(HallHalfLength, HallHalfWidth, 10.f));
	Root->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Root;

	LinesRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LinesRoot"));
	LinesRoot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cyl = CylFinder.Succeeded() ? CylFinder.Object : nullptr;

	BuildFloor(Cube);
	BuildHall(Cube);
	BuildNet(Cube, Cyl);
	BuildLighting();
	BuildStands(Cube, nullptr);

	// ---- Lines (5cm wide). Thin (1cm) and sunk 0.05cm into the floor so they
	// read as paint rather than a raised ridge, with a small offset to avoid
	// z-fighting. Collision disabled so they never affect ball or players. ----
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
	// Use the imported wood material if the editor asset-import step has run;
	// otherwise fall back to a flat tint (never pretend a .jpg loaded at runtime).
	static UMaterialInterface* WoodMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_WoodFloor.M_WoodFloor"));
	static UMaterialInterface* SportMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SportFloor.M_SportFloor"));
	UMaterialInterface* Chosen = nullptr;
	if (Comp == CourtFloor && WoodMat)       Chosen = WoodMat;
	else if (Comp == FreeZoneFloor && SportMat) Chosen = SportMat;
	if (Chosen)
	{
		Comp->SetMaterial(0, Chosen);
	}
	else if (UMaterialInstanceDynamic* M = MakeMID(Comp->GetMaterial(0), this, Fallback))
	{
		Comp->SetMaterial(0, M);
	}
}

void AVolleyballCourt::BuildFloor(UStaticMesh* Cube)
{
	if (!Cube) return;

	// Outer concourse (dark), 44m x 32m.
	ArenaFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArenaFloor"));
	ArenaFloor->SetupAttachment(Root);
	ArenaFloor->SetStaticMesh(Cube);
	ArenaFloor->SetRelativeScale3D(FVector(HallHalfLength*2/100.f, HallHalfWidth*2/100.f, 0.1f));
	ArenaFloor->SetRelativeLocation(FVector(0,0,-6.f));
	if (auto* M = MakeMID(ArenaFloor->GetMaterial(0), this, FLinearColor(0.15f,0.15f,0.18f)))
		ArenaFloor->SetMaterial(0, M);

	// Free zone (dark sport surround), 24m x 15m (3m around the court).
	FreeZoneFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FreeZone"));
	FreeZoneFloor->SetupAttachment(Root);
	FreeZoneFloor->SetStaticMesh(Cube);
	FreeZoneFloor->SetRelativeScale3D(FVector(
		(HalfCourtLength*2 + FreeZone*2)/100.f,
		(HalfCourtWidth*2  + FreeZone*2)/100.f, 0.1f));
	FreeZoneFloor->SetRelativeLocation(FVector(0,0,-5.5f));
	ApplyFloorMaterial(FreeZoneFloor, FLinearColor(0.28f,0.18f,0.12f));

	// Court wood (warm maple), 18m x 9m, top surface at Z = 0.
	CourtFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CourtFloor"));
	CourtFloor->SetupAttachment(Root);
	CourtFloor->SetStaticMesh(Cube);
	CourtFloor->SetRelativeScale3D(FVector(HalfCourtLength*2/100.f, HalfCourtWidth*2/100.f, 0.1f));
	CourtFloor->SetRelativeLocation(FVector(0,0,-5.f));
	ApplyFloorMaterial(CourtFloor, FLinearColor(0.78f,0.60f,0.40f));
}

void AVolleyballCourt::BuildHall(UStaticMesh* Cube)
{
	if (!Cube) return;
	const FLinearColor WallCol(0.13f, 0.14f, 0.17f);

	auto MakeWall = [&](const TCHAR* N, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(N);
		W->SetupAttachment(Root);
		W->SetStaticMesh(Cube);
		W->SetRelativeScale3D(Scale);
		W->SetRelativeLocation(Loc);
		W->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (auto* M = MakeMID(W->GetMaterial(0), this, WallCol)) W->SetMaterial(0, M);
		return W;
	};

	const float W = 20.f; // wall thickness cm /100
	// End walls span Y, at X = +/-HallHalfLength.
	WallEndA = MakeWall(TEXT("WallEndA"), FVector( HallHalfLength, 0, HallHeight/2), FVector(W/100.f, HallHalfWidth*2/100.f, HallHeight/100.f));
	WallEndB = MakeWall(TEXT("WallEndB"), FVector(-HallHalfLength, 0, HallHeight/2), FVector(W/100.f, HallHalfWidth*2/100.f, HallHeight/100.f));
	// Side walls span X, at Y = +/-HallHalfWidth.
	WallSideA = MakeWall(TEXT("WallSideA"), FVector(0,  HallHalfWidth, HallHeight/2), FVector(HallHalfLength*2/100.f, W/100.f, HallHeight/100.f));
	WallSideB = MakeWall(TEXT("WallSideB"), FVector(0, -HallHalfWidth, HallHeight/2), FVector(HallHalfLength*2/100.f, W/100.f, HallHeight/100.f));

	// Roof so no sky/sun/snow is ever visible.
	Roof = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Roof"));
	Roof->SetupAttachment(Root);
	Roof->SetStaticMesh(Cube);
	Roof->SetRelativeScale3D(FVector(HallHalfLength*2/100.f, HallHalfWidth*2/100.f, 0.2f));
	Roof->SetRelativeLocation(FVector(0,0,HallHeight));
	Roof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = MakeMID(Roof->GetMaterial(0), this, FLinearColor(0.10f,0.10f,0.12f))) Roof->SetMaterial(0, M);
}

void AVolleyballCourt::BuildNet(UStaticMesh* Cube, UStaticMesh* Cyl)
{
	const float NetW = HalfCourtWidth*2 + NetOverhang*2;  // total net width 1060
	const float NetBottom = NetHeight - NetBandHeight;    // 143
	const float NetCenter = (NetHeight + NetBottom) / 2.f; // 193

	// ---- Mesh grid as a single instanced component (visual only; the GameMode
	// is the sole authority for net collision). Populated in BeginPlay. ----
	NetGrid = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("NetGrid"));
	NetGrid->SetupAttachment(Root);
	if (Cube) NetGrid->SetStaticMesh(Cube);
	NetGrid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = MakeMID(NetGrid->GetMaterial(0), this, FLinearColor(0.68f,0.69f,0.72f)))
		NetGrid->SetMaterial(0, M);

	// Top white band: 7cm tall, opaque.
	NetTopBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetTopBand"));
	NetTopBand->SetupAttachment(Root);
	if (Cube)
	{
		NetTopBand->SetStaticMesh(Cube);
		NetTopBand->SetRelativeScale3D(FVector(0.07f, NetW/100.f, 0.07f));
		NetTopBand->SetRelativeLocation(FVector(0,0,NetHeight - 3.5f));
		if (auto* M = MakeMID(NetTopBand->GetMaterial(0), this, FLinearColor(0.95f,0.95f,0.95f)))
			NetTopBand->SetMaterial(0, M);
	}
	NetTopBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Bottom white band: 5cm tall, opaque.
	NetBottomBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NetBottomBand"));
	NetBottomBand->SetupAttachment(Root);
	if (Cube)
	{
		NetBottomBand->SetStaticMesh(Cube);
		NetBottomBand->SetRelativeScale3D(FVector(0.05f, NetW/100.f, 0.05f));
		NetBottomBand->SetRelativeLocation(FVector(0,0,NetBottom + 2.5f));
		if (auto* M = MakeMID(NetBottomBand->GetMaterial(0), this, FLinearColor(0.95f,0.95f,0.95f)))
			NetBottomBand->SetMaterial(0, M);
	}
	NetBottomBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Posts just outside the net overhang.
	auto MakePost = [&](const TCHAR* N, float Y)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(N);
		P->SetupAttachment(Root);
		if (Cyl)
		{
			P->SetStaticMesh(Cyl);
			P->SetRelativeScale3D(FVector(0.12f,0.12f,2.6f));
			P->SetRelativeLocation(FVector(0,Y,130.f));
			if (auto* M = MakeMID(P->GetMaterial(0), this, FLinearColor(0.75f,0.75f,0.8f)))
				P->SetMaterial(0, M);
		}
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return P;
	};
	PostLeft  = MakePost(TEXT("PostL"),  HalfCourtWidth + NetOverhang + 20.f);
	PostRight = MakePost(TEXT("PostR"), -(HalfCourtWidth + NetOverhang + 20.f));
}

void AVolleyballCourt::BuildLighting()
{
	auto MakeSpot = [this](const TCHAR* N, const FVector& Loc, float Intensity, float OuterCone)
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(N);
		L->SetupAttachment(Root);
		L->SetWorldLocation(Loc);
		L->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // straight down
		L->Intensity = Intensity;
		L->OuterConeAngle = OuterCone;
		L->InnerConeAngle = OuterCone * 0.5f;
		L->AttenuationRadius = 2600.f;
		L->SetLightColor(FLinearColor(1.f, 0.97f, 0.9f));
		return L;
	};
	// Two court fixtures + a wide fill. Tuned so the wood reads clearly without
	// blowing out to white where the cones overlap at centre court.
	CourtLightA = MakeSpot(TEXT("CourtLightA"), FVector( 500.f, 0.f, 950.f), 17000.f, 46.f);
	CourtLightB = MakeSpot(TEXT("CourtLightB"), FVector(-500.f, 0.f, 950.f), 17000.f, 46.f);
	FillLight = MakeSpot(TEXT("FillLight"), FVector(0.f, 0.f, 1050.f), 4500.f, 90.f);

	// A low ambient sky light lifts shadowed surfaces (stands/walls) so the hall
	// is never pure black even though it is fully enclosed with no outdoor sky.
	USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("HallSkyLight"));
	Sky->SetupAttachment(Root);
	Sky->Intensity = 0.9f;
	Sky->bLowerHemisphereIsBlack = false;

	// Dim warm corner point lights graze the stepped stands and roof structure.
	auto MakeCorner = [this](const TCHAR* N, const FVector& Loc)
	{
		UPointLightComponent* P = CreateDefaultSubobject<UPointLightComponent>(N);
		P->SetupAttachment(Root);
		P->SetWorldLocation(Loc);
		P->Intensity = 9500.f;
		P->AttenuationRadius = 1900.f;
		P->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
		PerimeterLights.Add(P);
	};
	MakeCorner(TEXT("Perim_PP"), FVector( 1500.f,  1050.f, 650.f));
	MakeCorner(TEXT("Perim_PN"), FVector( 1500.f, -1050.f, 650.f));
	MakeCorner(TEXT("Perim_NP"), FVector(-1500.f,  1050.f, 650.f));
	MakeCorner(TEXT("Perim_NN"), FVector(-1500.f, -1050.f, 650.f));
}

void AVolleyballCourt::BuildStands(UStaticMesh* Cube, UStaticMesh* Sphere)
{
	if (!Cube) return;

	StandSteps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StandSteps"));
	StandSteps->SetupAttachment(Root);
	StandSteps->SetStaticMesh(Cube);
	StandSteps->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (auto* M = MakeMID(StandSteps->GetMaterial(0), this, FLinearColor(0.30f,0.31f,0.36f)))
		StandSteps->SetMaterial(0, M);

	// One body ISM per clothing colour so the crowd has colour variation while
	// staying instanced (a handful of draw calls, no per-spectator actors).
	static const FLinearColor Clothing[] = {
		FLinearColor(0.75f,0.20f,0.18f), FLinearColor(0.15f,0.45f,0.85f),
		FLinearColor(0.90f,0.75f,0.15f), FLinearColor(0.20f,0.65f,0.30f),
		FLinearColor(0.85f,0.45f,0.15f), FLinearColor(0.55f,0.30f,0.70f)
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Clothing); ++i)
	{
		UInstancedStaticMeshComponent* B = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("CrowdBody_%d"), i));
		B->SetupAttachment(Root);
		B->SetStaticMesh(Cube);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (auto* M = SEMaterials::MakeCrowd(this, Clothing[i])) B->SetMaterial(0, M);
		CrowdBodies.Add(B);
	}

	// Heads share one skin-toned sphere ISM. Sphere is loaded in BeginPlay if the
	// constructor finder wasn't passed; here we rely on a runtime load.
	CrowdHeads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CrowdHeads"));
	CrowdHeads->SetupAttachment(Root);
	if (UStaticMesh* S = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		CrowdHeads->SetStaticMesh(S);
	CrowdHeads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = MakeMID(CrowdHeads->GetMaterial(0), this, FLinearColor(0.72f,0.55f,0.42f)))
		CrowdHeads->SetMaterial(0, M);
}

void AVolleyballCourt::BeginPlay()
{
	Super::BeginPlay();
	PopulateNetGrid();
	PopulateStands();
}

void AVolleyballCourt::PopulateNetGrid()
{
	if (!NetGrid) return;
	NetGrid->ClearInstances();

	const float NetW = HalfCourtWidth*2 + NetOverhang*2; // 1060
	const float HalfNetW = NetW / 2.f;
	const float NetBottom = NetHeight - NetBandHeight;    // 143
	const float Spacing = 12.f;

	// Horizontal cords (run along Y).
	const int32 HCount = FMath::Max(2, FMath::RoundToInt(NetBandHeight / Spacing));
	for (int32 i = 0; i <= HCount; ++i)
	{
		const float Z = NetBottom + (NetBandHeight * i / HCount);
		FTransform T(FRotator::ZeroRotator, FVector(0, 0, Z), FVector(0.012f, NetW/100.f, 0.012f));
		NetGrid->AddInstance(T);
	}
	// Vertical cords (run along Z), spaced along Y.
	for (float Y = -HalfNetW; Y <= HalfNetW + 0.1f; Y += Spacing)
	{
		FTransform T(FRotator::ZeroRotator, FVector(0, Y, (NetBottom+NetHeight)/2.f),
			FVector(0.012f, 0.012f, NetBandHeight/100.f));
		NetGrid->AddInstance(T);
	}
}

void AVolleyballCourt::PopulateStands()
{
	if (!StandSteps || !CrowdHeads || CrowdBodies.Num() == 0) return;
	StandSteps->ClearInstances();
	CrowdHeads->ClearInstances();
	for (auto& B : CrowdBodies) if (B) B->ClearInstances();

	const float SideY0 = HalfCourtWidth + FreeZone;   // 750
	const float EndX0  = HalfCourtLength + FreeZone;  // 1200
	const int32 BodyKinds = CrowdBodies.Num();

	auto AddSpectator = [&](const FVector& BaseLoc)
	{
		// BaseLoc is the standing surface. Torso (seated/standing stub) + head.
		const int32 Kind = FMath::RandRange(0, BodyKinds - 1);
		const float Sway = FMath::FRandRange(-3.f, 3.f);
		FVector BodyLoc(BaseLoc.X + Sway, BaseLoc.Y + Sway, BaseLoc.Z + 28.f);
		FVector HeadLoc(BaseLoc.X + Sway, BaseLoc.Y + Sway, BaseLoc.Z + 64.f);
		CrowdBodies[Kind]->AddInstance(FTransform(FRotator::ZeroRotator, BodyLoc, FVector(0.28f,0.20f,0.52f)));
		CrowdHeads->AddInstance(FTransform(FRotator::ZeroRotator, HeadLoc, FVector(0.16f,0.16f,0.16f)));
	};

	// Side stands (along X), with a central aisle gap of 2 columns.
	for (int32 Row = 0; Row < StandRows; ++Row)
	{
		const float Y = SideY0 + Row * StandStepDepth;
		const float Z = Row * StandStepHeight;
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, Y, Z + 5.f), FVector(HallHalfLength*2/100.f, StandStepDepth/100.f, 0.1f)));
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0,-Y, Z + 5.f), FVector(HallHalfLength*2/100.f, StandStepDepth/100.f, 0.1f)));
		for (int32 C = -12; C <= 12; ++C)
		{
			if (FMath::Abs(C) <= 1) continue; // central aisle / entrance
			const float X = C * 80.f;
			AddSpectator(FVector(X,  Y - 25.f, Z + 10.f));
			AddSpectator(FVector(X, -Y + 25.f, Z + 10.f));
		}
	}
	// End stands (along Y), with a central aisle.
	for (int32 Row = 0; Row < StandRows; ++Row)
	{
		const float X = EndX0 + Row * StandStepDepth;
		const float Z = Row * StandStepHeight;
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector( X, 0, Z + 5.f), FVector(StandStepDepth/100.f, HallHalfWidth*2/100.f, 0.1f)));
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-X, 0, Z + 5.f), FVector(StandStepDepth/100.f, HallHalfWidth*2/100.f, 0.1f)));
		for (int32 C = -8; C <= 8; ++C)
		{
			if (FMath::Abs(C) <= 1) continue;
			const float Y = C * 80.f;
			AddSpectator(FVector( X - 25.f, Y, Z + 10.f));
			AddSpectator(FVector(-X + 25.f, Y, Z + 10.f));
		}
	}
}

UStaticMeshComponent* AVolleyballCourt::MakeLine(const TCHAR* Name, float X, float Y, float ScaleX, float ScaleY)
{
	UStaticMeshComponent* Line = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Line->SetupAttachment(LinesRoot);
	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		Line->SetStaticMesh(CubeMesh);
		Line->SetRelativeScale3D(FVector(ScaleX, ScaleY, 0.01f));   // 1cm thin
		Line->SetRelativeLocation(FVector(X, Y, 0.45f));            // flush with floor
		Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (auto* M = MakeMID(Line->GetMaterial(0), this, FLinearColor(0.95f,0.95f,0.95f)))
			Line->SetMaterial(0, M);
	}
	return Line;
}
