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
	const FButtonStyle& Style, int32 FontSize)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(Style);

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

	// Controlled dim: arena stays partially visible behind the result card.
	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Dim->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.02f, 0.02f, 0.05f, 0.55f)));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
	}

	// Give the result hierarchy its own readable broadcast card.  Keeping the
	// arena visible is useful, but putting type directly over the player's back
	// made the earlier result screen change contrast from frame to frame.
	UImage* Card = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Card->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.025f, 0.055f, 0.10f, 0.90f)));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Card))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
		S->SetSize(FVector2D(560.f, 590.f));
	}

	// Team-colour bands framing the result (M11d-2 victory hierarchy).
	TopBand = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TopBand->SetBrush(SEUiStyle::SolidBrush(SEUiStyle::Colors::TeamA));
	TopBand->SetRenderOpacity(0.85f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(TopBand))
	{
		S->SetAnchors(FAnchors(0,0,1,0)); S->SetOffsets(FMargin(0.f, 0.f, 0.f, 6.f));
	}
	BottomBand = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	BottomBand->SetBrush(SEUiStyle::SolidBrush(SEUiStyle::Colors::TeamB));
	BottomBand->SetRenderOpacity(0.85f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BottomBand))
	{
		S->SetAnchors(FAnchors(0,1,1,1)); S->SetOffsets(FMargin(0.f, -6.f, 0.f, 0.f));
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
	Title->SetFont(SEUiStyle::FontBold(56));
	Title->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	Title->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(Title);

	WinnerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	WinnerText->SetFont(SEUiStyle::FontBold(38));
	WinnerText->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::TeamA));
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

	BtnRematch  = MakeMatchEndBtn(WidgetTree, Col, TEXT("再来一场"), SEUiStyle::PrimaryButton(), 26);
	BtnMainMenu = MakeMatchEndBtn(WidgetTree, Col, TEXT("返回主菜单"), SEUiStyle::SecondaryButton(), 22);
	BtnQuit     = MakeMatchEndBtn(WidgetTree, Col, TEXT("退出到桌面"), SEUiStyle::DangerButton(), 22);
}

void UMatchEndWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BtnRematch)  BtnRematch->OnClicked.AddUniqueDynamic(this, &UMatchEndWidget::HRematch);
	if (BtnMainMenu) BtnMainMenu->OnClicked.AddUniqueDynamic(this, &UMatchEndWidget::HMainMenu);
	if (BtnQuit)     BtnQuit->OnClicked.AddUniqueDynamic(this, &UMatchEndWidget::HQuit);
}

// M11f-3: focus is applied AFTER the widget is in the viewport (NativeConstruct
// is too early for SetKeyboardFocus). Safe, forward action stays 再来一场.
void UMatchEndWidget::SetInitialFocus()
{
	if (BtnRematch)
	{
		BtnRematch->SetKeyboardFocus();
	}
}

void UMatchEndWidget::SetResult(EVolleyballTeam Winner, const TArray<int32>& ScoresA, const TArray<int32>& ScoresB)
{
	const bool bA = (Winner == EVolleyballTeam::TeamA);
	if (WinnerText)
	{
		WinnerText->SetText(FText::FromString(FString::Printf(TEXT("%s 队获胜！"), bA ? TEXT("A") : TEXT("B"))));
		WinnerText->SetColorAndOpacity(FSlateColor(bA ? SEUiStyle::Colors::TeamA : SEUiStyle::Colors::TeamB));
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
