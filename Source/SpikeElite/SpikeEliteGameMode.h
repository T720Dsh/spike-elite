// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpikeEliteGameMode.generated.h"

/**
 * Default game mode for SPIKE ELITE.
 * M0: binds our pawn, spawns a procedural court and a ball.
 */
UCLASS()
class SPIKEELITE_API ASpikeEliteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpikeEliteGameMode();

	virtual void BeginPlay() override;
};
