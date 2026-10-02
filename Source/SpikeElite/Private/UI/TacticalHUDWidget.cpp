// SPDX-License-Identifier: MIT
#include "UI/TacticalHUDWidget.h"
#include "UI/SEUiStyle.h"
#include "SpikeEliteCharacter.h"
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
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
	AttackBorder = MakePanel(WidgetTree, Root, FVector2D(16.f, -16.f), FVector2D(380.f, 320.f));
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AttackBorder->AddChild(V);

	AttackHeader = MakeRowText(WidgetTree, V, TEXT("战术瞄准"), 22, SEUiStyle::Colors::Gold);
	AttackStage = MakeRowText(WidgetTree, V, TEXT("选择落点"), 16, SEUiStyle::Colors::White);
	AttackTarget = MakeRowText(WidgetTree, V, TEXT(""), 16, SEUiStyle::Colors::White);
	AttackPower = MakeRowText(WidgetTree, V, TEXT(""), 16, SEUiStyle::Colors::White);
	PowerBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	PowerBar->SetFillColorAndOpacity(SEUiStyle::Colors::TeamA);
	PowerBar->SetPercent(0.f);
	V->AddChild(PowerBar);
	AttackArc = MakeRowText(WidgetTree, V, TEXT(""), 16, SEUiStyle::Colors::White);
	AttackVerdict = MakeRowText(WidgetTree, V, TEXT(""), 18, SEUiStyle::Colors::White);
	TimingBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	TimingBar->SetFillColorAndOpacity(FLinearColor(0.2f, 1.f, 0.3f));
	TimingBar->SetPercent(0.f);
	V->AddChild(TimingBar);
	TimingLabel = MakeRowText(WidgetTree, V, TEXT(""), 15, SEUiStyle::Colors::White);
	AttackHelp = MakeRowText(WidgetTree, V, TEXT("鼠标移动选点 · 滚轮/W/S 力度 · Q/E 弧线 · 左键确认 · 右键/Esc 取消"), 13, SEUiStyle::Colors::Grey);
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

	SetTitle = MakeRowText(WidgetTree, V, TEXT("二传战术选择"), 22, SEUiStyle::Colors::Gold);
	SetCategory = MakeRowText(WidgetTree, V, TEXT(""), 14, SEUiStyle::Colors::Grey);
	// M11d-4: the 13+1 list lives in a ScrollBox so all items stay reachable at
	// 1280x720; the category line above names the currently selected play's group.
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	SetScroll = Scroll;
	Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	if (UVerticalBoxSlot* ScrollSlot = V->AddChildToVerticalBox(Scroll))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	SetList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UScrollBoxSlot* SS = Cast<UScrollBoxSlot>(Scroll->AddChild(SetList)))
	{
		SS->SetPadding(FMargin(0.f));
	}

	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	for (int32 i = 0; i < Plays.Num(); i++)
	{
		UTacticalChoiceButton* Btn = WidgetTree->ConstructWidget<UTacticalChoiceButton>(UTacticalChoiceButton::StaticClass());
		Btn->InitChoice(i);
		Btn->OnChoiceSelected.AddUObject(this, &UTacticalHUDWidget::HandleSetRowClick);
		Btn->SetStyle(SEUiStyle::ButtonStyle(
			FLinearColor(0.1f, 0.1f, 0.15f, 0.9f),
			FLinearColor(0.25f, 0.25f, 0.35f, 0.9f),
			FLinearColor(0.05f, 0.05f, 0.1f, 0.9f)));
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(FString::Printf(TEXT("%s — %s"), *Plays[i].Category, *Plays[i].DisplayName)));
		T->SetFont(SEUiStyle::Font(14));
		T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
		Btn->AddChild(T);
		UVerticalBoxSlot* BSlot = SetList->AddChildToVerticalBox(Btn);
		BSlot->SetPadding(FMargin(0.f, 1.f));
		BSlot->SetHorizontalAlignment(HAlign_Fill);
		SetRowButtons.Add(Btn);
		SetRows.Add(T);
	}
	SetDetails = MakeRowText(WidgetTree, V, TEXT(""), 14, FLinearColor(0.8f, 0.95f, 1.f));
	SetHelp = MakeRowText(WidgetTree, V, TEXT("鼠标点击或 Q/E 切换 · 左键确认 · 右键/Esc 取消 · 滚轮滚动列表"), 13, SEUiStyle::Colors::Grey);
}

void UTacticalHUDWidget::BuildDefensePanel()
{
	DefenseBorder = MakePanel(WidgetTree, Root, FVector2D(16.f, -330.f), FVector2D(400.f, 340.f));
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DefenseBorder->AddChild(V);

	DefenseTitle = MakeRowText(WidgetTree, V, TEXT("防守决策"), 22, SEUiStyle::Colors::Gold);

	// M11d-4: two labelled columns — 拦网策略 (4) | 后排防守 (4).
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	V->AddChild(H);

	auto MakeColumn = [this, H](const FString& Label, int32 Start, int32 Count, TObjectPtr<UVerticalBox>& OutBox) -> UVerticalBox*
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* HS = H->AddChildToHorizontalBox(Col))
		{
			HS->SetPadding(FMargin(6.f, 4.f));
			HS->SetHorizontalAlignment(HAlign_Fill);
		}
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		L->SetText(FText::FromString(Label));
		L->SetFont(SEUiStyle::Font(14));
		L->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::Gold));
		L->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* LS = Col->AddChildToVerticalBox(L))
		{
			LS->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			LS->SetHorizontalAlignment(HAlign_Fill);
		}
		OutBox = Col;
		return Col;
	};
	MakeColumn(TEXT("拦网策略"), 0, 4, DefenseListBlock);
	MakeColumn(TEXT("后排防守"), 4, 4, DefenseListDig);

	const TArray<FString>& Labels = DefenseOptionLabels();
	for (int32 i = 0; i < Labels.Num(); i++)
	{
		UVerticalBox* Parent = (i < 4) ? DefenseListBlock : DefenseListDig;
		UTacticalChoiceButton* Btn = WidgetTree->ConstructWidget<UTacticalChoiceButton>(UTacticalChoiceButton::StaticClass());
		Btn->InitChoice(i);
		Btn->OnChoiceSelected.AddUObject(this, &UTacticalHUDWidget::HandleDefenseRowClick);
		Btn->SetStyle(SEUiStyle::ButtonStyle(
			FLinearColor(0.1f, 0.1f, 0.15f, 0.9f),
			FLinearColor(0.25f, 0.25f, 0.35f, 0.9f),
			FLinearColor(0.05f, 0.05f, 0.1f, 0.9f)));
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Labels[i]));
		T->SetFont(SEUiStyle::Font(14));
		T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
		Btn->AddChild(T);
		UVerticalBoxSlot* BSlot = Parent->AddChildToVerticalBox(Btn);
		BSlot->SetPadding(FMargin(0.f, 2.f));
		BSlot->SetHorizontalAlignment(HAlign_Fill);
		DefenseRowButtons.Add(Btn);
		DefenseRows.Add(T);
	}
	DefenseHelp = MakeRowText(WidgetTree, V, TEXT("选择改变队友站位与拦网 · 超时使用 AI 默认"), 13, SEUiStyle::Colors::Grey);
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

namespace
{
	/** Friendly landing-zone name instead of raw cm coordinates. */
	FString ZoneFromTarget(const FVector& T, int32 TeamSide)
	{
		// X is distance from the net; Y is lateral. Left/right is relative to
		// the attacker facing the net, and mirrors for the opposite team.
		const float Lateral = T.Y * TeamSide;
		const FString LR = (Lateral > 150.f) ? TEXT("左") : (Lateral < -150.f) ? TEXT("右") : TEXT("中");
		const FString FB = (FMath::Abs(T.X) > 300.f) ? TEXT("后场") : TEXT("前场");
		return FB + LR;
	}

	FString ArcLabel(float ApexZ)
	{
		if (ApexZ < 160.f) { return TEXT("低"); }
		if (ApexZ < 260.f) { return TEXT("中"); }
		return TEXT("高");
	}
}

void UTacticalHUDWidget::UpdateAttackInfo(const FShotIntent& Intent, const SEVolleyballTrajectory::FShotSolution& Sol)
{
	const ASpikeEliteCharacter* Player = Cast<ASpikeEliteCharacter>(GetOwningPlayerPawn());
	const int32 TeamSide = Player ? Player->TeamSide : 1;
	FString Verdict;
	FLinearColor VerdictColor = SEUiStyle::Colors::Error;
	if (!Sol.bValid) { Verdict = TEXT("不可行"); }
	else if (Sol.bCrossedNet && Sol.bInBounds) { Verdict = TEXT("界内 可行"); VerdictColor = SEUiStyle::Colors::Safe; }
	else if (Sol.bCrossedNet || Sol.bInBounds) { Verdict = TEXT("接近边界/触网风险"); VerdictColor = SEUiStyle::Colors::Warn; }
	else { Verdict = TEXT("出界或触网"); }

	const FString Key = FString::Printf(TEXT("A|%.0f|%.0f|%.2f|%.2f|%.2f|%s"),
		Intent.TargetLocation.X, Intent.TargetLocation.Y, Intent.Power, Sol.FlightTime, Sol.Apex.Z, *Verdict);
	if (Key == LastAttackKey) { return; }
	LastAttackKey = Key;

	const FString TypeLabel = (Intent.TouchType == EBallTouchType::Set) ? TEXT("二传")
		: (Intent.TouchType == EBallTouchType::Receive) ? TEXT("接球") : TEXT("扣球/吊球");
	AttackHeader->SetText(FText::FromString(FString::Printf(TEXT("战术瞄准 — %s"), *TypeLabel)));
	AttackStage->SetText(FText::FromString(TEXT("选择落点")));
	AttackTarget->SetText(FText::FromString(FString::Printf(TEXT("目标区：%s"), *ZoneFromTarget(Intent.TargetLocation, TeamSide))));
	AttackPower->SetText(FText::FromString(FString::Printf(TEXT("力度 %d%%"), FMath::RoundToInt(Intent.Power * 100.f))));
	if (PowerBar) { PowerBar->SetPercent(FMath::Clamp(Intent.Power, 0.f, 1.f)); }
	AttackArc->SetText(FText::FromString(FString::Printf(TEXT("弧线 %s · 飞行 %.2f 秒"), *ArcLabel(Sol.Apex.Z), Sol.FlightTime)));
	AttackVerdict->SetText(FText::FromString(Verdict));
	AttackVerdict->SetColorAndOpacity(FSlateColor(VerdictColor));
}

void UTacticalHUDWidget::ShowTiming(float Progress01, const FString& Status)
{
	if (TimingBar) { TimingBar->SetPercent(FMath::Clamp(Progress01, 0.f, 1.f)); }
	if (AttackStage) { AttackStage->SetText(FText::FromString(TEXT("等待击球时机"))); }
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
	if (SetScroll && SetRowButtons.IsValidIndex(Selected))
	{
		SetScroll->ScrollWidgetIntoView(SetRowButtons[Selected], false);
	}
	const TArray<FSetPlayDefinition>& Plays = SESetPlays::GetPlays();
	for (int32 i = 0; i < SetRows.Num() && i < Plays.Num(); i++)
	{
		const bool bSel = (i == Selected);
		SetRows[i]->SetText(FText::FromString(FString::Printf(TEXT("%s %s — %s"), bSel ? TEXT("▶") : TEXT("  "), *Plays[i].Category, *Plays[i].DisplayName)));
		SetRows[i]->SetColorAndOpacity(FSlateColor(bSel ? SEUiStyle::Colors::Gold : SEUiStyle::Colors::White));
	}
	if (SetCategory)
	{
		SetCategory->SetText(FText::FromString(
			Plays.IsValidIndex(Selected)
			? FString::Printf(TEXT("当前分组：%s"), *Plays[Selected].Category)
			: TEXT("")));
	}
	if (SetDetails)
	{
		FString D = TEXT("");
		if (Plays.IsValidIndex(Selected))
		{
			const FSetPlayDefinition& P = Plays[Selected];
			const FString Risk = (P.RiskLevel == 2) ? TEXT("高") : (P.RiskLevel == 1) ? TEXT("中") : TEXT("低");
			D = FString::Printf(TEXT("后排:%s · 弧高 %.0f · 飞行 %.2fs · 风险 %s"),
				P.bBackRowAttack ? TEXT("是") : TEXT("否"), P.ApexHeight, P.DesiredFlightTime, *Risk);
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
