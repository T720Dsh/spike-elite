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
 * GameMode handles match lifecycle; this controller just shows/hides widgets
 * and flips the input mode so the mouse is never permanently captured.
 */
UCLASS()
class SPIKEELITE_API ASpikeElitePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASpikeElitePlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Called by GameMode once the match world is ready. */
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

	/** Persisted mouse sensitivity multiplier (1.0 = default). */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	float GetMouseSensitivity() const { return MouseSensitivity; }
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMouseSensitivity(float V) { MouseSensitivity = FMath::Clamp(V, 0.1f, 5.0f); SaveSettings(); }
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SaveSettings();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMainMenuWidget> MainMenuClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPauseMenuWidget> PauseMenuClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<USettingsWidget> SettingsClass;

	UPROPERTY()
	TObjectPtr<UMainMenuWidget> MainMenu;
	UPROPERTY()
	TObjectPtr<UPauseMenuWidget> PauseMenu;
	UPROPERTY()
	TObjectPtr<USettingsWidget> SettingsMenu;
	UPROPERTY()
	TObjectPtr<UScoreboardWidget> Scoreboard;

	EMenuState MenuState = EMenuState::MainMenu;

	float MouseSensitivity = 1.0f;

	void OnPausePressed();
	void SetGameInputMode();
	void SetUIInputMode();
	void HideAllMenus();

	/** Dev-only in-engine screenshot for headless UI verification. */
	void TakeDevShot();
	void DevAutoStart();
	FTimerHandle ShotTimer;
};
