// SPDX-License-Identifier: MIT
#include "UI/SettingsWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"

USettingsWidget::USettingsWidget(const FObjectInitializer& OI) : Super(OI) {}

static UVerticalBox* AddRow(UWidgetTree* T, UVerticalBox* Parent, const FString& Label)
{
	UTextBlock* L = T->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	L->SetText(FText::FromString(Label));
	FSlateFontInfo F = L->Font; F.Size = 18; L->SetFont(F);
	L->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Parent->AddChildToVerticalBox(L);
	UVerticalBox* R = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Parent->AddChildToVerticalBox(R);
	return R;
}

void USettingsWidget::PopulateResolutions()
{
	if (!Resolution) return;
	Resolution->ClearOptions();
	TArray<FIntPoint> Res = {
		{1280,720},{1366,768},{1600,900},{1920,1080},{2560,1440},{3840,2160}
	};
	for (auto& R : Res) Resolution->AddOption(FString::Printf(TEXT("%d x %d"), R.X, R.Y));
	if (UGameUserSettings* S = GEngine->GetGameUserSettings())
	{
		FIntPoint Cur = S->GetScreenResolution();
		FString CurStr = FString::Printf(TEXT("%d x %d"), Cur.X, Cur.Y);
		if (Resolution->FindOptionIndex(CurStr) != INDEX_NONE) Resolution->SetSelectedOption(CurStr);
		else Resolution->SetSelectedIndex(3);
	}
}

void USettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UImage* BG = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(White);
		Brush.TintColor = FSlateColor(FLinearColor(0.06f,0.07f,0.09f,0.97f));
		BG->SetBrush(Brush);
	}
	else BG->SetColorAndOpacity(FLinearColor(0.06f,0.07f,0.09f,0.97f));
	if (auto* S = Root->AddChildToCanvas(BG)) { S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0)); }

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (auto* S = Root->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f,0.5f,0.5f,0.5f));
		S->SetAlignment(FVector2D(0.5f,0.5f));
	}

	auto Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("设置")));
	FSlateFontInfo TF = Title->Font; TF.Size = 40; Title->SetFont(TF);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f,0.55f,1.f)));
	Col->AddChildToVerticalBox(Title);

	// Window mode
	AddRow(WidgetTree, Col, TEXT("窗口模式"));
	WindowMode = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	WindowMode->AddOption(TEXT("窗口化"));
	WindowMode->AddOption(TEXT("无边框窗口"));
	WindowMode->AddOption(TEXT("全屏"));
	WindowMode->SetSelectedIndex(0);
	Col->AddChildToVerticalBox(WindowMode);

	AddRow(WidgetTree, Col, TEXT("分辨率"));
	Resolution = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	PopulateResolutions();
	Col->AddChildToVerticalBox(Resolution);

	AddRow(WidgetTree, Col, TEXT("图形质量"));
	Quality = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	Quality->AddOption(TEXT("低"));
	Quality->AddOption(TEXT("中"));
	Quality->AddOption(TEXT("高"));
	Quality->AddOption(TEXT("极致"));
	Quality->SetSelectedIndex(2);
	Col->AddChildToVerticalBox(Quality);

	AddRow(WidgetTree, Col, TEXT("鼠标灵敏度"));
	SensSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	SensSlider->SetMinValue(0.1); SensSlider->SetMaxValue(3.0);
	SensSlider->SetValue(PendingSensitivity);
	SensSlider->OnValueChanged.AddDynamic(this, &USettingsWidget::OnSensChanged);
	Col->AddChildToVerticalBox(SensSlider);
	SensValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), PendingSensitivity)));
	Col->AddChildToVerticalBox(SensValue);

	// Buttons
	UHorizontalBox* Btns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col->AddChildToVerticalBox(Btns);
	BtnApply = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(TEXT("应用")));
		BtnApply->AddChild(T);
		Btns->AddChildToHorizontalBox(BtnApply);
	}
	BtnBack = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(TEXT("返回")));
		BtnBack->AddChild(T);
		Btns->AddChildToHorizontalBox(BtnBack);
	}
	BtnApply->OnClicked.AddDynamic(this, &USettingsWidget::ApplySettings);
	BtnBack->OnClicked.AddDynamic(this, &USettingsWidget::Back);
}

void USettingsWidget::SetCurrentSensitivity(float V)
{
	PendingSensitivity = V;
	if (SensSlider) SensSlider->SetValue(V);
	if (SensValue) SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), V)));
}

void USettingsWidget::OnSensChanged(float V)
{
	PendingSensitivity = V;
	if (SensValue) SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), V)));
}

void USettingsWidget::ApplySettings()
{
	UGameUserSettings* S = GEngine->GetGameUserSettings();
	if (!S) return;

	// Window mode.
	EWindowMode::Type WM = EWindowMode::Windowed;
	if (WindowMode)
	{
		switch (WindowMode->GetSelectedIndex())
		{
		case 0: WM = EWindowMode::Windowed; break;
		case 1: WM = EWindowMode::WindowedFullscreen; break;
		case 2: WM = EWindowMode::Fullscreen; break;
		}
	}
	S->SetFullscreenMode(WM);

	// Resolution.
	if (Resolution)
	{
		FString Sel = Resolution->GetSelectedOption();
		FString L, R;
		if (Sel.Split(TEXT(" x "), &L, &R))
		{
			FIntPoint Res(FCString::Atoi(*L), FCString::Atoi(*R));
			S->SetScreenResolution(Res);
		}
	}

	// Quality preset.
	if (Quality)
	{
		int32 Q = Quality->GetSelectedIndex();
		S->SetOverallScalabilityLevel(Q);  // 0 Low .. 3 Cinematic
	}

	S->ApplySettings(true);
	S->SaveSettings();

	OnSensitivityChanged.ExecuteIfBound(PendingSensitivity);
}

void USettingsWidget::Back()
{
	OnBack.ExecuteIfBound();
}
