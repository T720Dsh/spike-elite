// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;

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

protected:
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UImage> BallIcon;
	UPROPERTY() TObjectPtr<UButton> BtnStart;
	UPROPERTY() TObjectPtr<UButton> BtnSettings;
	UPROPERTY() TObjectPtr<UButton> BtnQuit;

	float AnimTime = 0.0f;
	void BuildWidgetTree();

	UFUNCTION() void HandleStartClick() { OnStart.ExecuteIfBound(); }
	UFUNCTION() void HandleSettingsClick() { OnSettings.ExecuteIfBound(); }
	UFUNCTION() void HandleQuitClick() { OnQuit.ExecuteIfBound(); }
};
