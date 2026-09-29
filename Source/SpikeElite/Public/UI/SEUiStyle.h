// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
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

	/**
	 * A runtime UFont built around the imported Microsoft YaHei UFontFace.
	 * The face asset (/Game/UI/msyh_source) is created by tools/setup_assets.py;
	 * Python cannot write the (protected) composite-font struct, so we assemble
	 * the Slate-ready UFont here in C++ at runtime and keep it rooted.
	 */
	inline UFont* ChineseFont()
	{
		static UFont* Cached = nullptr;
		if (!Cached)
		{
			UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/UI/msyh_source.msyh_source"));
			if (Face)
			{
				UFont* Built = NewObject<UFont>(GetTransientPackage(), NAME_None, RF_Transient);
				Built->FontCacheType = EFontCacheType::Runtime;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FCompositeFont& Composite = Built->GetMutableInternalCompositeFont();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				FTypefaceEntry Entry(FName(TEXT("Default")));
				Entry.Font = FFontData(Face, 0);
				Composite.DefaultTypeface.Fonts.Emplace(MoveTemp(Entry));
				Built->AddToRoot();
				Cached = Built;
			}
		}
		return Cached;
	}

	inline FSlateFontInfo Font(int32 Size)
	{
		if (UFont* F = ChineseFont())
		{
			return FSlateFontInfo(F, static_cast<float>(Size), FName(TEXT("Default")));
		}
		// Fallback: engine default font (Chinese glyphs may be missing).
		FSlateFontInfo Info;
		Info.Size = static_cast<float>(Size);
		return Info;
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
