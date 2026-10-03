// SPDX-License-Identifier: MIT
#include "UI/CoachPanelWidget.h"
#include "UI/SEUiStyle.h"
#include "UI/FocusableButton.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"

UCoachPanelWidget::UCoachPanelWidget(const FObjectInitializer& OI) : Super(OI) {}

static UImage* CoachMakeSolidImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Img->SetBrush(SEUiStyle::SolidBrush(Color));
	return Img;
}

static UButton* CoachMakeBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label, const FButtonStyle& Style)
{
	UButton* B = Tree->ConstructWidget<USEFocusableButton>(USEFocusableButton::StaticClass());
	B->SetStyle(Style);
	Cast<USEFocusableButton>(B)->SetFocusedStyle(Style);

	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	T->SetFont(SEUiStyle::Font(16));
	T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
	T->SetJustification(ETextJustify::Center);
	B->AddChild(T);

	USizeBox* WidthBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidthBox->SetWidthOverride(340.f);
	WidthBox->SetHeightOverride(40.f);
	if (USizeBoxSlot* ContentSlot = Cast<USizeBoxSlot>(WidthBox->AddChild(B)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(WidthBox);
	VSlot->SetPadding(FMargin(0.f, 4.f));
	VSlot->SetHorizontalAlignment(HAlign_Center);
	VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	return B;
}

TSharedRef<SWidget> UCoachPanelWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UCoachPanelWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// Panel backdrop: right side, does NOT cover the scoreboard centre.
	UImage* BG = CoachMakeSolidImage(WidgetTree, FLinearColor(0.02f, 0.05f, 0.10f, 0.88f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BG))
	{
		S->SetAnchors(FAnchors(.59f,.13f,.99f,.96f)); S->SetOffsets(FMargin(0));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("教练席")));
	Title->SetFont(SEUiStyle::Font(28));
	Title->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	Title->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Title))
	{
		S->SetAnchors(FAnchors(.60f,.14f,.98f,.20f)); S->SetOffsets(FMargin(0));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetFont(SEUiStyle::Font(13));
	StatusText->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White * 0.85f));
	StatusText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(StatusText))
	{
		S->SetAnchors(FAnchors(.60f,.21f,.98f,.31f)); S->SetOffsets(FMargin(0));
	}

	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Scroll->AddChild(Column);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Scroll))
	{
		S->SetAnchors(FAnchors(.60f,.32f,.98f,.94f)); S->SetOffsets(FMargin(0));
	}

	const FButtonStyle Normal = SEUiStyle::SecondaryButton();
	BtnTimeoutA = CoachMakeBtn(WidgetTree, Column, TEXT("A 队暂停（剩余 2）"), Normal);
	BtnTimeoutB = CoachMakeBtn(WidgetTree, Column, TEXT("B 队暂停（剩余 2）"), Normal);
	BtnSubA = CoachMakeBtn(WidgetTree, Column, TEXT("A 队完整换人名单"), Normal);
	BtnSubB = CoachMakeBtn(WidgetTree, Column, TEXT("B 队换人 1↔7（剩余 6）"), Normal);
	BtnServeZone = CoachMakeBtn(WidgetTree, Column, TEXT("发球落区：中"), Normal);
	BtnBlock = CoachMakeBtn(WidgetTree, Column, TEXT("拦网策略：单人"), Normal);
	BtnDefense = CoachMakeBtn(WidgetTree, Column, TEXT("防守深度：标准"), Normal);
	BtnSetter = CoachMakeBtn(WidgetTree, Column, TEXT("二传分配：默认"), Normal);
	BtnClose = CoachMakeBtn(WidgetTree, Column, TEXT("关闭教练席（Tab）"), SEUiStyle::PrimaryButton());

	BtnTimeoutA->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleTimeoutA);
	BtnTimeoutB->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleTimeoutB);
	BtnSubA->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleSubA);
	BtnSubB->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleSubB);
	BtnServeZone->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleServeZone);
	BtnBlock->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleBlock);
	BtnDefense->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleDefense);
	BtnSetter->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleSetter);
	BtnClose->OnClicked.AddDynamic(this, &UCoachPanelWidget::HandleClose);
	BtnTimeoutB->SetVisibility(ESlateVisibility::Collapsed);
	BtnSubB->SetVisibility(ESlateVisibility::Collapsed);
	StatusText->SetAutoWrapText(true);
}

void UCoachPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCoachPanelWidget::SetInitialFocus()
{
	if (BtnTimeoutA && BtnTimeoutA->IsValidLowLevel())
	{
		BtnTimeoutA->SetKeyboardFocus();
	}
}

void UCoachPanelWidget::SetStatus(const FString& Text)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Text));
	}
}

void UCoachPanelWidget::SetPrefLabels(const FString& ServeZone, const FString& Block, const FString& Defense, const FString& Setter)
{
	auto SetLabel = [](UButton* Btn, const FString& Text)
	{
		if (Btn && Btn->GetChildrenCount() > 0)
		{
			if (UTextBlock* T = Cast<UTextBlock>(Btn->GetChildAt(0)))
			{
				T->SetText(FText::FromString(Text));
			}
		}
	};
	SetLabel(BtnServeZone, FString::Printf(TEXT("发球落区：%s"), *ServeZone));
	SetLabel(BtnBlock, FString::Printf(TEXT("拦网策略：%s"), *Block));
	SetLabel(BtnDefense, FString::Printf(TEXT("防守深度：%s"), *Defense));
	SetLabel(BtnSetter, FString::Printf(TEXT("二传分配：%s"), *Setter));
}
