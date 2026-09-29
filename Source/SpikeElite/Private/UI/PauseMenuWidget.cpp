// SPDX-License-Identifier: MIT
#include "UI/PauseMenuWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"

static UImage* MakeDimImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(White);
		Brush.TintColor = FSlateColor(Color);
		Img->SetBrush(Brush);
	}
	else Img->SetColorAndOpacity(Color);
	return Img;
}

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& OI) : Super(OI) {}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UImage* Dim = MakeDimImage(WidgetTree, FLinearColor(0,0,0,0.7f));
	if (auto* S = Root->AddChildToCanvas(Dim)) { S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0)); }

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (auto* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
	}

	auto AddTitle = [&](const FString& T, int32 Sz, FLinearColor C)
	{
		UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TB->SetText(FText::FromString(T));
		FSlateFontInfo F = TB->Font; F.Size = Sz; TB->SetFont(F);
		TB->SetColorAndOpacity(FSlateColor(C));
		Col->AddChildToVerticalBox(TB);
	};
	AddTitle(TEXT("已暂停"), 48, FLinearColor(0.95f,0.95f,0.1f));

	auto AddBtn = [&](const FString& Label) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Label));
		FSlateFontInfo F = T->Font; F.Size = 22; T->SetFont(F);
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		B->AddChild(T);
		Col->AddChildToVerticalBox(B);
		return B;
	};
	BtnResume   = AddBtn(TEXT("继续游戏"));
	BtnSettings = AddBtn(TEXT("设置"));
	BtnMainMenu = AddBtn(TEXT("返回主菜单"));
	BtnQuit     = AddBtn(TEXT("退出到桌面"));

	BtnResume->OnClicked.AddDynamic(this, &UPauseMenuWidget::HResume);
	BtnSettings->OnClicked.AddDynamic(this, &UPauseMenuWidget::HSettings);
	BtnMainMenu->OnClicked.AddDynamic(this, &UPauseMenuWidget::HMainMenu);
	BtnQuit->OnClicked.AddDynamic(this, &UPauseMenuWidget::HQuit);
}
