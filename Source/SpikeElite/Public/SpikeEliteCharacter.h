// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Volleyball/VolleyballRules.h"
#include "SpikeEliteCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USceneComponent;
class ASpikeEliteGameMode;

/**
 * Articulated placeholder pose used by the procedural animation system.
 * Priority order (highest first) is applied inside UpdateProceduralAnimation.
 */
UENUM()
enum class EAnimPose : uint8
{
	Idle,
	Run,
	Jump,
	Receive,   // 垫球: arms pressed together forward, knees bent
	Set,       // 二传: hands raised to forehead height
	Spike,     // 扣球: wind-up -> swing -> follow-through (PoseTimer staged)
	Block,     // 拦网: both hands straight up overhead
	Dive,      // 倒地救球: forward/side lunge
	Recover,   // 恢复: low crouch, no second dive
	Serve,     // 发球: toss + arm swing
	RaiseHands // 抬手: continuous 0..1 arm raise (block/set/receive prep)
};

/**
 * One articulated limb: a pivot joint (shoulder/hip) with upper, lower and tip
 * segments. The lower segment attaches to an elbow/knee joint at the bottom of
 * the upper segment so it can bend independently. Pure reflection (no Blueprint
 * exposure — UHT rejects struct-of-component properties on BP-visible UPROPERTYs).
 */
USTRUCT()
struct FProceduralLimb
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USceneComponent> Joint;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Upper;

	UPROPERTY()
	TObjectPtr<USceneComponent> BendJoint;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Lower;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Tip;
};

/**
 * Player pawn for SPIKE ELITE.
 *
 * Can be controlled by a human (DefaultPawnClass) or spawned as AI bot:
 *  - bIsBot = true: Tick runs the GameMode-driven AI (the GameMode picks the
 *    primary handler and assigns an EAIBehavior + target each frame; the bot
 *    never searches for the ball itself).
 *  - bIsBot = false: human input via WASD/mouse/LMB/E/C/V/RMB.
 *
 * M10: characters NEVER ResetBall or Strike on their own. They ask the
 * GameMode (TryTouchBall / RequestServe), which is the single authority for
 * serve rights, touch rights and the three-touch rule.
 *
 * M11b-3: the blocky placeholder was upgraded to a fully articulated procedural
 * humanoid (head/torso/upper-arm/forearm/hand/thigh/shin/foot per side) driven
 * by joint SceneComponents and per-frame pose interpolation. No external model
 * or animation assets are required.
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

	// ---- M11b-3: raise-hands input (RMB held). 0..1 continuous. ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	float RaiseHandsAmount = 0.f;

	/** Last successful touch type, used to drive a short contact pose. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	EBallTouchType LastContactType = EBallTouchType::Unknown;

	/** M11c-3: authoritative dive lifecycle (None/Approach/Active/Recovery).
	 *  Shared pure-logic state — the GameMode reads IsActive()/IsRecovering()
	 *  for reach/touch gating and the procedural animation drives the pose.
	 *  Plain C++ member (not UPROPERTY: the struct is not UHT-reflectable). */
	SEVolleyballRules::FVolleyballDiveState DiveState;

	/** Convenience getters for the GameMode (reach/phase gates). */
	bool IsDiving() const { return DiveState.IsActive(); }
	bool IsDiveRecovering() const { return DiveState.IsRecovering(); }

	/** M11c-1: set by the GameMode while this player is the authorized server.
	 *  While true, movement bounds widen to the service zone (X up to ±1550,
	 *  Y ±450) so the human can move behind the end line to serve instead of
	 *  being clamped back inside the court. Cleared at EndRally/cleanup. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	bool bServiceZoneActive = false;

	/** Pose shown for this frame (drives the procedural joints). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	EAnimPose CurrentPose = EAnimPose::Idle;

	/** Remember a successful contact so the contact pose plays for a short window. */
	void NotifyContact(EBallTouchType Type);

	/** Dev aid (-Closeup): shorten the spring arm so the body fills the view. */
	void SetThirdPersonArmLength(float NewLength);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// ---- M11b-3: articulated procedural humanoid ----
	// Torso and head attach straight to the capsule root; each limb hangs from
	// a shoulder/hip joint SceneComponent. All segments are engine basic shapes
	// (zero external assets) tinted per team.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<USceneComponent> TorsoJoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> Torso;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY()
	FProceduralLimb ArmL;
	UPROPERTY()
	FProceduralLimb ArmR;
	UPROPERTY()
	FProceduralLimb LegL;
	UPROPERTY()
	FProceduralLimb LegR;


	/** Seconds since the last successful touch (drives short contact poses). */
	float ContactPoseTimer = 0.f;
	/** Local swing phase for the run cycle (radians). */
	float RunPhase = 0.f;

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

	/** Right mouse (held): raise hands; released: lower them. */
	void StartRaiseHands();
	void StopRaiseHands();

	void UpdateCameraView();

	/** AI bot per-frame logic (GameMode-driven). */
	void TickBot(float DeltaSeconds);

	/** Set jersey color (Team A blue, Team B red) on every body segment. */
	void ApplyJerseyColor();

	/** M11b-3: drive the articulated joints from pose + locomotion state. */
	void UpdateProceduralAnimation(float DeltaSeconds);

	/** Compute the target pose for this frame from movement/contact/AI state. */
	EAnimPose ResolvePose(float DeltaSeconds);

	/** Apply a pose to all joints (with smoothing toward the previous pose). */
	void ApplyPose(EAnimPose Pose, float DeltaSeconds);

	/** Reset every joint to the natural idle rest pose. */
	void SetPoseIdle();
};
