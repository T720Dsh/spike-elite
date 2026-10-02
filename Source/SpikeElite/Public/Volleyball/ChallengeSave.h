// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

/**
 * M11h-2b: 12-man Challenge mode — three original opponents with different
 * play styles, plus a schema-versioned save of challenge progress.
 *
 * Difficulty is applied ONLY through bounded attribute modifiers on the
 * opponent roster (skill never decides a hit outright; it shifts seeded
 * error/consistency within 0..1-clamped bounds). Stage 0..2:
 *   0 基础防守 — opponent serve/pass sloppier (easier)
 *   1 强发球   — opponent serve stays accurate, movement faster
 *   2 快攻拦网 — opponent block/pass/reaction sharper
 */
namespace SEChallenge
{
	constexpr int32 StageCount = 3;
	constexpr int32 SchemaVersion = 1;

	struct FSaveData
	{
		int32 SchemaVersion = SEChallenge::SchemaVersion;
		int32 Stage = 0;       // next challenge stage (0..2, 3 = all done)
		int32 Won = 0;         // consecutive challenge wins so far
		bool bCompleted = false;
	};

	inline FString GetSavePath()
	{
		// Project-local save (D:\projects\spike-elite\Saved), never C:\Users.
		return FPaths::ProjectSavedDir() + TEXT("ChallengeSave.ini");
	}

	inline bool Save(const FSaveData& Data)
	{
		// Plain key=value text (schema-versioned). No GConfig cache involvement,
		// so round-trips are deterministic in headless automation too.
		const FString Path = GetSavePath();
		const FString Text = FString::Printf(TEXT("SchemaVersion=%d\nStage=%d\nWon=%d\nCompleted=%d\n"),
			SEChallenge::SchemaVersion, Data.Stage, Data.Won, Data.bCompleted ? 1 : 0);
		return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	inline void Load(FSaveData& Out)
	{
		const FString Path = GetSavePath();
		if (!IFileManager::Get().FileExists(*Path))
		{
			Out = FSaveData();
			return;
		}
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			Out = FSaveData();
			Save(Out);
			return;
		}
		FSaveData D;
		int32 Ver = 0, Stage = 0, Won = 0, Completed = 0;
		const bool bOk = FParse::Value(*Text, TEXT("SchemaVersion="), Ver)
			&& FParse::Value(*Text, TEXT("Stage="), Stage)
			&& FParse::Value(*Text, TEXT("Won="), Won)
			&& FParse::Value(*Text, TEXT("Completed="), Completed);
		if (!bOk || Ver != SEChallenge::SchemaVersion)
		{
			// Missing/corrupt/unknown-schema file: default cleanly, rewrite.
			Out = FSaveData();
			Save(Out);
			return;
		}
		D.Stage = FMath::Clamp(Stage, 0, StageCount);
		D.Won = FMath::Max(Won, 0);
		D.bCompleted = Completed != 0;
		if (D.Stage >= StageCount) { D.bCompleted = true; D.Stage = StageCount; }
		Out = D;
	}

	// ---- bounded difficulty modifiers (0.8..1.2 range, all inputs clamped) ----

	inline float OpponentServeErrScale(int32 Stage)
	{
		// Multiplied against (1 - ServeAccuracy): stage 0 serves are looser.
		switch (Stage)
		{
		case 0: return 1.30f;
		case 1: return 0.75f; // strong serve: tighter placement
		default: return 1.0f;
		}
	}

	inline float OpponentPassErrScale(int32 Stage)
	{
		switch (Stage)
		{
		case 0: return 1.35f;
		case 1: return 1.0f;
		default: return 0.95f; // fast offense: cleaner passing
		}
	}

	inline float OpponentBlockScale(int32 Stage)
	{
		switch (Stage)
		{
		case 0: return 0.85f;
		case 1: return 0.95f;
		default: return 1.15f; // fast block
		}
	}

	inline float OpponentMoveScale(int32 Stage)
	{
		switch (Stage)
		{
		case 0: return 0.95f;
		case 1: return 1.08f; // strong serve + quick movement
		default: return 1.02f;
		}
	}

	inline float OpponentReactionScale(int32 Stage)
	{
		switch (Stage)
		{
		case 0: return 1.10f; // slower reaction
		case 1: return 1.0f;
		default: return 0.92f; // fast block/reaction
		}
	}

	/** Opponent display name for the stage. */
	inline const TCHAR* OpponentName(int32 Stage)
	{
		switch (Stage)
		{
		case 0: return TEXT("猎鹰队 · 基础防守");
		case 1: return TEXT("雷霆队 · 强发球");
		case 2: return TEXT("龙卷队 · 快攻拦网");
		default: return TEXT("");
		}
	}
}
