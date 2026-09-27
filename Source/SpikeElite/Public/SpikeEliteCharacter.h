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
 * M0 responsibilities:
 *  - WASD movement + mouse look
 *  - Jump
 *  - Press V to toggle between third-person over-shoulder and first-person views.
 *
 * First-person view is a core sell of this game (see GDD §2.1). The helper
 * systems (radar, ball trajectory line, off-screen ball indicator) are
 * added later in Blueprints / UI widgets; this class just exposes the
 * camera switch and the FP camera component.
 */
UCLASS()
class SPIKEELITE_API ASpikeEliteCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASpikeEliteCharacter();

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	/** Third-person follow camera boom. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Third-person camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	/** First-person camera (head-height, used when bFirstPerson is true). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** If true, the active view is FirstPersonCamera; otherwise ThirdPersonCamera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bFirstPerson = false;

	/** How far back the third-person boom sits (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "50.0", ClampMax = "800.0"))
	float ThirdPersonArmLength = 250.0f;

	/** Mouse / gamepad look sensitivity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float LookSensitivity = 1.0f;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void ToggleFirstPerson();

	/** Left mouse: hit the ball if it is within arm's reach. */
	void HitBall();

	/** Swaps which camera is considered "view target" by the player controller. */
	void UpdateCameraView();
};
