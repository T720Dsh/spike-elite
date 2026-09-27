// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolleyballCourt.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

/**
 * Procedurally-built FIVB indoor volleyball court.
 *
 * Dimensions (UE units = cm):
 *  - Playing court: 18 m x 9 m  (1800 x 900)
 *  - Free zone:     3 m on every side (visual only)
 *  - Net height:    2.43 m men's / 2.24 m women's (default 2.43)
 *  - Antenna:       80 cm above net
 *
 * M0: all geometry is simple boxes/planes with engine-default materials.
 * M1: swap in a textured floor mesh and a proper net texture.
 */
UCLASS()
class SPIKEELITE_API AVolleyballCourt : public AActor
{
	GENERATED_BODY()

public:
	AVolleyballCourt();

	virtual void BeginPlay() override;

	/** Net height in cm (men's 243, women's 224). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float NetHeight = 243.0f;

	/** Half of the 9m court width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float HalfCourtWidth = 450.0f;

	/** Half of the 18m court length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float HalfCourtLength = 900.0f;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NetMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PostLeft;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PostRight;

	/** Build all geometry. Called in BeginPlay. */
	void BuildCourt();
};
