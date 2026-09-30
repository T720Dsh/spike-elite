// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * In-match scoreboard. Pure C++ UMG — no editor assets.
 *
 * M10: shows the match phase, the current possessing team + touch count
 * ("A 2/3"), a legal serve hint ("按 E 发球") and a rally-result banner
 * (IN/OUT/four touches/double touch/score) that persists ~1.5 s.
 */
UCLASS()
class SPIKEELITE_API UScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UScoreboardWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

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

	void BuildWidgetTree();
};
