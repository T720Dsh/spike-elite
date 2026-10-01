// SPDX-License-Identifier: MIT
#include "UI/TacticalHUDWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"

UTacticalHUDWidget::UTacticalHUDWidget(const FObjectInitializer& OI) : Super(OI) {}

static const TArray<FString>& DefenseOptionLabels()
{
	static const TArray<FString> Labels = {
		TEXT("单人拦网"), TEXT("双人拦网"), TEXT("封直线"), TEXT("封斜线"),
		TEXT("后排直线防守"), TEXT("后排斜线防守"), TEXT("常规移动救球"), TEXT("倒地救球")
	};
	return Labels;
}

TSharedRef<SWidget> UTacticalHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

static UBorder* MakePanel(UWidgetTree* Tree, UCanvasPanel* Root, const FVector2D& Pos, const FVector2D& Size)
{
	UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.85f));
	B->SetPadding(FMargin(12.f));
	if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(B))
	{
		Slot->SetPosition(Pos);
		Slot->SetSize(Size);
		Slot->SetAnchors(FAnchors(0.f, 1.f)); // bottom-left anchored
		Slot->SetAlignment(FVector2D(0.f, 1.f));
		Slot->SetAutoSize(false);
	}
	return B;
}

static UTextBlock* MakeText(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Text));
	T->SetFont(SEUiStyle::Font(Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetAutoWrapText(true);
	Parent->AddChild(T);
	return T;
}

static UTextBlock* MakeRowText(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Text));
	T->SetFont(SEUiStyle::Font(Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(T);
	Slot->SetPadding(FMargin(0.f, 2.f));
	Slot->SetHorizontalAlignment(HAlign_Fill);
	return T;
}

void UTacticalHUDWidget::BuildWidgetTree()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;
	BuildAttackPanel();
	BuildSetPanel();
	BuildDefensePanel();
	HideAll();
}

void UTacticalHUDWidget::BuildAttackPanel()
{
	AttackBorder = MakePanel(WidgetTree, Root, FVector2D(16.f, -16.f), FVector2D(360.f, 300.f));
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AttackBorder->AddChild(V);

	AttackHeader = MakeRowText(WidgetTree, V, TEXT("战术瞄准"), 22, FLinearColor(1.f, 0.9f, 0.3f));
	AttackTarget = MakeRowText(WidgetTree, V, TEXT(""), 16, FLinearColor::White);
	AttackPower = MakeRowText(WidgetTree, V, TEXT(""), 16, FLinearColor::White);
	AttackArc = MakeRowText(WidgetTree, V, TEXT(""), 16, FLinearColor::White);
	AttackVerdict = MakeRowText(WidgetTree, V, TEXT(""), 18, FLinearColor::White);
	TimingBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	TimingBar->SetFillColorAndOpacity(FLinearColor(0.2f, 1.f, 0.3f));
	TimingBar->SetPercent(0.f);
	V->AddChild(TimingBar);
	TimingLabel = MakeRowText(WidgetTree, V, TEXT(""), 15, FLinearColor::White);
	AttackHelp = MakeRowText(WidgetTree, V, TEXT("左键确认 · 右键/Esc 取消 · 滚轮/W/S 力度 · Q/E 弧线"), 14, FLinearColor(0.7f, 0.7f, 0.7f));
}

void UTacticalHUDWidget::BuildSetPanel()
{
	SetBorder = MakePanel(WidgetTree, Root, FVector2D(-16.f, -16.f), FVector2D(420.f, 600.f));
	// SetPanel is anchored bottom-RIGHT; MovePanelToRight:
	if (UCanvasPanelSlot* RightSlot = Cast<UCanvasPanelSlot>(SetBorder->Slot))
	{
		RightSlot->SetAnchors(FAnchors(1.f, 1.f));
		RightSlot->SetAlignment(FVector2D(1.f, 1.f));
		RightSlot->SetPosition(FVector2D(-16.f, -16.f));
	}
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SetBorder->AddChild(V);

	SetTitle = MakeRowText(WidgetTree, V, TEXT("二传战术选择"), 22, FLinearColor(1.f, 0.9f, 0.3f));
	SetList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	V->AddChild(SetList);

	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	for (int32 i = 0; i < Plays.Num(); i++)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetStyle(SEUiStyle::ButtonStyle(
			FLinearColor(0.1f, 0.1f, 0.15f, 0.9f),
			FLinearColor(0.25f, 0.25f, 0.35f, 0.9f),
			FLinearColor(0.05f, 0.05f, 0.1f, 0.9f)));
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Plays[i].DisplayName));
		T->SetFont(SEUiStyle::Font(15));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Btn->AddChild(T);
		UVerticalBoxSlot* BSlot = SetList->AddChildToVerticalBox(Btn);
		BSlot->SetPadding(FMargin(0.f, 1.f));
		BSlot->SetHorizontalAlignment(HAlign_Fill);
		SetRowButtons.Add(Btn);
		SetRows.Add(T);
	}
	SetDetails = MakeRowText(WidgetTree, V, TEXT(""), 14, FLinearColor(0.8f, 0.95f, 1.f));
	SetHelp = MakeRowText(WidgetTree, V, TEXT("鼠标点击或 Q/E 切换 · 方向键上/下 · 左键确认 · 右键/Esc 取消"), 13, FLinearColor(0.7f, 0.7f, 0.7f));
}

void UTacticalHUDWidget::BuildDefensePanel()
{
	DefenseBorder = MakePanel(WidgetTree, Root, FVector2D(16.f, -330.f), FVector2D(340.f, 320.f));
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DefenseBorder->AddChild(V);

	DefenseTitle = MakeRowText(WidgetTree, V, TEXT("防守决策"), 22, FLinearColor(1.f, 0.9f, 0.3f));
	DefenseList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	V->AddChild(DefenseList);

	const TArray<FString>& Labels = DefenseOptionLabels();
	for (int32 i = 0; i < Labels.Num(); i++)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetStyle(SEUiStyle::ButtonStyle(
			FLinearColor(0.1f, 0.1f, 0.15f, 0.9f),
			FLinearColor(0.25f, 0.25f, 0.35f, 0.9f),
			FLinearColor(0.05f, 0.05f, 0.1f, 0.9f)));
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Labels[i]));
		T->SetFont(SEUiStyle::Font(15));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Btn->AddChild(T);
		UVerticalBoxSlot* BSlot = DefenseList->AddChildToVerticalBox(Btn);
		BSlot->SetPadding(FMargin(0.f, 1.f));
		BSlot->SetHorizontalAlignment(HAlign_Fill);
		DefenseRowButtons.Add(Btn);
		DefenseRows.Add(T);
	}
	DefenseHelp = MakeRowText(WidgetTree, V, TEXT("选择改变队友站位与拦网 · 超时使用 AI 默认"), 13, FLinearColor(0.7f, 0.7f, 0.7f));
}

void UTacticalHUDWidget::ShowAttackPanel()
{
	if (AttackBorder) { AttackBorder->SetVisibility(ESlateVisibility::Visible); }
	if (SetBorder) { SetBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (DefenseBorder) { DefenseBorder->SetVisibility(ESlateVisibility::Collapsed); }
}

void UTacticalHUDWidget::ShowSetPanel()
{
	if (AttackBorder) { AttackBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (SetBorder) { SetBorder->SetVisibility(ESlateVisibility::Visible); }
	if (DefenseBorder) { DefenseBorder->SetVisibility(ESlateVisibility::Collapsed); }
}

void UTacticalHUDWidget::ShowDefensePanel()
{
	if (AttackBorder) { AttackBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (SetBorder) { SetBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (DefenseBorder) { DefenseBorder->SetVisibility(ESlateVisibility::Visible); }
}

void UTacticalHUDWidget::HideAll()
{
	if (AttackBorder) { AttackBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (SetBorder) { SetBorder->SetVisibility(ESlateVisibility::Collapsed); }
	if (DefenseBorder) { DefenseBorder->SetVisibility(ESlateVisibility::Collapsed); }
}

void UTacticalHUDWidget::UpdateAttackInfo(const FShotIntent& Intent, const SEVolleyballTrajectory::FShotSolution& Sol)
{
	FString Verdict;
	FLinearColor VerdictColor = FLinearColor(1.f, 0.2f, 0.2f);
	if (!Sol.bValid) { Verdict = TEXT("不可行"); }
	else if (Sol.bCrossedNet && Sol.bInBounds) { Verdict = TEXT("界内 可行"); VerdictColor = FLinearColor(0.2f, 1.f, 0.3f); }
	else if (Sol.bCrossedNet || Sol.bInBounds) { Verdict = TEXT("接近边界/触网风险"); VerdictColor = FLinearColor(1.f, 0.9f, 0.2f); }
	else { Verdict = TEXT("出界或触网"); }

	const FString Key = FString::Printf(TEXT("A|%.0f|%.0f|%.2f|%.0f|%.2f|%.2f|%s"),
		Intent.TargetLocation.X, Intent.TargetLocation.Y, Intent.Power, Sol.InitialVelocity.Size(),
		Sol.FlightTime, Sol.Apex.Z, *Verdict);
	if (Key == LastAttackKey) { return; }
	LastAttackKey = Key;

	const FString TypeLabel = (Intent.TouchType == EBallTouchType::Set) ? TEXT("二传")
		: (Intent.TouchType == EBallTouchType::Receive) ? TEXT("接球") : TEXT("扣球/吊球");
	AttackHeader->SetText(FText::FromString(FString::Printf(TEXT("战术瞄准 — %s"), *TypeLabel)));
	AttackTarget->SetText(FText::FromString(FString::Printf(TEXT("目标落点 (%.0f, %.0f)"), Intent.TargetLocation.X, Intent.TargetLocation.Y)));
	AttackPower->SetText(FText::FromString(FString::Printf(TEXT("力度 %.0f%% · 初速 %.0f cm/s"), Intent.Power * 100.f, Sol.InitialVelocity.Size())));
	AttackArc->SetText(FText::FromString(FString::Printf(TEXT("弧高 %.0f cm · 飞行时间 %.2f s"), Sol.Apex.Z, Sol.FlightTime)));
	AttackVerdict->SetText(FText::FromString(Verdict));
	AttackVerdict->SetColorAndOpacity(FSlateColor(VerdictColor));
}

void UTacticalHUDWidget::ShowTiming(float Progress01, const FString& Status)
{
	if (TimingBar) { TimingBar->SetPercent(FMath::Clamp(Progress01, 0.f, 1.f)); }
	if (TimingLabel)
	{
		const FString Text = Status.IsEmpty() ? TEXT("时机窗口：按左键击球") : Status;
		if (Text != LastAttackKey)
		{
			LastAttackKey = Text;
			TimingLabel->SetText(FText::FromString(Text));
		}
	}
}

void UTacticalHUDWidget::UpdateSetList(int32 Selected)
{
	if (Selected == LastSetSelected) { return; }
	LastSetSelected = Selected;
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	for (int32 i = 0; i < SetRows.Num() && i < Plays.Num(); i++)
	{
		const bool bSel = (i == Selected);
		SetRows[i]->SetText(FText::FromString(FString::Printf(TEXT("%s %s"), bSel ? TEXT("▶") : TEXT("  "), *Plays[i].DisplayName)));
		SetRows[i]->SetColorAndOpacity(FSlateColor(bSel ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White));
	}
	if (SetDetails)
	{
		FString D = TEXT("");
		if (Plays.IsValidIndex(Selected))
		{
			const FSetPlayDefinition& P = Plays[Selected];
			const FString Risk = (P.RiskLevel == 2) ? TEXT("高") : (P.RiskLevel == 1) ? TEXT("中") : TEXT("低");
			D = FString::Printf(TEXT("分类:%s · 后排:%s · 弧高 %.0f · 飞行 %.2fs · 风险 %s"),
				*P.Category, P.bBackRowAttack ? TEXT("是") : TEXT("否"), P.ApexHeight, P.DesiredFlightTime, *Risk);
		}
		SetDetails->SetText(FText::FromString(D));
	}
}

void UTacticalHUDWidget::UpdateDefenseList(int32 Selected)
{
	if (Selected == LastDefenseSelected) { return; }
	LastDefenseSelected = Selected;
	const TArray<FString>& Labels = DefenseOptionLabels();
	for (int32 i = 0; i < DefenseRows.Num() && i < Labels.Num(); i++)
	{
		const bool bSel = (i == Selected);
		DefenseRows[i]->SetText(FText::FromString(FString::Printf(TEXT("%s %s"), bSel ? TEXT("▶") : TEXT("  "), *Labels[i])));
		DefenseRows[i]->SetColorAndOpacity(FSlateColor(bSel ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White));
	}
}
