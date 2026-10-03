#include "SpikeElitePlayerController.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballBall.h"
#include "Volleyball/VolleyballTrajectory.h"
#include "UI/MatchActionWidget.h"
#include "UI/TrajectoryPreviewComponent.h"
#include "UI/PauseMenuWidget.h"
#include "UI/MatchEndWidget.h"
#include "UI/TacticalContactComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"

#if !UE_BUILD_SHIPPING
void ASpikeEliteGameMode::DevPrepareHumanServe()
{
	bDevAuto=false; bFastFlow=false; ServingTeam=EVolleyballTeam::TeamA;
	MatchState=EMatchState::AwaitingReady;
	if(TeamAPlayers.Num()>0)
	{
		TeamAPlayers[0]->bIsBot=false;
		if(auto* PC=Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this,0))) PC->Possess(TeamAPlayers[0]);
	}
	BeginServiceAuthorized();
}
void ASpikeElitePlayerController::DevPlayabilityTest()
{
	bShouldPerformFullTickWhenPaused=true; PrimaryActorTick.bTickEvenWhenPaused=true;
	struct FEvent { float At; TFunction<void()> Action; }; TArray<FEvent> Events;
	auto At=[&](float T,TFunction<void()> F){ Events.Add({T,MoveTemp(F)}); };
	auto Mode=[this]{return GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>();};
	auto Click=[this](const FString& Label,UUserWidget* W)
	{
		bool Found=false;
		if(W && W->WidgetTree) W->WidgetTree->ForEachWidget([&](UWidget* Item){ if(auto* B=Cast<UButton>(Item)) if(auto* T=Cast<UTextBlock>(B->GetChildAt(0)); T && T->GetText().ToString()==Label) { Found=true; B->OnClicked.Broadcast(); } });
		DevVerify(Found,TEXT("production UMG button delegate: ")+Label);
	};
	At(.5f,[this]{StartMatchAs(EGameModeChoice::QuickMatch,false); StartPreparedMatch(); if(Tactical) Tactical->TacticalMode=0;});
	At(1.f,[this,Mode]{Mode()->DevPrepareHumanServe(); RefreshMatchInteraction(); DevView(FVector(1850,-1100,900),FRotator(-25,145,0));});
	At(1.5f,[this]{DevVerify(bServeAiming && MatchActions && bShowMouseCursor && !GetWorld()->IsPaused(),TEXT("normal match automatic serve aim + live cursor")); DevVerify(ServePreview && ServePreview->GetVisibleDotCount()>25,TEXT("visible pooled 3D white preview")); DevVerify(ServePreview && ServePreview->GetRenderedDotCount()>5,TEXT("preview actually rendered, not just visible flag")); DevShot(TEXT("play_01_serve_aim"));});
	At(2.f,[this,Mode]
	{
		const FVector Start=Mode()->GetBall()->GetActorLocation(),Before=GetServeAimVelocity(Start,1);
		FVector2D Pixel; ProjectWorldLocationToScreen(FVector(-650,160,0),Pixel);
		const FVector2D Desktop=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(Pixel/UWidgetLayoutLibrary::GetViewportScale(this));
		FVector2D Roundtrip,Viewport; USlateBlueprintLibrary::AbsoluteToViewport(this,Desktop,Roundtrip,Viewport);
		DevVerify(FVector2D::Distance(Pixel,Roundtrip)<1.5f,FString::Printf(TEXT("DPI coordinate roundtrip error %.3f pixels"),FVector2D::Distance(Pixel,Roundtrip)));
		const FVector2D End=Desktop+FVector2D(80,-100)*MatchActions->GetCachedGeometry().Scale;
		const TSet<FKey> DownKeys={EKeys::LeftMouseButton};
		const FPointerEvent Down(0,Desktop,Desktop,DownKeys,EKeys::LeftMouseButton,0,FModifierKeysState());
		const FPointerEvent Move(0,End,Desktop,DownKeys,EKeys::Invalid,0,FModifierKeysState());
		const FPointerEvent Up(0,End,End,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
		DevVerify(MatchActions->NativeOnMouseButtonDown(MatchActions->GetCachedGeometry(),Down).IsEventHandled(),TEXT("serve drag consumes pointer-down"));
		DevVerify(FVector::Dist2D(ServeTarget,FVector(-650,160,0))<20,FString::Printf(TEXT("court pick within one ball diameter / pixel rounding (selected %s)"),*ServeTarget.ToString()));
		const FVector PickedTarget=ServeTarget;
		MatchActions->NativeOnMouseMove(MatchActions->GetCachedGeometry(),Move);
		DevVerify(ServeTarget.Equals(PickedTarget,.01f),TEXT("arc drag preserves the chosen destination even with horizontal pointer drift"));
		DevVerify(MatchActions->NativeOnMouseButtonUp(MatchActions->GetCachedGeometry(),Up).IsEventHandled(),TEXT("drag releases capture on pointer-up"));
		const FVector After=GetServeAimVelocity(Start,1);
		DevVerify(After.Z>Before.Z && ServeTarget.Y>0,TEXT("mouse drag modifies arc and lateral placement")); DevShot(TEXT("play_02_dragged_arc"));
	});
	At(2.5f,[this]{ToggleMatchMouse(); DevVerify(!MatchActions && !bShowMouseCursor,TEXT("Alt hides cursor while keeping serve preview")); ToggleMatchMouse(); DevVerify(MatchActions && bShowMouseCursor,TEXT("Alt restores live clickable panel"));});
	At(3.f,[this,Click]{Click(TEXT("发球 / 跳过"),MatchActions); DevVerify(GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>()->MatchState==EMatchState::ServingToss,TEXT("mouse serve uses production request"));});
	At(3.7f,[this,Mode]{DevVerify(Mode()->GetBall()->GetVelocity().Size()>300 && Mode()->StatsA[0].ServeAttempts==1,TEXT("physical serve + exactly one stat event")); DevVerify(!bServeAiming,TEXT("serve preview stops after toss")); PauseGame();});
	At(4.2f,[this,Click]{Click(TEXT("操作说明"),PauseMenu); DevShot(TEXT("play_03_wrapped_help"));});
	At(5.f,[this]{ResumeGame(); RefreshMatchInteraction(); DevVerify(bShowMouseCursor && MatchActions,TEXT("resume preserves Alt cursor intent")); ToggleMatchMouse(); DevVerify(!bShowMouseCursor,TEXT("resume can recapture mouse"));});
	At(5.2f,[this]{ToggleMatchMouse(); Tactical->DevForcePlanning(EBallTouchType::Receive); RefreshMatchInteraction(); DevVerify(!MatchActions,TEXT("tactical UI has exclusive pointer ownership")); Tactical->CancelShot(); RefreshMatchInteraction(); DevVerify(MatchActions && bShowMouseCursor && !GetWorld()->IsPaused(),TEXT("tactical cancel restores Alt panel and normal time")); ToggleMatchMouse();});
	At(5.5f,[this,Mode]{Mode()->DevPrepareHumanServe(); ServeTarget.X=-710; RefreshMatchInteraction(); PauseGame(); RefreshMatchInteraction(); DevVerify(!MatchActions && bShowMouseCursor && GetWorld()->IsPaused(),TEXT("pause owns input over serve UI"));});
	At(6.f,[this,Mode]{ResumeGame(); RefreshMatchInteraction(); DevVerify(FMath::IsNearlyEqual(ServeTarget.X,-710.f),TEXT("pause/resume preserves chosen serve depth")); Mode()->MatchState=EMatchState::PreMatch; Mode()->GetTeamPlayers(EVolleyballTeam::TeamA)[0]->SetActorLocation(FVector(600,0,100)); SetPause(true); DevView(FVector(1050,-450,260),FRotator(-5,135,0));});
	for(int32 I=0;I<3;++I)
	{
		At(6.5f+I*1.2f,[this,Mode,I]{SetPause(false); auto* C=Mode()->GetTeamPlayers(EVolleyballTeam::TeamA)[0].Get(); C->NotifyContact(I==0?EBallTouchType::Receive:I==1?EBallTouchType::Set:EBallTouchType::Attack);});
		At(6.65f+I*1.2f,[this,I]{DevShot(FString::Printf(TEXT("play_04_contact_%d"),I));});
		At(6.95f+I*1.2f,[this,I]{DevShot(FString::Printf(TEXT("play_05_follow_%d"),I));});
	}
	At(9.8f,[this,Mode]{Mode()->DevPrepareHumanServe(); RefreshMatchInteraction();});
	At(10.2f,[this]{DevShot(TEXT("play_05b_player_serve_view"));});
	At(10.5f,[this,Mode]{SetPause(false); for(auto* Stats:{&Mode()->StatsA,&Mode()->StatsB}) for(auto& S:*Stats) { S.Receives=99; S.Sets=88; S.Attacks=77; S.Blocks=66; S.ServeAttempts=55; } OnMatchOver({26,24,26,24,16},{24,26,24,26,14},EVolleyballTeam::TeamA);});
	At(11.2f,[this]{DevShot(TEXT("play_06_dense_statistics")); DevVerify(MatchEnd && bShowMouseCursor,TEXT("24-player statistics screen"));});
	At(11.6f,[this]
	{
		bool Found=false;
		MatchEnd->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			if(auto* Scroll=Cast<UScrollBox>(W)) { Found=true; Scroll->ScrollToEnd(); }
			if(auto* B=Cast<UButton>(W))
			{
				const FGeometry G=B->GetCachedGeometry(),Root=MatchEnd->GetCachedGeometry();
				const FVector2D A=Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)),Z=Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));
				DevVerify(A.X>=0 && A.Y>=0 && Z.X<=Root.GetLocalSize().X && Z.Y<=Root.GetLocalSize().Y,TEXT("statistics footer button stays inside viewport"));
			}
		});
		DevVerify(Found,TEXT("full statistics scrollable to final roster rows"));
	});
	At(12.2f,[this]{DevShot(TEXT("play_07_statistics_bottom"));});
	At(13.f,[this]{UE_LOG(LogTemp,Log,TEXT("PLAYABILITY TEST: %s failures=%d (synthetic input/production handlers, not human playtest)"),DevVerifyFailures==0?TEXT("PASS"):TEXT("FAIL"),DevVerifyFailures); ConsoleCommand(TEXT("quit"));});
	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	// Cold shader/font/screenshot stalls must not execute several actions in one
	// frame (which would verify rendering before any draw, or pause before a toss
	// can advance). Bound test-clock progress and run at most one action per frame.
	// The outer verifier still has a real wall-clock timeout; no failure is retried.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Events=MoveTemp(Events),Index=0,Clock=0.f](float Dt) mutable
	{
		if(!Weak.IsValid()) return false;
		Clock+=FMath::Clamp(Dt,0.f,.05f);
		if(Index<Events.Num() && Clock>=Events[Index].At) { Events[Index].Action(); ++Index; }
		return Index<Events.Num();
	}));
}
#endif
