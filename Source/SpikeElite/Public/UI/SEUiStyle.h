// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/SoftObjectPtr.h"

/**
 * Shared pure-C++ UI styling helpers so every menu uses the same Chinese font,
 * solid-colour brushes and real normal/hover/pressed button states.
 */
namespace SEUiStyle
{
	inline UTexture2D* WhiteTexture()
	{
		static UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		return Tex;
	}

	inline FSlateFontInfo Font(int32 Size)
	{
		// Unreal's runtime core composite font includes its own international
		// fallbacks (including CJK). This avoids bundling a proprietary Windows
		// system font and works on machines where Microsoft YaHei is unavailable.
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	inline FSlateBrush SolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		if (UTexture2D* Tex = WhiteTexture())
		{
			Brush.SetResourceObject(Tex);
			Brush.DrawAs = ESlateBrushDrawType::Image;
		}
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	/** A solid rounded-less button with distinct normal / hover / pressed tints. */
	inline FButtonStyle ButtonStyle(const FLinearColor& Normal, const FLinearColor& Hover, const FLinearColor& Pressed)
	{
		FButtonStyle S;
		S.SetNormal(SolidBrush(Normal));
		S.SetHovered(SolidBrush(Hover));
		S.SetPressed(SolidBrush(Pressed));
		S.SetDisabled(SolidBrush(FLinearColor(0.15f, 0.15f, 0.17f, 1.f)));
		S.SetNormalPadding(FMargin(18.f, 10.f));
		S.SetPressedPadding(FMargin(18.f, 12.f));
		return S;
	}
}
