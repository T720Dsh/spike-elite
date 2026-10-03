// SPDX-License-Identifier: MIT
#include "UI/TeamRosterWidget.h"
#include "UI/TacticalHUDWidget.h"
#include "UI/SEUiStyle.h"
#include "SpikeEliteGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UTeamRosterWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget) Build();
	return Super::RebuildWidget();
}

void UTeamRosterWidget::Configure(ASpikeEliteGameMode* Mode, bool bEditor)
{
	GM=Mode; bEdit=bEditor; Team=EVolleyballTeam::TeamA;
	if (Mode) { DraftA=Mode->RosterA; DraftB=Mode->RosterB; }
	Refresh();
}

void UTeamRosterWidget::Build()
{
	UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
	UBorder* Backdrop=WidgetTree->ConstructWidget<UBorder>(); Backdrop->SetBrushColor(FLinearColor(.025f,.045f,.09f,.97f));
	if (auto* S=Root->AddChildToCanvas(Backdrop)) { S->SetAnchors(FAnchors(.12f,.07f,.88f,.94f)); S->SetOffsets(FMargin(0)); }
	auto* Layout=WidgetTree->ConstructWidget<UCanvasPanel>(); Backdrop->AddChild(Layout);
	UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>();
	if(auto* S=Layout->AddChildToCanvas(Scroll)) { S->SetAnchors(FAnchors(0,0,1,.78f)); S->SetOffsets(FMargin(0)); }
	Rows=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Rows);
	Message=WidgetTree->ConstructWidget<UTextBlock>(); Message->SetFont(SEUiStyle::Font(15));
	Message->SetColorAndOpacity(SEUiStyle::Colors::Gold); Message->SetAutoWrapText(true);
	if(auto* S=Layout->AddChildToCanvas(Message)) { S->SetAnchors(FAnchors(0,.79f,1,.86f)); S->SetOffsets(FMargin(14,0,14,0)); }
	auto* Footer=WidgetTree->ConstructWidget<UHorizontalBox>();
	if(auto* S=Layout->AddChildToCanvas(Footer)) { S->SetAnchors(FAnchors(0,.86f,1,.98f)); S->SetOffsets(FMargin(12,0,12,0)); }
	auto Button=[&](const TCHAR* Label)
	{
		auto* B=WidgetTree->ConstructWidget<UButton>(); B->SetStyle(SEUiStyle::PrimaryButton());
		auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label)); T->SetAutoWrapText(true);
		T->SetFont(SEUiStyle::Font(16)); T->SetColorAndOpacity(SEUiStyle::Colors::White); B->AddChild(T);
		auto* S=Footer->AddChildToHorizontalBox(B); S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(4)); return B;
	};
	auto* ConfirmButton=Button(TEXT("确认"));
	ConfirmLabel=Cast<UTextBlock>(ConfirmButton->GetChildAt(0));
	ConfirmButton->OnClicked.AddDynamic(this,&UTeamRosterWidget::Confirm);
	TeamSwitch=Button(TEXT("切换 A / B 队")); TeamSwitch->OnClicked.AddDynamic(this,&UTeamRosterWidget::SwitchTeam);
	Button(TEXT("取消 / 返回"))->OnClicked.AddDynamic(this,&UTeamRosterWidget::Back);
	SetIsFocusable(true);
}

void UTeamRosterWidget::Refresh()
{
	if (!Rows || !GM.IsValid()) return;
	Rows->ClearChildren();
	if(ConfirmLabel) ConfirmLabel->SetText(FText::FromString(bEdit?TEXT("确认 / 保存并开始"):TEXT("确认换人")));
	if(TeamSwitch) TeamSwitch->SetVisibility(bEdit?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	if (!bEdit) { DraftA=GM->RosterA; DraftB=GM->RosterB; }
	FTeamRosterState& R=Team==EVolleyballTeam::TeamA ? DraftA : DraftB;
	auto Text=[this](const FString& Value,int32 Size=16)
	{
		auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value));
		T->SetFont(SEUiStyle::Font(Size)); T->SetColorAndOpacity(SEUiStyle::Colors::White); T->SetAutoWrapText(true);
		Rows->AddChildToVerticalBox(T)->SetPadding(FMargin(14,6)); return T;
	};
	auto Choice=[&](const FString& Label,int32 Index,bool bSlot,bool bEnabled=true)
	{
		auto* B=WidgetTree->ConstructWidget<UTacticalChoiceButton>(); B->InitChoice(Index);
		B->SetStyle((bSlot && Index==SelectedSlot)?SEUiStyle::PrimaryButton():SEUiStyle::SecondaryButton());
		if(bSlot) B->OnChoiceSelected.AddUObject(this,&UTeamRosterWidget::ChooseSlot);
		else B->OnChoiceSelected.AddUObject(this,&UTeamRosterWidget::ChoosePlayer);
		auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label));
		T->SetFont(SEUiStyle::Font(16)); T->SetColorAndOpacity(SEUiStyle::Colors::White); T->SetAutoWrapText(true);
		B->AddChild(T); B->SetIsEnabled(bEnabled); Rows->AddChildToVerticalBox(B)->SetPadding(FMargin(14,3));
	};
	Text(bEdit?TEXT("赛前首发与 12 人名单"):TEXT("球队换人 · 选择期间系统暂停"),26);
	Text(FString::Printf(TEXT("%s 队 · 选择场上 P1–P6，再选择名单球员。%s"),Team==EVolleyballTeam::TeamA?TEXT("A"):TEXT("B"),
		bEdit?TEXT("选择仅预览；确认后保存，两队各 6 名首发。"):TEXT("仅哨前/球队暂停合法，取消不消耗次数。")));
	for(int32 I=0;I<R.OnCourtLineup.Num();++I)
		if(const auto* P=R.CourtPlayer(I)) Choice(FString::Printf(TEXT("P%d  #%d  %s · %s %s"),I+1,P->JerseyNumber,*P->DisplayName,
			SEVolleyballRoster::RoleDisplayName(P->Role),I==SelectedSlot?TEXT(" ← 已选槽位"):TEXT("")),I,true);
	Text(bEdit?TEXT("完整名单（在场球员可交换首发槽位）"):TEXT("候补名单（合法回归首发也会列在这里）"),19);
	for(int32 I=0;I<R.Registered.Num();++I)
	{
		const auto& P=R.Registered[I]; bool OnCourt=R.OnCourtLineup.Contains(P.PlayerId);
		if(!bEdit && OnCourt) continue;
		Choice(FString::Printf(TEXT("#%d %s · %s\n发球 %.0f / 传球 %.0f / 拦网 %.0f%s"),P.JerseyNumber,*P.DisplayName,
			SEVolleyballRoster::RoleDisplayName(P.Role),P.ServeAccuracy*100,P.PassAccuracy*100,P.BlockSkill*100,
			I==SelectedPlayer?TEXT("  [待确认]"):TEXT("")),I,false);
	}
	if(Message) Message->SetText(FText::FromString(SelectedPlayer==INDEX_NONE?TEXT("请选择球员；Esc 取消。"):TEXT("已选择球员；按确认提交。")));
}

void UTeamRosterWidget::ChooseSlot(int32 Index) { SelectedSlot=Index; SelectedPlayer=INDEX_NONE; Refresh(); }
void UTeamRosterWidget::ChoosePlayer(int32 Index)
{
	FTeamRosterState& R=Team==EVolleyballTeam::TeamA?DraftA:DraftB;
	if(!R.Registered.IsValidIndex(Index) || !R.OnCourtLineup.IsValidIndex(SelectedSlot)) return;
	SelectedPlayer=Index;
	if(bEdit)
	{
		const FString Id=R.Registered[Index].PlayerId; int32 Existing=R.OnCourtIndex(Id);
		if(Existing!=INDEX_NONE) Swap(R.OnCourtLineup[Existing],R.OnCourtLineup[SelectedSlot]);
		else R.OnCourtLineup[SelectedSlot]=Id;
		R.StartingLineup=R.OnCourtLineup;
	}
	Refresh();
}
void UTeamRosterWidget::Confirm()
{
	if(!GM.IsValid()) return;
	FString Reason;
	if(bEdit)
	{
		if(!SEVolleyballRoster::ValidateRoster(DraftA,Reason) || !SEVolleyballRoster::ValidateRoster(DraftB,Reason))
		{ Message->SetText(FText::FromString(Reason)); return; }
		GM->SetStartingLineup(EVolleyballTeam::TeamA,DraftA.StartingLineup,Reason);
		GM->SetStartingLineup(EVolleyballTeam::TeamB,DraftB.StartingLineup,Reason);
		if(!GM->SaveLineups()) { Message->SetText(FText::FromString(TEXT("名单保存失败，请检查磁盘空间"))); return; }
		OnDone.ExecuteIfBound();
	}
	else
	{
		const auto& R=GM->RosterA;
		if(!R.Registered.IsValidIndex(SelectedPlayer)) { Message->SetText(FText::FromString(TEXT("请先选择候补球员"))); return; }
		const FString Id=R.Registered[SelectedPlayer].PlayerId;
		if(!GM->CanRequestSubstitution(Team,SelectedSlot,Id,Reason) || !GM->RequestSubstitution(Team,SelectedSlot,Id))
		{ Message->SetText(FText::FromString(Reason)); return; }
		OnDone.ExecuteIfBound();
	}
}
void UTeamRosterWidget::Back() { OnBack.ExecuteIfBound(); }
void UTeamRosterWidget::SwitchTeam() { Team=Team==EVolleyballTeam::TeamA?EVolleyballTeam::TeamB:EVolleyballTeam::TeamA; SelectedPlayer=INDEX_NONE; Refresh(); }
FReply UTeamRosterWidget::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E)
{
	if(E.GetKey()==EKeys::Escape) { Back(); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(G,E);
}
