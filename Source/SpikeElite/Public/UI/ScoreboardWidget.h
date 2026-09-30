// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * In-match scoreboard. Pure C++ UMG — no editor assets.
 *
 * M10: shows the match phase, the current possessing team + touch count
 * ("A 2/3"), a legal serve hint ("按 E 发球") and a rally-result banner
 * (IN/OUT/four touches/double touch/score) that persists ~1.5 s.
 *
 * M11a: semi-transparent backdrop for readability over the bright arena;
 * full controls help line that auto-collapses after a few seconds; every
 * SetText is guarded so identical frames never rewrite widget text.
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
	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Set;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Sets;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Ball;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Help;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Phase;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Possession;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_RallyResult;

	UPROPERTY()
	TObjectPtr<UImage> Backdrop;

	/** M11a: seconds until the full controls help collapses to the short line. */
	float HelpTimer = 5.0f;
	bool bHelpCollapsed = false;

	// M11a: last pushed strings — skip SetText when nothing actually changed.
	FString LastSet, LastScore, LastSets, LastPhase, LastPossession, LastBall, LastRallyResult, LastHelp;

	void BuildWidgetTree();
};
