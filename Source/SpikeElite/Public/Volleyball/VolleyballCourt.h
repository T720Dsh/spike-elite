// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolleyballCourt.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInterface;
class UTexture2D;

/**
 * Procedurally-built FIVB indoor volleyball court.
 *
 * Dimensions (UE units = cm):
 *  - Playing court: 18 m x 9 m  (1800 x 900)
 *  - Free zone:     3 m on every side (visual only)
 *  - Net height:    2.43 m men's / 2.24 m women's (default 2.43)
 *  - Attack line:   3 m from center line on each side
 *
 * M1 upgrade:
 *  - Wood-tinted floor material (real Poly Haven wood_floor jpg dropped in
 *    Content/Textures/ — auto-imports on next editor open)
 *  - White boundary / center / attack lines as thin box strips
 *  - Net is a thin semi-transparent cloth plane (not a solid box)
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

	/** FIVB attack line is 3m from the net on each side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Court|Dimensions")
	float AttackLineOffset = 300.0f;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	/** Semi-transparent cloth net (a plane, not a solid box). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NetMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PostLeft;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PostRight;

	/** All white court line strips. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> LinesRoot;

	/** Build all geometry. Called in constructor / BeginPlay. */
	void BuildCourt();

	/** Helper: spawn a thin white box line strip. */
	class UStaticMeshComponent* MakeLine(const TCHAR* Name, float X, float Y, float ScaleX, float ScaleY);
};
