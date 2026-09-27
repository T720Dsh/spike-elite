// SPDX-License-Identifier: MIT
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/VolleyballBall.h"
#include "Engine/World.h"

ASpikeEliteGameMode::ASpikeEliteGameMode()
{
	DefaultPawnClass = ASpikeEliteCharacter::StaticClass();
}

void ASpikeEliteGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	// Spawn a procedural court at origin.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVolleyballCourt* Court = World->SpawnActor<AVolleyballCourt>(AVolleyballCourt::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// Spawn a ball near the net, slightly above it.
	if (Court)
	{
		FVector BallLoc = FVector(0.0f, 0.0f, Court->NetHeight + 50.0f);
		AVolleyballBall* Ball = World->SpawnActor<AVolleyballBall>(AVolleyballBall::StaticClass(), BallLoc, FRotator::ZeroRotator, Params);
		// Give it a gentle serve-like push so physics is obvious.
		if (Ball)
		{
			Ball->Strike(FVector(0.3f, 0.0f, 0.2f), 600.0f, 0.0f);
		}
	}
}
