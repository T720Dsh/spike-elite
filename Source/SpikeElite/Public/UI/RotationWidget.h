// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Volleyball/VolleyballRules.h"
#include "RotationWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

/**
 * Right-top rotation HUD. Shows the current rotation (1/6), serving team,
 * server's jersey, all six P1-P6 slots per team with the front-row band, the
 * local player highlight and the server mark.
 *
 * The widget holds NO own score/rotation copy: it renders FRotationViewState
 * built by the GameMode (the single authority). SetText is only called when the
 * signature of the view changes. Toggle with the H key (PlayerController).
 */
UCLASS()
class SPIKEELITE_API URotationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Push a new authoritative rotation snapshot; no-op when nothing changed. */
	void Refresh(const FRotationViewState& State);

	/** Show/hide the widget (H key). */
	void ToggleVisible();

	virtual void NativeConstruct() override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void BuildWidgetTree();

	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> NetLabel;
	/** 12 slot cells: 0..5 Team A (P1..P6), 6..11 Team B (P1..P6). */
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> SlotTexts;

	FString LastSignature;
};
