// SPDX-License-Identifier: MIT
#include "UI/MatchEndWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"

UMatchEndWidget::UMatchEndWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static UButton* MakeMatchEndBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label,
	const FLinearColor& Base, const FLinearColor& Hover, int32 FontSize)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(SEUiStyle::ButtonStyle(Base, Hover,
		FLinearColor(Base.R * 0.6f, Base.G * 0.6f, Base.B * 0.6f, 1.f)));

	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	T->SetFont(SEUiStyle::Font(FontSize));
	T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	T->SetJustification(ETextJustify::Center);
	B->AddChild(T);

	USizeBox* WidthBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidthBox->SetWidthOverride(300.f);
	if (USizeBoxSlot* ContentSlot = Cast<USizeBoxSlot>(WidthBox->AddChild(B)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(WidthBox);
	VSlot->SetPadding(FMargin(0.f, 8.f));
	VSlot->SetHorizontalAlignment(HAlign_Center);
	VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	return B;
}

TSharedRef<SWidget> UMatchEndWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UMatchEndWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// Dim overlay.
	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Dim->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.02f, 0.02f, 0.04f, 0.85f)));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
		S->SetAutoSize(true);
	}

	Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("比赛结束")));
	Title->SetFont(SEUiStyle::Font(56));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.1f)));
	Title->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(Title);

	WinnerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	WinnerText->SetFont(SEUiStyle::Font(34));
	WinnerText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.6f, 0.2f)));
	WinnerText->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(WinnerText);

	SetScoresText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SetScoresText->SetFont(SEUiStyle::Font(20));
	SetScoresText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.9f)));
	SetScoresText->SetJustification(ETextJustify::Center);
	SetScoresText->SetAutoWrapText(true);
	Col->AddChildToVerticalBox(SetScoresText);

	UTextBlock* Sp = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sp->SetText(FText::FromString(TEXT(" ")));
	Col->AddChildToVerticalBox(Sp);

	BtnRematch  = MakeMatchEndBtn(WidgetTree, Col, TEXT("再来一场"), FLinearColor(0.10f,0.50f,0.95f,1), FLinearColor(0.30f,0.68f,1.0f,1), 26);
	BtnMainMenu = MakeMatchEndBtn(WidgetTree, Col, TEXT("返回主菜单"), FLinearColor(0.16f,0.18f,0.24f,1), FLinearColor(0.28f,0.34f,0.46f,1), 22);
	BtnQuit     = MakeMatchEndBtn(WidgetTree, Col, TEXT("退出到桌面"), FLinearColor(0.30f,0.10f,0.10f,1), FLinearColor(0.55f,0.20f,0.20f,1), 22);
}

void UMatchEndWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BtnRematch)  BtnRematch->OnClicked.AddUniqueDynamic(this, &UMatchEndWidget::HRematch);
	if (BtnMainMenu) BtnMainMenu->OnClicked.AddUniqueDynamic(this, &UMatchEndWidget::HMainMenu);
	if (BtnQuit)     BtnQuit->OnClicked.AddUniqueDynamic(this, &UMatchEndWidget::HQuit);
}

void UMatchEndWidget::SetResult(EVolleyballTeam Winner, const TArray<int32>& ScoresA, const TArray<int32>& ScoresB)
{
	const bool bA = (Winner == EVolleyballTeam::TeamA);
	if (WinnerText)
	{
		WinnerText->SetText(FText::FromString(FString::Printf(TEXT("%s 队获胜！"), bA ? TEXT("A") : TEXT("B"))));
	}
	if (SetScoresText)
	{
		FString Lines;
		for (int32 i = 0; i < ScoresA.Num() && i < ScoresB.Num(); i++)
		{
			Lines += FString::Printf(TEXT("第 %d 局   A %d : %d B\n"), i + 1, ScoresA[i], ScoresB[i]);
		}
		SetScoresText->SetText(FText::FromString(Lines));
	}
}
