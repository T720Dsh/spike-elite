// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "SetPlay.generated.h"

/**
 * One data-driven set play (second-touch tactics). All coordinates are in the
 * team's LOCAL court space: +X points toward the opponent (toward the net),
 * Y is the court width (left = negative). The GameMode/tactical UI mirrors the
 * target to the serving team's half before executing.
 */
USTRUCT(BlueprintType)
struct FSetPlayDefinition
{
	GENERATED_BODY()

	/** 1..14 (14 = free trajectory). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PlayId = 1;

	/** Chinese display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	/** Category: 四号位 / 副攻 / 二号位 / 后排 / 组合 / 自由. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Category;

	/** Local target point (X toward opponent, Y across the width, Z ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D TargetLocal = FVector2D(140.f, 0.f);

	/** M11c-5: local attacker run-up start (where the chosen hitter sprints to
	 *  before the set arrives). Mirrored the same way as TargetLocal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D AttackRunupLocal = FVector2D(160.f, 0.f);

	/** Apex height above the contact point (cm); 0 = derive from flight time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ApexHeight = 300.f;

	/** Desired flight time (s). Fast quicks ~0.35, high sets ~1.2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DesiredFlightTime = 0.9f;

	/** True for a back-row (pipe) attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBackRowAttack = false;

	/** 0 = safe, 1 = moderate risk, 2 = high risk (fast/quicks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RiskLevel = 0;
};

namespace SESetPlays
{
	/** The 13 named set plays + free trajectory (id 14). Table is static and
	 *  data-driven; no scattered per-play switch/magic-coordinate code. */
	SPIKEELITE_API const TArray<FSetPlayDefinition>& GetPlays();

	/** Mirror a local target to Team A (+X) or Team B (-X) world coordinates. */
	SPIKEELITE_API FVector MirrorLocal(const FVector2D& Local, int32 TeamSide);
}
