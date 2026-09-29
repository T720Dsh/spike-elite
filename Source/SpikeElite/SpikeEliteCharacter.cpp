// SPDX-License-Identifier: MIT
#include "SpikeEliteCharacter.h"
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
	// TODO(M-models): swap this for a rigged skeletal mesh + animation blueprint.
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
	// Team A = electric blue, Team B = red. Jersey = torso + arms; shorts are a
	// darker shade of the team colour; head is a neutral skin tone.
	const FLinearColor Jersey = (TeamSide > 0) ? FLinearColor(0.10f, 0.50f, 1.00f) : FLinearColor(0.95f, 0.22f, 0.12f);
	const FLinearColor Shorts = (TeamSide > 0) ? FLinearColor(0.05f, 0.16f, 0.38f) : FLinearColor(0.38f, 0.07f, 0.05f);
	const FLinearColor Skin(0.82f, 0.64f, 0.48f, 1.0f);

	auto Tint = [&](UStaticMeshComponent* Comp, const FLinearColor& Col)
	{
		if (!Comp) return;
		// Use the project M_Tint material (guaranteed "Color" parameter); the
		// engine BasicShapes material does not tint reliably at runtime.
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
	// Find the ball.
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AVolleyballBall::StaticClass(), Found);
	if (Found.Num() == 0) return;
	AVolleyballBall* Ball = Cast<AVolleyballBall>(Found[0]);
	if (!Ball) return;

	const FVector MyLoc = GetActorLocation();
	const FVector BallLoc = Ball->GetActorLocation();
	const float DistToBall = FVector::Dist(MyLoc, BallLoc);

	BotHitTimer -= DeltaSeconds;

	// Is the ball on our side? (TeamSide +1 defends X>0, -1 defends X<0)
	const bool bBallOnOurSide = (TeamSide > 0) ? (BallLoc.X > 0.0f) : (BallLoc.X < 0.0f);
	const bool bBallHittable = BallLoc.Z > 120.0f && BallLoc.Z < 450.0f;

	if (bBallOnOurSide && bBallHittable && DistToBall < 250.0f && BotHitTimer <= 0.0f)
	{
		// Aim at the opponent's back court. A bounded error rate (wide / long /
		// into the net) keeps rallies finite so points are actually awarded;
		// without it bots perfectly return every ball forever.
		const float Roll = FMath::FRand();
		FVector Target;
		float MinZ = 0.2f;
		if (Roll < 0.06f)
		{
			// Wide: beyond a side line -> out.
			const float SideY = (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(560.f, 720.f);
			Target = FVector(-TeamSide * 700.f, SideY, BallLoc.Z);
		}
		else if (Roll < 0.15f)
		{
			// Long / into the net: low trajectory.
			Target = FVector(-TeamSide * FMath::FRandRange(700.f, 1050.f),
			                 FMath::FRandRange(-200.f, 200.f), BallLoc.Z);
			MinZ = FMath::FRandRange(-0.12f, 0.05f);
		}
		else
		{
			Target = FVector(-TeamSide * 700.0f, FMath::FRandRange(-250.0f, 250.0f), BallLoc.Z);
		}

		FVector Dir = (Target - BallLoc).GetSafeNormal();
		Dir.Z = FMath::Max(Dir.Z, MinZ);
		Dir.Normalize();
		Ball->SetLastHitTeam(TeamSide > 0 ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB);
		Ball->Strike(Dir, FMath::RandRange(900.0f, 1100.0f), 0.0f);
		BotHitTimer = 0.7f;
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
	Ball->SetLastHitTeam(TeamSide > 0 ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB);
	Ball->Strike(LookDir, bSpiking ? 1200.0f : 850.0f, 0.0f);
}

void ASpikeEliteCharacter::ServeBall()
{
	if (bIsBot) return;
	if (GetWorld() && GetWorld()->IsPaused()) return;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AVolleyballBall::StaticClass(), Found);
	if (Found.Num() == 0) return;
	AVolleyballBall* Ball = Cast<AVolleyballBall>(Found[0]);
	if (!Ball) return;
	const FVector MyLoc = GetActorLocation();
	Ball->ResetBall(FVector(MyLoc.X, MyLoc.Y, MyLoc.Z + 180.0f));
	Ball->SetLastHitTeam(TeamSide > 0 ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB);
	Ball->Strike(FVector(-0.85f, FMath::FRandRange(-0.1f, 0.1f), 0.5f), 1200.0f, 0.0f);
}
