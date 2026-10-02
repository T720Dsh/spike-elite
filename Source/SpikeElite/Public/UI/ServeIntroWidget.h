// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ServeIntroWidget.generated.h"

class UTextBlock;
class UImage;
class UCanvasPanelSlot;

/**
 * M11h-3: broadcast-style server introduction card. Shown by the PlayerController
 * during the ServePresentation phase (before the service whistle), one half of
 * the screen so the net and the receiving team stay visible. A short name/number
 * bar is used when the same player serves again consecutively.
 */
UCLASS()
class SPIKEELITE_API UServeIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UServeIntroWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Set the server's identity; animates the card in. */
	void SetServer(int32 JerseyNumber, const FString& Name, const FString& TeamLabel,
		const FString& Role, bool bShortBar);

	/** Fade out and hide (called when the phase ends). */
	void FadeOut();

	bool IsFullyHidden() const { return bHidden; }

protected:
	UPROPERTY() TObjectPtr<UTextBlock> NumberText;
	UPROPERTY() TObjectPtr<UTextBlock> NameText;
	UPROPERTY() TObjectPtr<UTextBlock> TeamRoleText;
	UPROPERTY() TObjectPtr<UImage> PanelBG;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> PanelSlot;

	float Alpha = 0.f;
	bool bFadingIn = false;
	bool bFadingOut = false;
	bool bHidden = true;
	bool bShort = false;

	void BuildWidgetTree();
};
