// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "SpikeElitePlayerController.h"
#include "Volleyball/VolleyballBall.h"
#include "Volleyball/PracticeFlow.h"
#include "UI/TacticalContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SEMaterials.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEPractice,Log,All);

void ASpikeEliteGameMode::BeginEntrance()
{
	MatchState=EMatchState::Entrance; EntranceRemaining=12.f;
	Ball->ResetBall(FVector(0,0,400));
	TArray<TObjectPtr<ASpikeEliteCharacter>> People=TeamAPlayers; People.Append(TeamBPlayers); People.Append(BenchPlayers);
	int32 A=0,B=0;
	for(auto& C : People) if(C)
	{
		const int32 I=C->TeamSide>0 ? A++ : B++;
		C->bCeremonyWalking=true;
		C->SetActorLocation(FVector(C->TeamSide*1450.f,-1450.f+I*55.f,100));
		C->SetActorRotation(FRotator(0,90,0));
	}
	if(auto* PC=UGameplayStatics::GetPlayerController(this,0))
	{
		// Ceremony-only camera is destroyed when the introduction ends or is skipped.
		ACameraActor* Cam=GetWorld()->SpawnActor<ACameraActor>();
		Cam->Tags.Add(TEXT("EntranceCamera"));
		const FVector P(1600,-1450,780); Cam->SetActorLocationAndRotation(P,(FVector(0,0,160)-P).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(85); PC->SetViewTarget(Cam);
	}
	UE_LOG(LogSEPractice,Log,TEXT("Entrance START registered=24 duration=12s input=locked whistle=not-authorized"));
}

void ASpikeEliteGameMode::TickEntrance(float Dt)
{
	EntranceRemaining-=Dt;
	TArray<TObjectPtr<ASpikeEliteCharacter>> People=TeamAPlayers; People.Append(TeamBPlayers); People.Append(BenchPlayers);
	for(auto& C : People) if(C)
	{
		const FVector Target=C->HomePosition+FVector(0,0,100);
		const FVector Before=C->GetActorLocation(); const FVector Next=FMath::VInterpConstantTo(Before,Target,Dt,420.f);
		C->SetActorLocation(Next); if(FVector::Dist2D(Before,Next)>.1f) C->SetActorRotation((Next-Before).Rotation());
	}
	if(EntranceRemaining<=0) FinishEntrance();
}

void ASpikeEliteGameMode::FinishEntrance()
{
	if(MatchState!=EMatchState::Entrance) return;
	TArray<TObjectPtr<ASpikeEliteCharacter>> People=TeamAPlayers; People.Append(TeamBPlayers); People.Append(BenchPlayers);
	for(auto& C : People) if(C) C->bCeremonyWalking=false;
	RespawnPlayersToPositions(); RefreshRegisteredBench();
	if(auto* PC=Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this,0))) PC->OnMatchStarted(Scoreboard);
	TArray<AActor*> Old; UGameplayStatics::GetAllActorsWithTag(this,TEXT("EntranceCamera"),Old);
	for(auto* Camera : Old) Camera->Destroy();
	MatchState=EMatchState::BetweenRallies; InterRallyTimer=InterRallyDelay; EntranceRemaining=0;
	UE_LOG(LogSEPractice,Log,TEXT("Entrance COMPLETE lineup=unchanged scores=0:0 pending-serve=0"));
}

void ASpikeEliteGameMode::StartTrainingAttempt()
{
	if(MatchModeConfig.Mode!=EGameModeChoice::Training || !Ball) return;
	if(auto* PC=Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this,0)))
	{ if(PC->Tactical) { PC->Tactical->CancelShot(); PC->Tactical->TacticalMode=2; } PC->HideServeIntro(); }
	++TrainingAttempts; TrainingPlayerTouches=0; bTrainingWaiting=false; TrainingTimer=16.f;
	TrainingFeedback=TEXT("完成触球并把球送入金色目标区；R 重试，Esc 退出/暂停。");
	if(MatchModeConfig.Drill==ETrainingDrill::ServePlacement) TrainingFeedback=TEXT("鼠标左右调整落点、上下调整深度；白色虚线为预览，E 跳过介绍/发球，R 重试。");
	RespawnPlayersToPositions();
	for(auto& C : TeamAPlayers) if(C) { C->bTouchArmed=true; C->bIsPrimaryHandler=false; C->AIBehavior=EAIBehavior::Wait; }
	for(auto& C : TeamBPlayers) if(C) { C->bTouchArmed=true; C->bIsPrimaryHandler=false; C->AIBehavior=EAIBehavior::Wait; }
	const ETrainingDrill Drill=MatchModeConfig.Drill;
	TrainingGoal=Drill==ETrainingDrill::ReceiveTarget?FVector(220,0,2):FVector(-620,(TrainingAttempts%3-1)*180.f,2);
	if(!TrainingMarker)
	{
		TrainingMarker=GetWorld()->SpawnActor<AActor>();
		auto* Root=NewObject<USceneComponent>(TrainingMarker); TrainingMarker->SetRootComponent(Root); Root->RegisterComponent();
		UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
		TrainingPreview=NewObject<UInstancedStaticMeshComponent>(TrainingMarker); TrainingPreview->SetupAttachment(Root);
		TrainingPreview->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		TrainingPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision); TrainingPreview->SetCastShadow(false);
		TrainingPreview->SetMaterial(0,SEMaterials::MakeSurface(TrainingPreview,FLinearColor(.95f,.98f,1.f),.9f)); TrainingPreview->RegisterComponent();
		for(int32 I=0;I<4;++I)
		{
			auto* Bar=NewObject<UStaticMeshComponent>(TrainingMarker); Bar->SetupAttachment(Root); Bar->SetStaticMesh(Cube);
			Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision); Bar->SetCastShadow(false);
			Bar->SetRelativeLocation(I<2?FVector(0,I==0?-240:240,0):FVector(I==2?-240:240,0,0));
			Bar->SetRelativeScale3D(I<2?FVector(4.8,.05,.015):FVector(.05,4.8,.015));
			Bar->SetMaterial(0,SEMaterials::MakeSurface(Bar,FLinearColor(1.f,.75f,.05f),.85f)); Bar->RegisterComponent();
		}
	}
	TrainingMarker->SetActorLocation(TrainingGoal);
	if(Drill==ETrainingDrill::ServePlacement)
	{
		ServingTeam=EVolleyballTeam::TeamA; SEVolleyballRules::BeginRally(RallyState,ServingTeam);
		MatchState=EMatchState::BetweenRallies; InterRallyTimer=.5f;
	}
	else
	{
		const bool bReceive=Drill==ETrainingDrill::ReceiveTarget;
		TeamAPlayers[0]->SetActorLocation(FVector(600,0,100)); TeamAPlayers[0]->HomePosition=FVector(600,0,0);
		// Keep the fixed broadcast camera's sightline clear behind the trainee.
		for(int32 I=4;I<6;++I) if(TeamAPlayers[I])
		{ TeamAPlayers[I]->HomePosition=FVector(820,I==4?420:-420,0); TeamAPlayers[I]->SetActorLocation(TeamAPlayers[I]->HomePosition+FVector(0,0,100)); }
		const FVector Start=bReceive?FVector(-400,0,500):FVector(600,0,430);
		const FVector Velocity=SEPractice::FeedVelocity(Start,FVector(600,0,210),bReceive?1.5f:1.1f);
		Ball->ResetBall(Start); Ball->Strike(Velocity.GetSafeNormal(),Velocity.Size());
		ServingTeam=bReceive?EVolleyballTeam::TeamB:EVolleyballTeam::TeamA;
		SEVolleyballRules::BeginRally(RallyState,ServingTeam);
		if(bReceive) SEVolleyballRules::RecordServeTouch(RallyState,ServingTeam,0);
		else { RallyState.PossessingTeam=EVolleyballTeam::TeamA; RallyState.TouchCount=1; RallyState.LastTouchTeam=EVolleyballTeam::TeamA; RallyState.LastTouchPlayerIndex=1; RallyState.LastTouchType=EBallTouchType::Receive; }
		SEVolleyballRules::StartPlay(RallyState); MatchState=EMatchState::Rally;
		ResetFlightTracking();
	}
	UE_LOG(LogSEPractice,Log,TEXT("Training START drill=%d attempt=%d target=(%.0f,%.0f) stats=isolated"),(int32)Drill,TrainingAttempts,TrainingGoal.X,TrainingGoal.Y);
}

void ASpikeEliteGameMode::FinishTrainingAttempt(bool bSuccess,const FString& Reason)
{
	if(bTrainingWaiting) return;
	if(auto* PC=Cast<ASpikeElitePlayerController>(UGameplayStatics::GetPlayerController(this,0))) if(PC->Tactical) PC->Tactical->CancelShot();
	if(bSuccess) ++TrainingSuccesses;
	bTrainingWaiting=true; TrainingTimer=2.f; TrainingFeedback=Reason;
	MatchState=EMatchState::PreMatch; SEVolleyballRules::SettleRally(RallyState);
	Ball->ResetBall(Ball->GetActorLocation());
	UE_LOG(LogSEPractice,Log,TEXT("Training RESULT=%s drill=%d attempt=%d successes=%d humanTouches=%d reason=%s"),bSuccess?TEXT("SUCCESS"):TEXT("MISS"),(int32)MatchModeConfig.Drill,TrainingAttempts,TrainingSuccesses,TrainingPlayerTouches,*Reason);
}

void ASpikeEliteGameMode::TickTraining(float Dt)
{
	if(TrainingPreview)
	{
		TrainingPreview->ClearInstances();
		if(MatchModeConfig.Drill==ETrainingDrill::ServePlacement && MatchState==EMatchState::ServiceAuthorized)
		{
			const auto* PC=UGameplayStatics::GetPlayerController(this,0);
			const FVector Start=Ball->GetActorLocation();
			const FVector Velocity=SEPractice::ServeVelocity(Start,PC?PC->GetControlRotation():FRotator(-12,180,0));
			const auto Preview=SEVolleyballTrajectory::Predict(Start,Velocity);
			for(int32 I=0;I<Preview.Points.Num();I+=4) TrainingPreview->AddInstance(FTransform(FQuat::Identity,Preview.Points[I].Location,FVector(.04f)),true);
		}
	}
	TrainingTimer-=Dt;
	if(TrainingTimer<=0)
	{
		if(bTrainingWaiting) StartTrainingAttempt();
		else FinishTrainingAttempt(false,TEXT("本次超时；请移动到落点并完成击球"));
	}
}

FString ASpikeEliteGameMode::GetPracticeStatus() const
{
	const TCHAR* Name=MatchModeConfig.Drill==ETrainingDrill::ServePlacement?TEXT("发球落点"):MatchModeConfig.Drill==ETrainingDrill::ReceiveTarget?TEXT("接发到位"):TEXT("二传配攻");
	return FString::Printf(TEXT("%s训练  第%d次 · 成功%d次 · %.0fs\n%s"),Name,TrainingAttempts,TrainingSuccesses,FMath::Max(0.f,TrainingTimer),*TrainingFeedback);
}

bool ASpikeEliteGameMode::ValidateRuntimeRoster(FString& Why) const
{
	if(!SEVolleyballRoster::ValidateRoster(RosterA,Why) || !SEVolleyballRoster::ValidateRoster(RosterB,Why)) return false;
	if(TeamAPlayers.Num()!=6 || TeamBPlayers.Num()!=6 || BenchPlayers.Num()!=12) { Why=TEXT("场上/候补人数错误"); return false; }
	TSet<FString> IDs;
	for(const auto& Team : {TeamAPlayers,TeamBPlayers,BenchPlayers}) for(const auto& C:Team)
	{
		if(!IsValid(C) || IDs.Contains(C->RosterPlayerId)) { Why=TEXT("重复或失效比赛身份"); return false; }
		IDs.Add(C->RosterPlayerId);
	}
	for(int32 I=0;I<6;++I)
		if(TeamAPlayers[I]->RosterPlayerId!=RosterA.OnCourtLineup[I] || TeamBPlayers[I]->RosterPlayerId!=RosterB.OnCourtLineup[I])
		{ Why=TEXT("身份与轮转槽位不同步"); return false; }
	return true;
}
