// SPDX-License-Identifier: MIT
#include "SpikeEliteCharacter.h"
#include "SpikeEliteGameMode.h"
#include "SEMaterials.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	Torso = MakeMesh(TEXT("Torso"), TorsoJoint, FVector(0.46f, 0.28f, 0.66f), FVector(0.f, 0.f, 30.f));
	Head = MakeMesh(TEXT("Head"), TorsoJoint, FVector(0.22f, 0.22f, 0.22f), FVector(0.f, 0.f, 78.f));
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);
	}

	// Limbs. Shoulder/hip joint at the root of each limb; upper segment hangs
	// down, bend joint at its bottom, lower segment, tip (hand/foot) last.
	auto BuildLimb = [&](const TCHAR* Base, USceneComponent* Root, float YSide, bool bArm)
	{
		FProceduralLimb L;
		const FVector JointLoc(0.f, YSide, bArm ? 62.f : -38.f);
		L.Joint = MakeJoint(*FString::Printf(TEXT("%sJoint"), Base), Root, JointLoc);
		const float UpLen = bArm ? 27.f : 24.f;
		const float LoLen = bArm ? 24.f : 23.f;
		L.Upper = MakeMesh(*FString::Printf(TEXT("%sUpper"), Base), L.Joint,
			bArm ? FVector(0.10f, 0.10f, UpLen * 0.01f) : FVector(0.14f, 0.14f, UpLen * 0.01f),
			FVector(0.f, 0.f, -UpLen * 0.5f));
		L.BendJoint = MakeJoint(*FString::Printf(TEXT("%sBend"), Base), L.Upper, FVector(0.f, 0.f, -UpLen * 0.5f));
		L.Lower = MakeMesh(*FString::Printf(TEXT("%sLower"), Base), L.BendJoint,
			bArm ? FVector(0.08f, 0.08f, LoLen * 0.01f) : FVector(0.11f, 0.11f, LoLen * 0.01f),
			FVector(0.f, 0.f, -LoLen * 0.5f));
		L.Tip = MakeMesh(*FString::Printf(TEXT("%sTip"), Base), L.Lower,
			bArm ? FVector(0.10f, 0.12f, 0.05f) : FVector(0.14f, 0.20f, 0.07f),
			FVector(0.f, bArm ? 0.f : 2.f, -LoLen - (bArm ? 2.f : 3.f)));
		return L;
	};

	ArmL = BuildLimb(TEXT("ArmL"), TorsoJoint, -24.f, true);
	ArmR = BuildLimb(TEXT("ArmR"), TorsoJoint,  24.f, true);
	LegL = BuildLimb(TEXT("LegL"), RootComponent, -10.f, false);
	LegR = BuildLimb(TEXT("LegR"), RootComponent,  10.f, false);

	GetCapsuleComponent()->SetCapsuleHalfHeight(84.f);
	GetCapsuleComponent()->SetCapsuleRadius(32.f);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;   // face move direction (good for bots)
	MoveComp->JumpZVelocity = 520.0f;
	MoveComp->AirControl = 0.5f;
	MoveComp->MaxWalkSpeed = 450.0f;

	// M10 camera pass: longer arm, raised + shoulder offset, collision tests and
	// a slight lag so the ball at court centre is not hidden behind the body.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 35.f);  // shoulder offset
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

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 10.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void ASpikeEliteCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyJerseyColor();
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
	const FLinearColor Skin(0.82f, 0.64f, 0.48f, 1.0f);

	auto Tint = [&](UStaticMeshComponent* Comp, const FLinearColor& Col)
	{
		if (!Comp) return;
		if (UMaterialInstanceDynamic* MID = SEMaterials::MakeTint(this, Col))
		{
			Comp->SetMaterial(0, MID);
		}
	};

	Tint(Torso, Jersey);
	Tint(ArmL.Upper, Jersey);
	Tint(ArmR.Upper, Jersey);
	Tint(ArmL.Lower, Jersey);
	Tint(ArmR.Lower, Jersey);
	Tint(ArmL.Tip, Skin);
	Tint(ArmR.Tip, Skin);
	Tint(LegL.Upper, Shorts);
	Tint(LegR.Upper, Shorts);
	Tint(LegL.Lower, Skin);
	Tint(LegR.Lower, Skin);
	Tint(LegL.Tip, Shorts);
	Tint(LegR.Tip, Shorts);
	Tint(Head, Skin);
}

void ASpikeEliteCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// M11b-3: procedural animation runs for bots and the human alike.
	UpdateProceduralAnimation(DeltaSeconds);

	if (bIsBot)
	{
		TickBot(DeltaSeconds);
		return;
	}

	// Human player boundary.
	FVector Loc = GetActorLocation();
	const float MaxX = 950.0f;
	const float MaxY = 500.0f;
	bool bClamped = false;
	if (Loc.X < 50.0f)   { Loc.X = 50.0f;   bClamped = true; }
	if (Loc.X > MaxX)    { Loc.X = MaxX;    bClamped = true; }
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
		NewLoc.X = (TeamSide > 0) ? FMath::Clamp(NewLoc.X, 30.f, 950.f) : FMath::Clamp(NewLoc.X, -950.f, -30.f);
		NewLoc.Y = FMath::Clamp(NewLoc.Y, -500.f, 500.f);
		SetActorLocation(NewLoc, true);
	}

	// ---- Boundary: stay on own half, don't run out ----
	FVector Loc = GetActorLocation();
	if (TeamSide > 0) { Loc.X = FMath::Clamp(Loc.X, 30.0f, 950.0f); }
	else              { Loc.X = FMath::Clamp(Loc.X, -950.0f, -30.0f); }
	Loc.Y = FMath::Clamp(Loc.Y, -500.0f, 500.0f);
	if (Loc != GetActorLocation()) { SetActorLocation(Loc, true); }

	// ---- Touch: only the primary handler, and only via the GameMode ----
	if (bIsPrimaryHandler)
	{
		// M11b-5c: begin the dive lunge when close to the save point.
		if (AIBehavior == EAIBehavior::Dive && !bDiving && !bDiveRecovering && Dist < 150.f)
		{
			bDiving = true;
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
	}
	else
	{
		if (ThirdPersonCamera) ThirdPersonCamera->SetActive(true);
		if (FirstPersonCamera) FirstPersonCamera->SetActive(false);
		if (Head) Head->SetVisibility(true);
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
	// Dive recovery timing (dive itself is triggered by the GameMode later).
	if (bDiving)
	{
		DiveRecoveryTimer = 0.8f;   // short lunge window
		bDiving = false;
		bDiveRecovering = true;
	}
	if (bDiveRecovering)
	{
		DiveRecoveryTimer -= DeltaSeconds;
		if (DiveRecoveryTimer <= 0.f)
		{
			bDiveRecovering = false;
		}
	}

	if (ContactPoseTimer > 0.f)
	{
		ContactPoseTimer -= DeltaSeconds;
		if (ContactPoseTimer < 0.f) ContactPoseTimer = 0.f;
	}

	// Walk/run cycle phase advances with speed.
	const float Speed = GetVelocity().Size2D();
	const float SpeedRatio = FMath::Clamp(Speed / 450.f, 0.f, 1.f);
	if (Speed > 30.f)
	{
		RunPhase += DeltaSeconds * (8.0f + 7.0f * SpeedRatio);
	}
	else
	{
		RunPhase *= 0.85f;
	}

	const EAnimPose Pose = ResolvePose(DeltaSeconds);
	CurrentPose = Pose;
	ApplyPose(Pose, DeltaSeconds);
}

EAnimPose ASpikeEliteCharacter::ResolvePose(float DeltaSeconds)
{
	const bool bAirborne = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	const float Speed = GetVelocity().Size2D();

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

	if (bDiveRecovering) return EAnimPose::Recover;

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
		Comp->SetRelativeRotation(FMath::RInterpTo(Comp->GetRelativeRotation(), Target, 1.f, Blend * 8.f));
	};

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

	switch (Pose)
	{
	case EAnimPose::Run:
	{
		const float Swing = FMath::Sin(RunPhase) * 26.f;
		const float SwingO = FMath::Sin(RunPhase + PI) * 26.f;
		const float KneeBend = 45.f + 30.f * (0.5f + 0.5f * FMath::Sin(RunPhase));
		HL = FRotator(-Swing, 0.f, 0.f);           // legs alternate
		HR = FRotator(-SwingO, 0.f, 0.f);
		KL = FRotator(KneeBend, 0.f, 0.f);
		KR = FRotator(KneeBend, 0.f, 0.f);
		SL = FRotator(SwingO * 0.8f, 0.f, 8.f);    // opposite arm swing
		SR = FRotator(Swing * 0.8f, 0.f, -8.f);
		EL = FRotator(60.f, 0.f, 0.f);
		ER = FRotator(60.f, 0.f, 0.f);
		T = FRotator(12.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Jump:
	{
		HL = FRotator(-18.f, 0.f, 0.f);
		HR = FRotator(-18.f, 0.f, 0.f);
		KL = FRotator(65.f, 0.f, 0.f);
		KR = FRotator(65.f, 0.f, 0.f);
		SL = FRotator(12.f, 0.f, 10.f);
		SR = FRotator(12.f, 0.f, -10.f);
		EL = FRotator(25.f, 0.f, 0.f);
		ER = FRotator(25.f, 0.f, 0.f);
		T = FRotator(8.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Receive:
	{
		// Arms pressed forward-down (platform receive), knees bent, torso leaning.
		SL = FRotator(-55.f, 0.f, 0.f);
		SR = FRotator(-55.f, 0.f, 0.f);
		EL = FRotator(-20.f, 0.f, 0.f);
		ER = FRotator(-20.f, 0.f, 0.f);
		HL = FRotator(8.f, 0.f, 0.f);
		HR = FRotator(8.f, 0.f, 0.f);
		KL = FRotator(50.f, 0.f, 0.f);
		KR = FRotator(50.f, 0.f, 0.f);
		T = FRotator(25.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Set:
	{
		// Both hands raised to forehead height, elbows bent.
		SL = FRotator(-150.f, 0.f, 0.f);
		SR = FRotator(-150.f, 0.f, 0.f);
		EL = FRotator(-85.f, 0.f, 0.f);
		ER = FRotator(-85.f, 0.f, 0.f);
		HL = FRotator(-8.f, 0.f, 0.f);
		HR = FRotator(-8.f, 0.f, 0.f);
		KL = FRotator(28.f, 0.f, 0.f);
		KR = FRotator(28.f, 0.f, 0.f);
		T = FRotator(6.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Spike:
	{
		// Wind-up (back/up) -> swing (forward/up) -> follow-through.
		const float Stage = FMath::Clamp(1.f - ContactPoseTimer / 0.5f, 0.f, 1.f);
		if (Stage < 0.3f)
		{
			SR = FRotator(110.f, 0.f, 0.f);      // wind-up: right arm back/up
			ER = FRotator(-100.f, 0.f, 0.f);
			SL = FRotator(-50.f, 0.f, 0.f);      // left arm forward guard
			EL = FRotator(-30.f, 0.f, 0.f);
		}
		else if (Stage < 0.6f)
		{
			SR = FRotator(-150.f, 0.f, 0.f);     // swing: right arm forward/up
			ER = FRotator(-50.f, 0.f, 0.f);
			SL = FRotator(-40.f, 0.f, 0.f);
			EL = FRotator(-20.f, 0.f, 0.f);
		}
		else
		{
			SR = FRotator(-70.f, 0.f, 0.f);      // follow-through
			ER = FRotator(-25.f, 0.f, 0.f);
			SL = FRotator(-30.f, 0.f, 0.f);
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
		// Both hands straight up overhead.
		SL = FRotator(-175.f, 0.f, 0.f);
		SR = FRotator(-175.f, 0.f, 0.f);
		EL = FRotator(5.f, 0.f, 0.f);
		ER = FRotator(5.f, 0.f, 0.f);
		HL = FRotator(-15.f, 0.f, 0.f);
		HR = FRotator(-15.f, 0.f, 0.f);
		KL = FRotator(55.f, 0.f, 0.f);
		KR = FRotator(55.f, 0.f, 0.f);
		T = FRotator(10.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Dive:
	{
		// Forward lunge: torso down, arms forward, legs trailing.
		SL = FRotator(-85.f, 0.f, 0.f);
		SR = FRotator(-85.f, 0.f, 0.f);
		EL = FRotator(-10.f, 0.f, 0.f);
		ER = FRotator(-10.f, 0.f, 0.f);
		HL = FRotator(35.f, 0.f, 0.f);
		HR = FRotator(35.f, 0.f, 0.f);
		KL = FRotator(10.f, 0.f, 0.f);
		KR = FRotator(10.f, 0.f, 0.f);
		T = FRotator(50.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Recover:
	{
		// Low crouch, arms forward-down for balance.
		SL = FRotator(-40.f, 0.f, 0.f);
		SR = FRotator(-40.f, 0.f, 0.f);
		EL = FRotator(-20.f, 0.f, 0.f);
		ER = FRotator(-20.f, 0.f, 0.f);
		HL = FRotator(5.f, 0.f, 0.f);
		HR = FRotator(5.f, 0.f, 0.f);
		KL = FRotator(85.f, 0.f, 0.f);
		KR = FRotator(85.f, 0.f, 0.f);
		T = FRotator(22.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::Serve:
	{
		// Right arm high/back for the serve swing, weight shift.
		SR = FRotator(120.f, 0.f, 0.f);
		ER = FRotator(-70.f, 0.f, 0.f);
		SL = FRotator(-10.f, 0.f, 0.f);
		EL = FRotator(15.f, 0.f, 0.f);
		HL = FRotator(-8.f, 0.f, 0.f);
		HR = FRotator(4.f, 0.f, 0.f);
		KL = FRotator(20.f, 0.f, 0.f);
		KR = FRotator(25.f, 0.f, 0.f);
		T = FRotator(10.f, 0.f, 0.f);
		break;
	}
	case EAnimPose::RaiseHands:
	{
		// Continuous arm raise driven by RaiseHandsAmount (0 = flat, 1 = high).
		const float A = RaiseHandsAmount;
		SL = FRotator(-90.f - 80.f * A, 0.f, 0.f);
		SR = FRotator(-90.f - 80.f * A, 0.f, 0.f);
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

	if (TorsoJoint) Lerp(TorsoJoint, T);
	if (ArmL.Joint) Lerp(ArmL.Joint, SL);
	if (ArmR.Joint) Lerp(ArmR.Joint, SR);
	if (ArmL.BendJoint) Lerp(ArmL.BendJoint, EL);
	if (ArmR.BendJoint) Lerp(ArmR.BendJoint, ER);
	if (LegL.Joint) Lerp(LegL.Joint, HL);
	if (LegR.Joint) Lerp(LegR.Joint, HR);
	if (LegL.BendJoint) Lerp(LegL.BendJoint, KL);
	if (LegR.BendJoint) Lerp(LegR.BendJoint, KR);
}

void ASpikeEliteCharacter::SetPoseIdle()
{
	if (TorsoJoint) TorsoJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmL.Joint) ArmL.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmR.Joint) ArmR.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmL.BendJoint) ArmL.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (ArmR.BendJoint) ArmR.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegL.Joint) LegL.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegR.Joint) LegR.Joint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegL.BendJoint) LegL.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
	if (LegR.BendJoint) LegR.BendJoint->SetRelativeRotation(FRotator::ZeroRotator);
}
