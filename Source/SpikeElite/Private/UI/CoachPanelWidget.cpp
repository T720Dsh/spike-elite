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
		S->SetAnchors(FAnchors(1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetSize(FVector2D(420.f, 720.f));
		S->SetAutoSize(false);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("教练席")));
	Title->SetFont(SEUiStyle::Font(28));
	Title->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	Title->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Title))
	{
		S->SetAnchors(FAnchors(1.f, 0.f));
		S->SetAlignment(FVector2D(1.f, 0.f));
		S->SetPosition(FVector2D(-440.f, 20.f));
		S->SetSize(FVector2D(400.f, 40.f));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetFont(SEUiStyle::Font(13));
	StatusText->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White * 0.85f));
	StatusText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(StatusText))
	{
		S->SetAnchors(FAnchors(1.f, 0.f));
		S->SetAlignment(FVector2D(1.f, 0.f));
		S->SetPosition(FVector2D(-440.f, 66.f));
		S->SetSize(FVector2D(400.f, 44.f));
	}

	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Column))
	{
		S->SetAnchors(FAnchors(1.f, 0.f));
		S->SetAlignment(FVector2D(1.f, 0.f));
		S->SetPosition(FVector2D(-440.f, 118.f));
		S->SetSize(FVector2D(360.f, 560.f));
	}

	const FButtonStyle Normal = SEUiStyle::SecondaryButton();
	BtnTimeoutA = CoachMakeBtn(WidgetTree, Column, TEXT("A 队暂停（剩余 2）"), Normal);
	BtnTimeoutB = CoachMakeBtn(WidgetTree, Column, TEXT("B 队暂停（剩余 2）"), Normal);
	BtnSubA = CoachMakeBtn(WidgetTree, Column, TEXT("A 队换人 1↔7（剩余 6）"), Normal);
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
