// SPDX-License-Identifier: MIT
#include "UI/RotationWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"

namespace
{
	FString SlotLabel(const FRotationSlotView& V)
	{
		FString Label = V.Jersey;
		if (V.bServer)    { Label += TEXT("●"); }
		if (V.bControlled) { Label += TEXT("★"); }
		return Label;
	}
}

TSharedRef<SWidget> URotationWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void URotationWidget::BuildWidgetTree()
{
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RotationRoot"));
	WidgetTree->RootWidget = Panel;

	// ---- backdrop (compact, ~14-16% of 1280 width) -----------------------------
	UImage* Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RotationBackdrop"));
	Backdrop->SetBrush(SEUiStyle::BackdropBrush(0.62f));
	UCanvasPanelSlot* BackSlot = Panel->AddChildToCanvas(Backdrop);
	BackSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
	BackSlot->SetAlignment(FVector2D(1.f, 0.f));
	BackSlot->SetPosition(FVector2D(-16.f, 64.f));
	BackSlot->SetSize(FVector2D(216.f, 300.f));

	// ---- title -------------------------------------------------------------------
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RotationTitle"));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.9f, 0.4f)));
	TitleText->SetFont(SEUiStyle::Font(16));
	TitleText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* TitleSlot = Panel->AddChildToCanvas(TitleText);
	TitleSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
	TitleSlot->SetAlignment(FVector2D(1.f, 0.f));
	TitleSlot->SetPosition(FVector2D(-220.f, 72.f));
	TitleSlot->SetSize(FVector2D(204.f, 24.f));

	// ---- mini court (right-anchored, inner box from x=-204..-36) ------------------
	auto MakeLine = [Panel](UWidgetTree* Tree, const FLinearColor& Color, float W, float H, float X, float Y) -> UImage*
	{
		UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrush(SEUiStyle::SolidBrush(Color));
		UCanvasPanelSlot* S = Panel->AddChildToCanvas(Img);
		S->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		S->SetAlignment(FVector2D(0.f, 0.f));
		S->SetPosition(FVector2D(X, Y));
		S->SetSize(FVector2D(W, H));
		return Img;
	};

	// Court frame: four thin lines (outline of the vertical court).
	const float CLeft = -204.f, CRight = -36.f, CTop = 104.f, CBottom = 352.f;
	MakeLine(WidgetTree, FLinearColor(0.75f, 0.80f, 0.85f, 0.55f), CRight - CLeft, 2.f, CLeft, CTop);
	MakeLine(WidgetTree, FLinearColor(0.75f, 0.80f, 0.85f, 0.55f), CRight - CLeft, 2.f, CLeft, CBottom);
	MakeLine(WidgetTree, FLinearColor(0.75f, 0.80f, 0.85f, 0.55f), 2.f, CBottom - CTop, CLeft, CTop);
	MakeLine(WidgetTree, FLinearColor(0.75f, 0.80f, 0.85f, 0.55f), 2.f, CBottom - CTop, CRight, CTop);

	// Net (horizontal, bright).
	NetLine = MakeLine(WidgetTree, FLinearColor(0.85f, 0.95f, 1.0f, 0.8f), CRight - CLeft, 3.f, CLeft, 228.f);

	// Three-metre lines (short segments, both halves).
	LineA3m = MakeLine(WidgetTree, FLinearColor(0.85f, 0.85f, 0.90f, 0.45f), CRight - CLeft, 2.f, CLeft, 288.f);
	LineB3m = MakeLine(WidgetTree, FLinearColor(0.85f, 0.85f, 0.90f, 0.45f), CRight - CLeft, 2.f, CLeft, 168.f);

	// ---- slot cells ---------------------------------------------------------------
	// Court inner columns (3) at x=-186 / -140 / -94, rows per half.
	// Team B (top): front row y=140 (P4 P3 P2 from left->right), back row y=192 (P5 P6 P1)
	// Team A (bottom): front row y=308 (P2 P3 P4), back row y=256 (P1 P6 P5)
	// Roster index -> slot: 0=P1(back-right), 1=P2(front-right), 2=P3(front-mid),
	// 3=P4(front-left), 4=P5(back-left), 5=P6(back-mid).
	static const int32 RowMap[6] = { 1, 0, 0, 0, 1, 1 };  // 0=front, 1=back
	static const int32 ColMap[6] = { 2, 2, 1, 0, 0, 1 };  // 0=left, 1=mid, 2=right
	const float ColX[3] = { -186.f, -140.f, -94.f };
	const float FrontY[2] = { 140.f, 308.f };  // [teamB, teamA]
	const float BackY[2]  = { 192.f, 256.f };  // [teamB, teamA]

	for (int32 i = 0; i < 12; ++i)
	{
		const bool bTeamB = i >= 6;
		const int32 Idx = i % 6;
		const float Y = (RowMap[Idx] == 0) ? FrontY[bTeamB ? 0 : 1] : BackY[bTeamB ? 0 : 1];
		// ColMap is in left->right court order; mirror the column for Team B so
		// its front row also reads P4-P3-P2 from ITS left sideline.
		const int32 Col = bTeamB ? (2 - ColMap[Idx]) : ColMap[Idx];
		const float X = ColX[Col];

		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Dot->SetBrush(SEUiStyle::SolidBrush(bTeamB ? SEUiStyle::Colors::TeamB : SEUiStyle::Colors::TeamA));
		UCanvasPanelSlot* DS = Panel->AddChildToCanvas(Dot);
		DS->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		DS->SetAlignment(FVector2D(0.f, 0.f));
		DS->SetPosition(FVector2D(X, Y));
		DS->SetSize(FVector2D(20.f, 20.f));
		SlotDots.Add(Dot);

		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetColorAndOpacity(FSlateColor(SEUiStyle::Colors::White));
		T->SetFont(SEUiStyle::Font(13));
		T->SetJustification(ETextJustify::Center);
		T->SetText(FText::FromString(TEXT("-")));
		UCanvasPanelSlot* TS = Panel->AddChildToCanvas(T);
		TS->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		TS->SetAlignment(FVector2D(0.f, 0.f));
		TS->SetPosition(FVector2D(X + 2.f, Y + 2.f));
		TS->SetSize(FVector2D(16.f, 16.f));
		SlotTexts.Add(T);
	}
}

void URotationWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void URotationWidget::Refresh(const FRotationViewState& State)
{
	if (!TitleText || SlotTexts.Num() != 12 || SlotDots.Num() != 12) { return; }

	const FString Rotating = State.bJustRotated ? TEXT(" · 轮转") : TEXT("");
	FString Sig = FString::Printf(TEXT("%d|%d|%d"), State.RotationIndex,
		(int32)State.ServingTeam, State.bJustRotated ? 1 : 0);
	for (const FRotationSlotView& V : State.TeamA)
	{
		Sig += FString::Printf(TEXT("|%s:%d:%d"), *V.Jersey, V.bServer ? 1 : 0, V.bControlled ? 1 : 0);
	}
	for (const FRotationSlotView& V : State.TeamB)
	{
		Sig += FString::Printf(TEXT("|%s:%d:%d"), *V.Jersey, V.bServer ? 1 : 0, V.bControlled ? 1 : 0);
	}
	if (Sig == LastSignature) { return; }
	LastSignature = Sig;

	const FString ServeName = (State.ServingTeam == EVolleyballTeam::TeamA) ? TEXT("A") : TEXT("B");
	TitleText->SetText(FText::FromString(
		FString::Printf(TEXT("轮次 %d/6  发球 %s%s"), State.RotationIndex, *ServeName, *Rotating)));

	for (int32 i = 0; i < 6; ++i)
	{
		if (State.TeamA.IsValidIndex(i))
		{
			const FRotationSlotView& V = State.TeamA[i];
			SlotTexts[i]->SetText(FText::FromString(SlotLabel(V)));
			SlotDots[i]->SetBrush(SEUiStyle::SolidBrush(
				V.bControlled ? SEUiStyle::Colors::White :
				(V.bServer ? SEUiStyle::Colors::Gold : SEUiStyle::Colors::TeamA)));
		}
		if (State.TeamB.IsValidIndex(i))
		{
			const FRotationSlotView& V = State.TeamB[i];
			SlotTexts[i + 6]->SetText(FText::FromString(SlotLabel(V)));
			SlotDots[i + 6]->SetBrush(SEUiStyle::SolidBrush(
				V.bControlled ? SEUiStyle::Colors::White :
				(V.bServer ? SEUiStyle::Colors::Gold : SEUiStyle::Colors::TeamB)));
		}
	}
}

void URotationWidget::ToggleVisible()
{
	SetVisibility(GetVisibility() == ESlateVisibility::Visible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}
