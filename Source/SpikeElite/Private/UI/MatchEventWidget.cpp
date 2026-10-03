// SPDX-License-Identifier: MIT
#include "UI/MatchEventWidget.h"
#include "UI/SEUiStyle.h"
#include "SpikeEliteGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
TSharedRef<SWidget> UMatchEventWidget::RebuildWidget()
{
	if(!WidgetTree->RootWidget)
	{
		auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
		Card=WidgetTree->ConstructWidget<UBorder>(); Card->SetBrushColor(FLinearColor(.02f,.05f,.1f,.92f)); Card->SetPadding(FMargin(12));
		if(auto* S=Root->AddChildToCanvas(Card)) { S->SetAnchors(FAnchors(.10f,.73f,.90f,.94f)); S->SetOffsets(FMargin(0)); }
		Text=WidgetTree->ConstructWidget<UTextBlock>(); Text->SetFont(SEUiStyle::Font(16)); Text->SetAutoWrapText(true);
		Text->SetColorAndOpacity(SEUiStyle::Colors::White); Card->AddChild(Text);
	}
	return Super::RebuildWidget();
}
void UMatchEventWidget::NativeTick(const FGeometry& G,float Dt)
{
	Super::NativeTick(G,Dt); auto* GM=Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)); if(!GM || !Card) return;
	FString Value;
	if(GM->MatchState==EMatchState::Entrance)
	{
		Value=FString::Printf(TEXT("球员入场 · %.0fs · E 跳过\n"),GM->EntranceRemaining);
		for(const auto& R : {GM->RosterA,GM->RosterB})
		{
			Value+=R.Registered[0].PlayerId.StartsWith(TEXT("A"))?TEXT("A 队首发："):TEXT("B 队首发：");
			for(const FString& Id:R.StartingLineup) if(const auto* P=R.FindById(Id)) Value+=FString::Printf(TEXT("#%d %s  "),P->JerseyNumber,*P->DisplayName);
			Value+=TEXT("\n");
		}
	}
	else if(GM->MatchState==EMatchState::Timeout)
		Value=FString::Printf(TEXT("%s 队球队暂停 · 剩余 %.0fs\n暂停 A:%d B:%d · 换人 A:%d B:%d · Tab 打开教练席，Esc 为系统暂停"),GM->TimeoutTeam==EVolleyballTeam::TeamA?TEXT("A"):TEXT("B"),FMath::CeilToFloat(GM->TimeoutTimer),GM->TimeoutLeftA,GM->TimeoutLeftB,GM->SubstitutionsLeftA,GM->SubstitutionsLeftB);
	else if(GM->GetMatchMode()==EGameModeChoice::Training) Value=GM->GetPracticeStatus();
	Card->SetVisibility(Value.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
	if(Value!=Last) { Last=Value; Text->SetText(FText::FromString(Value)); }
}
