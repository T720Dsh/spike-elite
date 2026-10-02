// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballArena.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "SEMaterials.h"
#include "SEArtGeometry.h"

static UMaterialInstanceDynamic* ArenaMakeMID(UObject* Owner, const FLinearColor& Color)
{
	return SEMaterials::MakeSurface(Owner, Color, .82f);
}

AVolleyballArena::AVolleyballArena()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(HallHalfLength, HallHalfWidth, 10.f));
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	// A real visible floor replaces the old invisible 20 cm root slab.
	MakeWall(TEXT("ConcourseFloor"), FVector(0, 0, -12.f), FVector(HallHalfLength*2/100.f, HallHalfWidth*2/100.f, .2f));
	auto Instances = [&](const TCHAR* Name, FLinearColor Color)
	{
		auto* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root); C->SetStaticMesh(Cube);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMaterial(0, ArenaMakeMID(C, Color)); return C;
	};
	Structure = Instances(TEXT("RoofStructure"), FLinearColor(.32f,.36f,.43f));
	Structure->SetMaterial(0, SEMaterials::MakeSurface(Structure, FLinearColor(.32f,.36f,.43f), .38f, .65f));
	LightFixtures = Instances(TEXT("LightFixtures"), FLinearColor(.85f,.88f,.94f));
	for (int32 Bay=-3; Bay<=3; ++Bay)
	{
		const float X=Bay*850.f;
		for (float Z : {1330.f, 1420.f})
			Structure->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X,0,Z),FVector(.1f,54.f,.1f)));
		for(int32 Segment=-8; Segment<=8; ++Segment)
		{
			const FVector A(X, Segment*300.f, 1330.f), B(X,(Segment+1)*300.f,1420.f);
			const FVector D=B-A;
			Structure->AddInstance(FTransform(D.Rotation(),(A+B)*.5f,FVector(D.Size()/100.f,.045f,.045f)));
		}
		for(float Y : {-900.f,900.f})
		{
			LightFixtures->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X,Y,1290.f),FVector(1.7f,.7f,.12f)));
			Structure->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X,Y,1350.f),FVector(.035f,.035f,1.2f)));
		}
		for(float Y : {-HallHalfWidth+65.f,HallHalfWidth-65.f})
			Structure->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X,Y,700.f),FVector(.32f,.32f,14.f)));
	}
	// End-wall service portals and frames, outside the playing/free area.
	for(int32 Side : {-1,1})
	{
		const float X=Side*(HallHalfLength-235.f);
		for(float Y : {-1500.f,1500.f})
		{
			auto* Door=MakeWall(*FString::Printf(TEXT("Portal_%d_%d"),Side,int32(Y)),FVector(X,Y,140.f),FVector(.1f,2.1f,2.8f));
			Door->SetMaterial(0,ArenaMakeMID(Door,FLinearColor(.035f,.065f,.09f)));
			for(float Offset : {-112.f,112.f})
				Structure->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X-Side*6.f,Y+Offset,145.f),FVector(.18f,.12f,2.9f)));
			Structure->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X-Side*6.f,Y,292.f),FVector(.18f,2.36f,.12f)));
		}
	}

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

	// M11d-5: two-tone acoustic panels on the side walls (arena look, not a flat
	// grey box). No collision, static colour — cheap, visible, packaged-safe.
	const FLinearColor AcousticCol(0.13f, 0.20f, 0.30f);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Y = (Side == 0) ? (HallHalfWidth - 40.f) : -(HallHalfWidth - 40.f);
		for (int32 Band = 0; Band < 3; ++Band)
		{
			const float X = -1200.f + Band * 850.f;
			UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Acoustic_%d_%d"), Side, Band));
			P->SetupAttachment(Root);
			P->SetStaticMesh(Cube);
			P->SetRelativeScale3D(FVector(800.f/100.f, 6.f/100.f, 420.f/100.f));
			P->SetRelativeLocation(FVector(X, Y, 700.f));
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (auto* M = ArenaMakeMID(P, AcousticCol)) P->SetMaterial(0, M);
		}
	}

	// M11d-5: un-branded arena signage on the end walls (SPIKE ELITE / PLAY FAIR,
	// original text only — no commercial trademarks).
	static ConstructorHelpers::FObjectFinder<UFont> RobotoFont(TEXT("/Engine/EngineFonts/Roboto"));
	auto MakeSign = [this](const TCHAR* N, const FString& Text, const FVector& Loc, const FRotator& Rot)
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(N);
		T->SetupAttachment(Root);
		T->SetRelativeLocation(Loc);
		T->SetRelativeRotation(Rot);
		T->SetWorldSize(70.f);
		T->SetTextRenderColor(FLinearColor(0.75f, 0.82f, 0.95f).ToFColor(true));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		if (RobotoFont.Succeeded()) { T->SetFont(RobotoFont.Object); }
		T->SetText(FText::FromString(Text));
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->SetCastShadow(false);
		return T;
	};
	MakeSign(TEXT("SignA"), TEXT("SPIKE ELITE"), FVector(HallHalfLength - 250.f, 0.f, 1100.f), FRotator(0.f, 180.f, 0.f));
	MakeSign(TEXT("SignB"), TEXT("SPIKE ELITE"), FVector(-(HallHalfLength - 250.f), 0.f, 1100.f), FRotator(0.f, 0.f, 0.f));
	MakeSign(TEXT("SignC"), TEXT("PLAY FAIR"), FVector(0.f, HallHalfWidth - 150.f, 900.f), FRotator(0.f, -90.f, 0.f));
	MakeSign(TEXT("SignD"), TEXT("PLAY FAIR"), FVector(0.f, -(HallHalfWidth - 150.f), 900.f), FRotator(0.f, 90.f, 0.f));
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
	MakeSpot(TEXT("OfficialsWorkLight"), FVector(0,-1200,850),9000.f,55.f);

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

	// Seated-leg silhouettes (dark) so the crowd reads as sitting in the stand
	// rows instead of a wall of upright coloured blocks.
	CrowdLegs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CrowdLegs"));
	CrowdLegs->SetupAttachment(Root);
	CrowdLegs->SetStaticMesh(Cube);
	CrowdLegs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (auto* M = ArenaMakeMID(CrowdLegs, FLinearColor(0.16f, 0.17f, 0.22f)))
		CrowdLegs->SetMaterial(0, M);
	auto Detail = [&](const TCHAR* Name, FLinearColor Color)
	{
		auto* C=CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root); C->SetStaticMesh(Cube);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMaterial(0,ArenaMakeMID(C,Color)); return C;
	};
	Seats=Detail(TEXT("SeatPans"), FLinearColor(.07f,.2f,.36f));
	SeatBacks=Detail(TEXT("SeatBacks"), FLinearColor(.09f,.27f,.47f));
	CrowdArms=Detail(TEXT("CrowdArms"), FLinearColor(.61f,.4f,.28f));
	CrowdThighs=Detail(TEXT("CrowdThighs"), FLinearColor(.12f,.14f,.19f));
	CrowdShoes=Detail(TEXT("CrowdShoes"), FLinearColor(.04f,.045f,.06f));
	CrowdHair=Detail(TEXT("CrowdHair"), FLinearColor(.035f,.022f,.016f));
}

void AVolleyballArena::BeginPlay()
{
	Super::BeginPlay();
	for (auto& B : CrowdBodies) B->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Torso));
	CrowdHeads->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Head));
	CrowdHair->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Head));
	CrowdLegs->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Calf));
	CrowdThighs->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Thigh));
	CrowdArms->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Forearm));
	CrowdShoes->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Shoe));
	Seats->SetStaticMesh(SEArtGeometry::Get(SEArtGeometry::EProfile::Seat));
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
	if (CrowdLegs) CrowdLegs->ClearInstances();
	Railings->ClearInstances();
	for (auto& B : CrowdBodies) if (B) B->ClearInstances();
	for(auto* C : {Seats.Get(), SeatBacks.Get(), CrowdArms.Get(), CrowdThighs.Get(), CrowdShoes.Get(), CrowdHair.Get()}) C->ClearInstances();
	FRandomStream CrowdRandom(1978); // art variation must not consume gameplay RNG

	const float SideY0 = StandClearanceY;          // 1150
	const float EndX0  = StandClearanceX;          // 1750
	const int32 BodyKinds = CrowdBodies.Num();

	auto AddSpectator = [&](const FVector& BaseLoc)
	{
		const int32 Kind = CrowdRandom.RandRange(0, BodyKinds - 1);
		const float Sway = CrowdRandom.FRandRange(-3.f, 3.f);
		// Seated silhouette: low wide body, dark legs below, head on top.
		FVector BodyLoc(BaseLoc.X + Sway, BaseLoc.Y + Sway, BaseLoc.Z + 66.f);
		FVector HeadLoc(BaseLoc.X + Sway, BaseLoc.Y + Sway, BaseLoc.Z + 108.f);
		// M11f-4: orient the elongated body axis toward court centre so the four
		// stands all face the match instead of one shared zero rotation.
		const float FacingYaw = FMath::RadiansToDegrees(FMath::Atan2(-BodyLoc.Y, -BodyLoc.X));
		const FRotator Facing(0.f, FacingYaw, 0.f);
		auto Put=[&](UInstancedStaticMeshComponent* C, FVector Offset, FVector Size, FRotator Local=FRotator::ZeroRotator)
		{ C->AddInstance(FTransform(Facing+Local,BaseLoc+Facing.RotateVector(Offset),Size/100.f)); };
		CrowdBodies[Kind]->AddInstance(FTransform(Facing, BodyLoc, FVector(.23f,.39f,.51f)));
		CrowdHeads->AddInstance(FTransform(Facing, HeadLoc, FVector(.18f,.17f,.24f)));
		Put(CrowdHair,FVector(Sway,Sway,116),FVector(18,17,10));
		Put(Seats,FVector(0,0,35),FVector(49,48,7));
		Put(SeatBacks,FVector(-22,0,62),FVector(6,48,51));
		for(float Side : {-1.f,1.f})
		{
			Put(CrowdThighs,FVector(17,Side*10,38),FVector(16,17,42),FRotator(90,0,0));
			Put(CrowdLegs,FVector(33,Side*10,18),FVector(11,12,33));
			Put(CrowdShoes,FVector(39,Side*10,4),FVector(25,13,8));
			Put(CrowdArms,FVector(15,Side*22,52),FVector(9,10,32),FRotator(45,0,0));
		}
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
			for(float Side : {-1.f,1.f})
			{
				for(float ZRail : {45.f,95.f}) Railings->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,Side*RY,ZRail),FVector(StepLen,.04f,.04f)));
				for(int32 Post=-16;Post<=16;++Post) Railings->AddInstance(FTransform(FRotator::ZeroRotator,FVector(Post*170.f,Side*RY,47.f),FVector(.04f,.04f,.94f)));
			}
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
			for(float Side : {-1.f,1.f})
			{
				for(float ZRail : {45.f,95.f}) Railings->AddInstance(FTransform(FRotator::ZeroRotator,FVector(Side*RX,0,ZRail),FVector(.04f,StepLen,.04f)));
				for(int32 Post=-12;Post<=12;++Post) Railings->AddInstance(FTransform(FRotator::ZeroRotator,FVector(Side*RX,Post*170.f,47.f),FVector(.04f,.04f,.94f)));
			}
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
