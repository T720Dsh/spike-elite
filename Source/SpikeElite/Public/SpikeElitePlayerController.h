// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpikeElitePlayerController.generated.h"

class UMainMenuWidget;
class UPauseMenuWidget;
class USettingsWidget;
class UScoreboardWidget;

UENUM(BlueprintType)
enum class EMenuState : uint8
{
	MainMenu,
	Playing,
	Paused,
	SettingsFromMenu,
	SettingsFromPause
};

/**
 * Owns all UI and input-mode transitions.
 * GameMode handles match lifecycle; this controller shows/hides widgets and
 * flips the input mode so the mouse is never permanently captured.
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

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void QuitToDesktop();

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

	EMenuState MenuState = EMenuState::MainMenu;

	float MouseSensitivity = 1.0f;

	/** Headless dev camera spawned by the -devauto capture sequence. */
	UPROPERTY() TObjectPtr<AActor> DevCam;

	/** Esc / Start button handler, valid in every menu state. */
	void OnPausePressed();

	/** GameOnly: hidden, captured and locked mouse for look. */
	void SetGameInputMode();
	/** UI: visible, unlocked mouse that can leave the window. */
	void SetUIInputMode(UUserWidget* FocusWidget);
	void HideAllMenus();

	/** Build the pause menu widget and bind it. */
	void BuildPauseMenu();

#if !UE_BUILD_SHIPPING
	/** Headless smoke-test hook driven by the -devauto command line. */
	void DevAutoStart();
	/** Request a named high-resolution screenshot (runs even while paused). */
	void DevShot(const FString& Name);
	/** Point the dev camera at a world transform, or back at the player pawn. */
	void DevView(const FVector& Loc, const FRotator& Rot);
	void DevViewPlayer();
	FTimerHandle DevTimer;
#endif
};
