// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * In-match scoreboard — M11d-3 broadcast layout.
 *
 * Top-centre mirrored scoreboard: TEAM A (blue) | SET + sets won | TEAM B
 * (orange), with the serving team flagged by a gold dot. Below it an
 * independent stage badge (准备发球 / 裁判检查 / 允许发球 / 回合进行 /
 * 局结束 / 比赛结束), three touch-count dots, and a legal-serve hint when
 * applicable. Rally results appear as a short centre banner (1.0–1.5 s fade).
 * The controls-help strip is separated from the scoreboard and collapses
 * after a few seconds.
 *
 * All data still comes from GameMode via UpdateScore; the widget never keeps
 * its own copy of the score. SetText is dirty-gated as before.
 */
UCLASS()
class SPIKEELITE_API UScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UScoreboardWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void UpdateScore(int32 SetNum, int32 AScore, int32 BScore, int32 ASets, int32 BSets,
		bool bTeamAServing, const FString& BallHint,
		const FString& Phase, const FString& Possession, const FString& ServeHint,
		const FString& RallyResult);

protected:
	// Mirrored scoreboard
	UPROPERTY() TObjectPtr<UImage> Backdrop;
	UPROPERTY() TObjectPtr<UImage> BlockA;
	UPROPERTY() TObjectPtr<UImage> BlockB;
	UPROPERTY() TObjectPtr<UTextBlock> Text_TeamA;
	UPROPERTY() TObjectPtr<UTextBlock> Text_TeamB;
	UPROPERTY() TObjectPtr<UTextBlock> Text_ScoreA;
	UPROPERTY() TObjectPtr<UTextBlock> Text_ScoreB;
	UPROPERTY() TObjectPtr<UTextBlock> Text_Set;
	UPROPERTY() TObjectPtr<UTextBlock> Text_Sets;
	UPROPERTY() TObjectPtr<UImage> ServeDotA;
	UPROPERTY() TObjectPtr<UImage> ServeDotB;

	// Stage badge + touch dots + serve hint
	UPROPERTY() TObjectPtr<UImage> StageBadge;
	UPROPERTY() TObjectPtr<UTextBlock> Text_Stage;
	UPROPERTY() TObjectPtr<UImage> Dot0;
	UPROPERTY() TObjectPtr<UImage> Dot1;
	UPROPERTY() TObjectPtr<UImage> Dot2;
	UPROPERTY() TObjectPtr<UTextBlock> Text_ServeHint;

	// Rally-result centre banner
	UPROPERTY() TObjectPtr<UImage> Banner;
	UPROPERTY() TObjectPtr<UTextBlock> Text_Banner;

	// Controls help (separated from the scoreboard)
	UPROPERTY() TObjectPtr<UTextBlock> Text_Help;

	float HelpTimer = 5.0f;
	bool bHelpCollapsed = false;
	float BannerTimer = -1.f;
	float BannerFade = 0.f;

	FString LastSet, LastScoreA, LastScoreB, LastSets, LastStage, LastTouch, LastHint, LastRally, LastHelp;
	FString LastPhase = TEXT("NONE");
	FString LastServeSide = TEXT("NONE");
	int32 LastTouchCount = -1;

	void BuildWidgetTree();
	void RefreshTouchDots(int32 Count);
	void UpdateBanner(float InDeltaTime);
};
