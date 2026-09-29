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
#include "Blueprint/WidgetTree.h"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& OI) : Super(OI) {}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Dim->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.02f,0.02f,0.03f,0.72f)));
	if (auto* S = Root->AddChildToCanvas(Dim)) { S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0)); }

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (auto* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("已暂停")));
	Title->SetFont(SEUiStyle::Font(48));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f,0.95f,0.1f)));
	Title->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(Title);

	auto AddBtn = [&](const FString& Label, const FLinearColor& Base, const FLinearColor& Hover) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(SEUiStyle::ButtonStyle(Base, Hover,
			FLinearColor(Base.R*0.6f, Base.G*0.6f, Base.B*0.6f, 1.f)));
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Label));
		T->SetFont(SEUiStyle::Font(22));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		T->SetJustification(ETextJustify::Center);
		B->AddChild(T);
		if (UVerticalBoxSlot* V = Col->AddChildToVerticalBox(B))
		{
			V->SetPadding(FMargin(0.f, 7.f));
			V->SetHorizontalAlignment(HAlign_Center);
		}
		return B;
	};

	BtnResume   = AddBtn(TEXT("继续游戏"),   FLinearColor(0.10f,0.50f,0.95f,1), FLinearColor(0.30f,0.68f,1.0f,1));
	BtnSettings = AddBtn(TEXT("设置"),       FLinearColor(0.16f,0.18f,0.24f,1), FLinearColor(0.28f,0.34f,0.46f,1));
	BtnMainMenu = AddBtn(TEXT("返回主菜单"), FLinearColor(0.22f,0.20f,0.12f,1), FLinearColor(0.40f,0.36f,0.20f,1));
	BtnQuit     = AddBtn(TEXT("退出到桌面"), FLinearColor(0.30f,0.10f,0.10f,1), FLinearColor(0.55f,0.20f,0.20f,1));

	BtnResume->OnClicked.AddDynamic(this, &UPauseMenuWidget::HResume);
	BtnSettings->OnClicked.AddDynamic(this, &UPauseMenuWidget::HSettings);
	BtnMainMenu->OnClicked.AddDynamic(this, &UPauseMenuWidget::HMainMenu);
	BtnQuit->OnClicked.AddDynamic(this, &UPauseMenuWidget::HQuit);

	if (BtnResume) BtnResume->SetKeyboardFocus();
}
