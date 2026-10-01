// SPDX-License-Identifier: MIT
#include "UI/RotationWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Stable slot->text helper (jersey + marks).
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
	// Right-top anchored panel. Everything is built here (RebuildWidget), which
	// is the correct lifecycle for pure-C++ UMG widgets: the Slate widget is
	// built from this tree when Super::RebuildWidget runs.
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RotationRoot"));
	WidgetTree->RootWidget = Panel;

	auto MakeCell = [this, Panel](TObjectPtr<UTextBlock>& OutSlot, float X, float Y, const FLinearColor& Color, int32 FontSize)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), FontSize));
		T->SetJustification(ETextJustify::Center);
		T->SetText(FText::FromString(TEXT("-")));
		UCanvasPanelSlot* CanvasSlot = Panel->AddChildToCanvas(T);
		// M11c-6: the slot is anchored at the RIGHT edge with right alignment —
		// its offset must be NEGATIVE (distance from the right edge), otherwise
		// the whole grid slides off-screen at 1280x720.
		CanvasSlot->SetPosition(FVector2D(X - 200.f, Y));
		CanvasSlot->SetSize(FVector2D(54.f, 30.f));
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetAutoSize(false);
		OutSlot = T;
	};

	// Translucent backdrop so text stays readable over the arena.
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RotationBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	UCanvasPanelSlot* BackSlot = Panel->AddChildToCanvas(Backdrop);
	BackSlot->SetPosition(FVector2D(0.f, 0.f));
	BackSlot->SetSize(FVector2D(200.f, 250.f));
	BackSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
	BackSlot->SetAlignment(FVector2D(1.f, 0.f));

	MakeCell(TitleText, 8.f, 8.f, FLinearColor(1.f, 1.f, 0.6f), 18);
	UCanvasPanelSlot* TitleSlot = Panel->AddChildToCanvas(TitleText);
	TitleSlot->SetPosition(FVector2D(-192.f, 8.f));
	TitleSlot->SetSize(FVector2D(184.f, 24.f));
	TitleSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
	TitleSlot->SetAlignment(FVector2D(1.f, 0.f));

	// Two 2x3 grids per team. FIVB slot layout (from each team's own back-line
	// perspective): front row P4 P3 P2 (left->right), back row P5 P6 P1.
	// Roster index -> slot: 0=P1(back-right), 1=P2(front-right), 2=P3(front-mid),
	// 3=P4(front-left), 4=P5(back-left), 5=P6(back-mid).
	static const int32 RowMap[6] = { 1, 0, 0, 0, 1, 1 };  // 0=front, 1=back
	static const int32 ColMap[6] = { 2, 2, 1, 0, 0, 1 };  // 0=left, 1=mid, 2=right
	for (int32 i = 0; i < 12; ++i)
	{
		const bool bTeamB = i >= 6;
		const int32 Idx = i % 6;
		const float Row = (float)RowMap[Idx];
		const float Col = (float)ColMap[Idx];
		const float X = 200.f - 8.f - (Col + 1.f) * 58.f;
		const float Y = (bTeamB ? 150.f : 40.f) + Row * 52.f;
		const bool bFront = (Idx == 1 || Idx == 2 || Idx == 3);
		TObjectPtr<UTextBlock> T;
		MakeCell(T, X, Y, bFront ? FLinearColor(1.f, 1.f, 1.f) : FLinearColor(0.75f, 0.75f, 0.78f), 22);
		SlotTexts.Add(T);
	}

	// Net divider label.
	NetLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NetLabel"));
	NetLabel->SetText(FText::FromString(TEXT("— 网 —")));
	NetLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.9f, 1.f)));
	NetLabel->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
	NetLabel->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* NetSlot = Panel->AddChildToCanvas(NetLabel);
	NetSlot->SetPosition(FVector2D(-168.f, 126.f));
	NetSlot->SetSize(FVector2D(160.f, 22.f));
	NetSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
	NetSlot->SetAlignment(FVector2D(1.f, 0.f));
}

void URotationWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void URotationWidget::Refresh(const FRotationViewState& State)
{
	if (!TitleText || SlotTexts.Num() != 12) return;

	FString Sig = FString::Printf(TEXT("%d|%d|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s"),
		State.RotationIndex, (int32)State.ServingTeam,
		*State.TeamA[0].Jersey, *State.TeamA[1].Jersey, *State.TeamA[2].Jersey,
		*State.TeamA[3].Jersey, *State.TeamA[4].Jersey, *State.TeamA[5].Jersey,
		*State.TeamB[0].Jersey, *State.TeamB[1].Jersey, *State.TeamB[2].Jersey,
		*State.TeamB[3].Jersey, *State.TeamB[4].Jersey, *State.TeamB[5].Jersey);
	if (Sig == LastSignature) { return; }
	LastSignature = Sig;

	const FString ServeName = (State.ServingTeam == EVolleyballTeam::TeamA) ? TEXT("A") : TEXT("B");
	TitleText->SetText(FText::FromString(
		FString::Printf(TEXT("轮次 %d/6  发球 %s"), State.RotationIndex, *ServeName)));

	for (int32 i = 0; i < 6; ++i)
	{
		if (State.TeamA.IsValidIndex(i)) SlotTexts[i]->SetText(FText::FromString(SlotLabel(State.TeamA[i])));
		if (State.TeamB.IsValidIndex(i)) SlotTexts[i + 6]->SetText(FText::FromString(SlotLabel(State.TeamB[i])));
	}
}

void URotationWidget::ToggleVisible()
{
	SetVisibility(GetVisibility() == ESlateVisibility::Visible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}