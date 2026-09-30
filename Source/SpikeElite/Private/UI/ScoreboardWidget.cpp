// SPDX-License-Identifier: MIT
#include "UI/ScoreboardWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"

UScoreboardWidget::UScoreboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Leave empty: UHT defines the ctor body in gen.cpp.
}

TSharedRef<SWidget> UScoreboardWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UScoreboardWidget::BuildWidgetTree()
{
	// Build the widget tree in C++: Canvas -> [backdrop + VerticalBox (top-center)].
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// M11a: semi-transparent dark backdrop so the scoreboard stays readable over
	// the bright arena floor. Fixed size (top-center) so it never jitters between
	// the collapsed/expanded help states.
	Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Backdrop->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.02f, 0.02f, 0.04f, 0.55f)));
	if (UCanvasPanelSlot* BSlot = Root->AddChildToCanvas(Backdrop))
	{
		BSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		BSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		BSlot->SetPosition(FVector2D(0.0f, 6.0f));
		BSlot->SetSize(FVector2D(920.0f, 330.0f));
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box);
	BoxSlot->SetPosition(FVector2D(0.0f, 18.0f));
	BoxSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BoxSlot->SetAutoSize(true);

	auto MakeRow = [&](FLinearColor Color, int32 FontSize, float TopPad) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetFont(SEUiStyle::Font(FontSize));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(T);
		Slot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		return T;
	};

	// M11a: aligned, evenly spaced rows (SET / score / sets / phase / possession
	// / ball / rally result / help) — compact enough to never overlap at 720p.
	Text_Set        = MakeRow(FLinearColor(1.0f, 0.9f, 0.2f), 24, 0.f);
	Text_Score      = MakeRow(FLinearColor(1.0f, 1.0f, 1.0f), 36, 2.f);
	Text_Sets       = MakeRow(FLinearColor(0.4f, 0.8f, 1.0f), 18, 4.f);
	Text_Phase      = MakeRow(FLinearColor(0.6f, 0.95f, 0.6f), 16, 4.f);
	Text_Possession = MakeRow(FLinearColor(1.0f, 0.75f, 0.35f), 16, 2.f);
	Text_Ball       = MakeRow(FLinearColor(1.0f, 0.6f, 0.2f), 18, 2.f);
	Text_RallyResult = MakeRow(FLinearColor(1.0f, 1.0f, 0.4f), 20, 2.f);
	Text_Help       = MakeRow(FLinearColor(0.75f, 0.78f, 0.82f), 14, 4.f);

	// M11a: full controls help appears at match start and collapses after a few
	// seconds; the pause menu offers the same list on demand.
	Text_Help->SetText(FText::FromString(TEXT("WASD 移动 · 空格 跳 · 左键 击球 · E 发球 · C/V 切视角 · Esc 暂停")));
	LastHelp = TEXT("WASD 移动 · 空格 跳 · 左键 击球 · E 发球 · C/V 切视角 · Esc 暂停");
	HelpTimer = 5.0f;
	bHelpCollapsed = false;
}

void UScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// M11a: auto-collapse the controls help after ~5 s of in-game time. Widget
	// ticks are Slate-driven and keep running while the world is paused, so the
	// collapse also fires cleanly on the paused / match-over screens.
	if (!bHelpCollapsed)
	{
		HelpTimer -= InDeltaTime;
		if (HelpTimer <= 0.0f)
		{
			bHelpCollapsed = true;
			const FString Collapsed = TEXT("ESC：暂停 / 释放鼠标");
			if (Text_Help && LastHelp != Collapsed)
			{
				Text_Help->SetText(FText::FromString(Collapsed));
				LastHelp = Collapsed;
			}
		}
	}
}

void UScoreboardWidget::UpdateScore(int32 SetNum, int32 AScore, int32 BScore, int32 ASets, int32 BSets,
	bool bTeamAServing, const FString& BallHint,
	const FString& Phase, const FString& Possession, const FString& ServeHint,
	const FString& RallyResult)
{
	// M11a: SetText only when a row's content actually changed. The GameMode
	// already skips whole updates via its signature; this is the second guard so
	// the widget never rewrites identical text on dynamic-only frames.
	const FString Set   = FString::Printf(TEXT("SET %d"), SetNum);
	const FString Score = FString::Printf(TEXT("A  %d  :  %d  B"), AScore, BScore);
	const FString Sets  = FString::Printf(TEXT("Sets  A %d - %d B    Serve: %s"),
		ASets, BSets, bTeamAServing ? TEXT("A") : TEXT("B"));
	const FString PhaseLine = FString::Printf(TEXT("阶段：%s"), *Phase);
	FString PossLine = FString::Printf(TEXT("控球 / 触球：%s"), *Possession);
	if (!ServeHint.IsEmpty())
	{
		PossLine += FString::Printf(TEXT("    %s"), *ServeHint);
	}

	if (Text_Set && LastSet != Set)          { Text_Set->SetText(FText::FromString(Set)); LastSet = Set; }
	if (Text_Score && LastScore != Score)    { Text_Score->SetText(FText::FromString(Score)); LastScore = Score; }
	if (Text_Sets && LastSets != Sets)       { Text_Sets->SetText(FText::FromString(Sets)); LastSets = Sets; }
	if (Text_Phase && LastPhase != PhaseLine){ Text_Phase->SetText(FText::FromString(PhaseLine)); LastPhase = PhaseLine; }
	if (Text_Possession && LastPossession != PossLine)
	{
		Text_Possession->SetText(FText::FromString(PossLine)); LastPossession = PossLine;
	}
	if (Text_Ball && LastBall != BallHint)
	{
		Text_Ball->SetText(FText::FromString(BallHint)); LastBall = BallHint;
	}
	if (Text_RallyResult && LastRallyResult != RallyResult)
	{
		Text_RallyResult->SetText(RallyResult.IsEmpty() ? FText::GetEmpty() : FText::FromString(RallyResult));
		LastRallyResult = RallyResult;
	}
}
