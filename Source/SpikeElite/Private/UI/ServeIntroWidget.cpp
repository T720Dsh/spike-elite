// SPDX-License-Identifier: MIT
#include "UI/ServeIntroWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"

UServeIntroWidget::UServeIntroWidget(const FObjectInitializer& OI):Super(OI) {}
TSharedRef<SWidget> UServeIntroWidget::RebuildWidget()
{
	if(!WidgetTree->RootWidget) BuildWidgetTree();
	return Super::RebuildWidget();
}
void UServeIntroWidget::BuildWidgetTree()
{
	auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
	auto* Card=WidgetTree->ConstructWidget<UCanvasPanel>();
	PanelSlot=Root->AddChildToCanvas(Card); PanelSlot->SetAnchors(FAnchors(.52f,.28f,.99f,.62f)); PanelSlot->SetOffsets(FMargin(0));
	PanelBG=WidgetTree->ConstructWidget<UImage>(); PanelBG->SetBrush(SEUiStyle::SolidBrush(FLinearColor(.03f,.06f,.12f,.90f)));
	auto* Background=Card->AddChildToCanvas(PanelBG); Background->SetAnchors(FAnchors(0,0,1,1)); Background->SetOffsets(FMargin(0));
	auto Text=[&](int32 Font,FLinearColor Color,FAnchors Bounds)
	{
		auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetFont(SEUiStyle::Font(Font)); T->SetColorAndOpacity(Color);
		T->SetAutoWrapText(true); auto* S=Card->AddChildToCanvas(T); S->SetAnchors(Bounds); S->SetOffsets(FMargin(12,0,12,0)); return T;
	};
	NumberText=Text(72,SEUiStyle::Colors::Gold,FAnchors(.02f,.12f,.26f,.75f));
	NameText=Text(34,SEUiStyle::Colors::White,FAnchors(.28f,.20f,.98f,.46f));
	TeamRoleText=Text(18,SEUiStyle::Colors::White,FAnchors(.28f,.51f,.98f,.78f));
	Text(13,SEUiStyle::Colors::Gold,FAnchors(.04f,.84f,.98f,.99f))->SetText(FText::FromString(TEXT("E 跳过介绍 · 哨响后再发球")));
	SetRenderOpacity(0);
}
void UServeIntroWidget::SetServer(int32 Number,const FString& Name,const FString& Team,const FString& Role,bool bShortBar)
{
	if(!NumberText || !NameText || !TeamRoleText) return;
	bShort=bShortBar; NumberText->SetText(FText::FromString(FString::Printf(TEXT("#%d"),Number)));
	NumberText->SetFont(SEUiStyle::Font(bShort?42:72)); NameText->SetFont(SEUiStyle::Font(bShort?26:34));
	NameText->SetText(FText::FromString(Name));
	TeamRoleText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s · 发球"),*Team,*Role)));
	if(PanelSlot) { PanelSlot->SetAnchors(bShort?FAnchors(.58f,.40f,.99f,.62f):FAnchors(.52f,.28f,.99f,.62f)); PanelSlot->SetOffsets(FMargin(0)); }
	Alpha=0; bFadingIn=true; bFadingOut=false; bHidden=false;
	SetRenderOpacity(0); SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UServeIntroWidget::FadeOut() { bFadingIn=false; bFadingOut=true; }
void UServeIntroWidget::NativeTick(const FGeometry& G,float Dt)
{
	Super::NativeTick(G,Dt);
	if(bHidden || UGameplayStatics::IsGamePaused(this)) return;
	const float Duration=SEUiStyle::IsReducedMotion()?.05f:(bFadingOut?.25f:.30f);
	Alpha=FMath::Clamp(Alpha+(bFadingIn?Dt:-Dt)/Duration,0.f,1.f);
	SetRenderOpacity(Alpha);
	SetRenderTranslation(FVector2D(SEUiStyle::IsReducedMotion()?0.f:(1.f-FMath::InterpEaseOut(0.f,1.f,Alpha,3.f))*G.GetLocalSize().X*.48f,0));
	if(bFadingOut && Alpha<=0) { bHidden=true; SetVisibility(ESlateVisibility::Collapsed); }
}
