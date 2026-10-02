// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfirmWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Small modal confirmation dialog (M10): a message plus 确认 / 取消.
 * Used before returning to the main menu or quitting so a mis-click cannot
 * throw away a live match.
 */
UCLASS()
class SPIKEELITE_API UConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UConfirmWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	DECLARE_DELEGATE(FOnConfirmAction);
	FOnConfirmAction OnConfirm;
	FOnConfirmAction OnCancel;

	void SetMessage(const FString& Text);

	/** M11f-3: focus the safe default (取消) AFTER the widget is in the viewport. */
	void SetInitialFocus();

protected:
	UPROPERTY() TObjectPtr<UTextBlock> MessageText;
	UPROPERTY() TObjectPtr<UButton> BtnConfirm;
	UPROPERTY() TObjectPtr<UButton> BtnCancel;

	void BuildWidgetTree();

	UFUNCTION() void HConfirm() { OnConfirm.ExecuteIfBound(); }
	UFUNCTION() void HCancel()  { OnCancel.ExecuteIfBound(); }
};
