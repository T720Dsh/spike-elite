// SPDX-License-Identifier: MIT
#include "UI/MainMenuWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEWidget, Log, All);

static UImage* MakeSolidImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Img->SetBrush(SEUiStyle::SolidBrush(Color));
	return Img;
}

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& OI) : Super(OI) {}

static UButton* MakeBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label,
	const FLinearColor& Base, const FLinearColor& Hover, int32 FontSize)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(SEUiStyle::ButtonStyle(
		Base,
		Hover,
		FLinearColor(Base.R * 0.6f, Base.G * 0.6f, Base.B * 0.6f, 1.f)));

	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	T->SetFont(SEUiStyle::Font(FontSize));
	T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	T->SetJustification(ETextJustify::Center);
	B->AddChild(T);

	UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(B);
	VSlot->SetPadding(FMargin(0.f, 8.f));
	VSlot->SetHorizontalAlignment(HAlign_Center);
	VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	return B;
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// Dark charcoal background.
	UImage* BG = MakeSolidImage(WidgetTree, FLinearColor(0.06f, 0.07f, 0.09f, 1.0f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BG))
	{
		S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
	}

	// Electric-blue accent stripe down the left edge.
	UImage* Stripe = MakeSolidImage(WidgetTree, FLinearColor(0.1f, 0.55f, 1.0f, 1.0f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Stripe))
	{
		S->SetAnchors(FAnchors(0,0,0,1)); S->SetOffsets(FMargin(0,0,10,0));
	}

	// Center column.
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
	}

	Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("SPIKE ELITE")));
	Title->SetFont(SEUiStyle::Font(72));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.1f)));  // neon yellow
	Title->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(Title);

	UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sub->SetText(FText::FromString(TEXT("室内排球 · 6v6")));
	Sub->SetFont(SEUiStyle::Font(20));
	Sub->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.7f,1.0f)));
	Sub->SetJustification(ETextJustify::Center);
	Col->AddChildToVerticalBox(Sub);

	UTextBlock* Sp = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sp->SetText(FText::FromString(TEXT(" ")));
	Col->AddChildToVerticalBox(Sp);

	BtnStart    = MakeBtn(WidgetTree, Col, TEXT("开始比赛"), FLinearColor(0.10f,0.50f,0.95f,1), FLinearColor(0.30f,0.68f,1.0f,1), 28);
	BtnSettings = MakeBtn(WidgetTree, Col, TEXT("设置"),     FLinearColor(0.16f,0.18f,0.24f,1), FLinearColor(0.28f,0.34f,0.46f,1), 24);
	BtnQuit     = MakeBtn(WidgetTree, Col, TEXT("退出游戏"), FLinearColor(0.30f,0.10f,0.10f,1), FLinearColor(0.55f,0.20f,0.20f,1), 24);

	if (BtnStart)    BtnStart->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStartClick);
	if (BtnSettings) BtnSettings->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettingsClick);
	if (BtnQuit)     BtnQuit->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClick);

	// Volleyball icon: imported project texture; falls back to a neutral disc.
	BallIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	bool bLoaded = false;
	if (UTexture2D* BallTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_VolleyballIcon.T_VolleyballIcon")))
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(BallTex);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		BallIcon->SetBrush(Brush);
		bLoaded = true;
	}
	else
	{
		UE_LOG(LogSEWidget, Warning, TEXT("Volleyball icon texture missing (/Game/UI/T_VolleyballIcon); using fallback"));
		BallIcon->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.9f,0.9f,0.85f,1)));
	}
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BallIcon))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
		S->SetSize(FVector2D(84,84));
		S->SetPosition(FVector2D(240, -160));
	}

	// Title starts transparent and fades/slides in once (not a high-freq flicker).
	Title->SetRenderOpacity(0.f);
	Title->SetRenderTranslation(FVector2D(0.f, -24.f));

	// Keyboard / gamepad focus starts on the first button.
	if (BtnStart) BtnStart->SetKeyboardFocus();

	UE_LOG(LogSEWidget, Log, TEXT("MainMenu built, ball icon loaded=%s"), bLoaded ? TEXT("yes") : TEXT("no(fallback)"));
}

void UMainMenuWidget::NativeTick(const FGeometry& Geo, float DT)
{
	Super::NativeTick(Geo, DT);
	AnimTime += DT;

	// Volleyball: gentle continuous spin + a looping bounce arc.
	if (BallIcon)
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(BallIcon->Slot))
		{
			const float Hop = FMath::Abs(FMath::Sin(AnimTime * 2.2f)) * 46.f;
			S->SetPosition(FVector2D(240.f, -160.f - Hop));
		}
		BallIcon->SetRenderTransformAngle(FMath::Fmod(AnimTime * 70.f, 360.f));
	}

	// One-shot title fade/slide-in over the first 0.7s, then hold.
	if (Title && Title->GetRenderOpacity() < 1.f)
	{
		const float A = FMath::Clamp(AnimTime / 0.7f, 0.f, 1.f);
		Title->SetRenderOpacity(A);
		Title->SetRenderTranslation(FVector2D(0.f, -24.f * (1.f - A)));
	}
}
