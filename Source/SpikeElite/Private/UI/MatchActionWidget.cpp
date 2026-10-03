#include "UI/MatchActionWidget.h"
#include "SpikeElitePlayerController.h"
#include "UI/SEUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"

TSharedRef<SWidget> UMatchActionWidget::RebuildWidget()
{
	SetIsFocusable(true);
	if (!WidgetTree->RootWidget)
	{
		auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
		auto* Surface=WidgetTree->ConstructWidget<UBorder>(); Surface->SetBrushColor(FLinearColor(0,0,0,0));
		auto* S=Root->AddChildToCanvas(Surface); S->SetAnchors(FAnchors(0,0,1,1)); S->SetOffsets(FMargin(0));
		auto* Background=WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor(.02f,.04f,.08f,.92f));
		S=Root->AddChildToCanvas(Background); S->SetAnchors(FAnchors(.78f,.48f,.98f,.86f)); S->SetOffsets(FMargin(0));
		auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Background->AddChild(Scroll);
		auto* Grid=WidgetTree->ConstructWidget<UUniformGridPanel>(); Grid->SetSlotPadding(FMargin(5)); Grid->SetMinDesiredSlotHeight(56); Scroll->AddChild(Grid);
		auto* Footer=WidgetTree->ConstructWidget<UHorizontalBox>();
		S=Root->AddChildToCanvas(Footer); S->SetAnchors(FAnchors(.78f,.88f,.98f,.97f)); S->SetOffsets(FMargin(0));
		const TCHAR* Labels[]={TEXT("发球 / 跳过"),TEXT("接一传"),TEXT("二传"),TEXT("扣球"),TEXT("抬手 / 拦网"),TEXT("跳跃"),TEXT("切换球员"),TEXT("教练"),TEXT("暂停"),TEXT("收起鼠标")};
		for (const TCHAR* Label : Labels)
		{
			auto* B=WidgetTree->ConstructWidget<UButton>(); B->SetStyle(SEUiStyle::SecondaryButton());
			auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label)); T->SetFont(SEUiStyle::Font(15)); T->SetAutoWrapText(true); T->SetJustification(ETextJustify::Center); T->SetColorAndOpacity(FLinearColor::White); B->AddChild(T);
			if(auto* ContentSlot=Cast<UButtonSlot>(T->Slot)) ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			if(Actions.Num()<7) { auto* GridSlot=Grid->AddChildToUniformGrid(B,Actions.Num()/2,Actions.Num()%2); GridSlot->SetHorizontalAlignment(HAlign_Fill); GridSlot->SetVerticalAlignment(VAlign_Fill); }
			else { auto* FooterSlot=Footer->AddChildToHorizontalBox(B); FooterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); FooterSlot->SetPadding(FMargin(3)); }
			Actions.Add(B);
		}
		Actions[0]->OnClicked.AddDynamic(this,&UMatchActionWidget::Serve); Actions[1]->OnClicked.AddDynamic(this,&UMatchActionWidget::Receive);
		Actions[2]->OnClicked.AddDynamic(this,&UMatchActionWidget::Set); Actions[3]->OnClicked.AddDynamic(this,&UMatchActionWidget::Attack);
		Actions[4]->OnClicked.AddDynamic(this,&UMatchActionWidget::Block); Actions[5]->OnClicked.AddDynamic(this,&UMatchActionWidget::Jump);
		Actions[6]->OnClicked.AddDynamic(this,&UMatchActionWidget::Switch); Actions[7]->OnClicked.AddDynamic(this,&UMatchActionWidget::Coach);
		Actions[8]->OnClicked.AddDynamic(this,&UMatchActionWidget::Pause); Actions[9]->OnClicked.AddDynamic(this,&UMatchActionWidget::Close);
		Hint=WidgetTree->ConstructWidget<UTextBlock>(); Hint->SetFont(SEUiStyle::Font(17)); Hint->SetAutoWrapText(true); Hint->SetColorAndOpacity(FLinearColor::White);
		S=Root->AddChildToCanvas(Hint); S->SetAnchors(FAnchors(.04f,.75f,.74f,.88f)); S->SetOffsets(FMargin(0)); Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}
void UMatchActionWidget::Refresh(bool bServing,float Arc)
{
	if(Hint) Hint->SetText(FText::FromString(bServing ? FString::Printf(TEXT("白色虚线 = 实际发球球路\n点击对方场地选择落点；按住左键上下拖动调弧度（%.1f 米）\n点击发球或按 E；Alt 收起鼠标。8 秒发球限制仍然有效。"),Arc/100.f) : TEXT("鼠标操作：点击右侧按钮；Alt 收起鼠标。\n比赛不会因操作面板而暂停；击球仍需在合法触球范围内。")));
	if(Actions.Num()==10) for(int32 I=1;I<=4;++I) Actions[I]->SetIsEnabled(!bServing);
}
FReply UMatchActionWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
	if(E.GetEffectingButton()==EKeys::LeftMouseButton)
	{
		auto* PC=GetOwningPlayer<ASpikeElitePlayerController>();
		if(PC && PC->IsServeAiming()) { bDragging=true; LastDrag=E.GetScreenSpacePosition(); PC->PickServeTarget(LastDrag); return FReply::Handled().CaptureMouse(TakeWidget()); }
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(G,E);
}
FReply UMatchActionWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
	if(bDragging) { const FVector2D P=E.GetScreenSpacePosition(); if(auto* PC=GetOwningPlayer<ASpikeElitePlayerController>()) PC->DragServeAim((P-LastDrag)/G.Scale); LastDrag=P; return FReply::Handled(); }
	return Super::NativeOnMouseMove(G,E);
}
FReply UMatchActionWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
	if(E.GetEffectingButton()==EKeys::LeftMouseButton) { bDragging=false; return FReply::Handled().ReleaseMouseCapture(); }
	return Super::NativeOnMouseButtonUp(G,E);
}
void UMatchActionWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& E) { bDragging=false; Super::NativeOnMouseCaptureLost(E); }
FReply UMatchActionWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
	if(E.GetKey()==EKeys::LeftAlt || E.GetKey()==EKeys::RightAlt) { if(!E.IsRepeat()) Close(); return FReply::Handled(); }
	if(E.GetKey()==EKeys::E) { Serve(); return FReply::Handled(); }
	if(E.GetKey()==EKeys::Escape) { Pause(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(G,E);
}
void UMatchActionWidget::Dispatch(int32 A) { if(auto* PC=GetOwningPlayer<ASpikeElitePlayerController>()) PC->PerformMatchAction(A); }
void UMatchActionWidget::Serve(){Dispatch(0);} void UMatchActionWidget::Receive(){Dispatch(1);} void UMatchActionWidget::Set(){Dispatch(2);}
void UMatchActionWidget::Attack(){Dispatch(3);} void UMatchActionWidget::Block(){Dispatch(4);} void UMatchActionWidget::Jump(){Dispatch(5);}
void UMatchActionWidget::Switch(){Dispatch(6);} void UMatchActionWidget::Coach(){Dispatch(7);} void UMatchActionWidget::Pause(){Dispatch(8);}
void UMatchActionWidget::Close(){Dispatch(9);}
