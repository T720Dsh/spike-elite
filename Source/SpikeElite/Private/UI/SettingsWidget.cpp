// SPDX-License-Identifier: MIT
#include "UI/SettingsWidget.h"
#include "UI/SEUiStyle.h"
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
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "RHI.h"

USettingsWidget::USettingsWidget(const FObjectInitializer& OI) : Super(OI) {}

static UVerticalBox* AddRow(UWidgetTree* T, UVerticalBox* Parent, const FString& Label)
{
	UTextBlock* L = T->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	L->SetText(FText::FromString(Label));
	L->SetFont(SEUiStyle::Font(18));
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

	// Build the list from the resolutions the OS/display actually reports.
	TArray<FIntPoint> List;
	TSet<FIntPoint> Seen;
	FScreenResolutionArray ResArray;
	if (RHIGetAvailableResolutions(ResArray, true))
	{
		for (const FScreenResolutionRHI& R : ResArray)
		{
			if (R.Width < 1280 || R.Height < 720) continue;
			FIntPoint P(R.Width, R.Height);
			if (!Seen.Contains(P)) { Seen.Add(P); List.Add(P); }
		}
	}
	// Fallback list if the RHI returned nothing.
	if (List.Num() == 0)
	{
		List = { {1280,720},{1366,768},{1600,900},{1920,1080},{2560,1440} };
	}
	List.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X < B.X; });

	FIntPoint Cur(1920, 1080);
	if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		Cur = S->GetScreenResolution();
	if (!Seen.Contains(Cur)) List.Add(Cur);   // ensure current value is present
	List.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X < B.X; });

	int32 SelectIdx = 0;
	for (int32 i = 0; i < List.Num(); ++i)
	{
		const FString Str = FString::Printf(TEXT("%d x %d"), List[i].X, List[i].Y);
		Resolution->AddOption(Str);
		if (List[i] == Cur) SelectIdx = i;
	}
	Resolution->SetSelectedIndex(SelectIdx);
}

void USettingsWidget::InitFromCurrentSettings()
{
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!S) return;

	PopulateResolutions();

	// Current window mode.
	if (WindowMode)
	{
		switch (S->GetFullscreenMode())
		{
		case EWindowMode::Windowed:           WindowMode->SetSelectedIndex(0); break;
		case EWindowMode::WindowedFullscreen: WindowMode->SetSelectedIndex(1); break;
		case EWindowMode::Fullscreen:         WindowMode->SetSelectedIndex(2); break;
		default:                              WindowMode->SetSelectedIndex(0); break;
		}
	}

	// Current resolution (select the matching option; PopulateResolutions already
	// appended it if it was missing).
	if (Resolution)
	{
		const FIntPoint Cur = S->GetScreenResolution();
		const FString CurStr = FString::Printf(TEXT("%d x %d"), Cur.X, Cur.Y);
		if (Resolution->FindOptionIndex(CurStr) != INDEX_NONE) Resolution->SetSelectedOption(CurStr);
	}

	// Current overall scalability: 0 Low, 1 Medium, 2 High, 3 Epic.
	if (Quality)
	{
		const int32 Q = FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3);
		Quality->SetSelectedIndex(Q);
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
	Title->SetFont(SEUiStyle::Font(40));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f,0.55f,1.f)));
	Col->AddChildToVerticalBox(Title);

	AddRow(WidgetTree, Col, TEXT("窗口模式"));
	WindowMode = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	WindowMode->AddOption(TEXT("窗口化"));
	WindowMode->AddOption(TEXT("无边框窗口"));
	WindowMode->AddOption(TEXT("全屏"));
	WindowMode->SetSelectedIndex(0);
	Col->AddChildToVerticalBox(WindowMode);

	AddRow(WidgetTree, Col, TEXT("分辨率"));
	Resolution = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	Col->AddChildToVerticalBox(Resolution);

	AddRow(WidgetTree, Col, TEXT("图形质量"));
	Quality = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	Quality->AddOption(TEXT("低"));
	Quality->AddOption(TEXT("中"));
	Quality->AddOption(TEXT("高"));
	Quality->AddOption(TEXT("极致"));   // 0 Low .. 3 Epic
	Quality->SetSelectedIndex(2);
	Col->AddChildToVerticalBox(Quality);

	AddRow(WidgetTree, Col, TEXT("鼠标灵敏度"));
	SensSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	SensSlider->SetMinValue(0.1f); SensSlider->SetMaxValue(3.0f);  // matches PlayerController
	SensSlider->SetValue(PendingSensitivity);
	SensSlider->OnValueChanged.AddDynamic(this, &USettingsWidget::OnSensChanged);
	Col->AddChildToVerticalBox(SensSlider);
	SensValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), PendingSensitivity)));
	SensValue->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f,0.85f,1.f)));
	Col->AddChildToVerticalBox(SensValue);

	UHorizontalBox* Btns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col->AddChildToVerticalBox(Btns);
	BtnApply = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	BtnApply->SetStyle(SEUiStyle::ButtonStyle(FLinearColor(0.10f,0.50f,0.95f,1), FLinearColor(0.30f,0.68f,1.0f,1), FLinearColor(0.06f,0.30f,0.57f,1)));
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(TEXT("应用")));
		T->SetFont(SEUiStyle::Font(20));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnApply->AddChild(T);
		Btns->AddChildToHorizontalBox(BtnApply);
	}
	BtnBack = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	BtnBack->SetStyle(SEUiStyle::ButtonStyle(FLinearColor(0.16f,0.18f,0.24f,1), FLinearColor(0.28f,0.34f,0.46f,1), FLinearColor(0.10f,0.11f,0.15f,1)));
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(TEXT("返回")));
		T->SetFont(SEUiStyle::Font(20));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnBack->AddChild(T);
		Btns->AddChildToHorizontalBox(BtnBack);
	}
	BtnApply->OnClicked.AddDynamic(this, &USettingsWidget::ApplySettings);
	BtnBack->OnClicked.AddDynamic(this, &USettingsWidget::Back);

	ApplyStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ApplyStatus->SetText(FText::GetEmpty());
	ApplyStatus->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f,0.95f,0.2f)));
	Col->AddChildToVerticalBox(ApplyStatus);

	// Reflect the live settings now (in case Init is called before construction).
	InitFromCurrentSettings();
	SetCurrentSensitivity(PendingSensitivity);
}

void USettingsWidget::SetCurrentSensitivity(float V)
{
	PendingSensitivity = FMath::Clamp(V, 0.1f, 3.0f);
	if (SensSlider) SensSlider->SetValue(PendingSensitivity);
	if (SensValue) SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), PendingSensitivity)));
}

void USettingsWidget::OnSensChanged(float V)
{
	PendingSensitivity = FMath::Clamp(V, 0.1f, 3.0f);
	if (SensValue) SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), PendingSensitivity)));
}

void USettingsWidget::ApplySettings()
{
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!S) return;

	// Apply only the values actually shown in the UI.
	if (WindowMode)
	{
		switch (WindowMode->GetSelectedIndex())
		{
		case 0: S->SetFullscreenMode(EWindowMode::Windowed); break;
		case 1: S->SetFullscreenMode(EWindowMode::WindowedFullscreen); break;
		case 2: S->SetFullscreenMode(EWindowMode::Fullscreen); break;
		}
	}
	if (Resolution)
	{
		FString Sel = Resolution->GetSelectedOption();
		FString L, R;
		if (Sel.Split(TEXT(" x "), &L, &R))
			S->SetScreenResolution(FIntPoint(FCString::Atoi(*L), FCString::Atoi(*R)));
	}
	if (Quality)
		S->SetOverallScalabilityLevel(FMath::Clamp(Quality->GetSelectedIndex(), 0, 3));

	S->ApplySettings(true);
	S->SaveSettings();

	// Sensitivity is committed only on Apply (Back discards the pending value).
	OnSensitivityChanged.ExecuteIfBound(PendingSensitivity);
	OnApply.ExecuteIfBound();

	if (ApplyStatus)
	{
		ApplyStatus->SetText(FText::FromString(TEXT("已应用")));
		TWeakObjectPtr<USettingsWidget> Weak(this);
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().SetTimer(StatusTimer, [Weak]()
			{
				if (Weak.IsValid() && Weak->ApplyStatus) Weak->ApplyStatus->SetText(FText::GetEmpty());
			}, 1.6f, false);
		}
	}
}

void USettingsWidget::Back()
{
	// Discard unapplied sensitivity / display changes.
	OnBack.ExecuteIfBound();
}
