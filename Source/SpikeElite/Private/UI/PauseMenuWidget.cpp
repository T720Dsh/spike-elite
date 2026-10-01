// SPDX-License-Identifier: MIT
#include "UI/PauseMenuWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& OI) : Super(OI) {}

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UPauseMenuWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Dim->SetBrush(SEUiStyle::SolidBrush(SEUiStyle::Colors::Navy));
	if (auto* S = Root->AddChildToCanvas(Dim)) { S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0)); }

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (auto* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
		S->SetAutoSize(true);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("已暂停")));
	Title->SetFont(SEUiStyle::FontBold(44));
	Title->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	Title->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(Title);

	auto AddBtn = [&](const FString& Label, const FButtonStyle& Style) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(Style);
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Label));
		T->SetFont(SEUiStyle::Font(22));
		T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
		T->SetJustification(ETextJustify::Center);
		B->AddChild(T);
		USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		WidthBox->SetWidthOverride(260.f);
		if (USizeBoxSlot* ContentSlot = Cast<USizeBoxSlot>(WidthBox->AddChild(B)))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		if (UVerticalBoxSlot* V = Col->AddChildToVerticalBox(WidthBox))
		{
			V->SetPadding(FMargin(0.f, 7.f));
			V->SetHorizontalAlignment(HAlign_Center);
		}
		return B;
	};

	BtnResume   = AddBtn(TEXT("继续游戏"),   SEUiStyle::PrimaryButton());
	BtnSettings = AddBtn(TEXT("设置"),       SEUiStyle::SecondaryButton());
	BtnMainMenu = AddBtn(TEXT("返回主菜单"), SEUiStyle::ButtonStyle(SEUiStyle::Colors::Slate, FLinearColor(0.28f,0.34f,0.46f,1.f), FLinearColor(0.10f,0.13f,0.18f,1.f)));
	BtnQuit     = AddBtn(TEXT("退出到桌面"), SEUiStyle::DangerButton());
	BtnHelp     = AddBtn(TEXT("操作说明"),   SEUiStyle::ButtonStyle(SEUiStyle::Colors::Panel, FLinearColor(0.22f,0.30f,0.42f,1.f), FLinearColor(0.10f,0.14f,0.20f,1.f)));

	// M11a: on-demand controls reference inside the pause menu.
	HelpText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HelpText->SetFont(SEUiStyle::Font(16));
	HelpText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f,0.85f,0.9f)));
	HelpText->SetJustification(ETextJustify::Center);
	HelpText->SetText(FText::FromString(
		TEXT("WASD 移动 · 空格 跳 · 左键 击球 · 右键 抬手 · E 发球 · C/V 切视角 · Esc 暂停")));
	HelpText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* V = Col->AddChildToVerticalBox(HelpText))
	{
		V->SetPadding(FMargin(0.f, 10.f));
		V->SetHorizontalAlignment(HAlign_Center);
	}
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BtnResume)
	{
		BtnResume->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HResume);
		BtnResume->SetKeyboardFocus();
	}
	if (BtnSettings) BtnSettings->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HSettings);
	if (BtnMainMenu) BtnMainMenu->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HMainMenu);
	if (BtnQuit) BtnQuit->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HQuit);
	if (BtnHelp) BtnHelp->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::ToggleHelp);
}

void UPauseMenuWidget::ToggleHelp()
{
	if (!HelpText) return;
	if (HelpText->GetVisibility() == ESlateVisibility::Collapsed)
	{
		HelpText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		HelpText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
