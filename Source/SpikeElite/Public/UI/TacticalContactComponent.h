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

	/** Touch type pending in the planning/armed phases (read by -TacticalTest). */
	EBallTouchType GetPendingTouchType() const { return PendingTouchType; }

#if !UE_BUILD_SHIPPING
	/**
	 * Automation driver used by -TacticalTest. Drives the tactical state
	 * machine without a real human: reports the current phase (0=idle,
	 * 1=planning, 2=armed), retargets the intent (SetPlayIndex for a set,
	 * TargetOverride for free aim), then optionally cancels (verifying the
	 * world restore), confirms (planning->armed) or executes a PERFECT-timing
	 * shot from armed. Returns false if the component is not in the expected
	 * phase.
	 */
	bool DevTacticalStep(int32& PhaseOut, int32 SetPlayIndex, const FVector& TargetOverride,
		float Power, float FlightTime, bool bCancel, bool bConfirm, bool bExecute);

	/**
	 * -TacticalTest fallback: force a planning window for the given touch type
	 * when the ball never reaches the human (steering failed). Uses the exact
	 * same EnterPlanning entry point as a real contact window — the world-freeze,
	 * mouse release, UMG and preview all run through the production path.
	 */
	void DevForcePlanning(EBallTouchType Type);
	/** -ShotSuite: force-open the defense planning panel through the production path. */
	void DevForceDefensePlanning();
	/** -ShotSuite: confirm the currently open defense plan (AI default if none picked). */
	void DevConfirmDefense();
	/** M11f-1 automation-only: exercise the protected defense entry/restore pair
	 *  (dilation save/restore regression runs the REAL production functions). */
	void EnterDefensePlanningForTest() { EnterDefensePlanning(); }
	void RestoreWorldStateForTest() { RestoreWorldState(); }
#endif

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
	/** M11f-1: explicit "this component owns a TimeDilation override" state.
	 *  Set on EVERY tactical entry (planning/armed/defense), cleared on every
	 *  exit through RestoreWorldState. Restoration is paired with the saved
	 *  value and never guessed from the current floating dilation — a defense
	 *  exit can no longer leave the world stuck at 0.3. Re-entering while
	 *  already active does NOT re-save (the original value survives). */
	bool bTimeOverrideActive = false;
	bool bWorldFrozen = false;
	int32 DefenseSelected = 0;

	/** Open the defense-planning panel (opponent about to attack). */
	void EnterDefensePlanning();

	/** Apply the selected defense plan and restore normal play. */
	void ConfirmDefensePlan();

	/** UI callbacks: mouse-click on a set-play / defense row. */
	void HandleSetPlayPicked(int32 Index);
	void HandleDefensePicked(int32 Index);

	/** Mirror one data-driven set play into the live Intent and refresh the
	 *  preview. Shared by keyboard cycling AND mouse-card clicks so both input
	 *  paths select the same index, target, flight time and apex. */
	void ApplySetPlayToIntent(int32 Index);

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
