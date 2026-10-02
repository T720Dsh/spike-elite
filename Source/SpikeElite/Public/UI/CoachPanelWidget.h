// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CoachPanelWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * M11h-6: coach / team-management panel (opened with Tab from any mode; in
 * coach mode the player sits off-court and directs). The panel only sends
 * INTENTS to the GameMode — the GameMode validates every request (timeout /
 * substitution windows, allowances, pairing) and is the single authority for
 * scores, lineup and serve authorisation. Preferences are stored on the
 * GameMode and read by the AI at the next safe decision point.
 */
UCLASS()
class SPIKEELITE_API UCoachPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCoachPanelWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	DECLARE_DELEGATE(FOnCoachAction);
	FOnCoachAction OnRequestTimeoutA;
	FOnCoachAction OnRequestTimeoutB;
	FOnCoachAction OnSubA;
	FOnCoachAction OnSubB;
	FOnCoachAction OnCycleServeZone;
	FOnCoachAction OnCycleBlock;
	FOnCoachAction OnCycleDefense;
	FOnCoachAction OnCycleSetter;
	FOnCoachAction OnClosePanel;

	void SetInitialFocus();
	/** Refresh the status line (score / phase / allowances). */
	void SetStatus(const FString& Text);
	/** Show the current preference values on the cycle buttons. */
	void SetPrefLabels(const FString& ServeZone, const FString& Block, const FString& Defense, const FString& Setter);

protected:
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	UPROPERTY() TObjectPtr<UButton> BtnTimeoutA;
	UPROPERTY() TObjectPtr<UButton> BtnTimeoutB;
	UPROPERTY() TObjectPtr<UButton> BtnSubA;
	UPROPERTY() TObjectPtr<UButton> BtnSubB;
	UPROPERTY() TObjectPtr<UButton> BtnServeZone;
	UPROPERTY() TObjectPtr<UButton> BtnBlock;
	UPROPERTY() TObjectPtr<UButton> BtnDefense;
	UPROPERTY() TObjectPtr<UButton> BtnSetter;
	UPROPERTY() TObjectPtr<UButton> BtnClose;

	void BuildWidgetTree();

	UFUNCTION() void HandleTimeoutA() { OnRequestTimeoutA.ExecuteIfBound(); }
	UFUNCTION() void HandleTimeoutB() { OnRequestTimeoutB.ExecuteIfBound(); }
	UFUNCTION() void HandleSubA() { OnSubA.ExecuteIfBound(); }
	UFUNCTION() void HandleSubB() { OnSubB.ExecuteIfBound(); }
	UFUNCTION() void HandleServeZone() { OnCycleServeZone.ExecuteIfBound(); }
	UFUNCTION() void HandleBlock() { OnCycleBlock.ExecuteIfBound(); }
	UFUNCTION() void HandleDefense() { OnCycleDefense.ExecuteIfBound(); }
	UFUNCTION() void HandleSetter() { OnCycleSetter.ExecuteIfBound(); }
	UFUNCTION() void HandleClose() { OnClosePanel.ExecuteIfBound(); }
};
