// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;

UCLASS()
class SPIKEELITE_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USettingsWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	DECLARE_DELEGATE(FOnMenuAction);
	FOnMenuAction OnBack;

	/** Fired after the user clicks Apply (owner persists sensitivity etc.). */
	FOnMenuAction OnApply;

	/** Fired on Apply with the chosen sensitivity. */
	DECLARE_DELEGATE_OneParam(FOnSensitivityChanged, float);
	FOnSensitivityChanged OnSensitivityChanged;

	/** Set the pending sensitivity from the owner's persisted value. */
	void SetCurrentSensitivity(float V);

	/** Populate window mode / resolution / quality from the live user settings. */
	void InitFromCurrentSettings();

protected:
	UPROPERTY() TObjectPtr<UComboBoxString> WindowMode;
	UPROPERTY() TObjectPtr<UComboBoxString> Resolution;
	UPROPERTY() TObjectPtr<UComboBoxString> Quality;
	UPROPERTY() TObjectPtr<USlider> SensSlider;
	UPROPERTY() TObjectPtr<UTextBlock> SensValue;
	UPROPERTY() TObjectPtr<USlider> UIScaleSlider;
	UPROPERTY() TObjectPtr<UTextBlock> UIScaleValue;
	UPROPERTY() TObjectPtr<UButton> ReducedMotionBtn;
	UPROPERTY() TObjectPtr<UTextBlock> ReducedMotionLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ApplyStatus;
	UPROPERTY() TObjectPtr<UButton> BtnApply;
	UPROPERTY() TObjectPtr<UButton> BtnBack;

	float PendingSensitivity = 1.0f;
	float PendingUIScale = 1.0f;
	bool bReducedMotion = false;
	float ApplyStatusSeconds = 0.0f;

	UFUNCTION() void OnSensChanged(float V);
	UFUNCTION() void OnUIScaleChanged(float V);
	UFUNCTION() void ToggleReducedMotion();
	UFUNCTION() void ApplySettings();
	UFUNCTION() void Back();

	void PopulateResolutions();
	void BuildWidgetTree();
};
