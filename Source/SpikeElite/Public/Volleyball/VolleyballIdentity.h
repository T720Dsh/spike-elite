// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Volleyball/VolleyballEnums.h"
#include "VolleyballIdentity.generated.h"

/**
 * M11h-1: data-driven player identity + roster authority.
 *
 * Each match player has a STABLE PlayerId ("A01".."A12") that never changes
 * across rotation, substitution, or array reshuffles. The roster model
 * separates:
 *   - RegisteredRoster (12): full squad definition (identity, jersey, role).
 *   - StartingLineup (6): this set's opening court players (PlayerIds).
 *   - OnCourtLineup (6): current court players; rotation only changes which
 *     slot a player occupies, never their identity/number/stats ownership.
 *   - Bench: Registered minus OnCourt.
 *   - RotationSlot: P1..P6 semantic slot (already in VolleyballEnums.h).
 *
 * Attributes are 0..1 bounded and feed PRODUCTION logic through bounded
 * helper formulas (serve angle error, pass landing error, move speed factor).
 * A player's skill NEVER decides a hit outright — it only adds a bounded,
 * seeded error, and never teleports anyone.
 */

UENUM(BlueprintType)
enum class EPlayerRole : uint8
{
	OutsideHitter UMETA(DisplayName = "主攻"),
	MiddleBlocker UMETA(DisplayName = "副攻"),
	Setter        UMETA(DisplayName = "二传"),
	Opposite      UMETA(DisplayName = "接应"),
	Libero        UMETA(DisplayName = "自由人(未启用)"),
	None          UMETA(Hidden)
};

/** Stable per-player definition used by roster/lineup/HUD/stats. */
USTRUCT(BlueprintType)
struct FPlayerIdentity
{
	GENERATED_BODY()

	/** Stable unique id, e.g. "A01". Never changes across rotation/substitution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString PlayerId;

	/** Original Chinese name (no real-player likeness). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DisplayName;

	/** Unique in-team jersey number. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 JerseyNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EPlayerRole Role = EPlayerRole::None;

	// ---- bounded attributes, 0..1 ----
	float ServeAccuracy = 0.7f; // serve placement / consistency
	float PassAccuracy = 0.7f;  // pass/set placement consistency
	float MoveSpeed = 0.7f;     // movement speed factor
	float Reaction = 0.7f;      // defensive reaction time factor
	float BlockSkill = 0.6f;    // block reach/timing factor
	float DigSkill = 0.6f;      // dig reach factor
	float Stamina = 1.0f;       // light-weight bounded stamina (0..1)

	// ---- appearance config (consumed by the art system) ----
	FLinearColor SkinTone = FLinearColor(0.95f, 0.82f, 0.72f);
	FLinearColor HairTone = FLinearColor(0.13f, 0.13f, 0.18f);
	int32 HairStyle = 0; // 0..3
	int32 BodyBuild = 0; // 0..2
};

/** Per-team authoritative roster + lineup state (pure data, testable). */
USTRUCT(BlueprintType)
struct FTeamRosterState
{
	GENERATED_BODY()

	/** Full 12-player squad. Index 0..11. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPlayerIdentity> Registered;

	/** This set's opening court six (PlayerIds). Size 6. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FString> StartingLineup;

	/** Current court six (PlayerIds). Rotation only shuffles slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FString> OnCourtLineup;

	/** Bench PlayerIds = Registered minus OnCourt. Derived. */
	TArray<FString> GetBench() const
	{
		TArray<FString> Out;
		for (const FPlayerIdentity& P : Registered)
		{
			if (!OnCourtLineup.Contains(P.PlayerId)) { Out.Add(P.PlayerId); }
		}
		return Out;
	}

	/** Index of a PlayerId in OnCourtLineup, or INDEX_NONE. */
	int32 OnCourtIndex(const FString& PlayerId) const
	{
		return OnCourtLineup.IndexOfByKey(PlayerId);
	}

	/** Find identity by PlayerId (Registered), or nullptr. */
	const FPlayerIdentity* FindById(const FString& PlayerId) const
	{
		for (const FPlayerIdentity& P : Registered)
		{
			if (P.PlayerId == PlayerId) { return &P; }
		}
		return nullptr;
	}
	int32 RegisteredIndex(const FString& Id) const
	{
		return Registered.IndexOfByPredicate([&Id](const FPlayerIdentity& P) { return P.PlayerId == Id; });
	}

	/** Identity of the player currently in court index i (0..5), or nullptr. */
	const FPlayerIdentity* CourtPlayer(int32 CourtIndex) const
	{
		if (!OnCourtLineup.IsValidIndex(CourtIndex)) { return nullptr; }
		return FindById(OnCourtLineup[CourtIndex]);
	}
};

/** Per-set normal-substitution ledger. Pairing is established on first use. */
struct FTeamRosterState;
struct SPIKEELITE_API FSubstitutionLedger
{
	TMap<FString, FString> SubstituteToStarter;
	TSet<FString> ExitedStarters, ReturnedStarters;
	int32 Remaining = 6;
	int32 LastRequestRally = INDEX_NONE;
	bool Validate(const FTeamRosterState& Roster, int32 Slot, const FString& Incoming, FString& Reason) const;
	void Apply(FTeamRosterState& Roster, int32 Slot, const FString& Incoming);
};

/** Roster helpers: default squads, validation, bounded attribute formulas. */
namespace SEVolleyballRoster
{
	constexpr int32 RegisteredSize = 12;
	constexpr int32 CourtSize = 6;

	/** Original Chinese position label (never localizes real-brand names). */
	inline const TCHAR* RoleDisplayName(EPlayerRole Role)
	{
		switch (Role)
		{
		case EPlayerRole::OutsideHitter: return TEXT("主攻");
		case EPlayerRole::MiddleBlocker: return TEXT("副攻");
		case EPlayerRole::Setter:        return TEXT("二传");
		case EPlayerRole::Opposite:      return TEXT("接应");
		case EPlayerRole::Libero:        return TEXT("自由人（未启用）");
		default:                         return TEXT("球员");
		}
	}

	/** Build the fixed original 12-player squad for a team. */
	SPIKEELITE_API void BuildDefaultRoster(EVolleyballTeam Team, FTeamRosterState& Out);

	/**
	 * Validate a roster: 12 registered, unique jersey numbers, unique PlayerIds,
	 * StartingLineup size 6, every lineup id present in Registered, libero not
	 * secretly enabled. Returns false + human-readable problem.
	 */
	SPIKEELITE_API bool ValidateRoster(const FTeamRosterState& R, FString& OutProblem);

	/** Bounded serve angle error in degrees (0..~4). 1.0 accuracy => 0. */
	inline float ServeAngleErrorDeg(const FPlayerIdentity& P, float In01)
	{
		return (1.f - FMath::Clamp(P.ServeAccuracy, 0.f, 1.f)) * 3.5f * FMath::Abs(FMath::Clamp(In01, -1.f, 1.f));
	}

	/** Bounded pass/set landing error in cm (0..~70). */
	inline float PassLandingErrorCm(const FPlayerIdentity& P, float In01)
	{
		return (1.f - FMath::Clamp(P.PassAccuracy, 0.f, 1.f)) * 70.f * FMath::Abs(FMath::Clamp(In01, -1.f, 1.f));
	}

	/** Movement speed multiplier, 0.85..1.15. */
	inline float MoveSpeedFactor(const FPlayerIdentity& P)
	{
		return 0.85f + 0.3f * FMath::Clamp(P.MoveSpeed, 0.f, 1.f);
	}

	/** Block reach factor, 0.9..1.1. */
	inline float BlockReachFactor(const FPlayerIdentity& P)
	{
		return 0.9f + 0.2f * FMath::Clamp(P.BlockSkill, 0.f, 1.f);
	}

	/** Dig reach factor, 0.9..1.1. */
	inline float DigReachFactor(const FPlayerIdentity& P)
	{
		return 0.9f + 0.2f * FMath::Clamp(P.DigSkill, 0.f, 1.f);
	}

	/** Reaction time multiplier, 1.15 (slow) .. 0.85 (fast). */
	inline float ReactionTimeFactor(const FPlayerIdentity& P)
	{
		return 1.15f - 0.3f * FMath::Clamp(P.Reaction, 0.f, 1.f);
	}
}
