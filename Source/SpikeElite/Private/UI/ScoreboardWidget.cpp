// SPDX-License-Identifier: MIT
#include "UI/ScoreboardWidget.h"
#include "UI/SEUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"

UScoreboardWidget::UScoreboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UScoreboardWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

static UTextBlock* MakeText(UWidgetTree* Tree, UCanvasPanel* Root, int32 FontSize,
	const FLinearColor& Color, const FAnchors& Anchors, const FVector2D& Pos, const FVector2D& Align)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetFont(SEUiStyle::Font(FontSize));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(T))
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		S->SetAutoSize(true);
	}
	return T;
}

static UImage* MakeImage(UWidgetTree* Tree, UCanvasPanel* Root, const FLinearColor& Color,
	const FAnchors& Anchors, const FVector2D& Pos, const FVector2D& Size, const FVector2D& Align)
{
	UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Img->SetBrush(SEUiStyle::SolidBrush(Color));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Img))
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		S->SetSize(Size);
	}
	return Img;
}

void UScoreboardWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// --- mirrored scoreboard (top-centre) ---------------------------------------
	// Keep a real centre gutter between the two team cards.  The first M11d
	// draft placed 176 px cards only 68 px apart; after Unreal's 720p DPI scale
	// the SET and set-tally labels collided with both scores.  This 620 px board
	// leaves a 160 px information column and remains compact at 1280x720.
	Backdrop = MakeImage(WidgetTree, Root, FLinearColor(0.02f, 0.03f, 0.05f, 0.60f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.f, 6.f), FVector2D(620.f, 88.f), FVector2D(0.5f, 0.f));

	// TEAM A block (electric blue)
	BlockA = MakeImage(WidgetTree, Root, FLinearColor(SEUiStyle::Colors::TeamA.R, SEUiStyle::Colors::TeamA.G, SEUiStyle::Colors::TeamA.B, 0.92f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(-300.f, 12.f), FVector2D(220.f, 76.f), FVector2D(0.f, 0.f));
	Text_TeamA = MakeText(WidgetTree, Root, 15, SEUiStyle::Colors::White,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(-272.f, 20.f), FVector2D(0.f, 0.f));
	Text_TeamA->SetText(FText::FromString(TEXT("TEAM A")));
	Text_ScoreA = MakeText(WidgetTree, Root, 40, SEUiStyle::Colors::White,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(-96.f, 20.f), FVector2D(1.f, 0.f));

	// TEAM B block (warm orange-red)
	BlockB = MakeImage(WidgetTree, Root, FLinearColor(SEUiStyle::Colors::TeamB.R, SEUiStyle::Colors::TeamB.G, SEUiStyle::Colors::TeamB.B, 0.92f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(80.f, 12.f), FVector2D(220.f, 76.f), FVector2D(0.f, 0.f));
	Text_TeamB = MakeText(WidgetTree, Root, 15, SEUiStyle::Colors::White,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(108.f, 20.f), FVector2D(0.f, 0.f));
	Text_TeamB->SetText(FText::FromString(TEXT("TEAM B")));
	Text_ScoreB = MakeText(WidgetTree, Root, 40, SEUiStyle::Colors::White,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(284.f, 20.f), FVector2D(1.f, 0.f));

	// Centre column: SET + sets won
	Text_Set = MakeText(WidgetTree, Root, 17, SEUiStyle::Colors::Gold,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.f, 17.f), FVector2D(0.5f, 0.f));
	Text_Sets = MakeText(WidgetTree, Root, 12, FLinearColor(0.72f, 0.84f, 0.96f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.f, 48.f), FVector2D(0.5f, 0.f));

	// Serving-team gold dots (over the team name corner)
	ServeDotA = MakeImage(WidgetTree, Root, SEUiStyle::Colors::Gold,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(-290.f, 23.f), FVector2D(9.f, 9.f), FVector2D(0.f, 0.f));
	ServeDotB = MakeImage(WidgetTree, Root, SEUiStyle::Colors::Gold,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(90.f, 23.f), FVector2D(9.f, 9.f), FVector2D(0.f, 0.f));

	// --- stage badge + touch dots + serve hint (below the board) ----------------
	StageBadge = MakeImage(WidgetTree, Root, FLinearColor(0.10f, 0.16f, 0.24f, 0.85f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.f, 102.f), FVector2D(210.f, 30.f), FVector2D(0.5f, 0.f));
	Text_Stage = MakeText(WidgetTree, Root, 15, SEUiStyle::Colors::White,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.f, 117.f), FVector2D(0.5f, 0.f));

	// Three touch dots (lit = used)
	Dot0 = MakeImage(WidgetTree, Root, FLinearColor(0.25f, 0.25f, 0.28f, 1.f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(118.f, 111.f), FVector2D(9.f, 9.f), FVector2D(0.f, 0.f));
	Dot1 = MakeImage(WidgetTree, Root, FLinearColor(0.25f, 0.25f, 0.28f, 1.f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(132.f, 111.f), FVector2D(9.f, 9.f), FVector2D(0.f, 0.f));
	Dot2 = MakeImage(WidgetTree, Root, FLinearColor(0.25f, 0.25f, 0.28f, 1.f),
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(146.f, 111.f), FVector2D(9.f, 9.f), FVector2D(0.f, 0.f));

	Text_ServeHint = MakeText(WidgetTree, Root, 14, SEUiStyle::Colors::Gold,
		FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.f, 139.f), FVector2D(0.5f, 0.f));

	// --- rally-result centre banner ----------------------------------------------
	Banner = MakeImage(WidgetTree, Root, FLinearColor(0.02f, 0.03f, 0.05f, 0.78f),
		FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.f, -70.f), FVector2D(460.f, 54.f), FVector2D(0.5f, 0.5f));
	Banner->SetRenderOpacity(0.f);
	Text_Banner = MakeText(WidgetTree, Root, 26, SEUiStyle::Colors::White,
		FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.f, -70.f), FVector2D(0.5f, 0.5f));
	Text_Banner->SetRenderOpacity(0.f);

	// --- controls help strip (bottom-centre, collapses after a few seconds) ------
	Text_Help = MakeText(WidgetTree, Root, SEUiStyle::Type::Tiny(), FLinearColor(0.75f, 0.78f, 0.82f),
		FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.f, -34.f), FVector2D(0.5f, 0.f));
	Text_Help->SetText(FText::FromString(TEXT("WASD移动 · 空格跳 · 左键击球 · 右键抬手 · E发球 · C切视角 · Esc暂停")));
	LastHelp = TEXT("WASD移动 · 空格跳 · 左键击球 · 右键抬手 · E发球 · C切视角 · Esc暂停");
	HelpTimer = 5.0f;
	bHelpCollapsed = false;
	BannerTimer = -1.f;
	BannerFade = 0.f;
	LastTouchCount = -1;

	RefreshTouchDots(0);
}

void UScoreboardWidget::RefreshTouchDots(int32 Count)
{
	for (int32 i = 0; i < 3; ++i)
	{
		UImage* D = (i == 0) ? Dot0 : (i == 1 ? Dot1 : Dot2);
		if (!D) { continue; }
		const bool bLit = (i < Count);
		D->SetBrush(SEUiStyle::SolidBrush(bLit
			? SEUiStyle::Colors::Gold
			: FLinearColor(0.22f, 0.22f, 0.25f, 1.f)));
	}
	LastTouchCount = Count;
}

void UScoreboardWidget::UpdateScore(int32 SetNum, int32 AScore, int32 BScore, int32 ASets, int32 BSets,
	bool bTeamAServing, const FString& BallHint,
	const FString& Phase, const FString& Possession, const FString& ServeHint,
	const FString& RallyResult)
{
	(void)BallHint; // dynamic debug hint no longer part of the broadcast HUD

	const FString Set   = FString::Printf(TEXT("SET %d"), SetNum);
	const FString ScoreA = FString::Printf(TEXT("%d"), AScore);
	const FString ScoreB = FString::Printf(TEXT("%d"), BScore);
	const FString Sets  = FString::Printf(TEXT("局  A %d - %d B"), ASets, BSets);
	const FString Stage = Phase;

	if (Text_Set && LastSet != Set) { Text_Set->SetText(FText::FromString(Set)); LastSet = Set; }
	if (Text_ScoreA && LastScoreA != ScoreA) { Text_ScoreA->SetText(FText::FromString(ScoreA)); LastScoreA = ScoreA; }
	if (Text_ScoreB && LastScoreB != ScoreB) { Text_ScoreB->SetText(FText::FromString(ScoreB)); LastScoreB = ScoreB; }
	if (Text_Sets && LastSets != Sets) { Text_Sets->SetText(FText::FromString(Sets)); LastSets = Sets; }

	// Stage badge colour by phase (gold = service ready, green = rally, red = over)
	if (Text_Stage && LastStage != Stage)
	{
		Text_Stage->SetText(FText::FromString(Stage));
		LastStage = Stage;
		if (Stage.Contains(TEXT("允许发球")) || Stage.Contains(TEXT("发球")))
		{
			StageBadge->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.55f, 0.45f, 0.08f, 0.90f)));
		}
		else if (Stage.Contains(TEXT("回合")))
		{
			StageBadge->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.10f, 0.40f, 0.18f, 0.90f)));
		}
		else if (Stage.Contains(TEXT("局结束")) || Stage.Contains(TEXT("比赛结束")))
		{
			StageBadge->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.45f, 0.12f, 0.10f, 0.90f)));
		}
		else
		{
			StageBadge->SetBrush(SEUiStyle::SolidBrush(FLinearColor(0.10f, 0.16f, 0.24f, 0.85f)));
		}
	}

	// Serving team: gold dot next to the serving team's name.
	const FString ServeSide = bTeamAServing ? TEXT("A") : TEXT("B");
	if (LastServeSide != ServeSide)
	{
		LastServeSide = ServeSide;
		if (ServeDotA) { ServeDotA->SetBrush(SEUiStyle::SolidBrush(bTeamAServing ? SEUiStyle::Colors::Gold : FLinearColor(0.15f, 0.15f, 0.18f, 0.6f))); }
		if (ServeDotB) { ServeDotB->SetBrush(SEUiStyle::SolidBrush(!bTeamAServing ? SEUiStyle::Colors::Gold : FLinearColor(0.15f, 0.15f, 0.18f, 0.6f))); }
	}

	// Touch dots parsed from the possession string ("B 2/3" -> 2, "-0/3" -> 0).
	int32 TouchCount = 0;
	{
		const int32 Slash = Possession.Find(TEXT("/"));
		if (Slash > 0)
		{
			const FString Num = Possession.Left(Slash);
			const int32 LastSpace = Num.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			const FString TouchStr = (LastSpace >= 0) ? Num.Right(Num.Len() - LastSpace - 1) : Num;
			TouchCount = FCString::Atoi(*TouchStr);
		}
	}
	if (LastTouchCount != TouchCount) { RefreshTouchDots(TouchCount); }

	if (Text_ServeHint && LastHint != ServeHint)
	{
		Text_ServeHint->SetText(ServeHint.IsEmpty() ? FText::GetEmpty() : FText::FromString(ServeHint));
		Text_ServeHint->SetRenderOpacity(ServeHint.IsEmpty() ? 0.f : 1.f);
		LastHint = ServeHint;
	}

	// Rally-result centre banner (shown ~1.2 s then fades out).
	if (LastRally != RallyResult)
	{
		LastRally = RallyResult;
		if (!RallyResult.IsEmpty())
		{
			if (Text_Banner)
			{
				Text_Banner->SetText(FText::FromString(RallyResult));
				BannerTimer = SEUiStyle::Anim::BannerHold();
				BannerFade = 0.f;
			}
		}
	}
}

void UScoreboardWidget::UpdateBanner(float InDeltaTime)
{
	if (BannerTimer < 0.f) { return; }
	BannerTimer -= InDeltaTime;
	const float Total = SEUiStyle::Anim::BannerHold();
	const float FadeIn = SEUiStyle::Anim::BannerOn();
	float Op = 1.f;
	if (Total - BannerTimer < FadeIn)
	{
		Op = FMath::Clamp((Total - BannerTimer) / FadeIn, 0.f, 1.f);
	}
	else if (BannerTimer < FadeIn)
	{
		Op = FMath::Clamp(BannerTimer / FadeIn, 0.f, 1.f);
	}
	Banner->SetRenderOpacity(Op);
	if (Text_Banner) { Text_Banner->SetRenderOpacity(Op); }
	if (BannerTimer <= 0.f)
	{
		BannerTimer = -1.f;
		Banner->SetRenderOpacity(0.f);
		if (Text_Banner) { Text_Banner->SetRenderOpacity(0.f); }
	}
}

void UScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHelpCollapsed)
	{
		HelpTimer -= InDeltaTime;
		if (HelpTimer <= 0.0f)
		{
			bHelpCollapsed = true;
			const FString Collapsed = TEXT("ESC：暂停 / 释放鼠标");
			if (Text_Help && LastHelp != Collapsed)
			{
				Text_Help->SetText(FText::FromString(Collapsed));
				LastHelp = Collapsed;
			}
		}
	}

	UpdateBanner(InDeltaTime);
}
