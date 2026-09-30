// SPDX-License-Identifier: MIT
#include "SpikeEliteCharacter.h"
#include "SpikeEliteGameMode.h"
#include "SpikeElitePlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Volleyball/VolleyballBall.h"
#include "SEMaterials.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ASpikeEliteCharacter::ASpikeEliteCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Hinge-style placeholder humanoid (per design doc: simple articulated stand-in
	// until proper rigged models are imported). Built from engine basic shapes with
	// a normal human proportion: torso, head, two arms, two legs. Zero asset deps.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	auto MakeBox = [this](const TCHAR* N, const FVector& Scale, const FVector& Loc) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(N);
		C->SetupAttachment(RootComponent);
		if (CubeMesh.Succeeded()) C->SetStaticMesh(CubeMesh.Object);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Loc);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // capsule is the only collider
		return C;
	};

	// Legs (16x16x86 cm), hips to feet.
	LegL = MakeBox(TEXT("LegL"), FVector(0.16f, 0.16f, 0.86f), FVector(0.f, -11.f, -48.f));
	LegR = MakeBox(TEXT("LegR"), FVector(0.16f, 0.16f, 0.86f), FVector(0.f,  11.f, -48.f));
	// Torso (46x26x66 cm).
	Torso = MakeBox(TEXT("Torso"), FVector(0.46f, 0.26f, 0.66f), FVector(0.f, 0.f, 16.f));
	// Arms (13x13x60 cm) at the sides of the torso.
	ArmL = MakeBox(TEXT("ArmL"), FVector(0.13f, 0.13f, 0.60f), FVector(0.f, -23.f, 12.f));
	ArmR = MakeBox(TEXT("ArmR"), FVector(0.13f, 0.13f, 0.60f), FVector(0.f,  23.f, 12.f));
	// Head (24 cm sphere).
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(RootComponent);
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);
		Head->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.24f));
	}
	Head->SetRelativeLocation(FVector(0.f, 0.f, 62.f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCapsuleComponent()->SetCapsuleHalfHeight(84.f);
	GetCapsuleComponent()->SetCapsuleRadius(32.f);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;   // face move direction (good for bots)
	MoveComp->JumpZVelocity = 520.0f;
	MoveComp->AirControl = 0.5f;
	MoveComp->MaxWalkSpeed = 450.0f;              // a bit faster for bots

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
	Tint(ArmL, Jersey);
	Tint(ArmR, Jersey);
	Tint(LegL, Shorts);
	Tint(LegR, Shorts);
	Tint(Head, Skin);
}

void ASpikeEliteCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
	const float BotSpeed = 450.0f;
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
		// TryTouchBall does the reach/phase/rules checks; cheap per frame.
		GM->TryTouchBall(this, EBallTouchType::Unknown);
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
	float Mult = LookSensitivity;
	if (auto* PC = Cast<APlayerController>(GetController()))
	{
		if (auto* SEPC = Cast<ASpikeElitePlayerController>(PC)) Mult *= SEPC->GetMouseSensitivity();
	}
	AddControllerYawInput(Value * Mult);
}
void ASpikeEliteCharacter::LookUpRate(float Value)
{
	float Mult = LookSensitivity;
	if (auto* PC = Cast<APlayerController>(GetController()))
	{
		if (auto* SEPC = Cast<ASpikeElitePlayerController>(PC)) Mult *= SEPC->GetMouseSensitivity();
	}
	AddControllerPitchInput(Value * Mult);
}
void ASpikeEliteCharacter::ToggleFirstPerson() { bFirstPerson = !bFirstPerson; UpdateCameraView(); }

void ASpikeEliteCharacter::UpdateCameraView()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	if (bFirstPerson)
	{
		PC->SetViewTarget(this);
		CameraBoom->Deactivate();
		FirstPersonCamera->Activate();
	}
	else
	{
		CameraBoom->TargetArmLength = ThirdPersonArmLength;
		CameraBoom->Activate();
		FirstPersonCamera->Deactivate();
		ThirdPersonCamera->Activate();
	}
}

void ASpikeEliteCharacter::HitBall()
{
	if (bIsBot) return;
	if (GetWorld() && GetWorld()->IsPaused()) return;   // never hit while paused
	if (!bTouchArmed) return;

	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->TryTouchBall(this, EBallTouchType::Unknown);
	}
}

void ASpikeEliteCharacter::ServeBall()
{
	if (bIsBot) return;
	if (GetWorld() && GetWorld()->IsPaused()) return;

	// The GameMode is the sole authority for who may serve and when.
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RequestServe(this);
	}
}
