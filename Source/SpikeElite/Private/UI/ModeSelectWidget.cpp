// SPDX-License-Identifier: MIT
#include "UI/ModeSelectWidget.h"
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

static UImage* ModeMakeSolidImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Img->SetBrush(SEUiStyle::SolidBrush(Color));
	return Img;
}

UModeSelectWidget::UModeSelectWidget(const FObjectInitializer& OI) : Super(OI) {}

static UButton* ModeMakeBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label,
	const FString& Sub, const FButtonStyle& Style)
{
	UButton* B = Tree->ConstructWidget<USEFocusableButton>(USEFocusableButton::StaticClass());
	B->SetStyle(Style);
	Cast<USEFocusableButton>(B)->SetFocusedStyle(Style);

	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	T->SetFont(SEUiStyle::Font(22));
	T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
	T->SetJustification(ETextJustify::Center);
	B->AddChild(T);

	if (!Sub.IsEmpty())
	{
		UTextBlock* S = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		S->SetText(FText::FromString(Sub));
		S->SetFont(SEUiStyle::Font(13));
		S->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White * 0.75f));
		S->SetJustification(ETextJustify::Center);
		B->AddChild(S);
	}

	USizeBox* WidthBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidthBox->SetWidthOverride(420.f);
	WidthBox->SetHeightOverride(Sub.IsEmpty() ? SEUiStyle::Spacing::ButtonH() : 74.f);
	if (USizeBoxSlot* ContentSlot = Cast<USizeBoxSlot>(WidthBox->AddChild(B)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(WidthBox);
	VSlot->SetPadding(FMargin(0.f, 6.f));
	VSlot->SetHorizontalAlignment(HAlign_Center);
	VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	return B;
}

TSharedRef<SWidget> UModeSelectWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UModeSelectWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UImage* BG = ModeMakeSolidImage(WidgetTree, SEUiStyle::Colors::Navy);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BG))
	{
		S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0));
	}

	Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("选择比赛模式")));
	Title->SetFont(SEUiStyle::Font(40));
	Title->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	Title->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Title))
	{
		S->SetAnchors(FAnchors(0.5f, 0.08f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(900.f, 60.f));
	}

	ButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(ButtonColumn))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(460.f, 540.f));
	}

	const FButtonStyle Normal = SEUiStyle::SecondaryButton();
	BtnQuickShort = ModeMakeBtn(WidgetTree, ButtonColumn, TEXT("快速体验 · 短局"), TEXT("1 局 3 分，快速上手"), Normal);
	BtnQuickFull = ModeMakeBtn(WidgetTree, ButtonColumn, TEXT("快速体验 · 正式五局"), TEXT("完整 25/15 分规则"), Normal);
	BtnChallenge = ModeMakeBtn(WidgetTree, ButtonColumn, TEXT("12 人名单挑战赛"), TEXT("连续 3 场原创对手，进度可保存"), Normal);
	BtnCoach = ModeMakeBtn(WidgetTree, ButtonColumn, TEXT("教练模式"), TEXT("场下指挥，12 名球员 AI 作战"), Normal);
	BtnTraining = ModeMakeBtn(WidgetTree, ButtonColumn, TEXT("训练模式"), TEXT("发球落点 / 接发到位 / 二传配攻"), Normal);
	BtnBack = ModeMakeBtn(WidgetTree, ButtonColumn, TEXT("返回主菜单"), TEXT(""), SEUiStyle::SecondaryButton());

	BtnQuickShort->OnClicked.AddDynamic(this, &UModeSelectWidget::HandleQuickShort);
	BtnQuickFull->OnClicked.AddDynamic(this, &UModeSelectWidget::HandleQuickFull);
	BtnChallenge->OnClicked.AddDynamic(this, &UModeSelectWidget::HandleChallenge);
	BtnCoach->OnClicked.AddDynamic(this, &UModeSelectWidget::HandleCoach);
	BtnTraining->OnClicked.AddDynamic(this, &UModeSelectWidget::HandleTraining);
	BtnBack->OnClicked.AddDynamic(this, &UModeSelectWidget::HandleBack);
}

void UModeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModeSelectWidget::SetInitialFocus()
{
	if (BtnQuickShort && BtnQuickShort->IsValidLowLevel())
	{
		BtnQuickShort->SetKeyboardFocus();
	}
}
