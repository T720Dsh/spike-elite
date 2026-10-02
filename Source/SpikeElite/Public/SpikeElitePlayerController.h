// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpikeEliteGameMode.h"
#include "SpikeElitePlayerController.generated.h"

class UTrajectoryPreviewComponent;
class UTacticalContactComponent;
class UMainMenuWidget;
class UPauseMenuWidget;
class USettingsWidget;
class UScoreboardWidget;
class UMatchEndWidget;
class UConfirmWidget;

UENUM(BlueprintType)
enum class EMenuState : uint8
{
	MainMenu,
	Playing,
	Paused,
	SettingsFromMenu,
	SettingsFromPause,
	MatchOver,
	Confirm
};

/**
 * Owns all UI and input-mode transitions.
 * GameMode handles match lifecycle; this controller shows/hides widgets and
 * flips the input mode so the mouse is never permanently captured.
 *
 * M10: also owns the end-of-match result screen (mouse released), a modal
 * confirm dialog before returning to menu / quitting, and the Esc routing
 * that never revives a finished match.
 */
UCLASS()
class SPIKEELITE_API ASpikeElitePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASpikeElitePlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Called by GameMode once the match world is ready (or nullptr on cleanup). */
	void OnMatchStarted(UScoreboardWidget* InScoreboard);

	/** Called by GameMode when the match finishes: shows the result screen. */
	void OnMatchOver(const TArray<int32>& ScoresA, const TArray<int32>& ScoresB, EVolleyballTeam Winner);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void StartMatch();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void PauseGame();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenSettingsFromMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenSettingsFromPause();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseSettings();

	/** Ask for confirmation, then return to the main menu. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ReturnToMainMenu();

	/** Ask for confirmation, then quit to desktop. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void QuitToDesktop();

	/** True while any menu / confirm dialog is on screen (gameplay input blocked). */
	bool IsMenuOpen() const
	{
		return MenuState != EMenuState::Playing;
	}

	/** Tactical slow-motion shot planner (human only). Created in BeginPlay. */
	UPROPERTY()
	TObjectPtr<UTacticalContactComponent> Tactical;

	/** UI button path: confirm dialog before returning to the menu. */
	void AskReturnToMainMenu();

	/** UI button path: confirm dialog before quitting. */
	void AskQuitToDesktop();

	/** Rematch from the end-of-match screen. */
	void Rematch();

	/** Shared sensitivity bounds (must match the settings slider). */
	static constexpr float MinSensitivity() { return 0.1f; }
	static constexpr float MaxSensitivity() { return 3.0f; }

	/** Persisted mouse sensitivity multiplier (1.0 = default). */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	float GetMouseSensitivity() const { return MouseSensitivity; }
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMouseSensitivity(float V) { MouseSensitivity = FMath::Clamp(V, MinSensitivity(), MaxSensitivity()); }

	/** Persist the current sensitivity to GameUserSettings ini. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SaveSettings();

protected:
	UPROPERTY() TObjectPtr<UMainMenuWidget> MainMenu;
	UPROPERTY() TObjectPtr<UPauseMenuWidget> PauseMenu;
	UPROPERTY() TObjectPtr<USettingsWidget> SettingsMenu;
	UPROPERTY() TObjectPtr<UScoreboardWidget> Scoreboard;
	UPROPERTY() TObjectPtr<UMatchEndWidget> MatchEnd;
	UPROPERTY() TObjectPtr<UConfirmWidget> Confirm;

	EMenuState MenuState = EMenuState::MainMenu;

	/** State to restore when a modal confirm dialog is cancelled. */
	EMenuState StateBeforeConfirm = EMenuState::MainMenu;

	/** Action to run when the confirm dialog is accepted. */
	TFunction<void()> PendingConfirmAction;

	float MouseSensitivity = 1.0f;

	/** Headless dev camera spawned by the -devauto capture sequence. */
	UPROPERTY() TObjectPtr<AActor> DevCam;

	/** Esc / Start button handler, valid in every menu state. */
	void OnPausePressed();

	/** H key: toggle the right-top rotation HUD. */
	void OnToggleRotation();

	/** GameOnly: hidden, captured and locked mouse for look. */
	void SetGameInputMode();
	/** UI: visible, unlocked mouse that can leave the window. */
	void SetUIInputMode(UUserWidget* FocusWidget);
	void HideAllMenus();

	/** Build the pause menu widget and bind it. */
	void BuildPauseMenu();

	/** Build the end-of-match widget and bind it. */
	void BuildMatchEnd();

	/** Show a modal confirm; on accept run Action, on cancel restore StateBefore. */
	void ShowConfirm(const FString& Message, EMenuState RestoreState, TFunction<void()> Action);
	/** Dismiss the confirm dialog without doing anything. */
	void CancelConfirm();
	/** Confirm dialog accepted: run the pending action. */
	void AcceptConfirm();

	/** Actually leave to the main menu (post-confirm). */
	void LeaveToMainMenu();
	/** Actually quit (post-confirm). */
	void QuitNow();

#if !UE_BUILD_SHIPPING
	/** Headless smoke-test hook driven by the -devauto command line. */
	void DevAutoStart();
	/** Unattended -QuickMatch: drive a full quick match to the result screen. */
	void DevQuickMatch();
	/** -TacticalTest: drive the real tactical UMG (cancel -> plan -> perfect shot). */
	void DevTacticalTest();
	/** -RematchStress: 5x unattended rematch pressure + authoritative actor audit. */
	void DevRematchStress();
	/** -ShotSuite: capture the M11c acceptance screenshot set from a real match. */
	void DevShotSuite();
	/** -PoseSuite: pin each procedural pose and capture front/side/back + run frames. */
	void DevPoseSuite();
	void DevArtSuite();
	/** Request a named high-resolution screenshot (runs even while paused). */
	void DevShot(const FString& Name);
	/** Point the dev camera at a world transform, or back at the player pawn. */
	void DevView(const FVector& Loc, const FRotator& Rot);
	void DevViewPlayer();
	/** Log a PASS/FAIL for an automated state-machine check and count failures. */
	void DevVerify(bool bCondition, const FString& Label);
	/** Non-zero means at least one DevVerify check failed (reported at exit). */
	int32 DevVerifyFailures = 0;
	FTimerHandle DevTimer;
#endif
};
