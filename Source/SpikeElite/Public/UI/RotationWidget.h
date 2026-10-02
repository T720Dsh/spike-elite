// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Volleyball/VolleyballRules.h"
#include "RotationWidget.generated.h"

class UTextBlock;
class UImage;
class UCanvasPanel;

/**
 * M11d-3: right-top "mini-court" rotation HUD.
 *
 * Draws a stylized vertical court with net + three-metre lines, two coloured
 * 2x3 formations (Team A blue, Team B orange), a gold ring on the server, a
 * white star on the controlled player, and a title with rotation / serving
 * team / whether a side-out rotation just happened.
 *
 * The widget holds NO own score/rotation copy: it renders FRotationViewState
 * built by the GameMode (the single authority). SetText / SetBrush only fire
 * when the signature changes. Toggle with the H key.
 */
UCLASS()
class SPIKEELITE_API URotationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Refresh(const FRotationViewState& State);
	void ToggleVisible();
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void BuildWidgetTree();

	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	/** Court outline / net / 3m lines (visual only). */
	UPROPERTY() TObjectPtr<UImage> CourtFrame;
	UPROPERTY() TObjectPtr<UImage> NetLine;
	UPROPERTY() TObjectPtr<UImage> LineA3m;
	UPROPERTY() TObjectPtr<UImage> LineB3m;
	/** 12 slot cells: 0..5 Team A (P1..P6), 6..11 Team B (P1..P6). */
	UPROPERTY() TArray<TObjectPtr<UImage>> SlotDots;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> SlotTexts;
	UPROPERTY() TArray<TObjectPtr<UImage>> ServeMarkers;

	FString LastSignature;
	FString BaseTitle;
	float RotationNoticeSeconds = 0.f;
};
