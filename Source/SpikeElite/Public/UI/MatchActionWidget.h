#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatchActionWidget.generated.h"

/** Live, non-pausing controls. The drag surface consumes mouse input so UI clicks never hit the ball. */
UCLASS()
class SPIKEELITE_API UMatchActionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
	void Refresh(bool bServing, float Arc);
private:
	UPROPERTY() TObjectPtr<class UTextBlock> Hint;
	UPROPERTY() TArray<TObjectPtr<class UButton>> Actions;
	bool bDragging=false;
	FVector2D LastDrag;
	UFUNCTION() void Serve();
	UFUNCTION() void Receive();
	UFUNCTION() void Set();
	UFUNCTION() void Attack();
	UFUNCTION() void Block();
	UFUNCTION() void Jump();
	UFUNCTION() void Switch();
	UFUNCTION() void Coach();
	UFUNCTION() void Pause();
	UFUNCTION() void Close();
	void Dispatch(int32 Action);
};
