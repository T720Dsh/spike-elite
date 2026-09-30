// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TrajectoryPreviewComponent.generated.h"

class UStaticMeshComponent;

/**
 * Pooled dotted trajectory preview ("Angry Birds style") used by the tactical
 * contact system. Shows the predicted flight for the current shot intent with
 * a landing marker; green = legal in-bounds over-net, yellow = near the net or
 * the boundary, red = out/net-touch. Points are pooled once and only re-laid
 * out when the input changes — never spawned per frame.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SPIKEELITE_API UTrajectoryPreviewComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTrajectoryPreviewComponent();

	/** Recompute and lay out the dotted flight for the given intent. */
	void ShowPreview(const FVector& Start, const FVector& InitialVelocity);

	/** Hide all pooled dots and the landing marker. */
	void HidePreview();

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Dots;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LandingMarker;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DotMID;

	int32 MaxDots = 15;
};
