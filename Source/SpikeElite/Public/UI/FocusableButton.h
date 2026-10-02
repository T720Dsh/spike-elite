// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Styling/SlateTypes.h"
#include "FocusableButton.generated.h"

/**
 * M11f-3: UButton subclass with a REAL keyboard-focus visual.
 *
 * SButton (the Slate widget behind UButton) paints only Disabled / Pressed /
 * Hovered / Normal — there is no focused brush in FButtonStyle and no default
 * focus ring, so keyboard focus was previously invisible. This subclass hooks
 * SButton's OnFocusReceived / OnFocusLost (via SetOnFocusReceived/SetOnFocusLost,
 * which SButton exposes publicly) and swaps the button style to a gold-rimmed
 * variant while focused, restoring the original style on focus loss. Hover and
 * press states keep working because the variant inherits the base hover/pressed
 * brushes.
 */
UCLASS()
class SPIKEELITE_API USEFocusableButton : public UButton
{
	GENERATED_BODY()

public:
	USEFocusableButton(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Style to show while the button holds keyboard focus. */
	void SetFocusedStyle(const FButtonStyle& InStyle);

protected:
	FButtonStyle BaseStyle;
	FButtonStyle FocusedStyle;
	bool bIsFocused = false;

	void OnFocusChanged(bool bFocused);
	void OnFocusReceivedEvent();
	void OnFocusLostEvent();
};
