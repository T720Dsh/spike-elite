// SPDX-License-Identifier: MIT
#include "SpikeElitePlayerController.h"
#include "SpikeEliteCharacter.h"
#include "UI/TacticalContactComponent.h"
#include "UI/TeamRosterWidget.h"
#include "Volleyball/VolleyballBall.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

#if !UE_BUILD_SHIPPING
void ASpikeElitePlayerController::DevCloseoutTest()
{
	bShouldPerformFullTickWhenPaused=true; PrimaryActorTick.bTickEvenWhenPaused=true;
	struct FEvent { float At; TFunction<void()> Action; };
	TArray<FEvent> Events;
	auto At=[&](float Seconds,TFunction<void()> Action) { Events.Add({Seconds,MoveTemp(Action)}); };
	auto Mode=[this] { return GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>(); };
	At(.5f,[this] { ShowModeSelect(); });
	At(1.f,[this] { DevShot(TEXT("close_01_modes")); });
	At(1.5f,[this] { StartMatchAs(EGameModeChoice::QuickMatch,true); });
	At(2.f,[this] { DevVerify(RosterMenu && IsMenuOpen(),TEXT("prematch roster UI")); DevShot(TEXT("close_02_lineup")); });
	At(2.5f,[this,Mode] { StartPreparedMatch(); FString Problems; DevVerify(Mode()->DevManagementChecks(Problems),TEXT("management runtime fixtures: ")+Problems); Mode()->BeginEntrance(); });
	At(4.f,[this,Mode] { DevVerify(Mode()->MatchState==EMatchState::Entrance,TEXT("entrance is locked phase")); DevShot(TEXT("close_03_entrance")); });
	At(5.f,[this,Mode] { Mode()->FinishEntrance(); FString Why; DevVerify(Mode()->ValidateRuntimeRoster(Why),TEXT("entrance finish identities")); Mode()->MatchState=EMatchState::AwaitingReady; Mode()->RequestTeamTimeout(EVolleyballTeam::TeamA); });
	At(6.f,[this] { DevShot(TEXT("close_04_timeout")); });
	At(6.3f,[this] { PauseGame(); });
	At(6.6f,[this] { DevShot(TEXT("close_04_pause_freeze")); });
	At(7.f,[this,Mode] { DevVerify(Mode()->TimeoutTimer>28.f,TEXT("Esc pause freezes timeout clock")); ResumeGame(); Mode()->MatchState=EMatchState::Timeout; ShowRoster(false); });
	At(7.5f,[this] { DevVerify(RosterMenu && GetWorld()->IsPaused(),TEXT("live substitution preview freezes safely")); DevShot(TEXT("close_05_substitution")); });
	At(8.f,[this,Mode] { CloseRoster(); DevVerify(!GetWorld()->IsPaused(),TEXT("closing substitution resumes world")); if(CoachPanel) ToggleCoachPanel(); Mode()->CancelTeamTimeout(); Mode()->MatchState=EMatchState::AwaitingReady; Mode()->EnterServePresentation(); });
	At(8.8f,[this] { DevShot(TEXT("close_06_serve_card")); });
	At(9.5f,[this] { ReturnToMainMenu(); StartMatchAs(EGameModeChoice::Coach,true); StartPreparedMatch(); });
	At(10.f,[this,Mode] {
		bool Bots=true; for(const auto& Team:{Mode()->GetTeamPlayers(EVolleyballTeam::TeamA),Mode()->GetTeamPlayers(EVolleyballTeam::TeamB)}) for(const auto& C:Team) Bots&=C && C->bIsBot;
		DevVerify(Bots && bShowMouseCursor,TEXT("coach has 12 AI players and free cursor")); DevShot(TEXT("close_07_coach")); });
	At(11.f,[this] { ReturnToMainMenu(); StartMatchAs(EGameModeChoice::Training,false,(int32)ETrainingDrill::ReceiveTarget); if(Tactical) Tactical->TacticalMode=0; });
	At(11.7f,[this,Mode] { DevVerify(Mode()->TrainingAttempts==1 && Mode()->GetBall()->GetVelocity().Size()>0,TEXT("receive drill launches real ballistic feed")); DevShot(TEXT("close_08_receive")); });
	At(12.3f,[this,Mode] {
		FShotIntent Shot; Shot.TouchType=EBallTouchType::Receive; Shot.TargetLocation=Mode()->TrainingGoal+FVector(0,0,8.5f); Shot.DesiredFlightTime=.7f;
		DevVerify(Mode()->ExecuteTacticalShot(Cast<ASpikeEliteCharacter>(GetPawn()),Shot),TEXT("receive drill accepts physical contact without teleport")); });
	At(13.5f,[this,Mode] { DevVerify(Mode()->TrainingSuccesses==1 && Mode()->bTrainingWaiting,TEXT("real received flight lands in training goal")); Mode()->StartTrainingAttempt(); Mode()->OnBallLanded(Mode()->TrainingGoal); DevVerify(Mode()->TrainingSuccesses==1,TEXT("untouched feed cannot score success even in target")); });
	At(14.f,[this] { ReturnToMainMenu(); StartMatchAs(EGameModeChoice::Training,false,(int32)ETrainingDrill::SetAttack); if(Tactical) Tactical->TacticalMode=0; });
	At(14.6f,[this,Mode] { DevVerify(Mode()->GetTouchCount()==1,TEXT("set drill starts after receive feed")); DevShot(TEXT("close_09_set_attack")); });
	At(15.5f,[this,Mode] { DevVerify(!Mode()->CanRequestTimeout(),TEXT("training excludes formal timeouts")); ReturnToMainMenu(); StartMatchAs(EGameModeChoice::Training,false,(int32)ETrainingDrill::ServePlacement); if(Tactical) Tactical->TacticalMode=0; });
	At(17.f,[this,Mode] { if(Mode()->MatchState==EMatchState::ServePresentation) Mode()->SkipServePresentation(); });
	At(17.1f,[this] { DevShot(TEXT("close_10_serve_drill")); });
	At(17.4f,[this,Mode] { if(Mode()->MatchState==EMatchState::ServiceAuthorized) Mode()->RequestServe(Cast<ASpikeEliteCharacter>(GetPawn())); });
	At(18.5f,[this,Mode] { DevVerify(Mode()->TeamAScore==0 && Mode()->TeamBScore==0 && Mode()->StatsA[0].ServeAttempts==0,TEXT("training isolated from scores and career stats")); ReturnToMainMenu(); });
	At(19.f,[this] { UE_LOG(LogTemp,Log,TEXT("CLOSEOUT TEST: %s failures=%d (deterministic fixtures, not manual full match)"),DevVerifyFailures==0?TEXT("PASS"):TEXT("FAIL"),DevVerifyFailures); ConsoleCommand(TEXT("quit")); });
	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this); const double Begin=FPlatformTime::Seconds();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Begin,Events=MoveTemp(Events),Index=0](float) mutable
	{
		if(!Weak.IsValid()) return false;
		const double T=FPlatformTime::Seconds()-Begin;
		while(Index<Events.Num() && T>=Events[Index].At) { Events[Index].Action(); ++Index; }
		return Index<Events.Num();
	}));
}

void ASpikeElitePlayerController::DevPerfSuite()
{
	// Frame wall time only: this must never be reported as isolated GPU timing.
	StartMatchAs(EGameModeChoice::Coach,false); StartPreparedMatch();
	if(CoachPanel) ToggleCoachPanel();
	ConsoleCommand(TEXT("r.VSync 0")); ConsoleCommand(TEXT("t.MaxFPS 0"));
	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this); const double Begin=FPlatformTime::Seconds();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Begin,Frames=TArray<float>()](float) mutable
	{
		if(!Weak.IsValid()) return false;
		const double Elapsed=FPlatformTime::Seconds()-Begin;
		if(Elapsed>=10.0 && Elapsed<55.0) Frames.Add(FApp::GetDeltaTime()*1000.f);
		if(Elapsed<55.0) return true;
		Frames.Sort();
		if(Frames.Num()>100)
		{
			const float Median=Frames[Frames.Num()/2],P95=Frames[FMath::Min(Frames.Num()-1,FMath::FloorToInt(Frames.Num()*.95f))];
			UE_LOG(LogTemp,Log,TEXT("PERF SUITE: PASS samples=%d warmup=10s sample=45s frame-wall-median=%.3fms frame-wall-p95=%.3fms vsync=0 maxfps=0 (not isolated GPU time)"),Frames.Num(),Median,P95);
		}
		else UE_LOG(LogTemp,Error,TEXT("PERF SUITE: FAIL insufficient frames=%d"),Frames.Num());
		if(auto* GM=Weak->GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>()) GM->DevAuditActors(1);
		Weak->ConsoleCommand(TEXT("quit")); return false;
	}));
}
#endif
