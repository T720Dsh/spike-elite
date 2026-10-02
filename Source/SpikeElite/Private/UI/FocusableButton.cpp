// SPDX-License-Identifier: MIT
#include "UI/FocusableButton.h"
#include "UI/SEUiStyle.h"
#include "Widgets/Input/SButton.h"

USEFocusableButton::USEFocusableButton(const FObjectInitializer& OI)
	: Super(OI)
{
	// UButton already defaults IsFocusable = true; nothing to force here.
}

TSharedRef<SWidget> USEFocusableButton::RebuildWidget()
{
	// UButton::RebuildWidget builds MyButton (TSharedPtr<SButton>) and returns
	// it. We cast to SButton (public API: SetOnFocusReceived / SetOnFocusLost,
	// both FSimpleDelegate) to drive our visible focus state.
	TSharedRef<SWidget> Widget = Super::RebuildWidget();

	if (MyButton.IsValid())
	{
		MyButton->SetOnFocusReceived(FSimpleDelegate::CreateUObject(this, &USEFocusableButton::OnFocusReceivedEvent));
		MyButton->SetOnFocusLost(FSimpleDelegate::CreateUObject(this, &USEFocusableButton::OnFocusLostEvent));
	}
	return Widget;
}

void USEFocusableButton::SetFocusedStyle(const FButtonStyle& InStyle)
{
	BaseStyle = InStyle;
	const bool bDanger = false; // (caller may special-case; kept simple)
	FocusedStyle = SEUiStyle::FocusedVariant(InStyle, bDanger);
	if (bIsFocused)
	{
		SetStyle(FocusedStyle);
	}
	else
	{
		SetStyle(BaseStyle);
	}
}

void USEFocusableButton::OnFocusChanged(bool bFocused)
{
	if (bIsFocused == bFocused)
	{
		return;
	}
	bIsFocused = bFocused;
	SetStyle(bIsFocused ? FocusedStyle : BaseStyle);
}

// Delegate shims (SButton fires these with no arguments).
void USEFocusableButton::OnFocusReceivedEvent()
{
	OnFocusChanged(true);
}

void USEFocusableButton::OnFocusLostEvent()
{
	OnFocusChanged(false);
}
