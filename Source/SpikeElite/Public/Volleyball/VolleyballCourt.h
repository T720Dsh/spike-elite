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
 * The playing court — floor, free zone, all lines, net, net posts and the
 * short service lines. This object is persistent across Rematch; only its
 * match state (handled by the GameMode) is reset.
 *
 * Arena shell (hall/stands/lighting/LED boards) lives in AVolleyballArena.
 *
 * Playing court: 18 m x 9 m (X = -900..+900, Y = -450..+450), line width 5 cm,
 * centre line X=0, attack lines X=±300. Lines are IN.
 * Free zone: 5 m past each sideline, 6.5 m past each end line
 * (full play+free area 31 x 19 m). Net top 243 cm, band 100 cm (bottom 143 cm).
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

	/** Free zone: distance past the sidelines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float SideFreeZone = 500.0f;

	/** Free zone: distance past the end lines (service zone depth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float EndFreeZone = 650.0f;

	/** Full play+free zone half extents. */
	float GetFreeZoneHalfLength() const { return HalfCourtLength + EndFreeZone; } // 1550
	float GetFreeZoneHalfWidth() const  { return HalfCourtWidth  + SideFreeZone; } // 950

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Root;

	// Floor layers (court-owned; the arena owns the outer concourse)
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CourtFloor;   // wood play area
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FreeZoneFloor; // sport surround

	// Net: instanced mesh grid (visual only) + opaque bands/posts
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> NetGrid;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetTopBand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> NetBottomBand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostLeft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PostRight;

	// Lines
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> LinesRoot;

	UStaticMeshComponent* MakeLine(const TCHAR* Name, float X, float Y, float LenX, float LenY);
	void BuildFloor(UStaticMesh* Cube);
	void BuildNet(UStaticMesh* Cube, UStaticMesh* Cyl);
	void BuildServeShortLines(UStaticMesh* Cube);
	void PopulateNetGrid();

	/** Assign a wood material if the imported project asset exists, else tint. */
	void ApplyFloorMaterial(UStaticMeshComponent* Comp, const FLinearColor& Fallback);
};
