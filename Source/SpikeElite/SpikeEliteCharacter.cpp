// SPDX-License-Identifier: MIT
#include "SpikeEliteCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Volleyball/VolleyballBall.h"
#include "Materials/MaterialInstanceDynamic.h"

ASpikeEliteCharacter::ASpikeEliteCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Skeletal mesh: Mannequin.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("/Game/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
	if (MannequinMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannequinMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	// Animation blueprint: idle/walk/jog/jump blending.
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBP(TEXT("/Game/Mannequins/Anims/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	if (AnimBP.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBP.Class);
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;   // face move direction (good for bots)
	MoveComp->JumpZVelocity = 520.0f;
	MoveComp->AirControl = 0.5f;
	MoveComp->MaxWalkSpeed = 450.0f;              // a bit faster for bots

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->bUsePawnControlRotation = true;

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
	// Team A = blue, Team B = red.
	FLinearColor Col = (TeamSide > 0) ? FLinearColor(0.15f, 0.35f, 0.9f) : FLinearColor(0.9f, 0.2f, 0.15f);
	if (GetMesh())
	{
		if (UMaterialInterface* Base = GetMesh()->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), Col);
			GetMesh()->SetMaterial(0, MID);
		}
	}
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
	// Find the ball.
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AVolleyballBall::StaticClass(), Found);
	if (Found.Num() == 0) return;
	AVolleyballBall* Ball = Cast<AVolleyballBall>(Found[0]);
	if (!Ball) return;

	const FVector MyLoc = GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float DistToBall = FVector::Dist(MyLoc, BallLoc);

	// Is the ball on our side? (TeamSide +1 defends X>0, -1 defends X<0)
	const bool bBallOnOurSide = (TeamSide > 0) ? (BallLoc.X > 0.0f) : (BallLoc.X < 0.0f);
	const bool bBallHittable = BallLoc.Z > 120.0f && BallLoc.Z < 450.0f;

	if (bBallOnOurSide && bBallHittable && DistToBall < 250.0f)
	{
		// Hit the ball toward the opponent's back court.
		FVector Target = FVector(-TeamSide * 700.0f, FMath::FRandRange(-250.0f, 250.0f), BallLoc.Z);
		FVector Dir = (Target - BallLoc).GetSafeNormal();
		Dir.Z = FMath::Max(Dir.Z, 0.2f);
		Dir.Normalize();
		Ball->Strike(Dir, FMath::RandRange(900.0f, 1100.0f), 0.0f);
		return;
	}

	// Otherwise: move toward the ball if it's coming our way, else go home.
	FVector Dest = (bBallOnOurSide && BallLoc.Z < 300.0f) ? BallLoc : HomePosition;
	Dest.Z = MyLoc.Z;
	FVector ToDest = Dest - MyLoc;
	ToDest.Z = 0;
	const float Dist = ToDest.Size();
	if (Dist > 30.0f)
	{
		AddMovementInput(ToDest.GetSafeNormal(), FMath::Min(1.0f, Dist / 200.0f));
	}

	// Don't cross the net or run out.
	FVector Loc = MyLoc;
	if (TeamSide > 0)
	{
		Loc.X = FMath::Clamp(Loc.X, 30.0f, 950.0f);
	}
	else
	{
		Loc.X = FMath::Clamp(Loc.X, -950.0f, -30.0f);
	}
	Loc.Y = FMath::Clamp(Loc.Y, -500.0f, 500.0f);
	if (Loc != MyLoc) SetActorLocation(Loc, true);
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

void ASpikeEliteCharacter::TurnRate(float Value) { AddControllerYawInput(Value * LookSensitivity); }
void ASpikeEliteCharacter::LookUpRate(float Value) { AddControllerPitchInput(Value * LookSensitivity); }
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
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AVolleyballBall::StaticClass(), Found);
	if (Found.Num() == 0) return;
	AVolleyballBall* Ball = Cast<AVolleyballBall>(Found[0]);
	if (!Ball) return;

	const FVector MyLoc = GetActorLocation();
	const float Dist = FVector::Dist(MyLoc, Ball->GetActorLocation());
	if (Dist > 220.0f) return;

	FVector LookDir = Controller ? Controller->GetControlRotation().Vector() : FVector::ForwardVector;
	LookDir.Z = FMath::Max(LookDir.Z, 0.15f);
	LookDir.Normalize();
	const bool bSpiking = !GetCharacterMovement()->IsMovingOnGround();
	Ball->Strike(LookDir, bSpiking ? 1200.0f : 850.0f, 0.0f);
}

void ASpikeEliteCharacter::ServeBall()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AVolleyballBall::StaticClass(), Found);
	if (Found.Num() == 0) return;
	AVolleyballBall* Ball = Cast<AVolleyballBall>(Found[0]);
	if (!Ball) return;
	const FVector MyLoc = GetActorLocation();
	Ball->ResetBall(FVector(MyLoc.X, MyLoc.Y, MyLoc.Z + 180.0f));
	Ball->Strike(FVector(-0.85f, FMath::FRandRange(-0.1f, 0.1f), 0.5f), 1200.0f, 0.0f);
}
