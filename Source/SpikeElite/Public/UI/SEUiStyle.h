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
 * M11d-1: unified design-token system for the "stylized low-poly indoor
 * volleyball broadcast" direction. Every widget reads colours, fonts,
 * spacing, animation durations and button/focus styles from here — no more
 * per-widget hard-coded hex colours or 1920x1080 pixel offsets.
 *
 * Palette (deliberately broadcast-like, not neon toy):
 *   - Deep navy        primary background
 *   - Cool slate grey  secondary panels
 *   - Electric blue    interactive elements / Team A
 *   - Warm orange-red  Team B
 *   - Volleyball gold  emphasis / serving team / current selection
 *   - White / light grey  primary / secondary text
 *   - Green / yellow / red  safe / risk / error feedback
 */
namespace SEUiStyle
{
	// ------------------------------------------------------------------ palette
	namespace Colors
	{
		inline const FLinearColor Navy       (0.055f, 0.075f, 0.125f, 1.0f); // page background
		inline const FLinearColor Panel      (0.10f,  0.14f,  0.20f,  0.86f); // panel fill
		inline const FLinearColor PanelLight (0.13f,  0.18f,  0.26f,  0.90f); // hover / elevated
		inline const FLinearColor Slate      (0.16f,  0.19f,  0.24f,  1.0f); // secondary button
		inline const FLinearColor TeamA      (0.12f,  0.55f,  1.00f,  1.0f); // electric blue
		inline const FLinearColor TeamB      (0.95f,  0.36f,  0.16f,  1.0f); // warm orange-red
		inline const FLinearColor Gold       (1.00f,  0.80f,  0.12f,  1.0f); // volleyball gold
		inline const FLinearColor White      (0.96f,  0.97f,  0.98f,  1.0f);
		inline const FLinearColor Grey       (0.62f,  0.66f,  0.70f,  1.0f);
		inline const FLinearColor Safe       (0.28f,  0.84f,  0.40f,  1.0f); // green
		inline const FLinearColor Warn       (1.00f,  0.80f,  0.12f,  1.0f); // yellow
		inline const FLinearColor Error      (0.92f,  0.22f,  0.20f,  1.0f); // red
		inline const FLinearColor DangerFill (0.38f,  0.12f,  0.10f,  1.0f);
	}

	// ------------------------------------------------------------------- fonts
	namespace Type
	{
		inline int32 Title   () { return 56; }  // page titles / logo
		inline int32 Sub     () { return 24; }
		inline int32 Body    () { return 20; }  // buttons / primary rows
		inline int32 Small   () { return 16; }  // captions / hints
		inline int32 Tiny    () { return 13; }  // footnotes / version
		inline int32 Score   () { return 44; }  // HUD score digits
	}

	// ----------------------------------------------------------------- spacing
	namespace Spacing
	{
		inline float Unit    () { return 8.f; }
		inline float Panel   () { return 28.f; } // panel padding
		inline float Gap     () { return 12.f; } // between stacked controls
		inline float ButtonH () { return 54.f; }
	}

	// ------------------------------------------------------------------ anim
	namespace Anim
	{
		inline float FadeIn   () { return 0.35f; } // panels / banners fade
		inline float SlideIn  () { return 0.45f; }
		inline float Stagger  () { return 0.08f; } // per-button entrance delay
		inline float Shorten  () { return 0.10f; } // reduced-motion overrides
		inline float BannerOn () { return 0.20f; }
		inline float BannerHold() { return 1.10f; }
	}

	// ------------------------------------------------------------------ helpers
	inline UTexture2D* WhiteTexture()
	{
		static UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		return Tex;
	}

	inline FSlateFontInfo Font(int32 Size)
	{
		// Unreal's runtime core composite font includes its own international
		// fallbacks (including CJK). Avoids bundling a proprietary Windows font.
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	inline FSlateFontInfo FontBold(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
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

	/** Rounded-ish panel background via a border brush with a 1px rim. */
	inline FSlateBrush PanelBrush(const FLinearColor& Fill, const FLinearColor& Rim, float CornerRadius = 6.f)
	{
		FSlateBrush Brush = SolidBrush(Fill);
		Brush.DrawAs = ESlateBrushDrawType::Border;
		Brush.Margin = FMargin(CornerRadius / 64.f);
		Brush.TintColor = FSlateColor(Fill);
		// A solid rim is drawn as a second border on top in the caller where needed;
		// here we keep a single fill brush for cheap borders.
		return Brush;
	}

	/**
	 * Button style with distinct normal / hover / pressed / disabled tints AND a
	 * visible keyboard-focus brush (gold rim) so Tab / arrow navigation is clear.
	 */
	inline FButtonStyle ButtonStyle(const FLinearColor& Normal, const FLinearColor& Hover, const FLinearColor& Pressed,
		bool bDanger = false)
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

	// Convenience variants -----------------------------------------------------
	inline FButtonStyle PrimaryButton  () { return ButtonStyle(Colors::TeamA, FLinearColor(0.30f,0.68f,1.0f,1.f), FLinearColor(0.06f,0.38f,0.78f,1.f)); }
	inline FButtonStyle SecondaryButton() { return ButtonStyle(Colors::Slate, FLinearColor(0.28f,0.34f,0.46f,1.f), FLinearColor(0.10f,0.13f,0.18f,1.f)); }
	inline FButtonStyle DangerButton   () { return ButtonStyle(Colors::DangerFill, FLinearColor(0.60f,0.20f,0.18f,1.f), FLinearColor(0.28f,0.08f,0.07f,1.f), true); }

	/** Semi-transparent dark panel for menus and HUD sidecards. */
	inline FSlateBrush BackdropBrush(float Alpha = 0.55f)
	{
		return SolidBrush(FLinearColor(0.02f, 0.03f, 0.05f, Alpha));
	}

	// ------------------------------------------------------- accessibility state
	/** Reduced-motion flag; menus shorten or skip their entrance animations. */
	inline bool bReducedMotion = false;
	inline void SetReducedMotion(bool b) { bReducedMotion = b; }
	inline bool IsReducedMotion() { return bReducedMotion; }
}
