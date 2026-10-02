// SPDX-License-Identifier: MIT
#include "Misc/AutomationTest.h"
#include "SpikeEliteCharacter.h"
#include "UI/TacticalHUDWidget.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSELimbAttachment, "SpikeElite.Tests.LimbAttachmentAndScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSELimbAttachment::RunTest(const FString& Parameters)
{
	ASpikeEliteCharacter* Character = GetMutableDefault<ASpikeEliteCharacter>();
	const USceneComponent* TorsoJoint = Cast<USceneComponent>(Character->GetDefaultSubobjectByName(TEXT("TorsoJoint")));
	const UStaticMeshComponent* Torso = Cast<UStaticMeshComponent>(Character->GetDefaultSubobjectByName(TEXT("Torso")));
	if (!TestNotNull(TEXT("Torso"), Torso) || !TestNotNull(TEXT("Torso joint"), TorsoJoint)) { return false; }
	for (const TCHAR* Name : {TEXT("ArmL"), TEXT("ArmR"), TEXT("LegL"), TEXT("LegR")})
	{
		auto Component = [Character, Name](const TCHAR* Suffix)
		{
			return Cast<USceneComponent>(Character->GetDefaultSubobjectByName(*FString::Printf(TEXT("%s%s"), Name, Suffix)));
		};
		const USceneComponent* Joint = Component(TEXT("Joint"));
		const USceneComponent* Bend = Component(TEXT("Bend"));
		const USceneComponent* Tip = Component(TEXT("Tip"));
		if (!TestNotNull(TEXT("Joint"), Joint) || !TestNotNull(TEXT("Bend"), Bend) || !TestNotNull(TEXT("Tip"), Tip)) { return false; }
		TestTrue(TEXT("Bend attaches to unscaled joint"), Bend->GetAttachParent() == Joint);
		TestTrue(TEXT("Tip attaches to unscaled bend"), Tip->GetAttachParent() == Bend);
		TestTrue(TEXT("Joint has unit scale"), Joint->GetRelativeScale3D().Equals(FVector::OneVector));
		TestTrue(TEXT("Bend has unit scale"), Bend->GetRelativeScale3D().Equals(FVector::OneVector));
		if (FString(Name).StartsWith(TEXT("Leg")))
		{
			TestTrue(TEXT("Hips follow torso lean"), Joint->GetAttachParent() == TorsoJoint);
			const float Bottom = Torso->GetRelativeLocation().Z - Torso->GetRelativeScale3D().Z * 50.f;
			TestTrue(TEXT("Hip starts at torso bottom (within 0.001 cm)"),
				FMath::IsNearlyEqual(static_cast<float>(Joint->GetRelativeLocation().Z), Bottom, 0.001f));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSETacticalClickIndex, "SpikeElite.Tests.TacticalButtonClickIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSETacticalClickIndex::RunTest(const FString& Parameters)
{
	UTacticalChoiceButton* Button = NewObject<UTacticalChoiceButton>();
	int32 Picked = INDEX_NONE;
	int32 Calls = 0;
	Button->OnChoiceSelected.AddLambda([&Picked, &Calls](int32 Index) { Picked = Index; ++Calls; });
	Button->InitChoice(13);
	Button->InitChoice(13); // rebuilding must not duplicate click delegates
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Last set-play row carries index 13"), Picked, 13);
	TestEqual(TEXT("One click dispatches once"), Calls, 1);
	Button->InitChoice(7);
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Last defense row carries index 7"), Picked, 7);
	TestEqual(TEXT("Second click dispatches once"), Calls, 2);
	return true;
}
#endif
