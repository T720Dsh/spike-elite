// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolleyballArena.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class USkyLightComponent;

/**
 * Persistent arena shell — hall walls/roof, stepped stands with instanced
 * crowd, court lighting and LED boards. This is a map-level persistent object:
 * it is created once and is NEVER rebuilt or destroyed by Rematch.
 *
 * Court-sized objects (floor, free zone, lines, net, posts) live in
 * AVolleyballCourt, which is also persistent and only has its match state
 * reset between matches.
 *
 * Playing area: 18 x 9 m. Free zone: 5 m from sidelines, 6.5 m from end lines
 * (full play+free zone 31 x 19 m). Hall interior ~60 x 44 m, roof 15 m high.
 */
UCLASS()
class SPIKEELITE_API AVolleyballArena : public AActor
{
	GENERATED_BODY()

public:
	AVolleyballArena();
	virtual void BeginPlay() override;

	/** Hall interior half extents (cm). Walls sit at +/- these values. */
	UPROPERTY(EditAnywhere, Category = "Arena|Hall")
	float HallHalfLength = 3000.0f;
	UPROPERTY(EditAnywhere, Category = "Arena|Hall")
	float HallHalfWidth = 2200.0f;
	UPROPERTY(EditAnywhere, Category = "Arena|Hall")
	float HallHeight = 1500.0f;

	/** Stands. */
	UPROPERTY(EditAnywhere, Category = "Arena|Stands")
	int32 StandRows = 10;
	UPROPERTY(EditAnywhere, Category = "Arena|Stands")
	float StandStepHeight = 45.0f;
	UPROPERTY(EditAnywhere, Category = "Arena|Stands")
	float StandStepDepth = 90.0f;

	/** Free-zone clearance so no stand/board intrudes into the play+free zone. */
	UPROPERTY(EditAnywhere, Category = "Arena|Clearance")
	float StandClearanceX = 1750.0f; // beyond end-line free zone (1550) + buffer
	UPROPERTY(EditAnywhere, Category = "Arena|Clearance")
	float StandClearanceY = 1150.0f; // beyond sideline free zone (950) + buffer

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Root;

	// Hall shell
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Roof;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallEndA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallEndB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallSideA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WallSideB;

	// Stands & crowd (instanced)
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> StandSteps;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> CrowdHeads;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> CrowdLegs;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UInstancedStaticMeshComponent>> CrowdBodies;

	// Railings + LED boards (instanced thin slabs)
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Railings;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> LedBoards;

	// Lighting
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> CourtLightA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> CourtLightB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> FillLight;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UPointLightComponent>> PerimeterLights;

	void BuildHall(UStaticMesh* Cube);
	void BuildLighting();
	void BuildStands(UStaticMesh* Cube);
	void PopulateStands();
};
