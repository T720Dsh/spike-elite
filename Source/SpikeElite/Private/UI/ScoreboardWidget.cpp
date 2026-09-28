// SPDX-License-Identifier: MIT
#include "UI/ScoreboardWidget.h"
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

void UScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Build the widget tree in C++: Canvas -> VerticalBox (top-center) -> 4 text rows.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box);
	BoxSlot->SetPosition(FVector2D(480.0f, 20.0f));
	BoxSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));

	auto MakeRow = [&](FLinearColor Color, int32 FontSize) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = T->Font;
		Font.Size = FontSize;
		T->SetFont(Font);
		T->SetColorAndOpacity(FSlateColor(Color));
		Box->AddChildToVerticalBox(T);
		return T;
	};

	Text_Set   = MakeRow(FLinearColor(1.0f, 0.9f, 0.2f), 24);
	Text_Score = MakeRow(FLinearColor(1.0f, 1.0f, 1.0f), 36);
	Text_Sets  = MakeRow(FLinearColor(0.4f, 0.8f, 1.0f), 18);
	Text_Ball  = MakeRow(FLinearColor(1.0f, 0.6f, 0.2f), 18);
}

void UScoreboardWidget::UpdateScore(int32 SetNum, int32 AScore, int32 BScore, int32 ASets, int32 BSets,
	bool bTeamAServing, const FString& BallHint)
{
	if (Text_Set)   Text_Set->SetText(FText::FromString(FString::Printf(TEXT("SET %d"), SetNum)));
	if (Text_Score) Text_Score->SetText(FText::FromString(FString::Printf(TEXT("A  %d  :  %d  B"), AScore, BScore)));
	if (Text_Sets)  Text_Sets->SetText(FText::FromString(FString::Printf(TEXT("Sets  A %d - %d B    Serve: %s"),
		ASets, BSets, bTeamAServing ? TEXT("A") : TEXT("B"))));
	if (Text_Ball)  Text_Ball->SetText(FText::FromString(BallHint));
}
