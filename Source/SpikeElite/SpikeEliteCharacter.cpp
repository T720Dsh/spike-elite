// SPDX-License-Identifier: MIT
#include "SpikeEliteCharacter.h"
#include "SpikeEliteGameMode.h"
#include "SEMaterials.h"
#include "SEArtGeometry.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ASpikeEliteCharacter::ASpikeEliteCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// ---- M11b-3: articulated procedural humanoid from engine basic shapes ----
	// Segment layout (cm, total height ~170). Every segment is a cube/sphere
	// scaled to a real body part; each limb hangs from a joint SceneComponent.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	auto MakeMesh = [this](const TCHAR* N, USceneComponent* Parent, const FVector& Scale, const FVector& Loc)
		-> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(N);
		C->SetupAttachment(Parent);
		if (CubeMesh.Succeeded()) C->SetStaticMesh(CubeMesh.Object);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Loc);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // capsule is the only collider
		return C;
	};
	auto MakeJoint = [this](const TCHAR* N, USceneComponent* Parent, const FVector& Loc) -> USceneComponent*
	{
		USceneComponent* J = CreateDefaultSubobject<USceneComponent>(N);
		J->SetupAttachment(Parent);
		J->SetRelativeLocation(Loc);
		return J;
	};

	// Torso + head (straight on the capsule root).
	TorsoJoint = MakeJoint(TEXT("TorsoJoint"), RootComponent, FVector(0.f, 0.f, 20.f));
	Torso = MakeMesh(TEXT("Torso"), TorsoJoint, FVector(0.28f, 0.46f, 0.66f), FVector(0.f, 0.f, 30.f));
	HeadJoint = MakeJoint(TEXT("HeadJoint"), TorsoJoint, FVector(0.f, 0.f, 78.f));
	Head = MakeMesh(TEXT("Head"), HeadJoint, FVector(0.19f, 0.175f, 0.25f), FVector::ZeroVector);
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);

		// M11d-6: rounded silhouette — sphere shoulder pads + a low hip block,
		// so the torso reads as a stylized athlete, not a single cube.
		ShoulderL = MakeMesh(TEXT("ShoulderL"), TorsoJoint, FVector(0.16f, 0.11f, 0.11f), FVector(0.f, -24.f, 57.f));
		ShoulderL->SetStaticMesh(SphereMesh.Object);
		ShoulderR = MakeMesh(TEXT("ShoulderR"), TorsoJoint, FVector(0.16f, 0.11f, 0.11f), FVector(0.f, 24.f, 57.f));
		ShoulderR->SetStaticMesh(SphereMesh.Object);
		HipPad = MakeMesh(TEXT("HipPad"), TorsoJoint, FVector(0.26f, 0.34f, 0.14f), FVector(0.f, 0.f, 2.f));
	}

	// Limbs. Shoulder/hip joint at the root of each limb; upper segment hangs
	// down, bend joint at its bottom, lower segment, tip (hand/foot) last.
	auto BuildLimb = [&](const TCHAR* Base, USceneComponent* Root, float YSide, bool bArm)
	{
		FProceduralLimb L;
		const FVector JointLoc(0.f, YSide, bArm ? 62.f : -3.f);
		L.Joint = MakeJoint(*FString::Printf(TEXT("%sJoint"), Base), Root, JointLoc);
		const float UpLen = bArm ? 27.f : 48.f;
		const float LoLen = bArm ? 24.f : 46.f;
		L.Upper = MakeMesh(*FString::Printf(TEXT("%sUpper"), Base), L.Joint,
			bArm ? FVector(0.10f, 0.10f, UpLen * 0.01f) : FVector(0.14f, 0.14f, UpLen * 0.01f),
			FVector(0.f, 0.f, -UpLen * 0.5f));
		// Joints inherit rotation, never the mesh's non-uniform scale. Otherwise
		// forearms/shins and their offsets are multiplied by the upper mesh scale.
		L.BendJoint = MakeJoint(*FString::Printf(TEXT("%sBend"), Base), L.Joint, FVector(0.f, 0.f, -UpLen));
		L.Lower = MakeMesh(*FString::Printf(TEXT("%sLower"), Base), L.BendJoint,
			bArm ? FVector(0.08f, 0.08f, LoLen * 0.01f) : FVector(0.11f, 0.11f, LoLen * 0.01f),
			FVector(0.f, 0.f, -LoLen * 0.5f));
		L.Tip = MakeMesh(*FString::Printf(TEXT("%sTip"), Base), L.BendJoint,
			bArm ? FVector(0.075f, 0.095f, 0.12f) : FVector(0.28f, 0.14f, 0.11f),
			FVector(bArm ? 0.f : 7.f, 0.f, -LoLen - (bArm ? 5.f : 1.5f)));
		return L;
	};

	ArmL = BuildLimb(TEXT("ArmL"), TorsoJoint, -24.f, true);
	ArmR = BuildLimb(TEXT("ArmR"), TorsoJoint,  24.f, true);
	// Torso bottom is local Z=-3 (centre 30, half-height 33). Attach hips
	// there, so leaning/dive poses cannot tear the torso away from the legs.
	LegL = BuildLimb(TEXT("LegL"), TorsoJoint, -10.f, false);
	LegR = BuildLimb(TEXT("LegR"), TorsoJoint,  10.f, false);
	BuildArtDetails();

	// M11d-6: jersey number on chest and back (engine default font, no external
	// assets). The capsule is the only collider and is hidden, so the text
	// renders through it without interfering with gameplay.
	// M11e-1 diagnosis: TextRender's default material (DefaultTextMaterialOpaque)
	// and the Roboto font load synchronously during CDO construction, i.e. during
	// engine init — this is the only M11d-added sync-load path that runs before
	// "Game Engine Initialized". `-NoJerseyText` skips both components so the
	// packaged startup hang can be bisected (Development diagnostic only).
	const bool bNoJerseyText = FParse::Param(FCommandLine::Get(), TEXT("NoJerseyText"));
	if (!bNoJerseyText)
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFont(TEXT("/Engine/EngineFonts/Roboto"));
		const FVector NumScale(1.f);
		const FLinearColor NumColor(1.f, 1.f, 1.f);

		JerseyFront = CreateDefaultSubobject<UTextRenderComponent>(TEXT("JerseyFront"));
		JerseyFront->SetupAttachment(TorsoJoint);
		// TextRender's glyph plane faces along local +X.  The torso cube is 46 cm
		// deep, so place the number just outside its surface (the old 0.26 cm
		// offset left both labels buried inside the opaque cube).
		JerseyFront->SetRelativeLocation(FVector(14.2f, 0.f, 34.f));
		JerseyFront->SetRelativeRotation(FRotator::ZeroRotator);
		JerseyFront->SetRelativeScale3D(NumScale);
		JerseyFront->SetWorldSize(24.f);
		JerseyFront->SetTextRenderColor(NumColor.ToFColor(true));
		JerseyFront->SetHorizontalAlignment(EHTA_Center);
		JerseyFront->SetVerticalAlignment(EVRTA_TextCenter);
		JerseyFront->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		JerseyFront->SetCastShadow(false);

		JerseyBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("JerseyBack"));
		JerseyBack->SetupAttachment(TorsoJoint);
		JerseyBack->SetRelativeLocation(FVector(-14.2f, 0.f, 34.f));
		JerseyBack->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		JerseyBack->SetRelativeScale3D(NumScale);
		JerseyBack->SetWorldSize(24.f);
		JerseyBack->SetTextRenderColor(NumColor.ToFColor(true));
		JerseyBack->SetHorizontalAlignment(EHTA_Center);
		JerseyBack->SetVerticalAlignment(EVRTA_TextCenter);
		JerseyBack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		JerseyBack->SetCastShadow(false);
	} // !bNoJerseyText

	GetCapsuleComponent()->SetCapsuleHalfHeight(84.f);
	GetCapsuleComponent()->SetCapsuleRadius(32.f);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;   // face move direction (good for bots)
	MoveComp->JumpZVelocity = 520.0f;
	MoveComp->AirControl = 0.5f;
	MoveComp->MaxWalkSpeed = 450.0f;

	// M10 camera pass: longer arm, raised + shoulder offset, collision tests and
	// a slight lag so the ball at court centre is not hidden behind the body.
	// M11f-2: raise the boom and lengthen the arm so the third-person frame shows
	// the lower body, the ball and the net together; the camera itself gets a
	// small downward tilt in UpdateCameraView so the court centre fills the frame.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 45.f);  // shoulder offset
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;          // never clip through walls/stands/players
	CameraBoom->ProbeSize = 14.f;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 9.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 12.0f;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;
	ThirdPersonCamera->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 10.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void ASpikeEliteCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyArtMeshes();
	ApplyJerseyColor();
	RefreshJerseyNumberVisual();

	if (!bIsBot)
	{
		UpdateCameraView();
	}
	else
	{
		// Bots don't need cameras.
		if (CameraBoom) CameraBoom->Deactivate();
		if (ThirdPersonCamera) ThirdPersonCamera->Deactivate();
		if (FirstPersonCamera) FirstPersonCamera->Deactivate();
	}
}

void ASpikeEliteCharacter::BuildArtDetails()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Part = [this, Sphere, Cube](const FString& Name, USceneComponent* Parent, FVector Loc, FVector Size, FName Finish, bool bRound = true)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		C->SetupAttachment(Parent); C->SetStaticMesh(bRound ? Sphere : Cube);
		C->SetRelativeLocation(Loc); C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->ComponentTags.Add(Finish); ArtDetails.Add(C);
		if(Size.GetMax()<10.f) C->SetCastShadow(false); // tiny facial/hand detail need not create shadow draws
		if (Parent == HeadJoint) { C->ComponentTags.Add(TEXT("FaceDetail")); }
		return C;
	};
	Part(TEXT("Neck"), TorsoJoint, FVector(0,0,67), FVector(11,12,16), TEXT("Skin"));
	Part(TEXT("Hair"), HeadJoint, FVector(-1,0,8.5), FVector(19,18.5,12), TEXT("Hair"));
	Part(TEXT("Nose"), HeadJoint, FVector(9,0,-.5), FVector(4.5,3.3,6), TEXT("Skin"));
	Part(TEXT("Mouth"), HeadJoint, FVector(8.7,0,-5), FVector(1.4,5,1), TEXT("Lip"));
	for (int32 S : {-1, 1})
	{
		const FString Side = S < 0 ? TEXT("L") : TEXT("R");
		Part(TEXT("Ear") + Side, HeadJoint, FVector(0,S*8.7,-1), FVector(4,3,6), TEXT("Skin"));
		Part(TEXT("EyeWhite") + Side, HeadJoint, FVector(8.5,S*4,2.5), FVector(2.2,3,2), TEXT("White"));
		Part(TEXT("Iris") + Side, HeadJoint, FVector(9.6,S*4,2.5), FVector(.8,1.25,1.25), TEXT("Hair"));
		Part(TEXT("Brow") + Side, HeadJoint, FVector(8.7,S*4,4.8), FVector(1.4,3.8,.9), TEXT("Hair"));
		FProceduralLimb& Arm = S < 0 ? ArmL : ArmR;
		FProceduralLimb& Leg = S < 0 ? LegL : LegR;
		Part(TEXT("Sleeve") + Side, Arm.Joint, FVector(0,0,-6), FVector(14,14,15), TEXT("Uniform"));
		Part(TEXT("Elbow") + Side, Arm.BendJoint, FVector::ZeroVector, FVector(9,9,9), TEXT("Skin"));
		Part(TEXT("WristTape") + Side, Arm.BendJoint, FVector(0,0,-22), FVector(7,8,3), TEXT("White"));
		for (int32 Finger = 0; Finger < 4; ++Finger)
		{
			Part(FString::Printf(TEXT("Finger%s%d"), *Side, Finger), Arm.BendJoint,
				FVector(0,(Finger-1.5f)*2.f,-33.5f + FMath::Abs(Finger-1.5f)), FVector(1.8,1.7,8), TEXT("Skin"));
		}
		Part(TEXT("Thumb") + Side, Arm.BendJoint, FVector(1,S*5,-28.5f), FVector(2.7,2.4,6), TEXT("Skin"));
		Part(TEXT("ShortsHem") + Side, Leg.Joint, FVector(0,0,-18), FVector(19,19,29), TEXT("Shorts"));
		Part(TEXT("KneePad") + Side, Leg.BendJoint, FVector(4,0,-1), FVector(15,15,16), TEXT("Dark"));
		Part(TEXT("Sock") + Side, Leg.BendJoint, FVector(0,0,-38), FVector(8,9,17), TEXT("White"));
		Part(TEXT("Sole") + Side, Leg.BendJoint, FVector(7,0,-49), FVector(27,14,2.5), TEXT("White"));
		for (int32 Lace = 0; Lace < 3; ++Lace)
		{
			Part(FString::Printf(TEXT("Lace%s%d"), *Side, Lace), Leg.BendJoint,
				FVector(7+Lace*2,0,-43.2f-Lace*.5f), FVector(1,9,1), TEXT("White"), false);
		}
	}
	// Seams and collar use unscaled joints; all dimensions above are centimetres.
	Part(TEXT("Collar"), TorsoJoint, FVector(0,0,61), FVector(15,19,4), TEXT("White"));
	Part(TEXT("Waistband"), TorsoJoint, FVector(0,0,2), FVector(28,36,4), TEXT("Dark"));
}

void ASpikeEliteCharacter::ApplyArtMeshes()
{
	using namespace SEArtGeometry;
	auto Replace = [](UStaticMeshComponent* C, EProfile Profile)
	{
		if (C) { if (UStaticMesh* M = Get(Profile)) { C->SetStaticMesh(M); } }
	};
	Replace(Torso, EProfile::Torso); Replace(Head, EProfile::Head);
	Replace(HipPad, EProfile::Torso);
	for(UStaticMeshComponent* Detail : ArtDetails)
	{
		if(Detail && Detail->GetName().StartsWith(TEXT("ShortsHem"))) Replace(Detail,EProfile::UpperArm);
	}
	for (FProceduralLimb* Arm : {&ArmL,&ArmR})
	{
		Replace(Arm->Upper,EProfile::UpperArm); Replace(Arm->Lower,EProfile::Forearm); Replace(Arm->Tip,EProfile::Palm);
	}
	for (FProceduralLimb* Leg : {&LegL,&LegR})
	{
		Replace(Leg->Upper,EProfile::Thigh); Replace(Leg->Lower,EProfile::Calf); Replace(Leg->Tip,EProfile::Shoe);
	}
}

FVector ASpikeEliteCharacter::GetHandWorldPosition(bool bLeft) const
{
	const FProceduralLimb& L = bLeft ? ArmL : ArmR;
	if (!L.BendJoint) { return FVector::ZeroVector; }
	// Hand tip mesh centre: 24cm forearm + 2cm offset below the bend joint.
	return L.Tip ? L.Tip->GetComponentLocation() : L.BendJoint->GetComponentLocation();
}

float ASpikeEliteCharacter::GetHeadHeight() const
{
	if (!Head) { return GetActorLocation().Z + 110.f; }
	// Head sphere centre is 78cm above the torso joint (20cm above root) + 11cm radius.
	return Head->GetComponentTransform().GetLocation().Z + 11.f;
}

float ASpikeEliteCharacter::GetTorsoJointHeight() const
{
	if (!TorsoJoint) { return GetActorLocation().Z; }
	return TorsoJoint->GetComponentTransform().GetLocation().Z;
}

void ASpikeEliteCharacter::RefreshJerseyNumberVisual()
{
	const FString Num = (JerseyNumber > 0) ? FString::FromInt(JerseyNumber) : TEXT("");
	if (JerseyFront) { JerseyFront->SetText(FText::FromString(Num)); }
	if (JerseyBack)  { JerseyBack->SetText(FText::FromString(Num)); }
}

void ASpikeEliteCharacter::SetThirdPersonArmLength(float NewLength)
{
	ThirdPersonArmLength = NewLength;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = NewLength;
		CameraBoom->SocketOffset = FVector(0.f, 55.f, 35.f);
		if (NewLength < 220.f)
		{
			// Closeup: swing the camera to the side/up so the court centre (and
			// the articulated body) fills the frame instead of the body's back.
			CameraBoom->SocketOffset = FVector(0.f, 90.f, 60.f);
		}
	}
}

void ASpikeEliteCharacter::ApplyJerseyColor()
{
	// Team A = electric blue, Team B = red. Jersey = torso + arms; shorts are a
	// darker shade of the team colour; head is a neutral skin tone.
	const FLinearColor Jersey = (TeamSide > 0) ? FLinearColor(0.10f, 0.50f, 1.00f) : FLinearColor(0.95f, 0.22f, 0.12f);
	const FLinearColor Shorts = (TeamSide > 0) ? FLinearColor(0.05f, 0.16f, 0.38f) : FLinearColor(0.38f, 0.07f, 0.05f);
	const FLinearColor SkinPalette[] = { FLinearColor(.63f,.40f,.26f), FLinearColor(.82f,.61f,.44f), FLinearColor(.40f,.23f,.15f), FLinearColor(.72f,.48f,.31f) };
	const FLinearColor Skin = SkinPalette[FMath::Max(0,PlayerId) % UE_ARRAY_COUNT(SkinPalette)];

		auto Tint = [&](UStaticMeshComponent* Comp, const FLinearColor& Col)
	{
		if (!Comp) return;
		if (UMaterialInstanceDynamic* MID = SEMaterials::MakeSurface(this, Col, Col.Equals(Skin) ? .62f : .88f))
		{
			Comp->SetMaterial(0, MID);
		}
	};

	Tint(Torso, Jersey);
	Tint(ShoulderL, Jersey);
	Tint(ShoulderR, Jersey);
	Tint(HipPad, Shorts);
	Tint(ArmL.Upper, Skin);
	Tint(ArmR.Upper, Skin);
	Tint(ArmL.Lower, Skin);
	Tint(ArmR.Lower, Skin);
	Tint(ArmL.Tip, Skin);
	Tint(ArmR.Tip, Skin);
	Tint(LegL.Upper, Shorts);
	Tint(LegR.Upper, Shorts);
	Tint(LegL.Lower, Skin);
	Tint(LegR.Lower, Skin);
	Tint(LegL.Tip, Shorts);
	Tint(LegR.Tip, Shorts);
	Tint(Head, Skin);
	const FName Finishes[] = {TEXT("Skin"),TEXT("Hair"),TEXT("White"),TEXT("Uniform"),TEXT("Shorts"),TEXT("Dark"),TEXT("Lip")};
	const FLinearColor Colors[] = {Skin,FLinearColor(.025f,.016f,.01f),FLinearColor(.92f,.94f,.97f),Jersey,Shorts,FLinearColor(.04f,.05f,.07f),Skin*.55f};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Finishes); ++I)
	{
		if (UMaterialInstanceDynamic* M = SEMaterials::MakeSurface(this, Colors[I], I == 0 ? .62f : .86f))
		{
			for (UStaticMeshComponent* C : ArtDetails) { if (C && C->ComponentHasTag(Finishes[I])) { C->SetMaterial(0,M); } }
		}
	}
}

void ASpikeEliteCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector Now = GetActorLocation();
	AnimationMoveSpeed = GetVelocity().Size2D();
	if (bIsBot && bAnimationLocationReady && DeltaSeconds > SMALL_NUMBER)
	{
		const float Travel = FVector::Dist2D(Now, PreviousAnimationLocation);
		if (Travel < 100.f) { AnimationMoveSpeed = Travel / DeltaSeconds; }
	}
	PreviousAnimationLocation = Now; bAnimationLocationReady = true;

	// M11b-3: procedural animation runs for bots and the human alike.
	UpdateProceduralAnimation(DeltaSeconds);

	if (bIsBot)
	{
		TickBot(DeltaSeconds);
		return;
	}

	// Human player boundary. M11c-1: while the authorized server, widen the X
	// range to the service zone behind the end line (X=±900) so the player can
	// step back to ±1150..1300 to serve; Y stays within the 9 m-wide service
	// zone. Normal play keeps the ±950 / ±500 court bounds.
	FVector Loc = GetActorLocation();
	const float MaxX = bServiceZoneActive ? 1550.0f : 950.0f;
	const float MinX = bServiceZoneActive ? (TeamSide > 0 ? 30.0f : -1550.0f) : 50.0f;
	const float MaxY = bServiceZoneActive ? 450.0f : 500.0f;
	bool bClamped = false;
	if (Loc.X < MinX)  { Loc.X = MinX; bClamped = true; }
	if (Loc.X > MaxX)  { Loc.X = MaxX; bClamped = true; }
	if (FMath::Abs(Loc.Y) > MaxY) { Loc.Y = FMath::Clamp(Loc.Y, -MaxY, MaxY); bClamped = true; }
	if (bClamped) SetActorLocation(Loc, true);
}

void ASpikeEliteCharacter::TickBot(float DeltaSeconds)
{
	// Cache the GameMode once; never call GetAllActorsOfClass every frame.
	if (!AIGameMode.IsValid())
	{
		if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			AIGameMode = GM;
		}
	}
	ASpikeEliteGameMode* GM = AIGameMode.Get();
	if (!GM) return;

	// Safety: keep simulated movement alive (deferred spawns can leave MOVE_None).
	if (UCharacterMovementComponent* MC = GetCharacterMovement())
	{
		if (MC->MovementMode == MOVE_None) { MC->SetMovementMode(MOVE_Walking); }
	}

	// ---- Movement from the GameMode's directive ----
	FVector Dest;
	switch (AIBehavior)
	{
	case EAIBehavior::MoveToReceive:
	case EAIBehavior::Set:
	case EAIBehavior::Attack:
	case EAIBehavior::MoveToBlock:
	case EAIBehavior::Dive:
		Dest = AITargetLocation;
		break;
	case EAIBehavior::Wait:
	case EAIBehavior::ReturnHome:
	default:
		Dest = HomePosition;
		break;
	}
	Dest.Z = GetActorLocation().Z;
	FVector ToDest = Dest - GetActorLocation();
	ToDest.Z = 0;
	const float Dist = ToDest.Size();
	// A dive is a fast lunge: the bot closes the last stretch quickly, then the
	// GameMode grants an extended reach while bDiving is set.
	const float BotSpeed = (AIBehavior == EAIBehavior::Dive) ? 640.0f : 450.0f;
	if (Dist > 30.0f)
	{
		// Direct, deterministic movement (no reliance on character-movement input
		// consumption, which deferred-spawned pawns without a controller may skip).
		const float Step = FMath::Min(BotSpeed * DeltaSeconds, Dist);
		FVector NewLoc = GetActorLocation() + ToDest.GetSafeNormal() * Step;
		const float XMax = bServiceZoneActive ? 1550.f : 950.f;
		NewLoc.X = (TeamSide > 0) ? FMath::Clamp(NewLoc.X, 30.f, XMax) : FMath::Clamp(NewLoc.X, -XMax, -30.f);
		NewLoc.Y = FMath::Clamp(NewLoc.Y, bServiceZoneActive ? -450.f : -500.f, bServiceZoneActive ? 450.f : 500.f);
		SetActorLocation(NewLoc, true);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToDest.Rotation(), DeltaSeconds, 10.f));
	}

	// ---- Boundary: stay on own half, don't run out ----
	FVector Loc = GetActorLocation();
	if (bServiceZoneActive)
	{
		Loc.X = (TeamSide > 0) ? FMath::Clamp(Loc.X, 30.f, 1550.f) : FMath::Clamp(Loc.X, -1550.f, -30.f);
		Loc.Y = FMath::Clamp(Loc.Y, -450.f, 450.f);
	}
	else
	{
		if (TeamSide > 0) { Loc.X = FMath::Clamp(Loc.X, 30.0f, 950.0f); }
		else              { Loc.X = FMath::Clamp(Loc.X, -950.0f, -30.0f); }
		Loc.Y = FMath::Clamp(Loc.Y, -500.0f, 500.0f);
	}
	if (Loc != GetActorLocation()) { SetActorLocation(Loc, true); }

	// ---- Touch: only the primary handler, and only via the GameMode ----
	if (bIsPrimaryHandler)
	{
		// M11c-3: begin the dive lunge when close to the save point, then enter
		// the extended-reach Active window when really close. The window persists
		// for 0.45 s (DiveState) instead of one frame, so the dive pose and reach
		// bonus are actually visible and usable.
		if (AIBehavior == EAIBehavior::Dive && DiveState.Phase == SEVolleyballRules::FVolleyballDiveState::EPhase::None)
		{
			DiveState.StartDive();
			UE_LOG(LogVolleyballRules, Log, TEXT("[DiveAttempt] %s starts dive approach"), *GetName());
		}
		if (AIBehavior == EAIBehavior::Dive && DiveState.Phase == SEVolleyballRules::FVolleyballDiveState::EPhase::Approach && Dist < 90.f)
		{
			DiveState.EnterActive();
			UE_LOG(LogVolleyballRules, Log, TEXT("[DiveActive] %s enters contact window (reach +90cm)"), *GetName());
		}
		// M11b-5: a blocker asks for a block (front-row gate inside), everyone
		// else uses the normal touch path. TryTouchBall / TryBlockBall do the
		// reach/phase/rules checks; cheap per frame.
		if (AIBehavior == EAIBehavior::MoveToBlock)
		{
			GM->TryBlockBall(this);
		}
		else
		{
			GM->TryTouchBall(this, EBallTouchType::Unknown);
		}
	}
}

void ASpikeEliteCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ASpikeEliteCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASpikeEliteCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ASpikeEliteCharacter::TurnRate);
	PlayerInputComponent->BindAxis("LookUp", this, &ASpikeEliteCharacter::LookUpRate);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("ToggleFirstPerson", IE_Pressed, this, &ASpikeEliteCharacter::ToggleFirstPerson);
	PlayerInputComponent->BindAction("HitBall", IE_Pressed, this, &ASpikeEliteCharacter::HitBall);
	PlayerInputComponent->BindAction("ServeBall", IE_Pressed, this, &ASpikeEliteCharacter::ServeBall);
	PlayerInputComponent->BindAction("RaiseHands", IE_Pressed, this, &ASpikeEliteCharacter::StartRaiseHands);
	PlayerInputComponent->BindAction("RaiseHands", IE_Released, this, &ASpikeEliteCharacter::StopRaiseHands);
}

void ASpikeEliteCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ASpikeEliteCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void ASpikeEliteCharacter::TurnRate(float Value)
{
	if (Controller)
	{
		AddControllerYawInput(Value * LookSensitivity * 100.f * GetWorld()->GetDeltaSeconds());
	}
}

void ASpikeEliteCharacter::LookUpRate(float Value)
{
	if (Controller)
	{
		AddControllerPitchInput(Value * LookSensitivity * 100.f * GetWorld()->GetDeltaSeconds());
	}
}

void ASpikeEliteCharacter::ToggleFirstPerson()
{
	if (bIsBot) return;
	bFirstPerson = !bFirstPerson;
	UpdateCameraView();
}

void ASpikeEliteCharacter::UpdateCameraView()
{
	if (bIsBot) return;
	if (bFirstPerson)
	{
		if (ThirdPersonCamera) ThirdPersonCamera->SetActive(false);
		if (FirstPersonCamera) FirstPersonCamera->SetActive(true);
		// M11b-3: hide our own head in first person so it cannot occlude the view.
		if (Head) Head->SetVisibility(false);
		for (UStaticMeshComponent* C : ArtDetails) { if (C && C->ComponentHasTag(TEXT("FaceDetail"))) { C->SetVisibility(false); } }
	}
	else
	{
		if (ThirdPersonCamera) ThirdPersonCamera->SetActive(true);
		if (FirstPersonCamera) FirstPersonCamera->SetActive(false);
		if (Head) Head->SetVisibility(true);
		for (UStaticMeshComponent* C : ArtDetails) { if (C && C->ComponentHasTag(TEXT("FaceDetail"))) { C->SetVisibility(true); } }
		// M11f-2: keep the third-person framing (slight downward tilt) whenever we
		// switch back — the court centre and net stay in frame, not the sky.
		if (ThirdPersonCamera) ThirdPersonCamera->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f));
	}
}

void ASpikeEliteCharacter::HitBall()
{
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->TryTouchBall(this, EBallTouchType::Unknown);
	}
}

void ASpikeEliteCharacter::ServeBall()
{
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		// M11h-3: E during the server-intro card skips the presentation (to the
		// whistle), it does NOT serve — the intro never authorizes an early serve.
		if (GM->MatchState == EMatchState::ServePresentation)
		{
			GM->SkipServePresentation();
			return;
		}
		GM->RequestServe(this);
	}
}

void ASpikeEliteCharacter::StartRaiseHands()
{
	RaiseHandsAmount = FMath::Clamp(RaiseHandsAmount + 1.f, 0.f, 1.f);
}

void ASpikeEliteCharacter::StopRaiseHands()
{
	RaiseHandsAmount = 0.f;
}

void ASpikeEliteCharacter::NotifyContact(EBallTouchType Type)
{
	LastContactType = Type;
	ContactPoseTimer = (Type == EBallTouchType::Attack) ? 0.5f : 0.45f;
}

// ---------------- M11b-3: procedural animation ----------------

void ASpikeEliteCharacter::UpdateProceduralAnimation(float DeltaSeconds)
{
	// M11c-3: dive lifecycle is driven by the shared FVolleyballDiveState.
	// Tick advances the Active window (timeout -> Miss -> Recovery) and the
	// Recovery timer (end -> None). A save is recorded by the GameMode when a
	// real touch lands (RecordSave), which ends Active immediately.
	if (DiveState.Phase != SEVolleyballRules::FVolleyballDiveState::EPhase::None)
	{
		const bool bTransitioned = DiveState.Tick(DeltaSeconds);
		if (bTransitioned)
		{
			if (DiveState.IsRecovering())
			{
				UE_LOG(LogVolleyballRules, Log, TEXT("[DiveMiss] %s missed (active window expired) -> recovery"), *GetName());
			}
			else
			{
				UE_LOG(LogVolleyballRules, Log, TEXT("[DiveRecoveryEnd] %s recovery finished"), *GetName());
			}
		}
	}

	if (ContactPoseTimer > 0.f)
	{
		ContactPoseTimer -= DeltaSeconds;
		if (ContactPoseTimer < 0.f) ContactPoseTimer = 0.f;
	}

	// Walk/run cycle phase advances with speed.
	const float Speed = AnimationMoveSpeed;
	const float SpeedRatio = FMath::Clamp(Speed / 450.f, 0.f, 1.f);
	if (Speed > 30.f)
	{
		RunPhase += DeltaSeconds * (8.0f + 7.0f * SpeedRatio);
	}
	else
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (!bDevPoseOverride)
#endif
		RunPhase *= 0.85f;
	}
	EAnimPose Pose = ResolvePose(DeltaSeconds);
#if WITH_DEV_AUTOMATION_TESTS
	if (bDevPoseOverride) { Pose = DevPose; }
#endif
	CurrentPose = Pose;
	ApplyPose(Pose, DeltaSeconds);
}

#if WITH_DEV_AUTOMATION_TESTS
void ASpikeEliteCharacter::DevSetPoseOverride(EAnimPose Pose, bool bEnable)
{
	bDevPoseOverride = bEnable;
	DevPose = Pose;
	CurrentPose = bEnable ? Pose : CurrentPose;
}

void ASpikeEliteCharacter::DevSetRunPhase(float Phase)
{
	RunPhase = Phase;
}
#endif

EAnimPose ASpikeEliteCharacter::ResolvePose(float DeltaSeconds)
{
	const bool bAirborne = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	const float Speed = AnimationMoveSpeed;

	// Contact poses (short window after a real touch) beat locomotion.
	if (ContactPoseTimer > 0.f)
	{
		switch (LastContactType)
		{
		case EBallTouchType::Set:    return EAnimPose::Set;
		case EBallTouchType::Attack: return EAnimPose::Spike;
		case EBallTouchType::Serve:  return EAnimPose::Serve;
		case EBallTouchType::Receive:
		default:                     return EAnimPose::Receive;
		}
	}

	// M11c-3: dive states drive the pose — Approach (low lunge run) and Active
	// (extended-reach contact window) both show the dive lunge, Recovery shows
	// the crouched recover. Pose is resolved BEFORE the state tick below so a
	// transition this frame still renders the new pose (no one-frame flash).
	if (DiveState.IsActive() || DiveState.Phase == SEVolleyballRules::FVolleyballDiveState::EPhase::Approach)
	{
		return EAnimPose::Dive;
	}
	if (DiveState.IsRecovering()) return EAnimPose::Recover;

	// Raise hands: overhead while airborne (block prep), high otherwise.
	if (RaiseHandsAmount > 0.05f)
	{
		return bAirborne ? EAnimPose::Block : EAnimPose::RaiseHands;
	}

	if (bAirborne) return EAnimPose::Jump;
	if (Speed > 30.f) return EAnimPose::Run;
	return EAnimPose::Idle;
}

void ASpikeEliteCharacter::ApplyPose(EAnimPose Pose, float DeltaSeconds)
{
	// Smoothing rate so poses never snap (walks back to idle naturally).
	const float Blend = FMath::Clamp(DeltaSeconds * 10.f, 0.f, 1.f);
	const float Slow = FMath::Clamp(DeltaSeconds * 6.f, 0.f, 1.f);

	auto Lerp = [Blend](USceneComponent* Comp, const FRotator& Target)
	{
		if (!Comp) return;
		// Euler pitch wraps at +/-90. Interpolate the actual quaternion so
		// overhead raises do not oscillate sideways at the wrap boundary.
		Comp->SetRelativeRotation(FQuat::Slerp(Comp->GetRelativeTransform().GetRotation(),
			Target.Quaternion(), Blend).GetNormalized());
	};
	// M11f-2: the torso joint also has a per-pose HEIGHT (the visual root of the
	// whole body). Dive lowers it toward the floor and shifts it forward (real
	// grounded lunge); Recover holds a low crouch; Receive sits slightly lower;
	// everything else stands at the default height. This is what stops the body
	// from floating mid-air in dive/recover poses.
	auto LerpLoc = [Blend](USceneComponent* Comp, const FVector& Target)
	{
		if (!Comp) return;
		Comp->SetRelativeLocation(FMath::VInterpTo(Comp->GetRelativeLocation(), Target, 1.f, Blend * 8.f));
	};

	const FVector StandHeight(0.f, 0.f, 20.f);
	const FVector DiveHeight(18.f, 0.f, -62.f);
	const FVector ReceiveHeight(0.f, 0.f, 8.f);

	// Base rest.
	const FRotator Rest(0.f, 0.f, 0.f);
	const FRotator TorsoRest(0.f, 0.f, 0.f);
	const FRotator ShoulderRest(0.f, 0.f, 0.f);
	const FRotator ElbowRest(0.f, 0.f, 0.f);
	const FRotator HipRest(0.f, 0.f, 0.f);
	const FRotator KneeRest(0.f, 0.f, 0.f);

	FRotator T = TorsoRest;
	FRotator SL = ShoulderRest, SR = ShoulderRest;
	FRotator EL = ElbowRest, ER = ElbowRest;
	FRotator HL = HipRest, HR = HipRest;
	FRotator KL = KneeRest, KR = KneeRest;
	FVector TorsoLoc = StandHeight;

	switch (Pose)
	{
	case EAnimPose::Run:
	{
		// UE is left-handed: POSITIVE pitch swings a hanging limb toward +X
		// (forward/up), negative toward -X (back/up). Legs alternate — the lead
		// leg swings forward (+), the trail leg back (-), knees bend forward.
		const float Swing = FMath::Sin(RunPhase) * 26.f;
		const float SwingO = FMath::Sin(RunPhase + PI) * 26.f;
		const float KneeBend = 15.f + 45.f * FMath::Max(0.f,-FMath::Sin(RunPhase));
		HL = FRotator(Swing, 0.f, 0.f);
		HR = FRotator(SwingO, 0.f, 0.f);
		KL = FRotator(-KneeBend, 0.f, 0.f);
		KR = FRotator(-15.f-45.f*FMath::Max(0.f,FMath::Sin(RunPhase)), 0.f, 0.f);
		SL = FRotator(SwingO * 0.8f, 0.f, 8.f);    // opposite arm swing
		SR = FRotator(Swing * 0.8f, 0.f, -8.f);
		EL = FRotator(60.f, 0.f, 0.f);
		ER = FRotator(60.f, 0.f, 0.f);
		T = FRotator(-8.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Jump:
	{
		// Tucked jump: thighs lift forward, knees fold, arms rise slightly.
		HL = FRotator(18.f, 0.f, 0.f);
		HR = FRotator(18.f, 0.f, 0.f);
		KL = FRotator(-65.f, 0.f, 0.f);
		KR = FRotator(-65.f, 0.f, 0.f);
		SL = FRotator(12.f, 0.f, 10.f);
		SR = FRotator(12.f, 0.f, -10.f);
		EL = FRotator(25.f, 0.f, 0.f);
		ER = FRotator(25.f, 0.f, 0.f);
		T = FRotator(8.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Receive:
	{
		// Platform receive: upper arms forward-down, forearms folded back into a
		// flat platform, knees bent, torso leaning forward.
		SL = FRotator(95.f, 0.f, -12.f);
		SR = FRotator(95.f, 0.f, 12.f);
		EL = FRotator(0.f, 0.f, 0.f);
		ER = FRotator(0.f, 0.f, 0.f);
		HL = FRotator(40.f, 0.f, 0.f);
		HR = FRotator(40.f, 0.f, 0.f);
		KL = FRotator(-55.f, 0.f, 0.f);
		KR = FRotator(-55.f, 0.f, 0.f);
		T = FRotator(-18.f, 0.f, 0.f);
		TorsoLoc = ReceiveHeight;
		break;
	}
	case EAnimPose::Set:
	{
		// Overhead set: upper arms fully raised (+180, vertical), forearms nearly
		// straight (measured 185 with 176/-20 — the straight vertical raise puts
		// the hands clearly above the head top, target handZ ≥ 205).
		SL = FRotator(180.f, 0.f, 0.f);
		SR = FRotator(180.f, 0.f, 0.f);
		EL = FRotator(-10.f, 0.f, 0.f);
		ER = FRotator(-10.f, 0.f, 0.f);
		HL = FRotator(-8.f, 0.f, 0.f);
		HR = FRotator(-8.f, 0.f, 0.f);
		KL = FRotator(-12.f, 0.f, 0.f);
		KR = FRotator(-12.f, 0.f, 0.f);
		T = FRotator(-5.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Spike:
	{
		// Wind-up (back/up) -> swing (forward/up) -> follow-through.
		const float Stage = FMath::Clamp(1.f - ContactPoseTimer / 0.5f, 0.f, 1.f);
		if (Stage < 0.3f)
		{
			SR = FRotator(-110.f, 0.f, 0.f);      // wind-up: right arm back/up
			ER = FRotator(-30.f, 0.f, 0.f);
			SL = FRotator(50.f, 0.f, 0.f);        // left arm forward guard
			EL = FRotator(-30.f, 0.f, 0.f);
		}
		else if (Stage < 0.6f)
		{
			SR = FRotator(150.f, 0.f, 0.f);       // swing: right arm forward/up
			ER = FRotator(-50.f, 0.f, 0.f);
			SL = FRotator(40.f, 0.f, 0.f);
			EL = FRotator(-20.f, 0.f, 0.f);
		}
		else
		{
			SR = FRotator(70.f, 0.f, 0.f);        // follow-through
			ER = FRotator(-25.f, 0.f, 0.f);
			SL = FRotator(30.f, 0.f, 0.f);
			EL = FRotator(-15.f, 0.f, 0.f);
		}
		HL = FRotator(-15.f, 0.f, 0.f);
		HR = FRotator(-15.f, 0.f, 0.f);
		KL = FRotator(45.f, 0.f, 0.f);
		KR = FRotator(45.f, 0.f, 0.f);
		T = FRotator(18.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Block:
	{
		// Both hands straight up FORWARD-overhead. +178 is essentially vertical
		// (hands measure ≈head+10); the old 168 left the hands 5cm BELOW the top
		// of the head, so the block read as "arms up" but not "hands over head".
		SL = FRotator(178.f, 0.f, 0.f);
		SR = FRotator(178.f, 0.f, 0.f);
		EL = FRotator(3.f, 0.f, 0.f);
		ER = FRotator(3.f, 0.f, 0.f);
		HL = FRotator(-15.f, 0.f, 0.f);
		HR = FRotator(-15.f, 0.f, 0.f);
		KL = FRotator(-15.f, 0.f, 0.f);
		KR = FRotator(-15.f, 0.f, 0.f);
		T = FRotator(-2.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Dive:
	{
		// M11f-2: grounded forward dive — torso nearly horizontal and LOW, arms
		// reaching forward-down toward the floor (positive pitch), legs stretched
		// back/up (negative pitch). The torso joint sinks 42cm and shifts forward
		// so the body never floats.
		SL = FRotator(150.f, 0.f, 0.f);
		SR = FRotator(150.f, 0.f, 0.f);
		EL = FRotator(12.f, 0.f, 0.f);
		ER = FRotator(12.f, 0.f, 0.f);
		HL = FRotator(-12.f, 0.f, 0.f);
		HR = FRotator(-12.f, 0.f, 0.f);
		KL = FRotator(0.f, 0.f, 0.f);
		KR = FRotator(0.f, 0.f, 0.f);
		T = FRotator(-78.f, 0.f, 0.f);
		TorsoLoc = DiveHeight;
		break;
	}
	case EAnimPose::Recover:
	{
		// Low crouch: hips sink, thighs come slightly forward, knees deep —
		// a grounded "get up" pose, feet planted under the hips, torso near
		// upright (the old 35/45 combo read as a backward lean).
		SL = FRotator(45.f, 0.f, 0.f);
		SR = FRotator(45.f, 0.f, 0.f);
		EL = FRotator(-35.f, 0.f, 0.f);
		ER = FRotator(-35.f, 0.f, 0.f);
		HL = FRotator(60.f, 0.f, 0.f);
		HR = FRotator(60.f, 0.f, 0.f);
		KL = FRotator(-100.f, 0.f, 0.f);
		KR = FRotator(-100.f, 0.f, 0.f);
		T = FRotator(-15.f, 0.f, 0.f);
		TorsoLoc = FVector(0.f, 0.f, -18.f);
		break;
	}
	case EAnimPose::Serve:
	{
		// Right arm high/back for the serve swing (negative pitch = back/up),
		// weight shift forward.
		SR = FRotator(-120.f, 0.f, 0.f);
		ER = FRotator(70.f, 0.f, 0.f);
		SL = FRotator(10.f, 0.f, 0.f);
		EL = FRotator(-15.f, 0.f, 0.f);
		HL = FRotator(-8.f, 0.f, 0.f);
		HR = FRotator(4.f, 0.f, 0.f);
		KL = FRotator(20.f, 0.f, 0.f);
		KR = FRotator(25.f, 0.f, 0.f);
		T = FRotator(10.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::RaiseHands:
	{
		// Continuous arm raise driven by RaiseHandsAmount (0 = flat forward,
		// 1 = straight up overhead), positive pitch up/forward.
		const float A = RaiseHandsAmount;
		SL = FRotator(90.f + 80.f * A, 0.f, 0.f);
		SR = FRotator(90.f + 80.f * A, 0.f, 0.f);
		EL = FRotator(25.f * (1.f - A), 0.f, 0.f);
		ER = FRotator(25.f * (1.f - A), 0.f, 0.f);
		HL = FRotator(-6.f * A, 0.f, 0.f);
		HR = FRotator(-6.f * A, 0.f, 0.f);
		KL = FRotator(20.f * A, 0.f, 0.f);
		KR = FRotator(20.f * A, 0.f, 0.f);
		T = FRotator(10.f * A, 0.f, 0.f);
		break;
	}
	case EAnimPose::Idle:
	default:
	{
		// Natural standing with a subtle breathing sway.
		const float Breath = FMath::Sin(GetWorld()->GetTimeSeconds() * 2.2f) * 1.5f;
		HL = FRotator(Breath, 0.f, 0.f);
		HR = FRotator(-Breath, 0.f, 0.f);
		SL = FRotator(Breath, 0.f, 4.f);
		SR = FRotator(-Breath, 0.f, -4.f);
		EL = FRotator(3.f, 0.f, 0.f);
		ER = FRotator(3.f, 0.f, 0.f);
		T = FRotator(Breath * 0.5f, 0.f, 0.f);
		break;
	}
	}

	if (TorsoJoint) LerpLoc(TorsoJoint, TorsoLoc);
	if (TorsoJoint) Lerp(TorsoJoint, T);
	if (ArmL.Joint) Lerp(ArmL.Joint, SL);
	if (ArmR.Joint) Lerp(ArmR.Joint, SR);
	if (ArmL.BendJoint) Lerp(ArmL.BendJoint, EL);
	if (ArmR.BendJoint) Lerp(ArmR.BendJoint, ER);
	if (LegL.Joint) Lerp(LegL.Joint, HL);
	if (LegR.Joint) Lerp(LegR.Joint, HR);
	if (LegL.BendJoint) Lerp(LegL.BendJoint, KL);
	if (LegR.BendJoint) Lerp(LegR.BendJoint, KR);
	// Plant the lower foot of grounded poses at the capsule's walking plane.
	// This shifts only the visual joint: collision/reach/rules are unchanged.
	if (TorsoJoint && Pose != EAnimPose::Dive && Pose != EAnimPose::Jump
		&& GetCharacterMovement() && !GetCharacterMovement()->IsFalling() && LegL.Tip && LegR.Tip)
	{
		const float FootMin = FMath::Min(LegL.Tip->CalcBounds(LegL.Tip->GetComponentTransform()).GetBox().Min.Z,
			LegR.Tip->CalcBounds(LegR.Tip->GetComponentTransform()).GetBox().Min.Z);
		const float Floor = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector Location = TorsoJoint->GetRelativeLocation();
		Location.Z += FMath::Clamp(Floor - FootMin, -85.f, 45.f);
		TorsoJoint->SetRelativeLocation(Location);
	}
}

void ASpikeEliteCharacter::SetPoseIdle()
{
	if (TorsoJoint)
	{
		TorsoJoint->SetRelativeRotation(FRotator::ZeroRotator);
		TorsoJoint->SetRelativeLocation(FVector(0.f, 0.f, 20.f));   // M11f-2: restore standing height
	}
	if (ArmL.Joint) ArmL.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmR.Joint) ArmR.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmL.BendJoint) ArmL.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmR.BendJoint) ArmR.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegL.Joint) LegL.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegR.Joint) LegR.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegL.BendJoint) LegL.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegR.BendJoint) LegR.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
}
