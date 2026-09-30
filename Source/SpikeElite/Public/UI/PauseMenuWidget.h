// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;

UCLASS()
class SPIKEELITE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	DECLARE_DELEGATE(FOnMenuAction);
	FOnMenuAction OnResume;
	FOnMenuAction OnSettings;
	FOnMenuAction OnMainMenu;
	FOnMenuAction OnQuit;

protected:
	UPROPERTY() TObjectPtr<UButton> BtnResume;
	UPROPERTY() TObjectPtr<UButton> BtnSettings;
	UPROPERTY() TObjectPtr<UButton> BtnMainMenu;
	UPROPERTY() TObjectPtr<UButton> BtnQuit;
	UPROPERTY() TObjectPtr<UButton> BtnHelp;
	UPROPERTY() TObjectPtr<class UTextBlock> HelpText;
	void BuildWidgetTree();

	UFUNCTION() void HResume()   { OnResume.ExecuteIfBound(); }
	UFUNCTION() void HSettings() { OnSettings.ExecuteIfBound(); }
	UFUNCTION() void HMainMenu() { OnMainMenu.ExecuteIfBound(); }
	UFUNCTION() void HQuit()     { OnQuit.ExecuteIfBound(); }
	UFUNCTION() void ToggleHelp();
};
