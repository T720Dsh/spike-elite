// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GameFramework/CharacterMovementComponent.h"

void ASpikeEliteGameMode::SynchronizeCourtIdentities()
{
	auto Sync = [](const FTeamRosterState& R, TArray<TObjectPtr<ASpikeEliteCharacter>>& Actors)
	{
		for (int32 I=0; I<Actors.Num(); ++I)
			if (Actors[I]) if (const FPlayerIdentity* P=R.CourtPlayer(I))
			{
				Actors[I]->RosterPlayerId=P->PlayerId; Actors[I]->JerseyNumber=P->JerseyNumber;
				Actors[I]->RefreshJerseyNumberVisual();
				Actors[I]->GetCharacterMovement()->MaxWalkSpeed=600.f*SEVolleyballRoster::MoveSpeedFactor(*P);
			}
	};
	Sync(RosterA, TeamAPlayers); Sync(RosterB, TeamBPlayers);
}

void ASpikeEliteGameMode::RefreshRegisteredBench()
{
	int32 SeatA=0, SeatB=0;
	for (auto& C : BenchPlayers)
	{
		if (!C) continue;
		const FTeamRosterState& R=C->TeamSide>0 ? RosterA : RosterB;
		int32& Seat=C->TeamSide>0 ? SeatA : SeatB;
		const TArray<FString> Bench=R.GetBench();
		if(Bench.IsValidIndex(Seat)) C->RosterPlayerId=Bench[Seat];
		if (const FPlayerIdentity* P=R.FindById(C->RosterPlayerId))
		{ C->JerseyNumber=P->JerseyNumber; C->RefreshJerseyNumberVisual(); }
		C->HomePosition=FVector(C->TeamSide*(400.f+Seat*90.f),-1320.f,0.f);
		C->SetActorLocation(C->HomePosition+FVector(0,0,100));
		C->GetCharacterMovement()->DisableMovement();
		C->SetActorRotation(FRotator(0,90,0)); ++Seat;
	}
}

bool ASpikeEliteGameMode::SetStartingLineup(EVolleyballTeam Team, const TArray<FString>& Lineup, FString& Reason)
{
	if (bMatchActive || Team==EVolleyballTeam::None)
	{ Reason=TEXT("比赛进行中不可编辑首发，请使用换人"); return false; }
	FTeamRosterState& R=Team==EVolleyballTeam::TeamA ? RosterA : RosterB;
	if (R.Registered.IsEmpty()) SEVolleyballRoster::BuildDefaultRoster(Team,R);
	FTeamRosterState Candidate=R;
	Candidate.StartingLineup=Lineup; Candidate.OnCourtLineup=Lineup;
	if (!SEVolleyballRoster::ValidateRoster(Candidate,Reason)) return false;
	R=MoveTemp(Candidate);
	(Team==EVolleyballTeam::TeamA ? SavedLineupA : SavedLineupB)=Lineup;
	return true;
}

void ASpikeEliteGameMode::LoadLineups()
{
	// A menu draft takes precedence over the disk; Rematch preserves its lineup.
	if (SavedLineupA.Num()==6 && SavedLineupB.Num()==6) return;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("LineupSave.ini")))) return;
	int32 Version=0; if (!FParse::Value(*Text,TEXT("Schema="),Version) || Version!=1) return;
	for (int32 Side=0; Side<2; ++Side)
	{
		FString Line; if (!FParse::Value(*Text,Side==0?TEXT("TeamA="):TEXT("TeamB="),Line)) continue;
		TArray<FString> Ids; Line.ParseIntoArray(Ids,TEXT(","),true);
		FString Reason; SetStartingLineup(Side==0?EVolleyballTeam::TeamA:EVolleyballTeam::TeamB,Ids,Reason);
	}
}

bool ASpikeEliteGameMode::SaveLineups() const
{
	IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(),true);
	return FFileHelper::SaveStringToFile(FString::Printf(TEXT("Schema=1\nTeamA=%s\nTeamB=%s\n"),
		*FString::Join(RosterA.StartingLineup,TEXT(",")),*FString::Join(RosterB.StartingLineup,TEXT(","))),
		*(FPaths::ProjectSavedDir()/TEXT("LineupSave.ini")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
