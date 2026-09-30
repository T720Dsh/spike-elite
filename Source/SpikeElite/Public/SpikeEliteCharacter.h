// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Volleyball/VolleyballRules.h"
#include "SpikeEliteCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class ASpikeEliteGameMode;

/**
 * Player pawn for SPIKE ELITE.
 *
 * Can be controlled by a human (DefaultPawnClass) or spawned as AI bot:
 *  - bIsBot = true: Tick runs the GameMode-driven AI (the GameMode picks the
 *    primary handler and assigns an EAIBehavior + target each frame; the bot
 *    never searches for the ball itself).
 *  - bIsBot = false: human input via WASD/mouse/LMB/E.
 *
 * M10: characters NEVER ResetBall or Strike on their own. They ask the
 * GameMode (TryTouchBall / RequestServe), which is the single authority for
 * serve rights, touch rights and the three-touch rule.
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

	/** Stable per-match player id (Team A: 0..5, Team B: 6..11). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bot")
	int32 PlayerId = -1;

	/** Jersey number shown on the rotation HUD (Team A 1..6, Team B 7..12). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bot")
	int32 JerseyNumber = 0;

	/** Position this bot returns to when not chasing the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	FVector HomePosition = FVector(500, 0, 0);

	// ---- M10: single-touch protection ----
	/** True while this character may touch the ball (rearmed when the ball leaves reach). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bot")
	bool bTouchArmed = true;

	// ---- M10: AI directives (written by the GameMode coordinator) ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bot")
	EAIBehavior AIBehavior = EAIBehavior::ReturnHome;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bot")
	FVector AITargetLocation = FVector::ZeroVector;

	/** True if this bot is the team's current primary handler (may touch). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bot")
	bool bIsPrimaryHandler = false;

	/** Cached GameMode reference (no per-frame GetAllActorsOfClass). */
	UPROPERTY()
	TWeakObjectPtr<ASpikeEliteGameMode> AIGameMode;

	/** Team enum derived from TeamSide. */
	EVolleyballTeam GetTeam() const { return TeamSide > 0 ? EVolleyballTeam::TeamA : EVolleyballTeam::TeamB; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Hinge-style placeholder humanoid built from engine basic shapes:
	// torso + head + two arms + two legs, tinted per team. Zero asset deps.
	// TODO(M-models): swap this for a rigged skeletal mesh + animation blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> Torso;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> ArmL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> ArmR;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> LegL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> LegR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bFirstPerson = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "50.0", ClampMax = "800.0"))
	float ThirdPersonArmLength = 380.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float LookSensitivity = 1.0f;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void ToggleFirstPerson();

	/** Left mouse: ask the GameMode to touch the ball if legal. */
	void HitBall();

	/** E key: ask the GameMode to serve (it validates serve rights). */
	void ServeBall();

	void UpdateCameraView();

	/** AI bot per-frame logic (GameMode-driven). */
	void TickBot(float DeltaSeconds);

	/** Set jersey color (Team A blue, Team B red). */
	void ApplyJerseyColor();
};
