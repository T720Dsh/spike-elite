// SPDX-License-Identifier: MIT
#include "UI/TrajectoryPreviewComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SEMaterials.h"
#include "Volleyball/VolleyballTrajectory.h"

UTrajectoryPreviewComponent::UTrajectoryPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTrajectoryPreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	// Runtime load (BeginPlay is outside any constructor, so ConstructorHelpers
	// object finders are not allowed here).
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	const FLinearColor White(1.f, 1.f, 1.f, 1.f);

	// Pool the dots once.
	for (int32 i = 0; i < MaxDots; ++i)
	{
		UStaticMeshComponent* Dot = NewObject<UStaticMeshComponent>(this);
		Dot->SetupAttachment(this);
		Dot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dot->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.07f));
		if (SphereMesh) { Dot->SetStaticMesh(SphereMesh); }
		UMaterialInstanceDynamic* MID = SEMaterials::MakeTint(this, White);
		if (MID) { Dot->SetMaterial(0, MID); DotMID = MID; }
		Dot->RegisterComponent();
		Dot->SetVisibility(false);
		Dots.Add(Dot);
	}

	LandingMarker = NewObject<UStaticMeshComponent>(this);
	LandingMarker->SetupAttachment(this);
	LandingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LandingMarker->SetRelativeScale3D(FVector(0.5f, 0.9f, 0.04f));
	if (CylinderMesh) { LandingMarker->SetStaticMesh(CylinderMesh); }
	if (UMaterialInstanceDynamic* MID = SEMaterials::MakeTint(this, FLinearColor(0.2f, 1.f, 0.3f)))
	{
		LandingMarker->SetMaterial(0, MID);
	}
	LandingMarker->RegisterComponent();
	LandingMarker->SetVisibility(false);
}

void UTrajectoryPreviewComponent::ShowPreview(const FVector& Start, const FVector& InitialVelocity)
{
	const SEVolleyballTrajectory::FTrajectoryResult Result = SEVolleyballTrajectory::Predict(Start, InitialVelocity);

	// Color: green = legal over-net in-bounds, yellow = near net/boundary,
	// red = net touch / out / invalid.
	FLinearColor Color = FLinearColor(0.25f, 0.9f, 0.3f);
	if (Result.bNetTouch || !Result.bValid) { Color = FLinearColor(0.95f, 0.15f, 0.15f); }
	else if (!Result.bInBounds) { Color = FLinearColor(0.95f, 0.15f, 0.15f); }
	else if (!Result.bCrossedNet) { Color = FLinearColor(0.95f, 0.65f, 0.1f); }
	else if (FMath::Abs(Result.Landing.X) > SEVolleyballTrajectory::CourtHalfLength - 60.f
		|| FMath::Abs(Result.Landing.Y) > SEVolleyballTrajectory::CourtHalfWidth - 40.f)
	{
		Color = FLinearColor(0.95f, 0.65f, 0.1f);
	}

	if (DotMID)
	{
		DotMID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// Lay dots along the sampled points (skip some for readability).
	const int32 Total = Result.Points.Num();
	const int32 Count = FMath::Min(MaxDots, Total);
	int32 ShowIdx = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Idx = (Total > 1) ? (i * (Total - 1)) / FMath::Max(1, Count - 1) : 0;
		ShowIdx = i;
		if (Dots.IsValidIndex(i) && Result.Points.IsValidIndex(Idx))
		{
			Dots[i]->SetWorldLocation(Result.Points[Idx].Location);
			Dots[i]->SetVisibility(true);
		}
	}
	for (int32 i = ShowIdx + 1; i < Dots.Num(); ++i)
	{
		if (Dots[i]) Dots[i]->SetVisibility(false);
	}

	// Landing marker.
	if (LandingMarker)
	{
		LandingMarker->SetWorldLocation(FVector(Result.Landing.X, Result.Landing.Y, SEVolleyballTrajectory::GroundZ + 2.f));
		LandingMarker->SetVisibility(true);
	}
}

void UTrajectoryPreviewComponent::HidePreview()
{
	for (auto& Dot : Dots)
	{
		if (Dot) Dot->SetVisibility(false);
	}
	if (LandingMarker) LandingMarker->SetVisibility(false);
}

