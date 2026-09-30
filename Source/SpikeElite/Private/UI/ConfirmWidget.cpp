// SPDX-License-Identifier: MIT
#include "UI/ConfirmWidget.h"
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

UConfirmWidget::UConfirmWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static UButton* MakeConfirmBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label,
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
	WidthBox->SetWidthOverride(260.f);
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

TSharedRef<SWidget> UConfirmWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UConfirmWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Dim->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.02f, 0.02f, 0.04f, 0.75f)));
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

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MessageText->SetFont(SEUiStyle::Font(24));
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f)));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(true);
	Col->AddChildToVerticalBox(MessageText);

	UTextBlock* Sp = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sp->SetText(FText::FromString(TEXT(" ")));
	Col->AddChildToVerticalBox(Sp);

	BtnConfirm = MakeConfirmBtn(WidgetTree, Col, TEXT("确认"), FLinearColor(0.10f,0.50f,0.95f,1), FLinearColor(0.30f,0.68f,1.0f,1), 22);
	BtnCancel  = MakeConfirmBtn(WidgetTree, Col, TEXT("取消"), FLinearColor(0.22f,0.24f,0.30f,1), FLinearColor(0.34f,0.38f,0.46f,1), 22);
}

void UConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BtnConfirm) BtnConfirm->OnClicked.AddUniqueDynamic(this, &UConfirmWidget::HConfirm);
	if (BtnCancel)  BtnCancel->OnClicked.AddUniqueDynamic(this, &UConfirmWidget::HCancel);

	// M11a: keyboard focus starts on 取消 so Enter/Space can never accidentally
	// confirm a destructive action. Tab/arrow keys then navigate both buttons.
	if (BtnCancel)
	{
		BtnCancel->SetKeyboardFocus();
	}
}

void UConfirmWidget::SetMessage(const FString& Text)
{
	if (MessageText)
	{
		MessageText->SetText(FText::FromString(Text));
	}
}
