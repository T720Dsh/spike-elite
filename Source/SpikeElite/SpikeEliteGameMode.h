// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpikeEliteGameMode.generated.h"

/**
 * Default game mode for SPIKE ELITE.
 * M0: just binds a pawn and HUD; rules engine comes in M1.
 */
UCLASS()
class SPIKEELITE_API ASpikeEliteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpikeEliteGameMode();
};
