// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatchOfficialManager.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class USoundWaveProcedural;

/**
 * Persistent match officials and sideline furniture:
 *  - 1st referee on a referee stand at one net end (source of the whistle);
 *  - 2nd referee at the opposite net end (readiness check, simplified);
 *  - scorer behind the scorer table (faces the 1st referee) + live scoreboard;
 *  - Team A / Team B benches outside the free zone with lightweight substitutes.
 *
 * The GameMode remains the ONLY score/rule authority; this actor is visual and
 * produces the whistle. No AI controllers, no tick, no collision for crowd.
 */
UCLASS()
class SPIKEELITE_API AMatchOfficialManager : public AActor
{
	GENERATED_BODY()

public:
	AMatchOfficialManager();

	/** Play the program-generated referee whistle (own sound, no external asset). */
	void Whistle();

	/** Push live score text onto the scorer's table scoreboard (event-driven). */
	void SetScorerText(const FString& Line1, const FString& Line2, const FString& Line3);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Root;

	// 1st referee stand
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StandPlatform;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StandLadder;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StandRailing;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StandRailingPostA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StandRailingPostB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StandPadding;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ref1Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ref1Head;

	// 2nd referee (ground, opposite net end)
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ref2Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ref2Head;

	// Scorer table
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScorerTable;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScorerChair;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScorerBody;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScorerHead;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScoreboardDevice;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ScoreboardPanel;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> ScoreboardText;

	// Benches (Team A / Team B) with lightweight substitutes
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BenchA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BenchB;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Substitutes;

	UPROPERTY() TObjectPtr<USoundWaveProcedural> WhistleSound;

	UStaticMeshComponent* MakeBlock(const TCHAR* Name, const FVector& Loc, const FVector& Scale,
		const FLinearColor& Color, ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision);
	void BuildStand();
	void BuildScorer();
	void BuildBenches();
};
