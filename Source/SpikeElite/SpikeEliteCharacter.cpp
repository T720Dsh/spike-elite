// SPDX-License-Identifier: MIT
#include "SpikeEliteCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Volleyball/VolleyballBall.h"

ASpikeEliteCharacter::ASpikeEliteCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Mirror rotation to the controller so the character faces where we look.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// ---- Skeletal mesh: UE5 Mannequin (copied from engine templates into /Game/Mannequins) ----
	// User asked for "just a skeleton for now, polish later". We bind the
	// template Mannequin so the capsule has a visible humanoid rig; the
	// materials / animations will be upgraded in later milestones.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("/Game/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
	if (MannequinMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannequinMesh.Object);
		// Mannequin is authored at ~180cm tall. Capsule default is 88 half-height.
		// Offset the mesh down so feet sit on the capsule bottom.
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	// Movement tuning: volleyball players are fast, short bursts.
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->JumpZVelocity = 520.0f;        // ~0.85 m vert jump, tune later with anims
	MoveComp->AirControl = 0.5f;
	MoveComp->MaxWalkSpeed = 380.0f;         // walking; sprint/spike burst added later

	// Third-person spring arm.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->bUsePawnControlRotation = true;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	// First-person camera: sit at head height (~ 65% of capsule half-height above center).
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 10.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void ASpikeEliteCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateCameraView();
}

void ASpikeEliteCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Use classic axis bindings for M0 so the project compiles without
	// shipping Enhanced Input mapping assets yet. We will migrate to
	// UInputAction / UInputMappingContext once the Input assets are
	// authored in the editor.
	PlayerInputComponent->BindAxis("MoveForward", this, &ASpikeEliteCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASpikeEliteCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ASpikeEliteCharacter::TurnRate);
	PlayerInputComponent->BindAxis("LookUp", this, &ASpikeEliteCharacter::LookUpRate);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("ToggleFirstPerson", IE_Pressed, this, &ASpikeEliteCharacter::ToggleFirstPerson);
	PlayerInputComponent->BindAction("HitBall", IE_Pressed, this, &ASpikeEliteCharacter::HitBall);
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
	AddControllerYawInput(Value * LookSensitivity);
}

void ASpikeEliteCharacter::LookUpRate(float Value)
{
	AddControllerPitchInput(Value * LookSensitivity);
}

void ASpikeEliteCharacter::ToggleFirstPerson()
{
	bFirstPerson = !bFirstPerson;
	UpdateCameraView();
}

void ASpikeEliteCharacter::UpdateCameraView()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	if (bFirstPerson)
	{
		PC->SetViewTarget(this); // uses FirstPersonCamera via camera manager pick
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
	// Find the volleyball in the world.
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AVolleyballBall::StaticClass(), Found);
	if (Found.Num() == 0) return;

	AVolleyballBall* Ball = Cast<AVolleyballBall>(Found[0]);
	if (!Ball) return;

	const FVector MyLoc = GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float Dist = FVector::Dist(MyLoc, BallLoc);

	// Arm's reach: ~180 cm. If the ball is farther than that, whiff.
	if (Dist > 220.0f)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(201, 1.0f, FColor::Red, FString::Printf(TEXT("Too far from ball (%.0f cm)"), Dist));
		return;
	}

	// Hit direction: where the player is looking, biased upward so the ball
	// clears the net. If the player is jumping, hit harder (spike).
	FVector LookDir = Controller ? Controller->GetControlRotation().Vector() : FVector::ForwardVector;
	LookDir.Z = FMath::Max(LookDir.Z, 0.15f);
	LookDir.Normalize();

	const bool bSpiking = !GetCharacterMovement()->IsMovingOnGround();
	const float Power = bSpiking ? 1200.0f : 850.0f;

	Ball->Strike(LookDir, Power, 0.0f);

	if (GEngine) GEngine->AddOnScreenDebugMessage(201, 1.0f, FColor::Green,
		bSpiking ? TEXT("SPIKE!") : TEXT("Hit!"));
}
