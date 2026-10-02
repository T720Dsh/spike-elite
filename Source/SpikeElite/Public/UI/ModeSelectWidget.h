// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModeSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * M11h-2: production mode-select page (pure C++ UMG, same style tokens as the
 * main menu). Picking a mode hands a FMatchModeConfig to the GameMode; the
 * developer-only command-line modes stay separate from these menu options.
 */
UCLASS()
class SPIKEELITE_API UModeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModeSelectWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	DECLARE_DELEGATE_OneParam(FOnPickQuick, bool /*bShortSets*/);
	FOnPickQuick OnPickQuick;
	DECLARE_DELEGATE(FOnPickMode);
	FOnPickMode OnPickChallenge;
	FOnPickMode OnPickCoach;
	FOnPickMode OnPickTraining;
	FOnPickMode OnBack;

	void SetInitialFocus();

protected:
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UVerticalBox> ButtonColumn;
	UPROPERTY() TObjectPtr<UButton> BtnQuickShort;
	UPROPERTY() TObjectPtr<UButton> BtnQuickFull;
	UPROPERTY() TObjectPtr<UButton> BtnChallenge;
	UPROPERTY() TObjectPtr<UButton> BtnCoach;
	UPROPERTY() TObjectPtr<UButton> BtnTraining;
	UPROPERTY() TObjectPtr<UButton> BtnBack;

	void BuildWidgetTree();

	UFUNCTION() void HandleQuickShort() { OnPickQuick.ExecuteIfBound(true); }
	UFUNCTION() void HandleQuickFull() { OnPickQuick.ExecuteIfBound(false); }
	UFUNCTION() void HandleChallenge() { OnPickChallenge.ExecuteIfBound(); }
	UFUNCTION() void HandleCoach() { OnPickCoach.ExecuteIfBound(); }
	UFUNCTION() void HandleTraining() { OnPickTraining.ExecuteIfBound(); }
	UFUNCTION() void HandleBack() { OnBack.ExecuteIfBound(); }
};
