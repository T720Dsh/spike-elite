// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * In-match scoreboard. Pure C++ UMG — no editor assets.
 */
UCLASS()
class SPIKEELITE_API UScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UScoreboardWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	void UpdateScore(int32 SetNum, int32 AScore, int32 BScore, int32 ASets, int32 BSets,
		bool bTeamAServing, const FString& BallHint);

protected:
	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Set;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Sets;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Ball;
};
