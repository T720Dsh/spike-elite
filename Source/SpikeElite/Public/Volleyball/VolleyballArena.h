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
 * (full play+free zone 31 x 19 m). Hall interior 64 x 56 m, roof 15 m high.
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
	float HallHalfLength = 3200.0f;
	UPROPERTY(EditAnywhere, Category = "Arena|Hall")
	float HallHalfWidth = 2800.0f;
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
	float StandClearanceX = 1800.0f; // free-zone plus circulation aisle
	UPROPERTY(EditAnywhere, Category = "Arena|Clearance")
	float StandClearanceY = 1550.0f; // officials/benches occupy a separate work aisle

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
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Seats;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SeatBacks;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrowdArms;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrowdThighs;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrowdShoes;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrowdHair;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Structure;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> LightFixtures;

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
