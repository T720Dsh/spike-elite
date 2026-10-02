// SPDX-License-Identifier: MIT
#include "SpikeElitePlayerController.h"
#include "Volleyball/VolleyballBall.h"
#include "UI/MainMenuWidget.h"
#include "UI/PauseMenuWidget.h"
#include "UI/SettingsWidget.h"
#include "UI/ScoreboardWidget.h"
#include "UI/MatchEndWidget.h"
#include "UI/ConfirmWidget.h"
#include "UI/RotationWidget.h"
#include "UI/SEUiStyle.h"
#include "Engine/UserInterfaceSettings.h"
#include "UI/TacticalContactComponent.h"
#include "SpikeEliteGameMode.h"
#include "SpikeEliteCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogSEMenu, Log, All);

ASpikeElitePlayerController::ASpikeElitePlayerController()
{
	// All menus are built in pure C++ UMG; no Blueprint assets are required.
}

void ASpikeElitePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// M11b-4: tactical slow-motion shot planner (human only, runtime component).
	Tactical = NewObject<UTacticalContactComponent>(this);
	Tactical->RegisterComponent();
	// Under -devauto the automation flow is unattended; the tactical planning
	// UI must not freeze the world waiting for a human to pick a target.
	// -TacticalTest is the exception: it drives the REAL tactical UMG and
	// needs the planning windows ENABLED (mode 2 = all touches).
	if (FParse::Param(FCommandLine::Get(), TEXT("devauto")))
	{
		Tactical->TacticalMode = FParse::Param(FCommandLine::Get(), TEXT("TacticalTest")) ? 2 : 0;
	}

	// Load persisted sensitivity from GameUserSettings ini.
	float Saved = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("MouseSensitivity"), Saved, GGameUserSettingsIni);
	}
	MouseSensitivity = FMath::Clamp(Saved, MinSensitivity(), MaxSensitivity());
	float SavedUIScale = 1.f;
	bool bSavedReducedMotion = false;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("UIScale"), SavedUIScale, GGameUserSettingsIni);
		GConfig->GetBool(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("ReducedMotion"), bSavedReducedMotion, GGameUserSettingsIni);
	}
	GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = FMath::Clamp(SavedUIScale, 0.8f, 1.4f);
	SEUiStyle::SetReducedMotion(bSavedReducedMotion);
	UE_LOG(LogSEMenu, Log, TEXT("PC BeginPlay, sensitivity=%.2f, showing main menu"), MouseSensitivity);
	ShowMainMenu();

#if !UE_BUILD_SHIPPING
	// Headless verification hook: -devauto walks every menu and viewpoint,
	// screenshots each, then quits. Timers are unpausable so the pause-menu
	// shot and the final quit still fire while the world is paused.
	if (FParse::Param(FCommandLine::Get(), TEXT("devauto")))
	{
		GetWorldTimerManager().SetTimer(DevTimer, this, &ASpikeElitePlayerController::DevAutoStart, 1.2f, false);
	}
#endif
}

#if !UE_BUILD_SHIPPING
void ASpikeElitePlayerController::DevShot(const FString& Name)
{
	// Plain Screenshot defaults to bShowUI=false (3D only). "SHOWUI" makes it
	// composite Slate/UMG (menus) into the captured backbuffer.
	ConsoleCommand(FString::Printf(TEXT("Screenshot SHOWUI filename=%s"), *Name), true);
	UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: shot requested: %s"), *Name);
}

void ASpikeElitePlayerController::DevVerify(bool bCondition, const FString& Label)
{
	UE_LOG(LogSEMenu, Log, TEXT("DEV VERIFY: %s -> %s"),
		*Label, bCondition ? TEXT("PASS") : TEXT("FAIL"));
	if (!bCondition) { DevVerifyFailures++; }
}

void ASpikeElitePlayerController::DevView(const FVector& Loc, const FRotator& Rot)
{
	if (!DevCam)
	{
		DevCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform());
		if (ACameraActor* Cam = Cast<ACameraActor>(DevCam))
		{
			Cam->GetCameraComponent()->SetFieldOfView(78.f);
			// Acceptance cameras teleport between distant viewpoints. Disable
			// motion blur for this dev-only camera so the next-frame asynchronous
			// screenshot is evidence of the scene, not a smeared transition.
			FPostProcessSettings& PP = Cam->GetCameraComponent()->PostProcessSettings;
			PP.bOverride_MotionBlurAmount = true;
			PP.MotionBlurAmount = 0.f;
			PP.bOverride_MotionBlurMax = true;
			PP.MotionBlurMax = 0.f;
		}
	}
	if (DevCam)
	{
		DevCam->SetActorLocationAndRotation(Loc, Rot);
		SetViewTarget(DevCam);
	}
}

void ASpikeElitePlayerController::DevViewPlayer()
{
	if (APawn* P = GetPawn())
	{
		// Returning from a dev camera can leave the view pitched straight down;
		// restore a natural slightly-elevated third-person aim.
		SetControlRotation(FRotator(-12.f, GetControlRotation().Yaw, 0.f));
		SetViewTarget(P);
	}
}

void ASpikeElitePlayerController::DevAutoStart()
{
	// -ShotSuite: capture the M11c acceptance screenshot set from a real running
	// match (server behind end line, first receive, dive active/save, defense
	// panel, attacker run-up, ball close-up, officials, MatchOver).
	if (FParse::Param(FCommandLine::Get(), TEXT("ShotSuite")))
	{
		DevShotSuite();
		return;
	}

	// -PoseSuite: pin each procedural pose and capture front/side/back plus a
	// run-cycle frame strip (leg alternation evidence).
	if (FParse::Param(FCommandLine::Get(), TEXT("PoseSuite")))
	{
		DevPoseSuite();
		return;
	}

	// -RematchStress: five full rematch cycles with authoritative actor audits.
	if (FParse::Param(FCommandLine::Get(), TEXT("RematchStress")))
	{
		DevRematchStress();
		return;
	}

	// -TacticalTest: drive the real tactical UMG (planning -> cancel -> plan ->
	// confirm -> perfect-timing shot), screenshot each phase, verify the world
	// restore, then quit. It needs a REAL rally, so it owns its own ticker.
	if (FParse::Param(FCommandLine::Get(), TEXT("TacticalTest")))
	{
		DevTacticalTest();
		return;
	}

	// -QuickMatch: drive a full unattended quick match (menu -> match -> AI plays
	// until MatchOver -> screenshot the result screen -> quit).
	if (FParse::Param(FCommandLine::Get(), TEXT("QuickMatch")))
	{
		DevQuickMatch();
		return;
	}

	// UE 5.8 removed per-timer bPauseable, and a paused game world no longer
	// ticks its timer manager. Drive the whole sequence from the core ticker,
	// which keeps running while the game is paused, so the pause-menu shot and
	// the final quit still fire.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };

	// --- Front end (world is on the main menu) ---
	At(1.5f,  [this]() { DevShot(TEXT("shot_01_menu")); });
	At(2.4f,  [this]() { OpenSettingsFromMenu(); });
	At(3.4f,  [this]() { DevShot(TEXT("shot_02_settings")); });
	At(4.2f,  [this]() { CloseSettings(); });
	At(4.8f,  [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: starting match")); StartMatch(); });

	// --- Third-person style broadcast view of the full court ---
	At(9.6f,  [this]()
	{
		const FVector Loc(1250.f, 0.f, 430.f);
		const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(-120.f, 0.f, 170.f));
		DevView(Loc, Rot);
	});
	At(10.5f, [this]() { DevShot(TEXT("shot_03_court")); });

	// --- Side/oblique close-up of the net (shows top/bottom tapes & gap) ---
	At(11.4f, [this]()
	{
		const FVector Loc(360.f, -470.f, 200.f);
		const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(0.f, 0.f, 195.f));
		DevView(Loc, Rot);
	});
	At(12.3f, [this]() { DevShot(TEXT("shot_04_net")); });

	// --- High corner: stands, seating aisles and the roof ---
	At(13.2f, [this]()
	{
		const FVector Loc(1650.f, 1150.f, 880.f);
		const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(0.f, 0.f, 220.f));
		DevView(Loc, Rot);
	});
	At(14.1f, [this]() { DevShot(TEXT("shot_05_stands_roof")); });

	// --- Back to player, open the pause menu and shoot it ---
	At(15.0f, [this]() { DevViewPlayer(); });
	At(15.4f, [this]() { PauseGame(); });
	At(16.3f, [this]() { DevShot(TEXT("shot_06_pause")); });

	// --- M11 confirm-dialog cancel path (pause): open 返回主菜单 confirm, shoot
	// it, cancel, and verify the game stays PAUSED with the pause menu intact. ---
	At(17.0f, [this]()
	{
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: opening confirm from pause"));
		AskReturnToMainMenu();
	});
	At(17.4f, [this]() { DevShot(TEXT("shot_07_confirm_pause")); });
	At(17.8f, [this]()
	{
		CancelConfirm();
		DevVerify(MenuState == EMenuState::Paused && PauseMenu != nullptr,
			TEXT("confirm-cancel from Paused restores Paused (menu intact)"));
		DevVerify(GetWorld() && GetWorld()->IsPaused(),
			TEXT("game still paused after confirm-cancel from Paused"));
	});
	At(18.3f, [this]() { ResumeGame(); });

	// --- Lifecycle cycle test: menu -> match -> menu -> match (twice) to prove
	// cleanup leaves no duplicate court/ball/players and level lights survive.
	At(20.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: return to menu #1")); ReturnToMainMenu(); });
	At(22.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: start match #2")); StartMatch(); });
	At(26.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: return to menu #2")); ReturnToMainMenu(); });
	At(28.5f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: start match #3")); StartMatch(); });

	// --- M11 confirm from a LIVE match (Playing): open quit confirm mid-match,
	// cancel, and verify the match is NOT abandoned (stays Playing). ---
	At(30.8f, [this]()
	{
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: opening quit confirm mid-match"));
		AskQuitToDesktop();
	});
	At(31.2f, [this]() { DevShot(TEXT("shot_08_confirm_quit_match")); });
	At(31.6f, [this]()
	{
		CancelConfirm();
		DevVerify(MenuState == EMenuState::Playing,
			TEXT("confirm-cancel mid-match restores Playing (match not abandoned)"));
		DevVerify(!(GetWorld() && GetWorld()->IsPaused()),
			TEXT("world unpaused after confirm-cancel mid-match"));
	});

	// --- M11 main-menu quit confirm: cancel keeps us on the main menu. ---
	At(32.2f, [this]() { ReturnToMainMenu(); });
	At(32.7f, [this]()
	{
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: opening quit confirm from main menu"));
		AskQuitToDesktop();
	});
	At(33.1f, [this]() { DevShot(TEXT("shot_09_confirm_quit_menu")); });
	At(33.5f, [this]()
	{
		CancelConfirm();
		DevVerify(MenuState == EMenuState::MainMenu && MainMenu != nullptr,
			TEXT("confirm-cancel from main menu stays on main menu"));
	});
	At(34.0f, [this]()
	{
		UE_LOG(LogSEMenu, Log, TEXT("DEV AUTO: quitting (DevVerifyFailures=%d)"), DevVerifyFailures);
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	});

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double StartSeconds = FPlatformTime::Seconds();
	int32 Index = 0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, StartSeconds, Index, Events = MoveTemp(Events)](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}
		return Index < Events.Num();
	}));
}

void ASpikeElitePlayerController::DevRematchStress()
{
	// -RematchStress: unattended 5x rematch pressure test. Starts a match, waits
	// for each MatchOver, audits authoritative singletons/roster/actor counts via
	// the GameMode (PendingKill excluded), rematches, and repeats 5 times. Any
	// duplicated arena/court/ball/officials/rotation widget/scoreboard or roster
	// drift shows up in the [DevAudit] lines. Non-Shipping only.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };
	At(1.5f, [this]() { DevShot(TEXT("shot_rs_01_menu")); });
	At(4.0f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV REMATCH STRESS: starting run 1")); StartMatch(); });

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double StartSeconds = FPlatformTime::Seconds();
	int32 Index = 0;
	int32 Run = 1;
	float RunStart = -1.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, StartSeconds, Index, Events = MoveTemp(Events), Run, RunStart](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}

		ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(PC));
		if (!GM) { return true; }

		if (GM->MatchState == EMatchState::MatchOver)
		{
			if (RunStart < 0.f)
			{
				RunStart = static_cast<float>(Elapsed);
			}
			if (Elapsed - RunStart > 1.2f)
			{
				// One audit per finished run, then rematch (or quit after run 5).
				GM->DevAuditActors(Run);
				// Run 5: capture the final rematch result screen (MatchOver with
				// full set scores) as the acceptance screenshot for the stress loop.
				if (Run == 5)
				{
					PC->DevShot(TEXT("shot_rs_05_matchover"));
				}
				if (Run >= 5)
				{
					UE_LOG(LogSEMenu, Log, TEXT("DEV REMATCH STRESS: PASS — 5 runs audited, quitting"));
					PC->ConsoleCommand(TEXT("quit"));
					return false;
				}
				Run++;
				UE_LOG(LogSEMenu, Log, TEXT("DEV REMATCH STRESS: rematch run %d"), Run);
				PC->Rematch();
				RunStart = -1.f;
			}
		}
		return true;
	}));
}


void ASpikeElitePlayerController::DevPoseSuite()
{
	// -PoseSuite: pin each procedural pose and capture front/side/back plus a
	// run-cycle frame strip (leg alternation evidence). Non-Shipping only.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };

	struct FPoseEntry { EAnimPose Pose; const TCHAR* Name; };
	const FPoseEntry Poses[] = {
		{ EAnimPose::Idle,       TEXT("idle") },
		{ EAnimPose::Run,        TEXT("run") },
		{ EAnimPose::Jump,       TEXT("jump") },
		{ EAnimPose::Receive,    TEXT("receive") },
		{ EAnimPose::Set,        TEXT("set") },
		{ EAnimPose::Spike,      TEXT("spike") },
		{ EAnimPose::Block,      TEXT("block") },
		{ EAnimPose::Dive,       TEXT("dive") },
		{ EAnimPose::Recover,    TEXT("recover") },
		{ EAnimPose::Serve,      TEXT("serve") },
		{ EAnimPose::RaiseHands, TEXT("raisehands") },
	};

	auto GetPoseTarget = [this]() -> ASpikeEliteCharacter*
	{
		if (ASpikeEliteCharacter* P = Cast<ASpikeEliteCharacter>(GetPawn())) { return P; }
		if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			const TArray<TObjectPtr<ASpikeEliteCharacter>>& RosterA = GM->GetTeamPlayers(EVolleyballTeam::TeamA);
			if (RosterA.Num() > 0) { return RosterA[0]; }
		}
		return nullptr;
	};
	auto SetCamYaw = [this, GetPoseTarget](float Yaw)
	{
		if (ASpikeEliteCharacter* C = GetPoseTarget())
		{
			// Aim at the torso (head-height base), keep the camera slightly above
			// and ~3.3m out so the whole body, from feet to raised hands, fits.
			const FVector Torso = C->GetActorLocation() + FVector(0.f, 0.f, 100.f);
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			const FVector CamPos = Torso + Dir * 330.f + FVector(0.f, 0.f, 25.f);
			DevView(CamPos, (Torso - CamPos).Rotation());
		}
	};
	auto SetPose = [this, GetPoseTarget](EAnimPose Pose)
	{
		if (ASpikeEliteCharacter* C = GetPoseTarget())
		{
			// Park the subject at a fixed court spot with zero velocity so the
			// pinned pose is framed cleanly and nothing walks it around.
			C->SetActorLocation(FVector(450.f, 0.f, C->GetActorLocation().Z));
			// M11f-2: face +X so the yaw-0 camera sees the FRONT. The pawn rotates
			// via bUseControllerRotationYaw, so the controller yaw must be reset
			// too — Actor-only rotation is overwritten on the next frame.
			if (AController* Ctrl = C->GetController()) { Ctrl->SetControlRotation(FRotator(0.f, 0.f, 0.f)); }
			C->SetActorRotation(FRotator(0.f, 0.f, 0.f));
			if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
			{
				Move->Velocity = FVector::ZeroVector;
			}
			// Push the match ball far away so it never blocks the pose close-up.
			if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				if (AVolleyballBall* Ball = GM->GetBall())
				{
					Ball->SetActorLocation(FVector(99999.f, 0.f, 200.f));
				}
			}
			C->DevSetPoseOverride(Pose, true);
			C->SetThirdPersonArmLength(150.f);
		}
	};
	auto DevShot3D = [this](const FString& Name)
	{
		// Pure 3D capture (no HUD/banners) so the pose itself fills the shot.
		ConsoleCommand(FString::Printf(TEXT("Screenshot filename=%s"), *Name), true);
	};
	// M11f-2: numeric pose verification logged AT SCREENSHOT TIME (the pose has
	// fully blended by then — logging inside SetPose only captured the first
	// interpolation frame). Block/Set must read hands-over-head, Dive/Recover
	// must read low/grounded. Floor is Z≈0 (capsule half height 84).
	auto LogPose = [this, GetPoseTarget](const FPoseEntry& P)
	{
		if (ASpikeEliteCharacter* C = GetPoseTarget())
		{
			UE_LOG(LogSEMenu, Log, TEXT("DEV POSE %s: headZ=%.0f handLZ=%.0f handRZ=%.0f torsoZ=%.0f"),
				P.Name, C->GetHeadHeight(), C->GetHandWorldPosition(true).Z,
				C->GetHandWorldPosition(false).Z, C->GetTorsoJointHeight());
		}
	};

	At(1.0f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV POSE SUITE: starting match")); StartMatch(); });

	float T = 3.0f;
	for (const FPoseEntry& P : Poses)
	{
		At(T, [SetPose, P]() { SetPose(P.Pose); });
		T += 0.9f;   // M11f-2: let the pose fully blend (RInterpTo ≈ 94% at 0.35s,
		             // >99% at 0.9s) so the screenshot and DEV POSE numbers show
		             // the STABLE pose, not an interpolation frame.
		At(T, [this, SetCamYaw, DevShot3D, LogPose, P]() { SetCamYaw(0.f); LogPose(P); DevShot3D(FString::Printf(TEXT("shot_pose_%s_front"), P.Name)); });
		T += 0.35f;
		At(T, [this, SetCamYaw, DevShot3D, P]() { SetCamYaw(90.f); DevShot3D(FString::Printf(TEXT("shot_pose_%s_side"), P.Name)); });
		T += 0.35f;
		At(T, [this, SetCamYaw, DevShot3D, P]() { SetCamYaw(180.f); DevShot3D(FString::Printf(TEXT("shot_pose_%s_back"), P.Name)); });
		T += 0.2f;
	}

	// Run-cycle frame strip: pinned Run pose, advancing phase, fixed side view.
	// M11f-2: 8 frames × π/4 cover a FULL 0..2π gait cycle (the old 0.25 step
	// only reached 1.75 rad, missing the stride/contact phases).
	At(T, [SetPose, SetCamYaw]() { SetPose(EAnimPose::Run); SetCamYaw(90.f); });
	T += 0.25f;
	for (int32 f = 0; f < 8; f++)
	{
		At(T, [this, f, GetPoseTarget, DevShot3D]() {
			if (ASpikeEliteCharacter* C = GetPoseTarget())
			{
				C->DevSetRunPhase(static_cast<float>(f) * PI / 4.f);
			}
			DevShot3D(FString::Printf(TEXT("shot_pose_run_frame%d"), f));
		});
		T += 0.3f;
	}

	At(T, [this, GetPoseTarget]() {
		if (ASpikeEliteCharacter* C = GetPoseTarget())
		{
			C->DevSetPoseOverride(EAnimPose::Idle, false);
		}
		UE_LOG(LogSEMenu, Log, TEXT("DEV POSE SUITE: done, quitting"));
		ConsoleCommand(TEXT("quit"));
	});

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double StartTime = FPlatformTime::Seconds();
	int32 Index = 0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, StartTime, Events = MoveTemp(Events), Index = 0](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}
		return Index < Events.Num();
	}));
}

void ASpikeElitePlayerController::DevShotSuite()
{
	// -ShotSuite: capture the M11c acceptance screenshot set from a REAL running
	// match (never editor static viewport). Non-Shipping only.
	//  ss_00 menu; ss_01 server behind end line; ss_02 first receive after serve;
	//  ss_03 dive active pose; ss_04 dive save touch; ss_05 defense panel;
	//  ss_06 attacker run-up after a confirmed set; ss_07 ball close-up;
	//  ss_08 officials / scorer table; ss_09 MatchOver scoreboard.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };
	At(1.5f, [this]() { DevShot(TEXT("shot_ss_00_menu")); });
	At(4.0f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV SHOT SUITE: starting match")); StartMatch(); });

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double Start = FPlatformTime::Seconds();
	int32 Index = 0;
	const int32 Num = 10;
	uint32 DoneMask = 0;
	float RunupAt = -1.f;
	bool bRunupPendingShot = false;
	float Shot4At = -1.f;
	float DefenseShotAt = -1.f;
	bool bDefenseOpened = false;
	float ShotLastAt = -1.f;
	int32 Shot8Mask = 0;
	float RestoreViewAt = -1.f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, Start, Index, Events = MoveTemp(Events), Num, DoneMask = 0u, RunupAt, bRunupPendingShot,
		Shot4At = -1.f, DefenseShotAt = -1.f, bDefenseOpened = false, ShotLastAt = -1.f,
		Shot8Mask = 0, RestoreViewAt = -1.f, CancelTacticalAt = -1.f,
		RestoreDilationAt = -1.f, SavedShotDilation = 1.f,
		FallbackDiver = TWeakObjectPtr<ASpikeEliteCharacter>(), FallbackDiveAt = -1.f](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - Start;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}
		ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(PC));
		if (!GM) { return true; }

		// Keep the human useful: steer the pawn onto the ball on its own half
		// (same helper as -TacticalTest), so the human's team defends/attacks
		// normally and the AI dive/block scenes actually occur. Never poaches the
		// opponent half.
		{
			auto SteerHumanToBall = [PC, GM]()
			{
				if (!GM->GetBall() || !PC->GetPawn()) { return; }
				const FVector Bl = GM->GetBall()->GetActorLocation();
				const FVector Pl = PC->GetPawn()->GetActorLocation();
				ASpikeEliteCharacter* Char = Cast<ASpikeEliteCharacter>(PC->GetPawn());
				const float Side = Char ? Char->TeamSide : 1.f;
				if (Side * Bl.X < -30.f) { return; }
				if (FVector::Dist2D(Pl, Bl) > 240.f)
				{
					PC->GetPawn()->SetActorLocation(FVector(Bl.X, Bl.Y, 0.f));
				}
			};
			// Only when the tactical UI is fully closed (steering during a frozen
			// planning window would fight the world freeze).
			if (!PC->Tactical || PC->Tactical->State == ETacticalState::Normal)
			{
				SteerHumanToBall();
			}
		}

		// Screenshot capture is single-buffer and async (the frame AFTER the
		// request). Restoring the player view immediately would make every dev
		// camera capture the player view instead — restore ~0.25 s later.
		auto RestoreViewLater = [&RestoreViewAt, &Elapsed, PC]()
		{
			RestoreViewAt = static_cast<float>(Elapsed) + 0.25f;
		};
		if (RestoreViewAt >= 0.f && Elapsed >= static_cast<double>(RestoreViewAt))
		{
			PC->DevViewPlayer();
			RestoreViewAt = -1.f;
		}
		if (CancelTacticalAt >= 0.f && Elapsed >= static_cast<double>(CancelTacticalAt))
		{
			if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalArmed)
			{
				int32 Phase = -1;
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.9f, 0.f, true, false, false);
			}
			CancelTacticalAt = -1.f;
		}
		if (RestoreDilationAt >= 0.f && Elapsed >= static_cast<double>(RestoreDilationAt))
		{
			if (PC->GetWorld()) { PC->GetWorld()->GetWorldSettings()->TimeDilation = SavedShotDilation; }
			RestoreDilationAt = -1.f;
		}

		const auto Mark = [&DoneMask](int32 Bit) { DoneMask |= (1u << Bit); };
		const auto Done = [&DoneMask](int32 Bit) { return (DoneMask & (1u << Bit)) != 0; };
		// Screenshot requests are single-buffered: only one request per ~0.35 s,
		// otherwise a later request overwrites an earlier one before capture.
		const auto CanShot = [&ShotLastAt, Elapsed]()
		{
			return ShotLastAt < 0.f || static_cast<float>(Elapsed) - ShotLastAt > 0.35f;
		};
		const auto ShotDone = [&ShotLastAt, Elapsed]() { ShotLastAt = static_cast<float>(Elapsed); };
		const auto FindDiving = [GM]() -> ASpikeEliteCharacter*
		{
			for (const ASpikeEliteCharacter* C : GM->GetTeamPlayers(EVolleyballTeam::TeamA))
			{
				if (C && C->IsDiving()) { return const_cast<ASpikeEliteCharacter*>(C); }
			}
			for (const ASpikeEliteCharacter* C : GM->GetTeamPlayers(EVolleyballTeam::TeamB))
			{
				if (C && C->IsDiving()) { return const_cast<ASpikeEliteCharacter*>(C); }
			}
			return nullptr;
		};
		const auto FindRecovering = [GM]() -> ASpikeEliteCharacter*
		{
			for (const ASpikeEliteCharacter* C : GM->GetTeamPlayers(EVolleyballTeam::TeamA))
			{
				if (C && C->IsDiveRecovering() && C->WasDiveSaveRecorded()) { return const_cast<ASpikeEliteCharacter*>(C); }
			}
			for (const ASpikeEliteCharacter* C : GM->GetTeamPlayers(EVolleyballTeam::TeamB))
			{
				if (C && C->IsDiveRecovering() && C->WasDiveSaveRecorded()) { return const_cast<ASpikeEliteCharacter*>(C); }
			}
			return nullptr;
		};

		// 01: server behind the end line, standing in the service zone. Captured
		// while the match is in a service phase (or up to 25 s as a fallback).
		if (!Done(1) && CanShot() && (GM->MatchState == EMatchState::ServiceAuthorized
			|| GM->MatchState == EMatchState::ServingToss || Elapsed > 25.0))
		{
			const FVector Loc(1500.f, -70.f, 230.f);
			const FRotator Rot = UKismetMathLibrary::FindLookAtRotation(Loc, FVector(400.f, 0.f, 160.f));
			PC->DevView(Loc, Rot);
			PC->DevShot(TEXT("shot_ss_01_server"));
			Mark(1);
			ShotDone();
			RestoreViewLater();
		}

		// 02: first receive after a legal serve (TouchCount==1, type Receive).
		// Natural capture when the rally provides it; deterministic fallback
		// after 20 s pins the Receive pose on a defender so the suite can
		// never stall on this shot forever.
		if (!Done(2) && CanShot() && GM->IsRallyLive())
		{
			const bool bRealReceive = GM->GetTouchCount() == 1
				&& GM->GetLastTouchType() == EBallTouchType::Receive
				&& GM->GetServeCrossedNet();
			ASpikeEliteCharacter* Rec = nullptr;
			int32 PIdx = -1;
			if (bRealReceive)
			{
				PIdx = GM->GetLastTouchPlayerIndex();
				const EVolleyballTeam LastTeam = GM->GetLastTouchTeam();
				const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = GM->GetTeamPlayers(LastTeam);
				if (Roster.IsValidIndex(PIdx)) { Rec = Roster[PIdx]; }
			}
			else if (Elapsed > 20.0)
			{
				// Deterministic fallback: pin a Receive pose on the defending
				// team's first player so the shot is reproducible even if the
				// fast rally window was sampled past.
				const EVolleyballTeam DefTeam = GM->GetPossessingTeam();
				const TArray<TObjectPtr<ASpikeEliteCharacter>>& Roster = GM->GetTeamPlayers(DefTeam);
				if (Roster.Num() > 0)
				{
					Rec = Roster[0];
					Rec->DevSetPoseOverride(EAnimPose::Receive, true);
					PIdx = 0;
					UE_LOG(LogSEMenu, Log, TEXT("DEV SHOT SUITE: first-receive fallback (team=%d)"),
						(int32)DefTeam);
				}
			}
			if (Rec)
			{
				const FVector L = Rec->GetActorLocation();
				const FVector Cam = L + FVector(-260.f, 190.f, 150.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, L + FVector(0.f, 0.f, 120.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_02_first_receive"));
				Mark(2);
				ShotDone();
				RestoreViewLater();
			}
		}

		// 03: DiveActive pose (visible lunge, extended reach).
		if (!Done(3) && CanShot())
		{
			if (ASpikeEliteCharacter* D = FindDiving())
			{
				const FVector L = D->GetActorLocation();
				const FVector Cam = L + FVector(-240.f, 170.f, 120.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, L + FVector(0.f, 0.f, 60.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_03_dive_active"));
				Mark(3);
				ShotDone();
				RestoreViewLater();
			}
		}

		// 04: DiveSave aftermath — a player in DiveRecovery right after a save.
		// The last-touch type flips to Set within ~70 ms once the setter plays the
		// ball, so this only requires the recovery pose itself (the save itself was
		// already logged: [DiveSave] ... type=Receive).
		if (!Done(4) && CanShot())
		{
			if (ASpikeEliteCharacter* D = FindRecovering())
			{
				const FVector L = D->GetActorLocation();
				const FVector Cam = L + FVector(-200.f, 150.f, 110.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, L + FVector(0.f, 0.f, 50.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_04_dive_save"));
				Mark(4);
				Shot4At = static_cast<float>(Elapsed);
				ShotDone();
				RestoreViewLater();
			}
		}

		// 05: defense planning panel. Only forced AFTER the dive shots are done so
		// the forced freeze can never kill the natural AI dive window. The
		// screenshot is captured asynchronously one frame after the request, so
		// OPEN -> wait -> SHOT -> wait -> CONFIRM keeps the panel in frame.
		if (!Done(5) && Done(3) && Done(4) && GM->IsRallyLive())
		{
			const EVolleyballTeam Poss = GM->GetPossessingTeam();
			ASpikeEliteCharacter* Human = Cast<ASpikeEliteCharacter>(PC->GetPawn());
			const bool bOppBallNearNet = (Poss != EVolleyballTeam::None && Human)
				&& (Poss != Human->GetTeam())
				&& GM->GetTouchCount() >= 2
				&& FMath::Abs(GM->GetBall()->GetActorLocation().X) < 700.f
				&& GM->GetBall()->GetVelocity().Size() > 350.f;
			// Prefer a natural opposing attack window, but do not let a short
			// deterministic match omit this acceptance frame entirely.  Once the
			// verified dive is captured, a 0.35 s fallback opens the same production
			// defense planner while the rally is still live.
			const bool bDefenseFallbackReady = Shot4At >= 0.f && Elapsed - Shot4At > 0.35f;
			if (!bDefenseOpened && (bOppBallNearNet || bDefenseFallbackReady) && PC->Tactical
				&& PC->Tactical->State == ETacticalState::Normal)
			{
				PC->Tactical->DevForceDefensePlanning();
				bDefenseOpened = true;
				DefenseShotAt = static_cast<float>(Elapsed);
			}
			if (bDefenseOpened && CanShot() && DefenseShotAt >= 0.f && Elapsed - DefenseShotAt > 0.25f)
			{
				PC->DevShot(TEXT("shot_ss_05_defense"));
				Mark(5);
				DefenseShotAt = static_cast<float>(Elapsed);
				ShotDone();
			}
			if (bDefenseOpened && DefenseShotAt >= 0.f && Elapsed - DefenseShotAt > 0.9f
				&& PC->Tactical && PC->Tactical->State == ETacticalState::DefensePlanning)
			{
				PC->Tactical->DevConfirmDefense();
				bDefenseOpened = false;
			}
		}

		// 06: attacker run-up after a confirmed set play. Deferred until the dive
		// shots are done so the slow-motion armed phase cannot swallow the first
		// rally's net-cross window.
		if (!Done(6) && CanShot() && Done(3) && Done(4) && Done(5)
			&& Shot4At >= 0.f && Elapsed - Shot4At > 0.6f)
		{
			if (RunupAt < 0.f && PC->Tactical && PC->Tactical->State == ETacticalState::Normal && GM->IsRallyLive())
			{
				// Open a SET planning window through the production path, confirm it
				// (GameMode registers the play and sends the attacker to the run-up),
				// then cancel AFTER the run-up frame has been captured.
				PC->Tactical->DevForcePlanning(EBallTouchType::Set);
				if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalPlanning)
				{
					int32 Phase = -1;
					PC->Tactical->DevTacticalStep(Phase, 2, FVector::ZeroVector, 0.9f, 0.9f, false, true, false);
					RunupAt = static_cast<float>(Elapsed);
					bRunupPendingShot = true;
				}
			}
			if (bRunupPendingShot && RunupAt > 0.f && Elapsed - RunupAt > 0.9f)
			{
				// Point the camera at the play's attacker if one was selected.
				const FVector Bl = GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector;
				const FVector Cam = Bl + FVector(-330.f, 220.f, 190.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, Bl + FVector(0.f, 0.f, 120.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_06_attacker_runup"));
				Mark(6);
				ShotDone();
				// Screenshot capture is asynchronous.  Leave the armed overlay alive
				// for one frame, then dismiss it before the ball/official photographs.
				CancelTacticalAt = static_cast<float>(Elapsed) + 0.25f;
				RestoreViewLater();
			}
		}

		// 07: ball close-up (yellow/blue un-branded match ball). Freeze the live
		// rally to 1% speed for the asynchronous next-frame capture, then restore
		// the exact prior dilation. This keeps the real ball and real match state
		// while preventing the camera from chasing a 10+ m/s projectile.
		AVolleyballBall* ShotBall = GM->GetBall();
		bool bBallClearOfPlayers = ShotBall != nullptr;
		if (ShotBall)
		{
			const FVector BallLocation = ShotBall->GetActorLocation();
			for (EVolleyballTeam Team : { EVolleyballTeam::TeamA, EVolleyballTeam::TeamB })
			{
				for (const TObjectPtr<ASpikeEliteCharacter>& Player : GM->GetTeamPlayers(Team))
				{
					if (Player && FVector::DistSquared(Player->GetActorLocation(), BallLocation) < FMath::Square(150.f))
					{
						bBallClearOfPlayers = false;
						break;
					}
				}
				if (!bBallClearOfPlayers) { break; }
			}
		}
		if (!Done(7) && CanShot() && Done(6) && ShotBall && GM->IsRallyLive() && bBallClearOfPlayers)
		{
			const FVector Bl = ShotBall->GetActorLocation();
			SavedShotDilation = PC->GetWorld()->GetWorldSettings()->TimeDilation;
			PC->GetWorld()->GetWorldSettings()->TimeDilation = 0.01f;
			RestoreDilationAt = static_cast<float>(Elapsed) + 0.3f;
			FVector ViewOffset = FVector::CrossProduct(ShotBall->GetVelocity().GetSafeNormal(), FVector::UpVector);
			if (ViewOffset.IsNearlyZero()) { ViewOffset = FVector::YAxisVector; }
			const FVector Cam = Bl + ViewOffset.GetSafeNormal() * 45.f + FVector(0.f, 0.f, 5.f);
			const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, Bl);
			PC->DevView(Cam, R);
			PC->DevShot(TEXT("shot_ss_07_ball"));
			Mark(7);
			ShotDone();
			RestoreViewLater();
		}

		// 08: officials — three captures: 1st referee stand (+Y net end), 2nd
		// referee (-Y net end), scorer table + physical scoreboard (X=1750).
		// Each capture waits its turn on the single-buffer screenshot gate.
		if (!Done(8) && CanShot() && Done(7) && GM->GetOfficials())
		{
			const FVector CourtY = FVector(0.f, 0.f, 0.f);
			if ((Shot8Mask & 1) == 0)
			{
				// Side approach from the +X/+Y corner, head-level, so the net post
				// does not occlude the referee standing on the platform.
				const FVector Cam(-300.f, 860.f, 210.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, FVector(0.f, 720.f, 320.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_08_ref1"));
				Shot8Mask |= 1;
				ShotDone();
			}
			else if ((Shot8Mask & 2) == 0)
			{
				const FVector Cam(-300.f, -860.f, 200.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, FVector(0.f, -720.f, 100.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_08_ref2"));
				Shot8Mask |= 2;
				ShotDone();
			}
			else if ((Shot8Mask & 4) == 0)
			{
				// Scorer table + physical scoreboard: tight low angle from the side
				// so the table, scorer and scoreboard text all fit the frame.
				// The scoreboard text faces the negative-Y aisle.  Photograph it
				// from that side so the glyphs are readable instead of mirrored.
				const FVector Cam(1600.f, -300.f, 130.f);
				const FRotator R = UKismetMathLibrary::FindLookAtRotation(Cam, FVector(1750.f, 0.f, 110.f));
				PC->DevView(Cam, R);
				PC->DevShot(TEXT("shot_ss_08_scorer"));
				Shot8Mask |= 4;
				ShotDone();
			}
			else
			{
				Mark(8);
				RestoreViewLater();
			}
		}

		// Fallback: if no natural AI dive happened within 8 s, drive the closest
		// defender through the PRODUCTION dive directive (SetAIDirective +
		// the character's own Dive state machine — StartDive/EnterActive/record),
		// re-asserted every tick (bPrimary) until the pose is captured. The rally
		// is still fully legal: the bot really lunges and really touches the ball.
		if (!Done(4) && Elapsed > 8.0 && GM->IsRallyLive())
		{
			AVolleyballBall* Ball = GM->GetBall();
			const FVector Landing = Ball ? Ball->GetActorLocation() + Ball->GetVelocity() * 0.5f
				: FVector::ZeroVector;
			ASpikeEliteCharacter* ActiveFallback = FallbackDiver.Get();
			if (ActiveFallback && ActiveFallback->DiveState.Phase == SEVolleyballRules::FVolleyballDiveState::EPhase::None)
			{
				FallbackDiver.Reset();
				ActiveFallback = nullptr;
			}
			if (!ActiveFallback && (FallbackDiveAt < 0.f || Elapsed - FallbackDiveAt > 1.5f))
			{
				const EVolleyballTeam DefTeam = GM->GetPossessingTeam();
				ASpikeEliteCharacter* Best = nullptr;
				float BestDist = TNumericLimits<float>::Max();
				for (const ASpikeEliteCharacter* C : GM->GetTeamPlayers(DefTeam))
				{
					if (C && C->bIsBot
						&& C->DiveState.Phase == SEVolleyballRules::FVolleyballDiveState::EPhase::None)
					{
						const float D = FVector::Dist2D(C->GetActorLocation(), Landing);
						if (D < BestDist) { BestDist = D; Best = const_cast<ASpikeEliteCharacter*>(C); }
					}
				}
				if (Best)
				{
					FallbackDiver = Best;
					FallbackDiveAt = static_cast<float>(Elapsed);
					ActiveFallback = Best;
					UE_LOG(LogSEMenu, Log, TEXT("DEV SHOT SUITE: fallback dive directive team=%d -> %s"),
						(int32)DefTeam, *Best->GetName());
				}
			}
			if (ActiveFallback && !ActiveFallback->IsDiveRecovering())
			{
				GM->SetAIDirective(ActiveFallback, EAIBehavior::Dive, Landing, true);
			}
		}

		// 09: MatchOver with full set scores.
		if (!Done(9) && CanShot() && GM->MatchState == EMatchState::MatchOver)
		{
			PC->DevViewPlayer();
			PC->DevShot(TEXT("shot_ss_09_matchover"));
			Mark(9);
			ShotDone();
		}

		if (Done(1) && Done(2) && Done(3) && Done(4) && Done(5) && Done(6) && Done(7) && Done(8) && Done(9)
			&& ShotLastAt >= 0.f && Elapsed - ShotLastAt > 1.0)
		{
			// Give the async screenshot for the last shot time to flush to disk.
			UE_LOG(LogSEMenu, Log, TEXT("DEV SHOT SUITE: all %d shot groups captured, quitting"), Num - 1);
			PC->ConsoleCommand(TEXT("quit"));
			return false;
		}
		if (Elapsed > 420.0)
		{
			UE_LOG(LogSEMenu, Log, TEXT("DEV SHOT SUITE: timeout (mask=%u), quitting"), DoneMask);
			PC->ConsoleCommand(TEXT("quit"));
			return false;
		}
		return true;
	}));
}

void ASpikeElitePlayerController::DevQuickMatch()
{
	// Unattended -QuickMatch: menu shot, start the match, then poll until
	// MatchOver, screenshot the result screen, exercise the confirm dialog
	// cancel path from MatchOver (Esc-equivalent must return to MatchOver),
	// click 再来一场 (Rematch), play a second full match, screenshot again,
	// then quit. Serve timeouts (under -devauto) let the AI finish matches
	// without keyboard input.
	struct FDevEvent { float Delay; TFunction<void()> Fn; };
	TArray<FDevEvent> Events;
	auto At = [&Events](float Delay, TFunction<void()> Fn) { Events.Add(FDevEvent{ Delay, MoveTemp(Fn) }); };

	At(1.5f, [this]() { DevShot(TEXT("shot_qm_01_menu")); });
	At(4.0f, [this]() { UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: starting")); StartMatch(); });

	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double StartSeconds = FPlatformTime::Seconds();
	int32 Index = 0;
	float ShotAt = -1.0f;
	float ConfirmAt = -1.0f;
	float ConfirmShotAt = -1.0f;
	float RematchAt = -1.0f;
	float ShotRematch = -1.0f;
	float RallyShotAt = -1.0f;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, StartSeconds, Index, Events = MoveTemp(Events), ShotAt, ConfirmAt, ConfirmShotAt, RematchAt, ShotRematch, RallyShotAt, bCloseupShotPending = false](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
		while (Index < Events.Num() && Elapsed >= static_cast<double>(Events[Index].Delay))
		{
			if (Events[Index].Fn) { Events[Index].Fn(); }
			++Index;
		}

		if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(PC)))
		{
			// M11b-2: one live-rally screenshot to verify the rotation HUD and
			// officials are actually rendered during play (not just logged).
			// M11b-3: -Closeup shortens the spring arm (and disables motion blur)
			// so the articulated body is clearly visible; the shot fires once the
			// camera has settled, ~0.5s later.
			if (GM->MatchState == EMatchState::Rally && RallyShotAt < 0.0f)
			{
				RallyShotAt = static_cast<float>(Elapsed);
				if (FParse::Param(FCommandLine::Get(), TEXT("Closeup")))
				{
					if (ASpikeEliteCharacter* C = Cast<ASpikeEliteCharacter>(PC->GetPawn()))
					{
						C->SetThirdPersonArmLength(140.f);
					}
					PC->ConsoleCommand(TEXT("r.MotionBlur.Max 0"));
					PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
					bCloseupShotPending = true;
				}
				else
				{
					UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: rally in progress, shooting rotation HUD"));
					PC->DevShot(TEXT("shot_qm_05_rally"));
				}
			}
			if (bCloseupShotPending && (Elapsed - RallyShotAt) > 0.5f)
			{
				bCloseupShotPending = false;
				UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: rally in progress (closeup), shooting rotation HUD"));
				PC->DevShot(TEXT("shot_qm_05_rally"));
			}
			if (GM->MatchState == EMatchState::MatchOver)
			{
				if (ShotAt < 0.0f)
				{
					ShotAt = static_cast<float>(Elapsed);
					UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: MatchOver reached (winner A=%d B=%d), shooting result screen"),
						GM->TeamASetsWon, GM->TeamBSetsWon);
					PC->DevShot(TEXT("shot_qm_02_matchover"));
				}
				// M11: open the 返回主菜单 confirm from the result screen, shoot
				// it, cancel, and verify we are still on MatchOver (never revived
				// a finished match behind the dialog).
				if (ShotAt > 0.0f && ConfirmAt < 0.0f && (Elapsed - ShotAt) > 1.0f)
				{
					ConfirmAt = static_cast<float>(Elapsed);
					UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: opening confirm from MatchOver"));
					PC->AskReturnToMainMenu();
				}
				if (ConfirmAt > 0.0f && ConfirmShotAt < 0.0f && (Elapsed - ConfirmAt) > 0.5f)
				{
					ConfirmShotAt = static_cast<float>(Elapsed);
					PC->DevShot(TEXT("shot_qm_03_confirm_matchover"));
				}
				if (ConfirmShotAt > 0.0f && RematchAt < 0.0f && (Elapsed - ConfirmShotAt) > 0.8f)
				{
					RematchAt = static_cast<float>(Elapsed);
					PC->CancelConfirm();
					PC->DevVerify(PC->MenuState == EMenuState::MatchOver && PC->MatchEnd != nullptr,
						TEXT("confirm-cancel from MatchOver restores MatchOver"));
					PC->DevVerify(PC->GetWorld() && PC->GetWorld()->IsPaused(),
						TEXT("world stays paused after confirm-cancel from MatchOver"));
					UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: clicking 再来一场 (Rematch)"));
					PC->Rematch();
				}
				// Second MatchOver (RematchAt>0 means we already rematched once).
				if (RematchAt > 0.0f && ShotRematch < 0.0f && (Elapsed - RematchAt) > 3.0f)
				{
					ShotRematch = static_cast<float>(Elapsed);
					UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: second MatchOver, shooting final screen"));
					PC->DevShot(TEXT("shot_qm_04_rematch_over"));
				}
				if (ShotRematch > 0.0f && (Elapsed - ShotRematch) > 3.0f)
				{
					UE_LOG(LogSEMenu, Log, TEXT("DEV QUICK MATCH: quitting (DevVerifyFailures=%d)"), PC->DevVerifyFailures);
					UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
					return false;
				}
			}
			// No MatchOver yet: if the rematch happened but the state left
			// MatchOver (it always does via StartMatch), we simply wait for the
			// second MatchOver above. Guard the whole sequence with a timeout.
		}

		if (Elapsed > 300.0)
		{
			UE_LOG(LogSEMenu, Error, TEXT("DEV QUICK MATCH: timed out waiting for MatchOver/Rematch"));
			UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
			return false;
		}
		return true;
	}));
}

void ASpikeElitePlayerController::DevTacticalTest()
{
	UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: starting (real rally -> tactical UMG -> cancel -> plan x2 types -> perfect shots)"));
	// Stage machine:
	//  0 menu shot + start match
	//  1 wait window #1 -> CANCEL path verification (screenshot UI + restore)
	//  2 wait window #2 (ball-steering keeps the human near the ball) -> pick
	//    tactic/aim -> screenshot -> confirm -> Armed
	//  3 wait Armed -> screenshot -> execute perfect-timing shot
	//  4 verify a real shot fired (ball moving) -> screenshot
	//  5 second touch type (the one NOT seen in #1): steering + fallback
	//    DevForcePlanning -> pick -> screenshot -> confirm -> armed -> shot
	//  6 verify second shot -> screenshot -> PASS -> quit
	//  9 FAIL
	struct FStage
	{
		int32 Stage = 0;
		double T3 = 0;
		double T4 = 0;
		double T5 = 0;
		double T6 = 0;
		FVector BallPos = FVector::ZeroVector;
		bool bWin1WasSet = false;
		bool bSteerActive = false;
		bool bSecondForced = false;
	};
	TWeakObjectPtr<ASpikeElitePlayerController> Weak(this);
	const double Start = FPlatformTime::Seconds();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Weak, Start, St = FStage()](float) mutable -> bool
	{
		ASpikeElitePlayerController* PC = Weak.Get();
		if (!PC) { return false; }
		ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(PC));
		if (!GM) { return true; }
		const double Elapsed = FPlatformTime::Seconds() - Start;

		// Ball steering: while waiting for a real contact window, keep the human
		// under the ball so the AI touch selection picks the human and the
		// window actually opens (devauto is allowed to move the human pawn).
		// The human only follows balls on THEIR OWN half (TeamSide), so devauto
		// never teleports the A-side human into the B-side half to poach the
		// opponent's rally — that would corrupt the pending touch type.
		auto SteerHumanToBall = [PC, GM]()
		{
			if (!GM->GetBall() || !PC->GetPawn()) { return; }
			const FVector Bl = GM->GetBall()->GetActorLocation();
			const FVector Pl = PC->GetPawn()->GetActorLocation();
			ASpikeEliteCharacter* Char = Cast<ASpikeEliteCharacter>(PC->GetPawn());
			const float Side = Char ? Char->TeamSide : 1.f;
			if (Side * Bl.X < -30.f) { return; } // ball on the other side of the net
			if (FVector::Dist2D(Pl, Bl) > 240.f)
			{
				PC->GetPawn()->SetActorLocation(FVector(Bl.X, Bl.Y, 0.f));
			}
		};

		switch (St.Stage)
		{
		case 0:
			if (Elapsed >= 1.5)
			{
				PC->DevShot(TEXT("shot_tac_01_menu"));
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: starting match"));
				PC->StartMatch();
				St.Stage = 1;
			}
			break;
		case 1: // wait for planning window #1 -> cancel path verification
			if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalPlanning)
			{
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: planning window #1 (cancel path)"));
				int32 Phase = -1;
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.8f, 0.f, false, false, false);
				PC->DevShot(TEXT("shot_tac_02_planning"));
				St.bWin1WasSet = (PC->Tactical->GetPendingTouchType() == EBallTouchType::Set);
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.8f, 0.f, true, false, false);
				const bool bCancelOk = (PC->Tactical->State == ETacticalState::Normal)
					&& (PC->GetWorld()->GetWorldSettings()->TimeDilation == 1.f);
				PC->DevVerify(bCancelOk, TEXT("TacticalTest cancel restores Normal + TimeDilation=1"));
				PC->DevVerify(!PC->Tactical->IsDefensePlanning(), TEXT("TacticalTest cancel leaves defense planning closed"));
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: window #1 type=%s cancelled"),
					St.bWin1WasSet ? TEXT("Set") : TEXT("Attack"));
				PC->DevShot(TEXT("shot_tac_03_cancel"));
				St.Stage = 2;
				St.T3 = Elapsed;
			}
			else if (Elapsed > 90.0)
			{
				PC->DevVerify(false, TEXT("TacticalTest timed out waiting for window #1"));
				St.Stage = 9;
			}
			break;
		case 2: // wait window #2 -> pick + confirm (steering active)
			SteerHumanToBall();
			if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalPlanning)
			{
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: planning window #2 (pick + confirm)"));
				int32 Phase = -1;
				const bool bSet = (PC->Tactical->GetPendingTouchType() == EBallTouchType::Set);
				// Set -> 副攻近体快 (index 2); Attack -> aimed landing into court.
				PC->Tactical->DevTacticalStep(Phase, bSet ? 2 : -1,
					bSet ? FVector::ZeroVector : FVector(-220.f, 160.f, 0.f),
					0.9f, 0.9f, false, false, false);
				PC->DevShot(TEXT("shot_tac_04_plan2"));
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.9f, 0.f, false, true, false);
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: window #2 type=%s confirmed -> Armed"), bSet ? TEXT("Set") : TEXT("Attack"));
				St.T4 = Elapsed;
				St.Stage = 3;
			}
			else if (Elapsed - St.T3 > 120.0)
			{
				// Steering failed -> force the second type directly.
				const EBallTouchType Forced = St.bWin1WasSet ? EBallTouchType::Attack : EBallTouchType::Set;
				PC->Tactical->DevForcePlanning(Forced);
				St.bSecondForced = true;
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: steering timeout -> forced planning type=%s"),
					Forced == EBallTouchType::Set ? TEXT("Set") : TEXT("Attack"));
				St.Stage = 2; // re-enter the same stage; the window is now open
				St.T3 = Elapsed; // reset the steering timeout
			}
			break;
		case 3: // armed -> screenshot -> execute perfect-timing shot
			if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalArmed && (Elapsed - St.T4) > 0.4)
			{
				PC->DevShot(TEXT("shot_tac_05_armed"));
				int32 Phase = -1;
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.f, 0.f, false, false, true);
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: perfect-timing shot #1 executed"));
				St.T5 = Elapsed;
				St.BallPos = GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector;
				St.Stage = 4;
			}
			else if (Elapsed - St.T4 > 60.0)
			{
				PC->DevVerify(false, TEXT("TacticalTest armed-phase timeout"));
				St.Stage = 9;
			}
			break;
		case 4: // shot #1 must leave tactical and the ball must actually move
			{
				const FVector BNow = GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector;
				const float Moved = FVector::Dist(BNow, St.BallPos);
				if ((Elapsed - St.T5) > 0.4 && Moved > 30.f)
				{
					PC->DevVerify(true, TEXT("TacticalTest shot #1 was a real shot (ball moved)"));
					PC->DevShot(TEXT("shot_tac_06_impact"));
					St.Stage = 5;
					St.T6 = Elapsed;
				}
				else if (Elapsed - St.T5 > 60.0)
				{
					PC->DevVerify(false, TEXT("TacticalTest shot #1 execution timeout"));
					St.Stage = 9;
				}
			}
			break;
		case 5: // second touch type: MUST be a Set window (13+1 tactic picker).
			SteerHumanToBall();
			if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalPlanning)
			{
				const bool bSet = (PC->Tactical->GetPendingTouchType() == EBallTouchType::Set);
				if (!bSet)
				{
					// A real Attack window arrived first — cancel it and force the
					// Set planning window through the production EnterPlanning path.
					UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: window #3 was Attack, forcing Set planning"));
					int32 Phase = -1;
					PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.8f, 0.f, true, false, false);
					PC->Tactical->DevForcePlanning(EBallTouchType::Set);
				}
				UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: planning window #3 (type=Set)"));
				int32 Phase = -1;
				PC->Tactical->DevTacticalStep(Phase, 2, FVector::ZeroVector, 0.9f, 0.9f, false, false, false);
				// Wait a beat so the screenshot captures the 13+1 set list before
				// confirming into Armed.
				St.T6 = Elapsed;
				St.Stage = 50;
			}
			else if (Elapsed - St.T6 > 120.0 && !St.bSecondForced)
			{
				const EBallTouchType Forced = St.bWin1WasSet ? EBallTouchType::Attack : EBallTouchType::Set;
				PC->Tactical->DevForcePlanning(Forced);
				St.bSecondForced = true;
				St.T6 = Elapsed;
			}
			break;
		case 50: // second-type planning screenshot -> confirm -> Armed
			if (Elapsed - St.T6 > 0.5)
			{
				// Screenshot capture is ASYNC — fire it, then wait a beat BEFORE
				// confirming so the frame that gets captured is still the set
				// planning panel (13+1 list), not the Armed attack panel.
				PC->DevShot(TEXT("shot_tac_07_plan3"));
				St.T6 = Elapsed;
				St.Stage = 51;
			}
			break;
		case 51:
			if (Elapsed - St.T6 > 0.8)
			{
				int32 Phase = -1;
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.9f, 0.f, false, true, false);
				St.T6 = Elapsed;
				St.Stage = 6;
			}
			break;
		case 6: // armed -> execute shot #2 -> verify
			if (PC->Tactical && PC->Tactical->State == ETacticalState::TacticalArmed && (Elapsed - St.T6) > 0.4)
			{
				PC->DevShot(TEXT("shot_tac_08_armed2"));
				int32 Phase = -1;
				PC->Tactical->DevTacticalStep(Phase, 0, FVector::ZeroVector, 0.f, 0.f, false, false, true);
				St.T6 = Elapsed;
				St.BallPos = GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector;
				St.Stage = 7;
			}
			else if (Elapsed - St.T6 > 60.0)
			{
				PC->DevVerify(false, TEXT("TacticalTest armed-phase #2 timeout"));
				St.Stage = 9;
			}
			break;
		case 7:
			{
				const FVector BNow = GM->GetBall() ? GM->GetBall()->GetActorLocation() : FVector::ZeroVector;
				const float Moved = FVector::Dist(BNow, St.BallPos);
				if ((Elapsed - St.T6) > 0.4 && Moved > 30.f)
				{
					PC->DevVerify(true, TEXT("TacticalTest shot #2 was a real shot (ball moved)"));
					PC->DevShot(TEXT("shot_tac_09_impact2"));
					UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: PASS (failures=%d)"), PC->DevVerifyFailures);
					UE_LOG(LogSEMenu, Log, TEXT("DEV TACTICAL TEST: quitting"));
					PC->ConsoleCommand(TEXT("quit"));
					return false;
				}
				if (Elapsed - St.T6 > 60.0)
				{
					PC->DevVerify(false, TEXT("TacticalTest shot #2 execution timeout"));
					St.Stage = 9;
				}
			}
			break;
		case 9:
			UE_LOG(LogSEMenu, Error, TEXT("DEV TACTICAL TEST: FAIL (failures=%d)"), PC->DevVerifyFailures);
			PC->ConsoleCommand(TEXT("quit"));
			return false;
		}
		return true;
	}));
}
#endif

void ASpikeElitePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Esc must keep working while the game is paused, so enable it there too.
	FInputActionBinding& PauseBind =
		InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &ASpikeElitePlayerController::OnPausePressed);
	PauseBind.bExecuteWhenPaused = true;

	// H toggles the right-top rotation HUD (gameplay only).
	InputComponent->BindAction(TEXT("ToggleRotation"), IE_Pressed, this, &ASpikeElitePlayerController::OnToggleRotation);
}

void ASpikeElitePlayerController::OnToggleRotation()
{
	if (ASpikeEliteGameMode* GM = GetWorld()->GetAuthGameMode<ASpikeEliteGameMode>())
	{
		if (GM->GetRotationWidget()) { GM->GetRotationWidget()->ToggleVisible(); }
	}
}

void ASpikeElitePlayerController::OnPausePressed()
{
	// M11b-4: while the tactical planning UI is open, Esc cancels the tactical
	// shot instead of opening the pause menu.
	if (Tactical && Tactical->State == ETacticalState::TacticalPlanning)
	{
		Tactical->CancelShot();
		return;
	}

	switch (MenuState)
	{
	case EMenuState::Playing:
		PauseGame();
		break;
	case EMenuState::Paused:
		ResumeGame();
		break;
	case EMenuState::SettingsFromPause:
		CloseSettings();   // returns to the pause menu
		break;
	case EMenuState::SettingsFromMenu:
		CloseSettings();   // returns to the main menu
		break;
	case EMenuState::Confirm:
		CancelConfirm();   // Esc backs out of the confirm dialog
		break;
	case EMenuState::MatchOver:
	default:
		// M10: on the result screen Esc deliberately does NOTHING — it must
		// never revive a finished match. Use the on-screen buttons.
		break;
	}
}

void ASpikeElitePlayerController::SetGameInputMode()
{
	// GameOnly defaults to capturing the mouse permanently for look control.
	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	bShowMouseCursor = false;
	bEnableMouseOverEvents = false;
	bEnableClickEvents = false;

	if (UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		VC->SetMouseLockMode(EMouseLockMode::LockOnCapture);
	}
	SetPause(false);
}

void ASpikeElitePlayerController::SetUIInputMode(UUserWidget* FocusWidget)
{
	// GameAndUI keeps the Esc action routed to this controller (UIOnly does not
	// reliably deliver keyboard input) while the game world is paused so that
	// movement / hit actions cannot leak through.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	if (FocusWidget)
	{
		Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	SetInputMode(Mode);

	bShowMouseCursor = true;
	bEnableMouseOverEvents = true;
	bEnableClickEvents = true;

	if (UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		VC->SetMouseLockMode(EMouseLockMode::DoNotLock);
	}

	// Release any held capture so the cursor can leave the window immediately.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		Slate.ReleaseAllPointerCapture();
	}
}

void ASpikeElitePlayerController::HideAllMenus()
{
	if (MainMenu)     { MainMenu->RemoveFromParent();     MainMenu = nullptr; }
	if (PauseMenu)    { PauseMenu->RemoveFromParent();    PauseMenu = nullptr; }
	if (SettingsMenu) { SettingsMenu->RemoveFromParent(); SettingsMenu = nullptr; }
	if (MatchEnd)     { MatchEnd->RemoveFromParent();     MatchEnd = nullptr; }
	if (Confirm)      { Confirm->RemoveFromParent();      Confirm = nullptr; }
	PendingConfirmAction = nullptr;
}

void ASpikeElitePlayerController::ShowMainMenu()
{
	UE_LOG(LogSEMenu, Log, TEXT("ShowMainMenu"));
	HideAllMenus();
	MenuState = EMenuState::MainMenu;
	SetPause(false);

	MainMenu = CreateWidget<UMainMenuWidget>(this);
	if (MainMenu)
	{
		MainMenu->OnStart.BindUObject(this, &ASpikeElitePlayerController::StartMatch);
		MainMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromMenu);
		// M11: the main menu "退出游戏" button also goes through the confirm dialog
		// so a stray click cannot kill the process while a match is in progress.
		MainMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::AskQuitToDesktop);
		MainMenu->AddToViewport(10);
		SetUIInputMode(MainMenu);
		MainMenu->SetInitialFocus();   // M11f-3: focus 开始比赛 after the menu is live
		UE_LOG(LogSEMenu, Log, TEXT("MainMenu added. InViewport=%s"),
			MainMenu->IsInViewport() ? TEXT("yes") : TEXT("no"));
	}
	else
	{
		UE_LOG(LogSEMenu, Error, TEXT("MainMenu CreateWidget returned null"));
	}
}

void ASpikeElitePlayerController::StartMatch()
{
	HideAllMenus();
	MenuState = EMenuState::Playing;
	SetGameInputMode();
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StartMatch();
	}
}

void ASpikeElitePlayerController::OnMatchStarted(UScoreboardWidget* InScoreboard)
{
	Scoreboard = InScoreboard;
}

void ASpikeElitePlayerController::BuildPauseMenu()
{
	PauseMenu = CreateWidget<UPauseMenuWidget>(this);
	if (PauseMenu)
	{
		PauseMenu->OnResume.BindUObject(this, &ASpikeElitePlayerController::ResumeGame);
		PauseMenu->OnSettings.BindUObject(this, &ASpikeElitePlayerController::OpenSettingsFromPause);
		PauseMenu->OnMainMenu.BindUObject(this, &ASpikeElitePlayerController::AskReturnToMainMenu);
		PauseMenu->OnQuit.BindUObject(this, &ASpikeElitePlayerController::AskQuitToDesktop);
		PauseMenu->AddToViewport(20);
	}
}

void ASpikeElitePlayerController::PauseGame()
{
	if (MenuState != EMenuState::Playing) return;
	MenuState = EMenuState::Paused;
	SetPause(true);
	BuildPauseMenu();
	SetUIInputMode(PauseMenu);
	PauseMenu->SetInitialFocus();   // M11f-3: focus 继续游戏
}

void ASpikeElitePlayerController::ResumeGame()
{
	if (PauseMenu) { PauseMenu->RemoveFromParent(); PauseMenu = nullptr; }
	MenuState = EMenuState::Playing;
	SetPause(false);
	SetGameInputMode();
}

void ASpikeElitePlayerController::BuildMatchEnd()
{
	MatchEnd = CreateWidget<UMatchEndWidget>(this);
	if (MatchEnd)
	{
		MatchEnd->OnRematch.BindUObject(this, &ASpikeElitePlayerController::Rematch);
		MatchEnd->OnMainMenu.BindUObject(this, &ASpikeElitePlayerController::AskReturnToMainMenu);
		MatchEnd->OnQuit.BindUObject(this, &ASpikeElitePlayerController::AskQuitToDesktop);
		MatchEnd->AddToViewport(20);
	}
}

void ASpikeElitePlayerController::OnMatchOver(const TArray<int32>& ScoresA, const TArray<int32>& ScoresB, EVolleyballTeam Winner)
{
	UE_LOG(LogSEMenu, Log, TEXT("Match over: winner=%s"), Winner == EVolleyballTeam::TeamA ? TEXT("A") : Winner == EVolleyballTeam::TeamB ? TEXT("B") : TEXT("-"));
	// M11f-2: stable broadcast end-of-match view. Point the player camera at the
	// court centre from a slight elevation instead of leaving the last rally's
	// tight framing (player back or scorer text filling the result background).
	if (APawn* P = GetPawn())
	{
		const FVector Me = P->GetActorLocation();
		const FRotator Look = (FVector(0.f, 0.f, 120.f) - Me).Rotation();
		SetControlRotation(FRotator(-14.f, Look.Yaw, 0.f));
	}
	// M11 input gate: FREEZE the world. Without this, character input, bot Tick
	// directives and the ball's projectile all kept running behind the result
	// screen even though MatchState was MatchOver. UI stays fully interactive
	// (buttons/mouse/keyboard are driven by this controller, not the world tick).
	SetPause(true);
	MenuState = EMenuState::MatchOver;
	// The result card is a final broadcast state, not another modal stacked on
	// top of live match instrumentation.  Hiding these also prevents a stale
	// score/rotation readout from competing with the final per-set result.
	if (Scoreboard) { Scoreboard->SetVisibility(ESlateVisibility::Collapsed); }
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (GM->GetRotationWidget()) { GM->GetRotationWidget()->SetVisibility(ESlateVisibility::Collapsed); }
	}
	BuildMatchEnd();
	if (MatchEnd) { MatchEnd->SetResult(Winner, ScoresA, ScoresB); }
	SetUIInputMode(MatchEnd);
	MatchEnd->SetInitialFocus();   // M11f-3
}

void ASpikeElitePlayerController::Rematch()
{
	UE_LOG(LogSEMenu, Log, TEXT("Rematch requested"));
	if (MatchEnd) { MatchEnd->RemoveFromParent(); MatchEnd = nullptr; }
	HideAllMenus();
	MenuState = EMenuState::Playing;
	SetGameInputMode();
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StartMatch();   // StartMatch cleans up the finished match first.
	}
}

void ASpikeElitePlayerController::ShowConfirm(const FString& Message, EMenuState RestoreState, TFunction<void()> Action)
{
	// M11: never stack a second dialog. If one is already up, just refocus it
	// (the caller was a duplicate "返回主菜单"/"退出" press).
	if (Confirm)
	{
		SetUIInputMode(Confirm);
		Confirm->SetInitialFocus();   // M11f-3: duplicate press just refocuses 取消
		return;
	}

	StateBeforeConfirm = RestoreState;
	PendingConfirmAction = MoveTemp(Action);

	Confirm = CreateWidget<UConfirmWidget>(this);
	if (Confirm)
	{
		Confirm->SetMessage(Message);
		Confirm->OnConfirm.BindUObject(this, &ASpikeElitePlayerController::AcceptConfirm);
		Confirm->OnCancel.BindUObject(this, &ASpikeElitePlayerController::CancelConfirm);
		Confirm->AddToViewport(40);
		// M11: the confirm dialog BECOMES the active state. Esc then routes to
		// CancelConfirm instead of accidentally resuming the game, and the pause/
		// result screens underneath can never be operated behind the dialog.
		MenuState = EMenuState::Confirm;
		SetUIInputMode(Confirm);
		Confirm->SetInitialFocus();   // M11f-3: safe default = 取消
	}
}

void ASpikeElitePlayerController::AcceptConfirm()
{
	if (Confirm) { Confirm->RemoveFromParent(); Confirm = nullptr; }
	TFunction<void()> Action = MoveTemp(PendingConfirmAction);
	PendingConfirmAction = nullptr;
	if (Action) { Action(); }
}

void ASpikeElitePlayerController::CancelConfirm()
{
	if (Confirm) { Confirm->RemoveFromParent(); Confirm = nullptr; }
	PendingConfirmAction = nullptr;

	// M11: restore exactly the state that opened the dialog — including Playing
	// (a confirm opened mid-match and cancelled must NOT kick the player back to
	// the main menu) — and rebuild the target widget if it was lost somehow.
	if (StateBeforeConfirm == EMenuState::Paused && PauseMenu)
	{
		MenuState = EMenuState::Paused;
		SetPause(true);
		SetUIInputMode(PauseMenu);
		PauseMenu->SetInitialFocus();   // M11f-3
	}
	else if (StateBeforeConfirm == EMenuState::MatchOver && MatchEnd)
	{
		MenuState = EMenuState::MatchOver;
		SetPause(true);
		SetUIInputMode(MatchEnd);
		MatchEnd->SetInitialFocus();   // M11f-3
	}
	else if (StateBeforeConfirm == EMenuState::Playing)
	{
		MenuState = EMenuState::Playing;
		SetPause(false);
		SetGameInputMode();
	}
	else
	{
		MenuState = EMenuState::MainMenu;
		SetPause(false);
		if (MainMenu) { SetUIInputMode(MainMenu); MainMenu->SetInitialFocus(); }   // M11f-3
		else { ShowMainMenu(); }
	}
}

void ASpikeElitePlayerController::AskReturnToMainMenu()
{
	ShowConfirm(TEXT("返回主菜单？未结束的比赛进度将丢失。"), MenuState,
		[this]() { LeaveToMainMenu(); });
}

void ASpikeElitePlayerController::AskQuitToDesktop()
{
	ShowConfirm(TEXT("确定退出游戏？"), MenuState,
		[this]() { QuitNow(); });
}

void ASpikeElitePlayerController::LeaveToMainMenu()
{
	SetPause(false);
	if (ASpikeEliteGameMode* GM = Cast<ASpikeEliteGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ReturnToMainMenu();
	}
	Scoreboard = nullptr;   // GameMode destroyed its scoreboard widget.
	if (MatchEnd) { MatchEnd->RemoveFromParent(); MatchEnd = nullptr; }
	ShowMainMenu();
}

void ASpikeElitePlayerController::ReturnToMainMenu()
{
	// Direct version used by dev automation and lifecycle tests (no dialog).
	LeaveToMainMenu();
}

void ASpikeElitePlayerController::QuitToDesktop()
{
	UE_LOG(LogSEMenu, Log, TEXT("Quit to desktop requested"));
	QuitNow();
}

void ASpikeElitePlayerController::QuitNow()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ASpikeElitePlayerController::OpenSettingsFromMenu()
{
	if (MainMenu) MainMenu->RemoveFromParent();
	MainMenu = nullptr;
	MenuState = EMenuState::SettingsFromMenu;

	SettingsMenu = CreateWidget<USettingsWidget>(this);
	if (SettingsMenu)
	{
		SettingsMenu->InitFromCurrentSettings();
		SettingsMenu->SetCurrentSensitivity(MouseSensitivity);
		SettingsMenu->OnBack.BindUObject(this, &ASpikeElitePlayerController::CloseSettings);
		SettingsMenu->OnApply.BindUObject(this, &ASpikeElitePlayerController::SaveSettings);
		SettingsMenu->OnSensitivityChanged.BindUObject(this, &ASpikeElitePlayerController::SetMouseSensitivity);
		SettingsMenu->AddToViewport(30);
		SetUIInputMode(SettingsMenu);
		SettingsMenu->SetInitialFocus();   // M11f-3: safe default = 返回
	}
}

void ASpikeElitePlayerController::OpenSettingsFromPause()
{
	if (PauseMenu) PauseMenu->RemoveFromParent();
	PauseMenu = nullptr;
	MenuState = EMenuState::SettingsFromPause;

	SettingsMenu = CreateWidget<USettingsWidget>(this);
	if (SettingsMenu)
	{
		SettingsMenu->InitFromCurrentSettings();
		SettingsMenu->SetCurrentSensitivity(MouseSensitivity);
		SettingsMenu->OnBack.BindUObject(this, &ASpikeElitePlayerController::CloseSettings);
		SettingsMenu->OnApply.BindUObject(this, &ASpikeElitePlayerController::SaveSettings);
		SettingsMenu->OnSensitivityChanged.BindUObject(this, &ASpikeElitePlayerController::SetMouseSensitivity);
		SettingsMenu->AddToViewport(30);
		SetUIInputMode(SettingsMenu);
		SettingsMenu->SetInitialFocus();   // M11f-3: safe default = 返回
	}
}

void ASpikeElitePlayerController::CloseSettings()
{
	if (SettingsMenu) { SettingsMenu->RemoveFromParent(); SettingsMenu = nullptr; }

	if (MenuState == EMenuState::SettingsFromMenu)
	{
		ShowMainMenu();
	}
	else if (MenuState == EMenuState::SettingsFromPause)
	{
		MenuState = EMenuState::Paused;
		SetPause(true);
		BuildPauseMenu();
		SetUIInputMode(PauseMenu);
		PauseMenu->SetInitialFocus();   // M11f-3
	}
}

void ASpikeElitePlayerController::SaveSettings()
{
	if (GConfig)
	{
		GConfig->SetFloat(TEXT("/Script/SpikeElite.SpikeEliteSettings"), TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
