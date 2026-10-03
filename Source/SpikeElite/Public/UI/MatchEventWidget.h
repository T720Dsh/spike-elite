// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatchEventWidget.generated.h"
class UTextBlock;
class UBorder;
/** Live entrance / timeout / practice information, never a rules authority. */
UCLASS()
class SPIKEELITE_API UMatchEventWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& G,float Dt) override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> Text;
	UPROPERTY() TObjectPtr<UBorder> Card;
	FString Last;
};
