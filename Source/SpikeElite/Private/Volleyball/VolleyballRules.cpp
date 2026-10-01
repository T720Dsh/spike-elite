// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballRules.h"

DEFINE_LOG_CATEGORY(LogVolleyballRules);

namespace SEVolleyballRules
{
	EVolleyballTeam DetermineScoringTeamOnLand(bool bInBounds, EVolleyballTeam LastTouchTeam, bool bLandedOnPositiveX)
	{
		if (bInBounds)
		{
			// Landed in a court: the side defending that half loses the rally.
			// X > 0 is Team A's half, so Team B scores (and vice versa).
			return bLandedOnPositiveX ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
		}

		// Landed OUT: opponent of the last touching team scores.
		if (LastTouchTeam == EVolleyballTeam::TeamA) { return EVolleyballTeam::TeamB; }
		if (LastTouchTeam == EVolleyballTeam::TeamB) { return EVolleyballTeam::TeamA; }

		// No touch recorded (e.g. serve never touched by anyone): fall back to half.
		return bLandedOnPositiveX ? EVolleyballTeam::TeamB : EVolleyballTeam::TeamA;
	}

	ETouchResult EvaluateTouch(FVolleyballRallyState& State, EVolleyballTeam Team, int32 PlayerIndex, EBallTouchType Type)
	{
		if (State.bRallySettled) { return ETouchResult::RallySettled; }
		if (Team == EVolleyballTeam::None || PlayerIndex < 0) { return ETouchResult::WrongTeam; }

		// M11c: serve-flight gate. The serve was recorded (LastTouchType == Serve)
		// but has not yet legally crossed the net, so NOBODY may touch the ball —
		// not the serving team (a second illegal contact), not the receiving team
		// (the ball is not over the net yet). This prevents the old P0 sequence
		// [Serve] -> [Touch 2/3 Set] -> [Touch 3/3 Attack] before [NetCross].
		if (State.LastTouchType == EBallTouchType::Serve && !State.bServeCrossedNet)
		{
			return ETouchResult::WrongPhase;
		}

		// M11b-5 block: a front-row block is legal at any touch count, does not
		// consume one of the team's three touches and does not trip the
		// double-touch rule (the blocker may touch again right after). It also
		// bypasses the possession check — a block is a defensive action against
		// the possessing attacker's ball. It still records LastTouch so an
		// off-the-hands out call goes to the attacker.
		if (Type == EBallTouchType::Block)
		{
			State.LastTouchTeam = Team;
			State.LastTouchPlayerIndex = PlayerIndex;
			State.LastTouchType = EBallTouchType::Block;
			return ETouchResult::Allowed;
		}

		// If a team is in possession, only that team may touch.
		if (State.PossessingTeam != EVolleyballTeam::None && State.PossessingTeam != Team)
		{
			return ETouchResult::WrongTeam;
		}

		// Fourth touch by the same team: fault -> opponent scores.
		if (State.TouchCount >= 3)
		{
			return ETouchResult::FourTouchesFault;
		}

		// Same player touching twice in a row: fault -> opponent scores
		// (a block does NOT arm the double-touch rule: the blocker may touch again).
		if (State.LastTouchType != EBallTouchType::Block
			&& State.LastTouchTeam == Team && State.LastTouchPlayerIndex == PlayerIndex)
		{
			return ETouchResult::DoubleTouchFault;
		}

		// Legal touch.
		State.LastTouchTeam = Team;
		State.LastTouchPlayerIndex = PlayerIndex;
		State.LastTouchType = Type;
		State.TouchCount += 1;
		if (State.PossessingTeam == EVolleyballTeam::None)
		{
			State.PossessingTeam = Team;
		}
		return ETouchResult::Allowed;
	}

	void RecordServeTouch(FVolleyballRallyState& State, EVolleyballTeam Team, int32 PlayerIndex)
	{
		State.LastTouchTeam = Team;
		State.LastTouchPlayerIndex = PlayerIndex;
		State.LastTouchType = EBallTouchType::Serve;
		State.PossessingTeam = EVolleyballTeam::None;   // nobody possesses during serve flight
		State.bServeCrossedNet = false;
		State.TouchCount = 0;                           // the serve is NOT a team touch
	}

	void OnBallCrossedNet(FVolleyballRallyState& State, EVolleyballTeam NewPossessor)
	{
		State.bServeCrossedNet = true;
		State.PossessingTeam = NewPossessor;
		State.TouchCount = 0;
		// LastTouchTeam/LastTouchPlayerIndex stay unchanged: they still decide OUT calls.
	}

	void BeginRally(FVolleyballRallyState& State, EVolleyballTeam ServingTeam)
	{
		State.PossessingTeam = EVolleyballTeam::None;  // no possession until the serve crosses
		State.TouchCount = 0;
		State.LastTouchTeam = EVolleyballTeam::None;
		State.LastTouchPlayerIndex = -1;
		State.LastTouchType = EBallTouchType::Unknown;
		State.bRallySettled = false;
		State.bBallInPlay = false;
		State.bServeCrossedNet = false;
	}

	bool SettleRally(FVolleyballRallyState& State)
	{
		if (State.bRallySettled) { return false; }
		State.bRallySettled = true;
		State.bBallInPlay = false;
		return true;
	}

	FString TouchLabel(const FVolleyballRallyState& State)
	{
		const TCHAR* Team = (State.PossessingTeam == EVolleyballTeam::TeamA) ? TEXT("A")
			: (State.PossessingTeam == EVolleyballTeam::TeamB) ? TEXT("B") : TEXT("-");
		return FString::Printf(TEXT("%s %d/3"), Team, FMath::Min(State.TouchCount, 3));
	}

	FString RallyReasonLabel(ERallyEndReason Reason)
	{
		switch (Reason)
		{
		case ERallyEndReason::BallIn:        return TEXT("界内");
		case ERallyEndReason::BallOut:       return TEXT("界外");
		case ERallyEndReason::FourTouches:   return TEXT("四次触球");
		case ERallyEndReason::DoubleTouch:   return TEXT("连续触球");
		case ERallyEndReason::ServeFault:    return TEXT("发球失误");
		case ERallyEndReason::Cancelled:     return TEXT("回合取消");
		default:                             return TEXT("回合结束");
		}
	}

	FString TouchTypeLabel(EBallTouchType Type)
	{
		switch (Type)
		{
		case EBallTouchType::Receive: return TEXT("Receive");
		case EBallTouchType::Set:     return TEXT("Set");
		case EBallTouchType::Attack:  return TEXT("Attack");
		case EBallTouchType::Serve:   return TEXT("Serve");
		case EBallTouchType::Block:   return TEXT("Block");
		default:                      return TEXT("Unknown");
		}
	}

	TArray<FVector> GetSlotFormationA()
	{
		// M11c-2: FIVB slot coordinates for Team A (X>0 side, facing -X). "Right"
		// for a team facing the net from +X is -Y; B mirrors both axes.
		return {
			FVector( 820.f, -280.f, 0.f),  // P1 back-right (serve slot)
			FVector( 150.f, -280.f, 0.f),  // P2 front-right
			FVector( 150.f,    0.f, 0.f),  // P3 front-middle
			FVector( 150.f,  280.f, 0.f),  // P4 front-left
			FVector( 820.f,  280.f, 0.f),  // P5 back-left
			FVector( 820.f,    0.f, 0.f),  // P6 back-middle
		};
	}
}
