// SPDX-License-Identifier: MIT
#include "UI/ServeIntroWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

UServeIntroWidget::UServeIntroWidget(const FObjectInitializer& OI) : Super(OI) {}

static UImage* IntroMakeSolidImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Img->SetBrush(SEUiStyle::SolidBrush(Color));
	return Img;
}

TSharedRef<SWidget> UServeIntroWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UServeIntroWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// Panel card: right half of the screen (~45-50% width), so the net and the
	// receiving team stay visible on the left. Anchored to the right edge.
	PanelBG = IntroMakeSolidImage(WidgetTree, FLinearColor(0.03f, 0.06f, 0.12f, 0.85f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(PanelBG))
	{
		S->SetAnchors(FAnchors(1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetSize(FVector2D(560.f, 300.f));
		PanelSlot = S;
	}
	PanelBG->SetRenderOpacity(0.f);

	NumberText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NumberText->SetFont(SEUiStyle::Font(72));
	NumberText->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	NumberText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(NumberText))
	{
		S->SetAnchors(FAnchors(1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetPosition(FVector2D(-470.f, -96.f));
		S->SetSize(FVector2D(120.f, 100.f));
	}
	NumberText->SetRenderOpacity(0.f);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NameText->SetFont(SEUiStyle::Font(34));
	NameText->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
	NameText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(NameText))
	{
		S->SetAnchors(FAnchors(1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetPosition(FVector2D(-330.f, -70.f));
		S->SetSize(FVector2D(300.f, 46.f));
	}
	NameText->SetRenderOpacity(0.f);

	TeamRoleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TeamRoleText->SetFont(SEUiStyle::Font(18));
	TeamRoleText->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White * 0.8f));
	TeamRoleText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(TeamRoleText))
	{
		S->SetAnchors(FAnchors(1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetPosition(FVector2D(-330.f, -14.f));
		S->SetSize(FVector2D(300.f, 28.f));
	}
	TeamRoleText->SetRenderOpacity(0.f);
}

void UServeIntroWidget::SetServer(int32 JerseyNumber, const FString& Name, const FString& TeamLabel,
	const FString& Role, bool bShortBar)
{
	bShort = bShortBar;
	const FString Number = FString::Printf(TEXT("%d"), JerseyNumber);
	NumberText->SetText(FText::FromString(Number));
	NameText->SetText(FText::FromString(Name));
	TeamRoleText->SetText(FText::FromString(
		bShortBar ? FString::Printf(TEXT("%s · 发球"), *TeamLabel)
		          : FString::Printf(TEXT("%s · %s · 发球"), *TeamLabel, *Role)));

	// Card height differs: full card vs short name bar.
	if (PanelSlot)
	{
		PanelSlot->SetSize(FVector2D(560.f, bShortBar ? 160.f : 300.f));
		PanelSlot->SetPosition(FVector2D(0.f, 0.f));
	}
	// Align number/name for the short bar layout.
	NumberText->SetVisibility(bShortBar ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	Alpha = 0.f;
	bFadingIn = true;
	bFadingOut = false;
	bHidden = false;
}

void UServeIntroWidget::FadeOut()
{
	bFadingIn = false;
	bFadingOut = true;
}

void UServeIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bHidden) { return; }

	if (bFadingIn)
	{
		Alpha = FMath::Min(1.f, Alpha + InDeltaTime / 0.30f);
	}
	else if (bFadingOut)
	{
		Alpha = FMath::Max(0.f, Alpha - InDeltaTime / 0.25f);
		if (Alpha <= 0.f)
		{
			Alpha = 0.f;
			bHidden = true;
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	const float Op = Alpha;
	PanelBG->SetRenderOpacity(Op * 0.85f);
	NumberText->SetRenderOpacity(Op);
	NameText->SetRenderOpacity(Op);
	TeamRoleText->SetRenderOpacity(Op);
}
