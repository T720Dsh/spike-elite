// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Volleyball/VolleyballTrajectory.h"
#include "Volleyball/VolleyballEnums.h"
#include "TacticalContactComponent.generated.h"

class UTrajectoryPreviewComponent;
class UTextRenderComponent;
class UTacticalHUDWidget;
class ASpikeEliteCharacter;
class ASpikeEliteGameMode;
class ASpikeElitePlayerController;

/** Tactical slow-motion state machine phases. */
UENUM(BlueprintType)
enum class ETacticalState : uint8
{
	Normal,
	ContactWindow,
	TacticalPlanning,
	TacticalArmed,
	ContactResolved,
	DefensePlanning
};

/**
 * Tactical shot state machine for the local human player.
 *
 * Normal -> ContactWindow -> TacticalPlanning -> TacticalArmed -> ContactResolved.
 *  - ContactWindow: the ball entered the player's legal touch window (~0.35s).
 *  - TacticalPlanning: world time dilation frozen at 0, mouse released, dotted
 *    trajectory preview shown; mouse picks the landing spot, W/S or wheel sets
 *    power, Q/E sets arc/flight time, LMB confirms, RMB/Esc cancels.
 *  - TacticalArmed: time dilation ~0.15, the ball continues slowly; LMB inside
 *    the timing window executes the shot with a timing-quality error.
 *  - Timeout: a conservative default shot fires so the match can never hang.
 *
 * Only the locally controlled human triggers this; AI never opens the UI.
 * The GameMode remains the sole authority for touch legality (ExecuteTacticalShot
 * goes through the same DoTouch path as TryTouchBall).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SPIKEELITE_API UTacticalContactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTacticalContactComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

	/** Current state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical")
	ETacticalState State = ETacticalState::Normal;

	/** 0 = off, 1 = key moments (set/attack only, default), 2 = all touches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical")
	int32 TacticalMode = 1;

	/** Current shot intent being planned. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tactical")
	FShotIntent Intent;

	/** True while in the planning/armed phases. */
	bool IsTacticalActive() const { return State == ETacticalState::TacticalPlanning || State == ETacticalState::TacticalArmed; }

	/** Cancel the planning phase (restores dilation, input, mouse). */
	void CancelShot();

	/** Current human defense plan (read by the GameMode while defending). */
	EVolleyballDefensePlan DefensePlan = EVolleyballDefensePlan::NoPlan;

	/** True while the defense-planning panel is open. */
	bool IsDefensePlanning() const { return State == ETacticalState::DefensePlanning; }

protected:
	UPROPERTY()
	TObjectPtr<UTrajectoryPreviewComponent> Preview;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> HintText;

	UPROPERTY()
	TObjectPtr<UTacticalHUDWidget> TacticalUI;

	UPROPERTY()
	TWeakObjectPtr<ASpikeElitePlayerController> OwnerPC;

	UPROPERTY()
	TWeakObjectPtr<ASpikeEliteCharacter> Pawn;

	UPROPERTY()
	TWeakObjectPtr<ASpikeEliteGameMode> GM;

	float WindowTimer = 0.f;
	float ArmedTimer = 0.f;
	float DefenseTimer = 0.f;
	float SavedTimeDilation = 1.f;
	bool bWorldFrozen = false;
	int32 DefenseSelected = 0;

	/** Open the defense-planning panel (opponent about to attack). */
	void EnterDefensePlanning();

	/** Apply the selected defense plan and restore normal play. */
	void ConfirmDefensePlan();

	/** UI callbacks: mouse-click on a set-play / defense row. */
	void HandleSetPlayPicked(int32 Index);
	void HandleDefensePicked(int32 Index);

	EBallTouchType PendingTouchType = EBallTouchType::Unknown;

	/** Selected set-play index into SESetPlays::GetPlays() (-1 = mouse aim). */
	int32 SelectedPlay = -1;

	/** Advance to TacticalPlanning (freeze world, show UI, release mouse). */
	void EnterPlanning(EBallTouchType Type);

	/** Advance to TacticalArmed (slow motion, start the timing window). */
	void EnterArmed();

	/** Execute the shot with the given timing error. */
	void ExecuteTimedShot(float TimingError);

	/** Planning-phase input: pick target, adjust power/arc, confirm/cancel. */
	void TickPlanning(float DeltaTime);

	/** Pick the ground point under the mouse cursor. */
	FVector PickGroundPoint();

	/** Rebuild Intent + preview from current mouse/power/arc state. */
	void RebuildPreview();

	/** Restore dilation, input and mouse state after planning/armed. */
	void RestoreWorldState();
};
