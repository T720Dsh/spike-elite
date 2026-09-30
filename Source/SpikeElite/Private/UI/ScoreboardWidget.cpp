// SPDX-License-Identifier: MIT
#include "UI/ScoreboardWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"

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
	// Build the widget tree in C++: Canvas -> VerticalBox (top-center) -> text rows.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box);
	BoxSlot->SetPosition(FVector2D(0.0f, 20.0f));
	BoxSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BoxSlot->SetAutoSize(true);

	auto MakeRow = [&](FLinearColor Color, int32 FontSize) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetFont(SEUiStyle::Font(FontSize));
		T->SetColorAndOpacity(FSlateColor(Color));
		Box->AddChildToVerticalBox(T);
		return T;
	};

	Text_Set        = MakeRow(FLinearColor(1.0f, 0.9f, 0.2f), 24);
	Text_Score      = MakeRow(FLinearColor(1.0f, 1.0f, 1.0f), 36);
	Text_Sets       = MakeRow(FLinearColor(0.4f, 0.8f, 1.0f), 18);
	Text_Phase      = MakeRow(FLinearColor(0.6f, 0.95f, 0.6f), 16);
	Text_Possession = MakeRow(FLinearColor(1.0f, 0.75f, 0.35f), 16);
	Text_Ball       = MakeRow(FLinearColor(1.0f, 0.6f, 0.2f), 18);
	Text_RallyResult = MakeRow(FLinearColor(1.0f, 1.0f, 0.4f), 20);
	Text_Help       = MakeRow(FLinearColor(0.75f, 0.78f, 0.82f), 14);
	Text_Help->SetText(FText::FromString(TEXT("ESC：暂停 / 释放鼠标")));
}

void UScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UScoreboardWidget::UpdateScore(int32 SetNum, int32 AScore, int32 BScore, int32 ASets, int32 BSets,
	bool bTeamAServing, const FString& BallHint,
	const FString& Phase, const FString& Possession, const FString& ServeHint,
	const FString& RallyResult)
{
	if (Text_Set)   Text_Set->SetText(FText::FromString(FString::Printf(TEXT("SET %d"), SetNum)));
	if (Text_Score) Text_Score->SetText(FText::FromString(FString::Printf(TEXT("A  %d  :  %d  B"), AScore, BScore)));
	if (Text_Sets)  Text_Sets->SetText(FText::FromString(FString::Printf(TEXT("Sets  A %d - %d B    Serve: %s"),
		ASets, BSets, bTeamAServing ? TEXT("A") : TEXT("B"))));
	if (Text_Phase) Text_Phase->SetText(FText::FromString(FString::Printf(TEXT("阶段：%s"), *Phase)));
	if (Text_Possession)
	{
		FString Line = FString::Printf(TEXT("控球 / 触球：%s"), *Possession);
		if (!ServeHint.IsEmpty())
		{
			Line += FString::Printf(TEXT("    %s"), *ServeHint);
		}
		Text_Possession->SetText(FText::FromString(Line));
	}
	if (Text_Ball)  Text_Ball->SetText(FText::FromString(BallHint));
	if (Text_RallyResult)
	{
		Text_RallyResult->SetText(RallyResult.IsEmpty() ? FText::GetEmpty() : FText::FromString(RallyResult));
	}
}
