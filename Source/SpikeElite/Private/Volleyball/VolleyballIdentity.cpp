// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballIdentity.h"

namespace SEVolleyballRoster
{

namespace
{
	void AddPlayer(FTeamRosterState& Out, const TCHAR* Id, const TCHAR* Name, int32 Jersey,
		EPlayerRole Role, float Serve, float Pass, float Move, float React, float Block, float Dig,
		float Stamina, const FLinearColor& Skin, const FLinearColor& Hair, int32 HairStyle, int32 BodyBuild)
	{
		FPlayerIdentity P;
		P.PlayerId = Id;
		P.DisplayName = Name;
		P.JerseyNumber = Jersey;
		P.Role = Role;
		P.ServeAccuracy = Serve;
		P.PassAccuracy = Pass;
		P.MoveSpeed = Move;
		P.Reaction = React;
		P.BlockSkill = Block;
		P.DigSkill = Dig;
		P.Stamina = Stamina;
		P.SkinTone = Skin;
		P.HairTone = Hair;
		P.HairStyle = HairStyle;
		P.BodyBuild = BodyBuild;
		Out.Registered.Add(P);
	}
}

void BuildDefaultRoster(EVolleyballTeam Team, FTeamRosterState& Out)
{
	Out.Registered.Reset();
	Out.StartingLineup.Reset();
	Out.OnCourtLineup.Reset();

	const FLinearColor SkinA1(0.96f, 0.84f, 0.74f);
	const FLinearColor SkinA2(0.72f, 0.58f, 0.47f);
	const FLinearColor SkinB1(0.90f, 0.80f, 0.70f);
	const FLinearColor HairBlack(0.10f, 0.10f, 0.14f);
	const FLinearColor HairBrown(0.24f, 0.16f, 0.10f);
	const FLinearColor HairDark(0.12f, 0.12f, 0.16f);

	if (Team == EVolleyballTeam::TeamA)
	{
		// 6 starters + 6 bench, all original names/numbers, in-team unique.
		AddPlayer(Out, TEXT("A01"), TEXT("林一鸣"), 1, EPlayerRole::OutsideHitter,
			0.80f, 0.72f, 0.78f, 0.70f, 0.60f, 0.72f, 1.00f, SkinA1, HairBlack, 1, 1);
		AddPlayer(Out, TEXT("A02"), TEXT("陈浩然"), 2, EPlayerRole::OutsideHitter,
			0.74f, 0.68f, 0.74f, 0.66f, 0.58f, 0.70f, 0.96f, SkinA2, HairBrown, 0, 0);
		AddPlayer(Out, TEXT("A03"), TEXT("王志远"), 3, EPlayerRole::MiddleBlocker,
			0.66f, 0.60f, 0.68f, 0.62f, 0.88f, 0.60f, 0.94f, SkinA1, HairBlack, 2, 2);
		AddPlayer(Out, TEXT("A04"), TEXT("赵天宇"), 4, EPlayerRole::MiddleBlocker,
			0.62f, 0.58f, 0.66f, 0.60f, 0.90f, 0.58f, 0.92f, SkinA2, HairDark, 2, 2);
		AddPlayer(Out, TEXT("A05"), TEXT("李承泽"), 5, EPlayerRole::Setter,
			0.70f, 0.90f, 0.62f, 0.64f, 0.50f, 0.66f, 0.95f, SkinA1, HairBrown, 1, 0);
		AddPlayer(Out, TEXT("A06"), TEXT("周启航"), 6, EPlayerRole::Opposite,
			0.78f, 0.66f, 0.72f, 0.68f, 0.62f, 0.68f, 0.97f, SkinA2, HairBlack, 0, 1);
		// Bench 7..12
		AddPlayer(Out, TEXT("A07"), TEXT("孙立峰"), 7, EPlayerRole::OutsideHitter,
			0.72f, 0.64f, 0.70f, 0.62f, 0.54f, 0.64f, 0.90f, SkinA1, HairDark, 1, 1);
		AddPlayer(Out, TEXT("A08"), TEXT("吴俊杰"), 8, EPlayerRole::MiddleBlocker,
			0.60f, 0.54f, 0.64f, 0.58f, 0.84f, 0.54f, 0.88f, SkinA2, HairBlack, 2, 2);
		AddPlayer(Out, TEXT("A09"), TEXT("郑海涛"), 9, EPlayerRole::Setter,
			0.66f, 0.86f, 0.58f, 0.60f, 0.46f, 0.62f, 0.89f, SkinA1, HairBrown, 1, 0);
		AddPlayer(Out, TEXT("A10"), TEXT("冯致远"), 10, EPlayerRole::Opposite,
			0.74f, 0.62f, 0.68f, 0.64f, 0.58f, 0.64f, 0.91f, SkinA2, HairDark, 0, 1);
		AddPlayer(Out, TEXT("A11"), TEXT("何明轩"), 11, EPlayerRole::OutsideHitter,
			0.70f, 0.62f, 0.68f, 0.60f, 0.52f, 0.62f, 0.87f, SkinA1, HairBlack, 0, 0);
		AddPlayer(Out, TEXT("A12"), TEXT("沈国梁"), 12, EPlayerRole::MiddleBlocker,
			0.58f, 0.52f, 0.62f, 0.56f, 0.82f, 0.52f, 0.86f, SkinA2, HairDark, 2, 2);
	}
	else
	{
		AddPlayer(Out, TEXT("B01"), TEXT("高晨风"), 1, EPlayerRole::OutsideHitter,
			0.82f, 0.74f, 0.76f, 0.72f, 0.62f, 0.74f, 1.00f, SkinB1, HairBrown, 1, 1);
		AddPlayer(Out, TEXT("B02"), TEXT("唐景行"), 2, EPlayerRole::OutsideHitter,
			0.76f, 0.70f, 0.72f, 0.68f, 0.60f, 0.72f, 0.97f, SkinB1, HairBlack, 0, 0);
		AddPlayer(Out, TEXT("B03"), TEXT("孟朝阳"), 3, EPlayerRole::MiddleBlocker,
			0.64f, 0.58f, 0.66f, 0.60f, 0.92f, 0.58f, 0.93f, SkinB1, HairDark, 2, 2);
		AddPlayer(Out, TEXT("B04"), TEXT("郭俊熙"), 4, EPlayerRole::MiddleBlocker,
			0.60f, 0.56f, 0.64f, 0.58f, 0.94f, 0.56f, 0.91f, SkinB1, HairBrown, 2, 2);
		AddPlayer(Out, TEXT("B05"), TEXT("许云帆"), 5, EPlayerRole::Setter,
			0.68f, 0.92f, 0.60f, 0.62f, 0.48f, 0.64f, 0.94f, SkinB1, HairBlack, 1, 0);
		AddPlayer(Out, TEXT("B06"), TEXT("邵俊贤"), 6, EPlayerRole::Opposite,
			0.80f, 0.68f, 0.74f, 0.70f, 0.64f, 0.70f, 0.98f, SkinB1, HairDark, 0, 1);
		AddPlayer(Out, TEXT("B07"), TEXT("邓凯文"), 7, EPlayerRole::OutsideHitter,
			0.74f, 0.66f, 0.68f, 0.64f, 0.56f, 0.66f, 0.90f, SkinB1, HairBrown, 1, 1);
		AddPlayer(Out, TEXT("B08"), TEXT("贾一鸣"), 8, EPlayerRole::MiddleBlocker,
			0.58f, 0.52f, 0.62f, 0.56f, 0.86f, 0.52f, 0.88f, SkinB1, HairDark, 2, 2);
		AddPlayer(Out, TEXT("B09"), TEXT("钟子昂"), 9, EPlayerRole::Setter,
			0.64f, 0.88f, 0.56f, 0.58f, 0.44f, 0.60f, 0.89f, SkinB1, HairBlack, 1, 0);
		AddPlayer(Out, TEXT("B10"), TEXT("顾星辰"), 10, EPlayerRole::Opposite,
			0.76f, 0.64f, 0.70f, 0.66f, 0.60f, 0.66f, 0.92f, SkinB1, HairDark, 0, 1);
		AddPlayer(Out, TEXT("B11"), TEXT("潘乐天"), 11, EPlayerRole::OutsideHitter,
			0.72f, 0.64f, 0.66f, 0.62f, 0.54f, 0.64f, 0.88f, SkinB1, HairBrown, 0, 0);
		AddPlayer(Out, TEXT("B12"), TEXT("罗子谦"), 12, EPlayerRole::MiddleBlocker,
			0.56f, 0.50f, 0.60f, 0.54f, 0.84f, 0.50f, 0.85f, SkinB1, HairDark, 2, 2);
	}

	// Default starting lineup: first six registered (balanced 4-2: 2 OH, 2 MB,
	// 1 S, 1 OPP). Court = starting lineup at match start.
	for (int32 i = 0; i < CourtSize; ++i)
	{
		Out.StartingLineup.Add(Out.Registered[i].PlayerId);
		Out.OnCourtLineup.Add(Out.Registered[i].PlayerId);
	}
}

bool ValidateRoster(const FTeamRosterState& R, FString& OutProblem)
{
	if (R.Registered.Num() != RegisteredSize)
	{
		OutProblem = FString::Printf(TEXT("registered=%d (want %d)"), R.Registered.Num(), RegisteredSize);
		return false;
	}
	TSet<int32> Jerseys;
	TSet<FString> Ids;
	for (const FPlayerIdentity& P : R.Registered)
	{
		if (Jerseys.Contains(P.JerseyNumber))
		{
			OutProblem = FString::Printf(TEXT("duplicate jersey %d"), P.JerseyNumber);
			return false;
		}
		Jerseys.Add(P.JerseyNumber);
		if (Ids.Contains(P.PlayerId))
		{
			OutProblem = FString::Printf(TEXT("duplicate id %s"), *P.PlayerId);
			return false;
		}
		Ids.Add(P.PlayerId);
		if (P.JerseyNumber <= 0 || P.JerseyNumber > 99)
		{
			OutProblem = FString::Printf(TEXT("jersey %d out of range"), P.JerseyNumber);
			return false;
		}
	}
	if (R.StartingLineup.Num() != CourtSize)
	{
		OutProblem = FString::Printf(TEXT("starting=%d (want %d)"), R.StartingLineup.Num(), CourtSize);
		return false;
	}
	if (R.OnCourtLineup.Num() != CourtSize)
	{
		OutProblem = FString::Printf(TEXT("oncourt=%d (want %d)"), R.OnCourtLineup.Num(), CourtSize);
		return false;
	}
	TSet<FString> StartSet;
	for (const FString& Id : R.StartingLineup)
	{
		if (!R.FindById(Id))
		{
			OutProblem = FString::Printf(TEXT("starting id %s not registered"), *Id);
			return false;
		}
		if (StartSet.Contains(Id))
		{
			OutProblem = FString::Printf(TEXT("starting duplicate %s"), *Id);
			return false;
		}
		StartSet.Add(Id);
	}
	for (const FString& Id : R.OnCourtLineup)
	{
		if (!R.FindById(Id))
		{
			OutProblem = FString::Printf(TEXT("oncourt id %s not registered"), *Id);
			return false;
		}
	}
	return true;
}

} // namespace SEVolleyballRoster
