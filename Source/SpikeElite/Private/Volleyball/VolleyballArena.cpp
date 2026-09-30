// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballArena.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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

static UMaterialInstanceDynamic* ArenaMakeMID(UObject* Owner, const FLinearColor& Color)
{
	return SEMaterials::MakeTint(Owner, Color);
}

AVolleyballArena::AVolleyballArena()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(HallHalfLength, HallHalfWidth, 10.f));
	Root->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	BuildHall(Cube);
	BuildLighting();
	BuildStands(Cube);
}

void AVolleyballArena::BuildHall(UStaticMesh* Cube)
{
	if (!Cube) return;
	const FLinearColor WallCol(0.10f, 0.11f, 0.14f);

	auto MakeWall = [&](const TCHAR* N, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(N);
		W->SetupAttachment(Root);
		W->SetStaticMesh(Cube);
		W->SetRelativeScale3D(Scale);
		W->SetRelativeLocation(Loc);
		W->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (auto* M = ArenaMakeMID(W, WallCol)) W->SetMaterial(0, M);
		return W;
	};

	const float W = 30.f; // wall thickness cm
	WallEndA = MakeWall(TEXT("WallEndA"), FVector( HallHalfLength, 0, HallHeight/2), FVector(W/100.f, HallHalfWidth*2/100.f, HallHeight/100.f));
	WallEndB = MakeWall(TEXT("WallEndB"), FVector(-HallHalfLength, 0, HallHeight/2), FVector(W/100.f, HallHalfWidth*2/100.f, HallHeight/100.f));
	WallSideA = MakeWall(TEXT("WallSideA"), FVector(0,  HallHalfWidth, HallHeight/2), FVector(HallHalfLength*2/100.f, W/100.f, HallHeight/100.f));
	WallSideB = MakeWall(TEXT("WallSideB"), FVector(0, -HallHalfWidth, HallHeight/2), FVector(HallHalfLength*2/100.f, W/100.f, HallHeight/100.f));

	// Deep dark far wall colour is already the wall tint; add a large dark
	// backdrop plane slightly inside the end walls so the hall reads as a big
	// volume rather than a small box.
	UStaticMeshComponent* BackdropA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropA"));
	BackdropA->SetupAttachment(Root);
	BackdropA->SetStaticMesh(Cube);
	BackdropA->SetRelativeScale3D(FVector(20.f/100.f, HallHalfWidth*2/100.f, HallHeight*0.75f/100.f));
	BackdropA->SetRelativeLocation(FVector(HallHalfLength - 220.f, 0, HallHeight*0.35f));
	BackdropA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(BackdropA, FLinearColor(0.045f, 0.05f, 0.065f))) BackdropA->SetMaterial(0, M);
	UStaticMeshComponent* BackdropB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropB"));
	BackdropB->SetupAttachment(Root);
	BackdropB->SetStaticMesh(Cube);
	BackdropB->SetRelativeScale3D(FVector(20.f/100.f, HallHalfWidth*2/100.f, HallHeight*0.75f/100.f));
	BackdropB->SetRelativeLocation(FVector(-(HallHalfLength - 220.f), 0, HallHeight*0.35f));
	BackdropB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(BackdropB, FLinearColor(0.045f, 0.05f, 0.065f))) BackdropB->SetMaterial(0, M);

	// Roof (no sky ever visible).
	Roof = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Roof"));
	Roof->SetupAttachment(Root);
	Roof->SetStaticMesh(Cube);
	Roof->SetRelativeScale3D(FVector(HallHalfLength*2/100.f, HallHalfWidth*2/100.f, 0.25f));
	Roof->SetRelativeLocation(FVector(0, 0, HallHeight));
	Roof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(Roof, FLinearColor(0.09f, 0.09f, 0.11f))) Roof->SetMaterial(0, M);

	// LED boards: bright thin slabs along both end walls just below the first
	// stand row (they read as advertising boards and give the hall depth).
	LedBoards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LedBoards"));
	LedBoards->SetupAttachment(Root);
	LedBoards->SetStaticMesh(Cube);
	LedBoards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(LedBoards, FLinearColor(0.02f, 0.04f, 0.08f))) LedBoards->SetMaterial(0, M);
	// A few luminous strips with a cooler tint near the floor line.
	LedBoards->AddInstance(FTransform(FRotator::ZeroRotator, FVector( HallHalfLength - 60.f, 0, 140.f), FVector(1.4f, 28.f, 1.1f)));
	LedBoards->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-HallHalfLength + 60.f, 0, 140.f), FVector(1.4f, 28.f, 1.1f)));
}

void AVolleyballArena::BuildLighting()
{
	auto MakeSpot = [this](const TCHAR* N, const FVector& Loc, float Intensity, float OuterCone)
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(N);
		L->SetupAttachment(Root);
		L->SetWorldLocation(Loc);
		L->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		L->Intensity = Intensity;
		L->OuterConeAngle = OuterCone;
		L->InnerConeAngle = OuterCone * 0.5f;
		L->AttenuationRadius = 3800.f;
		L->SetLightColor(FLinearColor(1.f, 0.97f, 0.9f));
		return L;
	};
	CourtLightA = MakeSpot(TEXT("CourtLightA"), FVector( 500.f, 0.f, 1250.f), 22000.f, 52.f);
	CourtLightB = MakeSpot(TEXT("CourtLightB"), FVector(-500.f, 0.f, 1250.f), 22000.f, 52.f);
	FillLight = MakeSpot(TEXT("FillLight"), FVector(0.f, 0.f, 1400.f), 6000.f, 100.f);

	USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("HallSkyLight"));
	Sky->SetupAttachment(Root);
	Sky->Intensity = 0.9f;
	Sky->bLowerHemisphereIsBlack = false;

	auto MakeCorner = [this](const TCHAR* N, const FVector& Loc)
	{
		UPointLightComponent* P = CreateDefaultSubobject<UPointLightComponent>(N);
		P->SetupAttachment(Root);
		P->SetWorldLocation(Loc);
		P->Intensity = 12000.f;
		P->AttenuationRadius = 2600.f;
		P->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
		PerimeterLights.Add(P);
	};
	MakeCorner(TEXT("Perim_PP"), FVector( 2200.f,  1600.f, 900.f));
	MakeCorner(TEXT("Perim_PN"), FVector( 2200.f, -1600.f, 900.f));
	MakeCorner(TEXT("Perim_NP"), FVector(-2200.f,  1600.f, 900.f));
	MakeCorner(TEXT("Perim_NN"), FVector(-2200.f, -1600.f, 900.f));
}

void AVolleyballArena::BuildStands(UStaticMesh* Cube)
{
	if (!Cube) return;

	// M11b-6: crowd density presets -CrowdLow / -CrowdHigh (default Medium 10).
	const int32 Rows = [this]() -> int32
	{
		const FString Cmd = FCommandLine::Get();
		if (FParse::Param(*Cmd, TEXT("CrowdLow")))  return 5;
		if (FParse::Param(*Cmd, TEXT("CrowdHigh"))) return 12;
		return StandRows;
	}();
	const int32 CrowdRows = FMath::Clamp(Rows, 5, 12);

	StandSteps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StandSteps"));
	StandSteps->SetupAttachment(Root);
	StandSteps->SetStaticMesh(Cube);
	StandSteps->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (auto* M = ArenaMakeMID(StandSteps, FLinearColor(0.30f, 0.31f, 0.36f)))
		StandSteps->SetMaterial(0, M);

	// Railings in front of the first row (keep the crowd off the concourse).
	Railings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Railings"));
	Railings->SetupAttachment(Root);
	Railings->SetStaticMesh(Cube);
	Railings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(Railings, FLinearColor(0.42f, 0.44f, 0.50f)))
		Railings->SetMaterial(0, M);

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

	CrowdHeads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CrowdHeads"));
	CrowdHeads->SetupAttachment(Root);
	if (UStaticMesh* S = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		CrowdHeads->SetStaticMesh(S);
	CrowdHeads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(CrowdHeads, FLinearColor(0.72f, 0.55f, 0.42f)))
		CrowdHeads->SetMaterial(0, M);
}

void AVolleyballArena::BeginPlay()
{
	Super::BeginPlay();
	PopulateStands();
}

void AVolleyballArena::PopulateStands()
{
	if (!StandSteps || !CrowdHeads || CrowdBodies.Num() == 0) return;
	// M11b-6: crowd density presets -CrowdLow / -CrowdHigh (default Medium 10).
	const FString Cmd = FCommandLine::Get();
	int32 CrowdRows = StandRows;
	if (FParse::Param(*Cmd, TEXT("CrowdLow")))  CrowdRows = 5;
	if (FParse::Param(*Cmd, TEXT("CrowdHigh"))) CrowdRows = 12;
	CrowdRows = FMath::Clamp(CrowdRows, 5, 12);
	StandSteps->ClearInstances();
	CrowdHeads->ClearInstances();
	Railings->ClearInstances();
	for (auto& B : CrowdBodies) if (B) B->ClearInstances();

	const float SideY0 = StandClearanceY;          // 1150
	const float EndX0  = StandClearanceX;          // 1750
	const int32 BodyKinds = CrowdBodies.Num();

	auto AddSpectator = [&](const FVector& BaseLoc)
	{
		const int32 Kind = FMath::RandRange(0, BodyKinds - 1);
		const float Sway = FMath::FRandRange(-3.f, 3.f);
		FVector BodyLoc(BaseLoc.X + Sway, BaseLoc.Y + Sway, BaseLoc.Z + 30.f);
		FVector HeadLoc(BaseLoc.X + Sway, BaseLoc.Y + Sway, BaseLoc.Z + 68.f);
		CrowdBodies[Kind]->AddInstance(FTransform(FRotator::ZeroRotator, BodyLoc, FVector(0.28f, 0.20f, 0.55f)));
		CrowdHeads->AddInstance(FTransform(FRotator::ZeroRotator, HeadLoc, FVector(0.16f, 0.16f, 0.16f)));
	};

	// Side stands (along X), 10 rows, central aisle gap + corner aisles.
	for (int32 Row = 0; Row < CrowdRows; ++Row)
	{
		const float Y = SideY0 + Row * StandStepDepth;
		const float Z = Row * StandStepHeight;
		const float StepLen = (HallHalfLength * 2 - 600.f) / 100.f; // aisles at corners
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, Y, Z + 5.f), FVector(StepLen, StandStepDepth/100.f, 0.1f)));
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, -Y, Z + 5.f), FVector(StepLen, StandStepDepth/100.f, 0.1f)));
		if (Row == 0)
		{
			const float RY = Y - 20.f;
			Railings->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, RY, 55.f), FVector(StepLen, 0.06f, 0.55f)));
			Railings->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, -RY, 55.f), FVector(StepLen, 0.06f, 0.55f)));
		}
		for (int32 C = -16; C <= 16; ++C)
		{
			if (FMath::Abs(C) <= 1) continue; // central aisle
			const float X = C * 85.f;
			AddSpectator(FVector(X, Y - 30.f, Z + 10.f));
			AddSpectator(FVector(X, -Y + 30.f, Z + 10.f));
		}
	}
	// End stands (along Y), 10 rows, central + corner aisles.
	for (int32 Row = 0; Row < CrowdRows; ++Row)
	{
		const float X = EndX0 + Row * StandStepDepth;
		const float Z = Row * StandStepHeight;
		const float StepLen = (HallHalfWidth * 2 - 500.f) / 100.f;
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector( X, 0, Z + 5.f), FVector(StandStepDepth/100.f, StepLen, 0.1f)));
		StandSteps->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-X, 0, Z + 5.f), FVector(StandStepDepth/100.f, StepLen, 0.1f)));
		if (Row == 0)
		{
			const float RX = X - 20.f;
			Railings->AddInstance(FTransform(FRotator::ZeroRotator, FVector( RX, 0, 55.f), FVector(0.06f, StepLen, 0.55f)));
			Railings->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-RX, 0, 55.f), FVector(0.06f, StepLen, 0.55f)));
		}
		for (int32 C = -10; C <= 10; ++C)
		{
			if (FMath::Abs(C) <= 1) continue;
			const float Y = C * 90.f;
			AddSpectator(FVector( X - 30.f, Y, Z + 10.f));
			AddSpectator(FVector(-X + 30.f, Y, Z + 10.f));
		}
	}
}