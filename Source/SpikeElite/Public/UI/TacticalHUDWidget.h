// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Volleyball/VolleyballTrajectory.h"
#include "Volleyball/SetPlay.h"
#include "TacticalHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UCanvasPanel;
class UVerticalBox;
class UBorder;
class UButton;
class UScrollBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSetPlaySelected, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDefensePlanSelected, int32);

/** UMG's click event has no index; carry one with each tactical row. */
UCLASS()
class SPIKEELITE_API UTacticalChoiceButton : public UButton
{
	GENERATED_BODY()
public:
	void InitChoice(int32 InIndex)
	{
		ChoiceIndex = InIndex;
		OnClicked.AddUniqueDynamic(this, &UTacticalChoiceButton::DispatchChoice);
	}
	FOnSetPlaySelected OnChoiceSelected;
private:
	int32 ChoiceIndex = INDEX_NONE;
	UFUNCTION() void DispatchChoice() { OnChoiceSelected.Broadcast(ChoiceIndex); }
};

/**
 * M11c-5: the REAL screen UMG for tactical play — replaces the world-space
 * UTextRenderComponent hint. One widget, three panels:
 *  - AttackPanel: touch type, target landing, power, solved initial speed,
 *    arc apex, flight time, net/in-bounds verdict and the armed timing bar.
 *  - SetPanel: the full 13+1 data-driven set-play list with highlight,
 *    category / attacker / apex / speed / flight time / risk details.
 *  - DefensePanel: the 8 defensive plan choices (block / line / dig).
 * Panels are hidden except while their state is active; every SetText call is
 * gated by a dirty comparison so nothing is rebuilt per frame.
 */
UCLASS()
class SPIKEELITE_API UTacticalHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTacticalHUDWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Panels: which mode is visible right now. */
	void ShowAttackPanel();
	void ShowSetPanel();
	void ShowDefensePanel();
	void HideAll();

	/** Attack panel data (called only when the plan actually changes). */
	void UpdateAttackInfo(const FShotIntent& Intent, const SEVolleyballTrajectory::FShotSolution& Sol);
	void ShowTiming(float Progress01, const FString& Status);

	/** True when the cursor is over one of the tactical panels. The contact
	 *  component uses this to keep UMG card clicks (select-only) from being
	 *  misread as a world-space LMB confirm. */
	bool IsPointerOverPanel() const;

	/** Set-play list: rebuild highlight / details from the current selection. */
	void UpdateSetList(int32 Selected);
	/** Defense list highlight. */
	void UpdateDefenseList(int32 Selected);

	FOnSetPlaySelected OnSetPlaySelected;
	FOnDefensePlanSelected OnDefensePlanSelected;

protected:
	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UBorder> AttackBorder;
	UPROPERTY() TObjectPtr<UBorder> SetBorder;
	UPROPERTY() TObjectPtr<UBorder> DefenseBorder;

	UPROPERTY() TObjectPtr<UTextBlock> AttackHeader;
	UPROPERTY() TObjectPtr<UTextBlock> AttackStage;
	UPROPERTY() TObjectPtr<UTextBlock> AttackTarget;
	UPROPERTY() TObjectPtr<UTextBlock> AttackPower;
	UPROPERTY() TObjectPtr<UProgressBar> PowerBar;
	UPROPERTY() TObjectPtr<UTextBlock> AttackArc;
	UPROPERTY() TObjectPtr<UTextBlock> AttackVerdict;
	UPROPERTY() TObjectPtr<UTextBlock> AttackHelp;
	UPROPERTY() TObjectPtr<UProgressBar> TimingBar;
	UPROPERTY() TObjectPtr<UTextBlock> TimingLabel;
	/** Attack panel body container (for pointer-over detection). */
	UPROPERTY() TObjectPtr<UVerticalBox> AttackList;

	UPROPERTY() TObjectPtr<UTextBlock> SetTitle;
	UPROPERTY() TObjectPtr<UTextBlock> SetCategory;
	UPROPERTY() TObjectPtr<UVerticalBox> SetList;
	UPROPERTY() TObjectPtr<UScrollBox> SetScroll;
	UPROPERTY() TObjectPtr<UTextBlock> SetDetails;
	UPROPERTY() TObjectPtr<UTextBlock> SetHelp;
	TArray<TObjectPtr<UTextBlock>> SetRows;
	TArray<TObjectPtr<UButton>> SetRowButtons;

	UPROPERTY() TObjectPtr<UTextBlock> DefenseTitle;
	UPROPERTY() TObjectPtr<UVerticalBox> DefenseListBlock;
	UPROPERTY() TObjectPtr<UVerticalBox> DefenseListDig;
	UPROPERTY() TObjectPtr<UTextBlock> DefenseHelp;
	TArray<TObjectPtr<UTextBlock>> DefenseRows;
	TArray<TObjectPtr<UButton>> DefenseRowButtons;

	FString LastAttackKey;
	FString LastSetKey;
	int32 LastSetSelected = -1;
	int32 LastDefenseSelected = -1;

	void BuildWidgetTree();
	void BuildAttackPanel();
	void BuildSetPanel();
	void BuildDefensePanel();

	UFUNCTION() void HandleSetRowClick(int32 Index) { OnSetPlaySelected.Broadcast(Index); }
	UFUNCTION() void HandleDefenseRowClick(int32 Index) { OnDefensePlanSelected.Broadcast(Index); }
};
