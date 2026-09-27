// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SpikeEliteCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Player pawn for SPIKE ELITE.
 *
 * Can be controlled by a human (DefaultPawnClass) or spawned as AI bot:
 *  - bIsBot = true: Tick runs simple pursuit logic (go to ball, hit, return home)
 *  - bIsBot = false: human input via WASD/mouse/LMB/E
 */
UCLASS()
class SPIKEELITE_API ASpikeEliteCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASpikeEliteCharacter();

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	/** If true, this character is an AI bot (no human input). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	bool bIsBot = false;

	/** Which side: +1 = Team A (X>0), -1 = Team B (X<0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	int32 TeamSide = 1;

	/** Position this bot returns to when not chasing the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	FVector HomePosition = FVector(500, 0, 0);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bFirstPerson = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "50.0", ClampMax = "800.0"))
	float ThirdPersonArmLength = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float LookSensitivity = 1.0f;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void ToggleFirstPerson();

	/** Left mouse: hit the ball if it is within arm's reach. */
	void HitBall();

	/** E key: serve. */
	void ServeBall();

	void UpdateCameraView();

	/** AI bot per-frame logic. */
	void TickBot(float DeltaSeconds);

	/** Set jersey color (Team A blue, Team B red). */
	void ApplyJerseyColor();
};
