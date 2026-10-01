// SPDX-License-Identifier: MIT
#include "UI/MainMenuWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEWidget, Log, All);

bool UMainMenuWidget::bReducedMotion = false;

static UImage* MakeSolidImage(UWidgetTree* Tree, const FLinearColor& Color)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Img->SetBrush(SEUiStyle::SolidBrush(Color));
	return Img;
}

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& OI) : Super(OI) {}

static UButton* MakeBtn(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label,
	const FButtonStyle& Style, int32 FontSize)
{
	UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(Style);

	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	T->SetFont(SEUiStyle::Font(FontSize));
	T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
	T->SetJustification(ETextJustify::Center);
	B->AddChild(T);

	USizeBox* WidthBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidthBox->SetWidthOverride(300.f);
	WidthBox->SetHeightOverride(SEUiStyle::Spacing::ButtonH());
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

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UMainMenuWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// --- fullscreen navy backdrop --------------------------------------------
	UImage* BG = MakeSolidImage(WidgetTree, SEUiStyle::Colors::Navy);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BG))
	{
		S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
	}

	// --- soft upper glow (broadcast light wash) -------------------------------
	UImage* Glow = MakeSolidImage(WidgetTree, FLinearColor(0.10f, 0.22f, 0.40f, 0.10f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Glow))
	{
		S->SetAnchors(FAnchors(0,0,1,0));
		S->SetOffsets(FMargin(0.f, 0.f, 0.f, 620.f));
	}

	// --- bottom court band (stylized floor) -----------------------------------
	UImage* Court = MakeSolidImage(WidgetTree, FLinearColor(0.035f, 0.055f, 0.10f, 0.95f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Court))
	{
		S->SetAnchors(FAnchors(0,1,1,1));
		S->SetOffsets(FMargin(0.f, -250.f, 0.f, 0.f));
	}

	// --- net line (thin white horizontal) --------------------------------------
	UImage* Net = MakeSolidImage(WidgetTree, FLinearColor(0.85f, 0.90f, 0.95f, 0.20f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Net))
	{
		S->SetAnchors(FAnchors(0,0,1,0));
		S->SetOffsets(FMargin(0.f, 300.f, 0.f, 3.f));
	}

	// --- sweeping spotlights (two thin beams, gently moving) ------------------
	SpotL = MakeSolidImage(WidgetTree, FLinearColor(1.0f, 0.95f, 0.75f, 0.08f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(SpotL))
	{
		S->SetAnchors(FAnchors(0,0,0,0));
		S->SetSize(FVector2D(14.f, 1500.f));
		S->SetPosition(FVector2D(-40.f, -60.f));
	}
	SpotR = MakeSolidImage(WidgetTree, FLinearColor(0.45f, 0.75f, 1.0f, 0.07f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(SpotR))
	{
		S->SetAnchors(FAnchors(0,0,0,0));
		S->SetSize(FVector2D(18.f, 1500.f));
		S->SetPosition(FVector2D(-60.f, -40.f));
	}

	// --- logo cluster (locked up: ball LEFT of the wordmark) -------------------
	LogoCluster = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(LogoCluster))
	{
		S->SetAnchors(FAnchors(0.5f, 0.30f, 0.5f, 0.30f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	UHorizontalBox* LogoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LogoCluster->AddChildToVerticalBox(LogoRow);

	// Volleyball icon (project texture, fallback disc), left of the wordmark.
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
		BallIcon->SetBrush(SEUiStyle::SolidBrush(SEUiStyle::Colors::Gold));
	}
	if (UHorizontalBoxSlot* HSlot = LogoRow->AddChildToHorizontalBox(BallIcon))
	{
		HSlot->SetPadding(FMargin(0.f, 8.f, 18.f, 0.f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	}
	BallIcon->SetDesiredSizeOverride(FVector2D(58.f, 58.f));

	Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("SPIKE ELITE")));
	Title->SetFont(SEUiStyle::FontBold(58));
	Title->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
	Title->SetJustification(ETextJustify::Center);
	LogoRow->AddChildToHorizontalBox(Title);

	SubTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SubTitle->SetText(FText::FromString(TEXT("室内排球 · 6v6 · 风格化转播")));
	SubTitle->SetFont(SEUiStyle::Font(20));
	SubTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.72f, 1.0f)));
	SubTitle->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* SubSlot = LogoCluster->AddChildToVerticalBox(SubTitle);
	SubSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	Divider = MakeSolidImage(WidgetTree, SEUiStyle::Colors::Gold);
	Divider->SetDesiredSizeOverride(FVector2D(180.f, 3.f));
	if (UVerticalBoxSlot* DSlot = LogoCluster->AddChildToVerticalBox(Divider))
	{
		DSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		DSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// --- button column ---------------------------------------------------------
	ButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(ButtonColumn))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D(0.f, 60.f));
		S->SetAutoSize(true);
	}

	BtnStart    = MakeBtn(WidgetTree, ButtonColumn, TEXT("开始比赛"), SEUiStyle::PrimaryButton(),  24);
	BtnSettings = MakeBtn(WidgetTree, ButtonColumn, TEXT("设置"),     SEUiStyle::SecondaryButton(), 20);
	BtnQuit     = MakeBtn(WidgetTree, ButtonColumn, TEXT("退出游戏"), SEUiStyle::DangerButton(),    20);

	// --- version / milestone (bottom-right corner, unobtrusive) ----------------
	UTextBlock* Ver = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Ver->SetText(FText::FromString(TEXT("M11d · 开发版 · 低多边形转播")));
	Ver->SetFont(SEUiStyle::Font(SEUiStyle::Type::Tiny()));
	Ver->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Grey));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Ver))
	{
		S->SetAnchors(FAnchors(1,1,1,1));
		S->SetAlignment(FVector2D(1.f, 1.f));
		S->SetPosition(FVector2D(-16.f, -12.f));
		S->SetAutoSize(true);
	}

	// --- entrance animation state ---------------------------------------------
	LogoCluster->SetRenderOpacity(0.f);
	LogoCluster->SetRenderTranslation(FVector2D(0.f, -20.f));
	for (UWidget* W : { (UWidget*)BtnStart, (UWidget*)BtnSettings, (UWidget*)BtnQuit })
	{
		if (W) { W->SetRenderOpacity(0.f); W->SetRenderTranslation(FVector2D(0.f, 14.f)); }
	}

	UE_LOG(LogSEWidget, Log, TEXT("MainMenu rebuilt (navy broadcast look), ball icon loaded=%s"),
		bLoaded ? TEXT("yes") : TEXT("no(fallback)"));
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	AnimTime = 0.0f;
	if (BtnStart)
	{
		BtnStart->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleStartClick);
		BtnStart->SetKeyboardFocus();
	}
	if (BtnSettings) BtnSettings->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleSettingsClick);
	if (BtnQuit) BtnQuit->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleQuitClick);
}

void UMainMenuWidget::NativeTick(const FGeometry& Geo, float DT)
{
	Super::NativeTick(Geo, DT);
	AnimTime += DT;
	const bool bFast = SEUiStyle::IsReducedMotion();

	// --- sweeping spotlights (paused when reduced-motion is on) ----------------
	if (SpotL && SpotR)
	{
		if (!bFast)
		{
			LightPhase += DT * 0.22f;
		}
		const float W = Geo.GetLocalSize().X;
		const float LX = FMath::Fmod(LightPhase * W + W, W) - 200.f;
		const float RX = W - FMath::Fmod(LightPhase * W * 0.7f + W * 0.3f, W) - 200.f;
		if (UCanvasPanelSlot* SL = Cast<UCanvasPanelSlot>(SpotL->Slot))
		{
			SL->SetPosition(FVector2D(LX, -60.f));
			SpotL->SetRenderTransformAngle(FMath::Sin(LightPhase * 1.7f) * 8.f);
		}
		if (UCanvasPanelSlot* SR = Cast<UCanvasPanelSlot>(SpotR->Slot))
		{
			SR->SetPosition(FVector2D(RX, -40.f));
			SpotR->SetRenderTransformAngle(-FMath::Sin(LightPhase * 1.3f) * 10.f);
		}
	}

	// --- one-shot staggered entrance -------------------------------------------------
	// Logo cluster first, then the three buttons (0.08 s apart each).
	const float Slow = bFast ? SEUiStyle::Anim::Shorten() : SEUiStyle::Anim::SlideIn();
	if (LogoCluster && LogoCluster->GetRenderOpacity() < 1.f)
	{
		const float A = FMath::Clamp(AnimTime / Slow, 0.f, 1.f);
		LogoCluster->SetRenderOpacity(A);
		LogoCluster->SetRenderTranslation(FVector2D(0.f, -20.f * (1.f - A)));
	}
	const float BtnDelay = 0.f; (void)BtnDelay; // (kept for clarity of stagger math)
	if (BtnStart && BtnStart->GetRenderOpacity() < 1.f)
	{
		const float A = FMath::Clamp((AnimTime - Slow) / Slow, 0.f, 1.f);
		BtnStart->SetRenderOpacity(A);
		BtnStart->SetRenderTranslation(FVector2D(0.f, 14.f * (1.f - A)));
	}
	if (BtnSettings && BtnSettings->GetRenderOpacity() < 1.f)
	{
		const float A = FMath::Clamp((AnimTime - Slow - SEUiStyle::Anim::Stagger()) / Slow, 0.f, 1.f);
		BtnSettings->SetRenderOpacity(A);
		BtnSettings->SetRenderTranslation(FVector2D(0.f, 14.f * (1.f - A)));
	}
	if (BtnQuit && BtnQuit->GetRenderOpacity() < 1.f)
	{
		const float A = FMath::Clamp((AnimTime - Slow - SEUiStyle::Anim::Stagger() * 2.f) / Slow, 0.f, 1.f);
		BtnQuit->SetRenderOpacity(A);
		BtnQuit->SetRenderTranslation(FVector2D(0.f, 14.f * (1.f - A)));
	}

	// --- volleyball: gentle bounce + spin (kept from M11b; frozen when reduced) --
	if (BallIcon)
	{
		if (!bFast)
		{
			const float Hop = FMath::Abs(FMath::Sin(AnimTime * 2.2f)) * 6.f;
			BallIcon->SetRenderTranslation(FVector2D(0.f, -Hop));
			BallIcon->SetRenderTransformAngle(FMath::Fmod(AnimTime * 70.f, 360.f));
		}
		else
		{
			BallIcon->SetRenderTranslation(FVector2D(0.f, 0.f));
			BallIcon->SetRenderTransformAngle(0.f);
		}
	}
}
