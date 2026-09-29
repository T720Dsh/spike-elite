// SPDX-License-Identifier: MIT
#include "UI/MainMenuWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEWidget, Log, All);

static UTexture2D* GetWhiteTexture()
{
	static UTexture2D* White = nullptr;
	if (!White)
		White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	return White;
}

static UImage* MakeImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* White = GetWhiteTexture())
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(White);
		Brush.TintColor = FSlateColor(Color);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Img->SetBrush(Brush);
	}
	else
	{
		Img->SetColorAndOpacity(Color);
	}
	return Img;
}

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& OI) : Super(OI) {}

static UButton* MakeBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label,
	FLinearColor Base, FLinearColor Hover, float FontSize)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	FSlateFontInfo F = T->Font; F.Size = FontSize;
	T->SetFont(F);
	T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	B->AddChild(T);
	Parent->AddChildToVerticalBox(B);
	return B;
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogSEWidget, Log, TEXT("MainMenu NativeConstruct start, WidgetTree=%s"), WidgetTree ? TEXT("valid") : TEXT("NULL"));

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// Dark charcoal background image.
	UImage* BG = MakeImage(WidgetTree, FLinearColor(0.06f, 0.07f, 0.09f, 1.0f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BG))
	{
		S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
	}

	// Accent stripe (electric blue).
	UImage* Stripe = MakeImage(WidgetTree, FLinearColor(0.1f, 0.55f, 1.0f, 1.0f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Stripe))
	{
		S->SetAnchors(FAnchors(0,0,0,1)); S->SetOffsets(FMargin(0,0,8,0));
	}

	// Center column.
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
		S->SetPosition(FVector2D(0,0));
	}

	Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("SPIKE ELITE")));
	{
		FSlateFontInfo F = Title->Font; F.Size = 72;
		Title->SetFont(F);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.1f)));  // neon yellow
		Col->AddChildToVerticalBox(Title);
	}

	UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sub->SetText(FText::FromString(TEXT("室内排球 · 6v6")));
	{
		FSlateFontInfo F = Sub->Font; F.Size = 20;
		Sub->SetFont(F);
		Sub->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.7f,1.0f)));
		Col->AddChildToVerticalBox(Sub);
	}

	// Spacer
	UTextBlock* Sp = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sp->SetText(FText::FromString(TEXT(" ")));
	Col->AddChildToVerticalBox(Sp);

	BtnStart    = MakeBtn(WidgetTree, Col, TEXT("开始比赛"), FLinearColor(0.1f,0.55f,1.f,1), FLinearColor(0.3f,0.7f,1.f,1), 28);
	BtnSettings = MakeBtn(WidgetTree, Col, TEXT("设置"),     FLinearColor(0.15f,0.16f,0.2f,1), FLinearColor(0.25f,0.3f,0.4f,1), 24);
	BtnQuit     = MakeBtn(WidgetTree, Col, TEXT("退出游戏"), FLinearColor(0.3f,0.1f,0.1f,1), FLinearColor(0.5f,0.2f,0.2f,1), 24);

	if (BtnStart)    BtnStart->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStartClick);
	if (BtnSettings) BtnSettings->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettingsClick);
	if (BtnQuit)     BtnQuit->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClick);

	// Volleyball icon (yellow circle placeholder) that bounces.
	BallIcon = MakeImage(WidgetTree, FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BallIcon))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
		S->SetSize(FVector2D(60,60));
		S->SetPosition(FVector2D(280, -120));
	}

	UE_LOG(LogSEWidget, Log, TEXT("MainMenu NativeConstruct done, Root=%s whiteTex=%s"),
		Root ? TEXT("ok") : TEXT("NULL"),
		GetWhiteTexture() ? TEXT("ok") : TEXT("MISSING"));
}

void UMainMenuWidget::NativeTick(const FGeometry& Geo, float DT)
{
	Super::NativeTick(Geo, DT);
	AnimTime += DT;
	if (BallIcon)
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(BallIcon->Slot))
		{
			// Bouncing ball: vertical sine + slight horizontal drift.
			float Y = -120.0f + FMath::Abs(FMath::Sin(AnimTime * 3.0f)) * -60.0f;
			S->SetPosition(FVector2D(280.0f + FMath::Sin(AnimTime*0.8f)*40.0f, Y));
			float Scale = 1.0f + FMath::Sin(AnimTime*3.0f)*0.1f;
			BallIcon->SetRenderScale(FVector2D(Scale, Scale));
		}
	}
	if (Title)
	{
		// Subtle pulse on the title.
		float A = 0.85f + 0.15f*FMath::Sin(AnimTime*2.0f);
		Title->SetRenderOpacity(A);
	}
}

// Dynamic delegates need UFUNCTION thunks; bind via lambdas in the controller instead.
// We forward clicks through the simple delegates.
