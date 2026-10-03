// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Volleyball/VolleyballIdentity.h"
#include "TeamRosterWidget.generated.h"
class UVerticalBox;
class UTextBlock;
class UButton;
class ASpikeEliteGameMode;

/** Transactional lineup/substitution UI. Choices are previews until Confirm. */
UCLASS()
class SPIKEELITE_API UTeamRosterWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	void Configure(ASpikeEliteGameMode* Mode, bool bEditor);
	DECLARE_DELEGATE(FAction);
	FAction OnDone, OnBack;
private:
	UPROPERTY() TObjectPtr<UVerticalBox> Rows;
	UPROPERTY() TObjectPtr<UTextBlock> Message;
	UPROPERTY() TObjectPtr<UTextBlock> ConfirmLabel;
	UPROPERTY() TObjectPtr<UButton> TeamSwitch;
	TWeakObjectPtr<ASpikeEliteGameMode> GM;
	FTeamRosterState DraftA, DraftB;
	EVolleyballTeam Team=EVolleyballTeam::TeamA;
	bool bEdit=false;
	int32 SelectedSlot=0, SelectedPlayer=INDEX_NONE;
	void Build();
	void Refresh();
	void ChooseSlot(int32 Index);
	void ChoosePlayer(int32 Index);
	UFUNCTION() void Confirm();
	UFUNCTION() void Back();
	UFUNCTION() void SwitchTeam();
};
