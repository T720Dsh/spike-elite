// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;
class UCheckBox;

UCLASS()
class SPIKEELITE_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USettingsWidget(const FObjectInitializer& ObjectInitializer);
	virtual void NativeConstruct() override;

	DECLARE_DELEGATE(FOnMenuAction);
	FOnMenuAction OnBack;

	/** Called after Apply so the owner can persist values. */
	DECLARE_DELEGATE_OneParam(FOnSensitivityChanged, float);
	FOnSensitivityChanged OnSensitivityChanged;

	void SetCurrentSensitivity(float V);

protected:
	UPROPERTY() TObjectPtr<UComboBoxString> WindowMode;
	UPROPERTY() TObjectPtr<UComboBoxString> Resolution;
	UPROPERTY() TObjectPtr<UComboBoxString> Quality;
	UPROPERTY() TObjectPtr<USlider> SensSlider;
	UPROPERTY() TObjectPtr<UTextBlock> SensValue;
	UPROPERTY() TObjectPtr<UButton> BtnApply;
	UPROPERTY() TObjectPtr<UButton> BtnBack;

	float PendingSensitivity = 1.0f;

	UFUNCTION() void OnSensChanged(float V);
	UFUNCTION() void ApplySettings();
	UFUNCTION() void Back();

	void PopulateResolutions();
};
