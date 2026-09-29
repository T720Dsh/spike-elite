// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolleyballCourt.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;
class USpotLightComponent;
class UPointLightComponent;

/**
 * Procedural FIVB indoor volleyball arena.
 *
 * Playing court: 18m x 9m.
 * Free zone: 3m around.
 * Net: top 243cm, band 100cm (bottom 143cm).
 * Enclosed indoor hall (walls + roof, no sky), stepped stands on four sides
 * with instanced head+torso crowd, and bright court lighting.
 */
UCLASS()
class SPIKEELITE_API AVolleyballCourt : public AActor
{
	GENERATED_BODY()

public:
	AVolleyballCourt();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float NetHeight = 243.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float NetBandHeight = 100.0f;       // net cloth height

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float NetOverhang = 80.0f;          // net extends past each sideline

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float HalfCourtWidth = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float HalfCourtLength = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float AttackLineOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float FreeZone = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Stands")
	int32 StandRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Stands")
	float StandStepHeight = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Stands")
	float StandStepDepth = 90.0f;

	/** Half extents (cm) of the enclosed hall interior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Hall")
	float HallHalfLength = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Hall")
	float HallHalfWidth = 1600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Hall")
	float HallHeight = 1200.0f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Root;

	// Floor layers
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CourtFloor;   // wood play area
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FreeZoneFloor; // surround
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ArenaFloor;    // outer concourse

	// Enclosed hall shell
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Roof;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallEndA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallEndB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallSideA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallSideB;

	// Net: instanced mesh grid (visual only, no physics) + opaque bands/posts
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> NetGrid;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetTopBand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetBottomBand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostLeft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostRight;

	// Lines
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> LinesRoot;

	// Stands & crowd (instanced)
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> StandSteps;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> CrowdHeads;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UInstancedStaticMeshComponent>> CrowdBodies; // one ISM per clothing colour

	// Court lighting (destroyed with the court; level lights are never touched)
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> CourtLightA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> CourtLightB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> FillLight;
	/** Dim corner fixtures that lift the stands/roof out of pure black. */
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UPointLightComponent>> PerimeterLights;

	UStaticMeshComponent* MakeLine(const TCHAR* Name, float X, float Y, float LenX, float LenY);
	void BuildFloor(UStaticMesh* Cube);
	void BuildHall(UStaticMesh* Cube);
	void BuildNet(UStaticMesh* Cube, UStaticMesh* Cyl);
	void BuildLighting();
	void BuildStands(UStaticMesh* Cube, UStaticMesh* Sphere);
	void PopulateNetGrid();
	void PopulateStands();

	/** Assign a wood material if the imported project asset exists, else tint. */
	void ApplyFloorMaterial(UStaticMeshComponent* Comp, const FLinearColor& Fallback);
};
