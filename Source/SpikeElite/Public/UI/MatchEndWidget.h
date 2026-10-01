// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpikeEliteGameMode.h"
#include "MatchEndWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;

/**
 * End-of-match result screen (M10).
 *
 * Shows the winner, each set's final score, and buttons for rematch / main
 * menu / quit-to-desktop. The owning PlayerController releases the mouse and
 * switches to UI input mode while it is up; Esc does NOT revive the match.
 */
UCLASS()
class SPIKEELITE_API UMatchEndWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMatchEndWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	DECLARE_DELEGATE(FOnMenuAction);
	FOnMenuAction OnRematch;
	FOnMenuAction OnMainMenu;
	FOnMenuAction OnQuit;

	/** Populate the result screen. */
	void SetResult(EVolleyballTeam Winner, const TArray<int32>& ScoresA, const TArray<int32>& ScoresB);

protected:
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UTextBlock> WinnerText;
	UPROPERTY() TObjectPtr<UTextBlock> SetScoresText;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreCardA;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreCardB;
	UPROPERTY() TObjectPtr<UButton> BtnRematch;
	UPROPERTY() TObjectPtr<UButton> BtnMainMenu;
	UPROPERTY() TObjectPtr<UButton> BtnQuit;

	UPROPERTY() TObjectPtr<UImage> TopBand;
	UPROPERTY() TObjectPtr<UImage> BottomBand;

	void BuildWidgetTree();

	UFUNCTION() void HRematch()   { OnRematch.ExecuteIfBound(); }
	UFUNCTION() void HMainMenu()  { OnMainMenu.ExecuteIfBound(); }
	UFUNCTION() void HQuit()      { OnQuit.ExecuteIfBound(); }
};
