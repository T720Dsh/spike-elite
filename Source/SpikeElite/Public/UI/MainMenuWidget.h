// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UVerticalBox;

UCLASS()
class SPIKEELITE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geo, float DT) override;

	DECLARE_DELEGATE(FOnMenuAction);
	FOnMenuAction OnStart;
	FOnMenuAction OnSettings;
	FOnMenuAction OnQuit;

	/** M11d-2: global "reduce dynamic effects" flag now lives in SEUiStyle
	 *  (set from Settings); this widget only reads it in NativeTick. */
	static bool bReducedMotion; // legacy alias kept for ABI safety

protected:
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UTextBlock> SubTitle;
	UPROPERTY() TObjectPtr<UImage> BallIcon;
	UPROPERTY() TObjectPtr<UImage> Divider;
	UPROPERTY() TObjectPtr<UVerticalBox> LogoCluster;
	UPROPERTY() TObjectPtr<UVerticalBox> ButtonColumn;
	UPROPERTY() TObjectPtr<UButton> BtnStart;
	UPROPERTY() TObjectPtr<UButton> BtnSettings;
	UPROPERTY() TObjectPtr<UButton> BtnQuit;
	UPROPERTY() TObjectPtr<UImage> SpotL;
	UPROPERTY() TObjectPtr<UImage> SpotR;

	float AnimTime = 0.0f;
	float LightPhase = 0.0f;

	void BuildWidgetTree();

	UFUNCTION() void HandleStartClick() { OnStart.ExecuteIfBound(); }
	UFUNCTION() void HandleSettingsClick() { OnSettings.ExecuteIfBound(); }
	UFUNCTION() void HandleQuitClick() { OnQuit.ExecuteIfBound(); }
};
