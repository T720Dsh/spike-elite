// SPDX-License-Identifier: MIT
#include "Misc/AutomationTest.h"
#include "SpikeEliteCharacter.h"
#include "UI/TacticalHUDWidget.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "SEArtGeometry.h"
#include "Volleyball/VolleyballArena.h"
#include "Volleyball/VolleyballCourt.h"
#include "Volleyball/MatchOfficialManager.h"

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEArtProfiles, "SpikeElite.Tests.ArtProfilesSharedFinite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEArtProfiles::RunTest(const FString& Parameters)
{
	for(auto P : {SEArtGeometry::EProfile::Torso,SEArtGeometry::EProfile::Head,
		SEArtGeometry::EProfile::UpperArm,SEArtGeometry::EProfile::Forearm,
		SEArtGeometry::EProfile::Thigh,SEArtGeometry::EProfile::Calf,
		SEArtGeometry::EProfile::Palm,SEArtGeometry::EProfile::Shoe,SEArtGeometry::EProfile::Seat})
	{
		UStaticMesh* M=SEArtGeometry::Get(P);
		if(!TestNotNull(TEXT("Built profile"),M)) return false;
		TestTrue(TEXT("Repeated requests share the same mesh"),M==SEArtGeometry::Get(P));
		TestTrue(TEXT("Finite positive bounds"),!M->GetBounds().Origin.ContainsNaN() && M->GetBounds().SphereRadius>0 && M->GetBounds().SphereRadius<120);
		TestEqual(TEXT("Runtime mesh has one LOD"),M->GetNumLODs(),1);
		TestTrue(TEXT("Packaged streaming UV metadata is initialized"),M->GetStaticMaterials()[0].UVChannelData.bInitialized);
		TestTrue(TEXT("Streaming density is positive and finite"),FMath::IsFinite(M->GetStaticMaterials()[0].UVChannelData.LocalUVDensities[0]) && M->GetStaticMaterials()[0].UVChannelData.LocalUVDensities[0]>0);
		TestTrue(TEXT("No expensive collision on visual profiles"),!M->GetBodySetup() || M->GetBodySetup()->AggGeom.GetElementCount()==0);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEArtDetails, "SpikeElite.Tests.ArtDetailsNoGameplayCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEArtDetails::RunTest(const FString& Parameters)
{
	auto* C=GetMutableDefault<ASpikeEliteCharacter>();
	TArray<UStaticMeshComponent*> Pieces; C->GetComponents(Pieces);
	int32 Faces=0,Fingers=0;
	for(auto* P : Pieces)
	{
		TestTrue(TEXT("Visible anatomy cannot block gameplay"),P->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
		if(P->ComponentHasTag(TEXT("FaceDetail"))) ++Faces;
		if(P->GetName().Contains(TEXT("Finger"))) ++Fingers;
	}
	TestTrue(TEXT("Facial detail exists"),Faces>=8);
	TestTrue(TEXT("Separate fingers exist"),Fingers>=8);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEFloorAuthority, "SpikeElite.Tests.VisibleFloorCollisionOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEFloorAuthority::RunTest(const FString& Parameters)
{
	for(AActor* A : {static_cast<AActor*>(GetMutableDefault<AVolleyballCourt>()),static_cast<AActor*>(GetMutableDefault<AVolleyballArena>()),static_cast<AActor*>(GetMutableDefault<AMatchOfficialManager>())})
		TestTrue(TEXT("Root is not an invisible raised slab"),Cast<UPrimitiveComponent>(A->GetRootComponent())->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	auto* Floor=Cast<UStaticMeshComponent>(GetMutableDefault<AVolleyballCourt>()->GetDefaultSubobjectByName(TEXT("CourtFloor")));
	if(!TestNotNull(TEXT("Visible playing surface"),Floor)) return false;
	TestTrue(TEXT("Floor carries collision"),Floor->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
	TestTrue(TEXT("Floor top is zero within 0.001 cm"),FMath::IsNearlyZero(Floor->GetRelativeLocation().Z+Floor->GetRelativeScale3D().Z*50.f,.001));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEWorkAisle, "SpikeElite.Tests.ArenaWorkAisleClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEWorkAisle::RunTest(const FString& Parameters)
{
	auto* A=GetMutableDefault<AVolleyballArena>();
	TestTrue(TEXT("Bench back remains outside first stand row"),A->StandClearanceY-A->StandStepDepth*.5f>1390.f);
	TestTrue(TEXT("All twelve rows fit side walls"),A->StandClearanceY+11*A->StandStepDepth+A->StandStepDepth*.5f<A->HallHalfWidth);
	TestTrue(TEXT("All twelve rows fit end walls"),A->StandClearanceX+11*A->StandStepDepth+A->StandStepDepth*.5f<A->HallHalfLength);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSEOfficialStand, "SpikeElite.Tests.RefereeStandAndScoreboardGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSEOfficialStand::RunTest(const FString& Parameters)
{
	auto* A=GetMutableDefault<AMatchOfficialManager>();
	auto Part=[&](const TCHAR* Name){return Cast<USceneComponent>(A->GetDefaultSubobjectByName(Name));};
	TestNotNull(TEXT("Ladder has actual rungs"),Part(TEXT("LadderRung5")));
	TestNotNull(TEXT("Tower has support legs"),Part(TEXT("TowerLeg11")));
	auto* Board=Part(TEXT("ScoreboardPanel"));
	if(!TestNotNull(TEXT("Physical board"),Board))return false;
	TestTrue(TEXT("Panel front faces court (+Y), thin Y axis"),Board->GetRelativeScale3D().Y<Board->GetRelativeScale3D().X);
	TestTrue(TEXT("Table outside free zone"),Board->GetRelativeLocation().Y<-950);
	return true;
}
#endif
