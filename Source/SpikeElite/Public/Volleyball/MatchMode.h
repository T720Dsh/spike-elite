// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "MatchMode.generated.h"

/**
 * M11h-2: selectable game modes.
 *
 * - QuickMatch  (快速体验): dev-friendly quick game; the menu can pick a
 *   short-set (1 set x 3 pts) or a full FIVB-style match. The -QuickMatch
 *   command line stays a developer shortcut that defaults to the same mode.
 * - Challenge12 (12人名单挑战赛): three consecutive original opponents with
 *   different play styles (basic defense / heavy serve / fast offense+block).
 *   Roster, scores and challenge progress are saved; retry allowed.
 * - Coach       (教练模式): the player sits on the bench and directs; all 12
 *   on-court players are AI-driven. Lineup, timeout, substitution, serve
 *   placement, block/defense focus, setter distribution.
 * - Training    (训练模式): repeatable drills (serve placement / receive to
 *   target / set+attack). Does not affect official match stats.
 *
 * The mode object is carried into the GameMode; it is NOT a pile of
 * command-line branches stacked in Tick.
 */
UENUM(BlueprintType)
enum class EGameModeChoice : uint8
{
	QuickMatch  UMETA(DisplayName = "快速体验"),
	Challenge12 UMETA(DisplayName = "12人名单挑战赛"),
	Coach       UMETA(DisplayName = "教练模式"),
	Training    UMETA(DisplayName = "训练模式"),
	None        UMETA(Hidden)
};

/** Training drills (M11h-2). */
UENUM(BlueprintType)
enum class ETrainingDrill : uint8
{
	ServePlacement UMETA(DisplayName = "发球落点"),
	ReceiveTarget  UMETA(DisplayName = "接发到位"),
	SetAttack      UMETA(DisplayName = "二传配攻"),
	None           UMETA(Hidden)
};

/** A single, serializable mode configuration handed to the GameMode. */
USTRUCT(BlueprintType)
struct FMatchModeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGameModeChoice Mode = EGameModeChoice::QuickMatch;

	/** QuickMatch: true = 1 set x 3 pts (menu option); false = official rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShortSets = true;

	/** Challenge12: 0..2 (three original opponents). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChallengeStage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETrainingDrill Drill = ETrainingDrill::None;

	/** The active mode's short display name (original text). */
	static const TCHAR* DisplayName(EGameModeChoice Mode)
	{
		switch (Mode)
		{
		case EGameModeChoice::QuickMatch:  return TEXT("快速体验");
		case EGameModeChoice::Challenge12: return TEXT("12人名单挑战赛");
		case EGameModeChoice::Coach:       return TEXT("教练模式");
		case EGameModeChoice::Training:    return TEXT("训练模式");
		default: return TEXT("");
		}
	}
};

/**
 * M11h-6: coach / team-management preferences. The coach UI writes these; the
 * AI decision points (serve placement, block line, defensive depth, setter
 * distribution, risk tolerance) read them. Defaults are sensible for normal
 * play (no coach panel), so AI behaviour is unchanged unless the coach sets
 * a preference. Preferences apply at the next safe decision point — never
 * mid-flight teleports or ball-speed edits.
 */
USTRUCT(BlueprintType)
struct FCoachPreferences
{
	GENERATED_BODY()

	/** Serve placement from the server's viewpoint: -1 left / 0 middle / +1 right.
	 *  Applied as a lateral bias to the serve target (not an absolute zone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ServeZone = 0;

	/** Block strategy: 0 no block / 1 single block / 2 double block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BlockPreference = 1;

	/** Defensive depth: -1 press / 0 standard / +1 drop back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DefenseDepth = 0;

	/** Setter distribution bias: 0 default (play-driven) / 1 outside / 2 middle
	 *  / 3 opposite / 4 back-row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SetterPreference = 0;

	/** Risk tolerance 0..1 (0 conservative, 1 aggressive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RiskTolerance = 0.5f;

	/** Lateral bias (cm) for serve placement. */
	float ServeZoneBiasCm() const
	{
		return FMath::Clamp(ServeZone, -1, 1) * 240.f;
	}
};
