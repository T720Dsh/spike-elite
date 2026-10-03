#include "SpikeElitePlayerController.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballBall.h"
#include "UI/MatchActionWidget.h"
#include "UI/TrajectoryPreviewComponent.h"
#include "UI/TacticalContactComponent.h"
#include "Volleyball/VolleyballTrajectory.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

void ASpikeElitePlayerController::ToggleMatchMouse()
{
	if(IsMenuOpen() || (Tactical && (Tactical->IsTacticalActive() || Tactical->IsDefensePlanning()))) return;
	const bool bWasActive=IsMatchMouseActive() && !bServeMouseHidden;
	bMatchMouse=!bWasActive; bServeMouseHidden=bWasActive;
	RefreshMatchInteraction();
}
void ASpikeElitePlayerController::RestoreGameplayInput()
{
	if(MenuState!=EMenuState::Playing || CoachPanel || RosterMenu) return;
	if(MatchActions) SetUIInputMode(MatchActions); else SetGameInputMode();
}
FVector ASpikeElitePlayerController::GetServeAimVelocity(const FVector& Start,int32 Side) const
{
	FShotIntent Intent; Intent.TouchType=EBallTouchType::Serve;
	Intent.TargetLocation=FVector(-Side*FMath::Clamp(FMath::Abs(ServeTarget.X),80.f,880.f),FMath::Clamp(ServeTarget.Y,-430.f,430.f),0);
	Intent.ApexHeight=FMath::Clamp(ServeArc,80.f,600.f); Intent.Power=1;
	return SEVolleyballTrajectory::BuildShotSolution(Start,Intent,0).InitialVelocity;
}
void ASpikeElitePlayerController::DragServeAim(const FVector2D& D)
{
	if(!bServeAiming) return;
	ServeArc=FMath::Clamp(ServeArc-D.Y*1.6f,80.f,600.f);
	// Arc adjustment never moves the selected landing point; click again to retarget.
}
void ASpikeElitePlayerController::PickServeTarget(const FVector2D& DesktopPosition)
{
	if(!bServeAiming) return;
	// Pointer events use desktop coordinates. Convert through the viewport geometry (window borders/DPI).
	FVector2D Pixel,Viewport;
	USlateBlueprintLibrary::AbsoluteToViewport(this,DesktopPosition,Pixel,Viewport);
	FVector Origin,Dir;
	if(DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Origin,Dir) && Dir.Z<-.001f)
	{
		const FVector P=Origin+Dir*(-Origin.Z/Dir.Z);
		if(const auto* C=Cast<ASpikeEliteCharacter>(GetPawn()); C && P.X*C->TeamSide<0 && FMath::Abs(P.X)<=900 && FMath::Abs(P.Y)<=450)
			ServeTarget=FVector(P.X,P.Y,0);
	}
}
void ASpikeElitePlayerController::RefreshMatchInteraction()
{
	auto* GM=GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>();
	auto* C=Cast<ASpikeEliteCharacter>(GetPawn());
	const bool bTactical=Tactical && (Tactical->IsTacticalActive() || Tactical->IsDefensePlanning());
	const bool bCanAim=GM && C && !C->bIsBot && !IsMenuOpen() && !bTactical && GM->MatchState==EMatchState::ServiceAuthorized && C->GetTeam()==GM->ServingTeam && GM->GetTeamPlayers(C->GetTeam()).Num()>0 && GM->GetTeamPlayers(C->GetTeam())[0]==C;
	if(bCanAim && !bServeAiming)
	{
		// Preserve the selected depth when resuming from pause. Mirror only if
		// control moved to the opposite team; a new match resets the plan explicitly.
		if(ServeTarget.X*C->TeamSide>=0) ServeTarget.X=-C->TeamSide*600.f;
		bServeMouseHidden=false;
		// A raised, oblique service view keeps the entire 3D arc and opponent court
		// visible. A low player camera could clip a high arc completely above the screen.
		if(!ServeAimCamera)
		{
			BeforeServeView=GetViewTarget(); ServeAimCamera=GetWorld()->SpawnActor<ACameraActor>();
			const FVector Location(C->TeamSide*1800.f,-1100.f,1000.f),Target(-C->TeamSide*150.f,0,180.f);
			ServeAimCamera->SetActorLocationAndRotation(Location,(Target-Location).Rotation());
			ServeAimCamera->GetCameraComponent()->SetFieldOfView(78.f);
			SetViewTargetWithBlend(ServeAimCamera,.25f);
		}
	}
	if(ServeAimCamera && (!GM || IsMenuOpen() || (!bCanAim && GM->MatchState!=EMatchState::ServingToss))) ReleaseServeCamera();
	bServeAiming=bCanAim;
	if(bCanAim && GM->GetBall())
	{
		if(!ServePreview) { ServePreview=NewObject<UTrajectoryPreviewComponent>(this); ServePreview->RegisterComponent(); }
		ServePreview->ShowPreview(GM->GetBall()->GetActorLocation(),GetServeAimVelocity(GM->GetBall()->GetActorLocation(),C->TeamSide),true);
	}
	else if(ServePreview) ServePreview->HidePreview();
	const bool bShow=!IsMenuOpen() && !bTactical && (bMatchMouse || (bCanAim && !bServeMouseHidden));
	if(bShow)
	{
		if(!MatchActions) { MatchActions=CreateWidget<UMatchActionWidget>(this); MatchActions->AddToViewport(12); SetUIInputMode(MatchActions); }
		MatchActions->Refresh(bCanAim,ServeArc);
	}
	else if(MatchActions)
	{
		MatchActions->RemoveFromParent(); MatchActions=nullptr;
		if(!IsMenuOpen() && !bTactical) SetGameInputMode();
	}
}
void ASpikeElitePlayerController::ReleaseServeCamera()
{
	if(!ServeAimCamera) return;
	// Restore before destroying the temporary actor; never leave a pending
	// camera blend referencing an actor that has already been destroyed.
	if(GetViewTarget()==ServeAimCamera && IsValid(BeforeServeView)) SetViewTarget(BeforeServeView);
	ServeAimCamera->Destroy(); ServeAimCamera=nullptr; BeforeServeView=nullptr;
}
void ASpikeElitePlayerController::PerformMatchAction(int32 Action)
{
	if(IsMenuOpen()) return;
	auto* GM=GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>(); auto* C=Cast<ASpikeEliteCharacter>(GetPawn()); if(!GM || !C) return;
	switch(Action)
	{
	case 0: if(GM->MatchState==EMatchState::Entrance) GM->FinishEntrance(); else if(GM->MatchState==EMatchState::ServePresentation) GM->SkipServePresentation(); else GM->RequestServe(C); break;
	case 1: GM->TryTouchBall(C,EBallTouchType::Receive); break;
	case 2: GM->TryTouchBall(C,EBallTouchType::Set); break;
	case 3: GM->TryTouchBall(C,EBallTouchType::Attack); break;
	case 4: C->RaiseHandsAmount=C->RaiseHandsAmount>.5f?0.f:1.f; if(GM->MatchState==EMatchState::Rally) GM->TryBlockBall(C); break;
	case 5: C->PlayerJump(); break;
	case 6: SwitchControlledPlayer(); break;
	case 7: ToggleCoachPanel(); break;
	case 8: PauseGame(); break;
	case 9: ToggleMatchMouse(); break;
	}
	RefreshMatchInteraction();
}
