// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolleyballCourt.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Procedural FIVB indoor volleyball arena.
 *
 * Playing court: 18m x 9m.
 * Free zone: 3m around.
 * Net: top 243cm, band 100cm (bottom 143cm).
 * Stepped stands + instanced crowd beyond the free zone.
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

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Root;

	// Floor layers
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CourtFloor;   // wood play area
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FreeZoneFloor; // darker surround
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ArenaFloor;    // outer concrete

	// Net
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetTopBand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetBottomBand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostLeft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostRight;

	// Lines
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> LinesRoot;

	// Stands & crowd (instanced)
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> StandSteps;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Crowd;

	UStaticMeshComponent* MakeLine(const TCHAR* Name, float X, float Y, float LenX, float LenY);
	void BuildFloor(UStaticMesh* Cube);
	void BuildNet(UStaticMesh* Cube, UStaticMesh* Plane, UStaticMesh* Cyl);
	void BuildStands(UStaticMesh* Cube);
	void PopulateStands();
};
