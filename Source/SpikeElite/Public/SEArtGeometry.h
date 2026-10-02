// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
class UStaticMesh;

/** Original, shared, UV-mapped art meshes. Generated once per profile, not per actor/tick.
 * Normalized bounds are approximately 100cm; instance scale specifies physical dimensions.
 * No physics bodies: production capsules and court colliders remain authoritative. */
namespace SEArtGeometry
{
	enum class EProfile : uint8 { Torso, Head, UpperArm, Forearm, Thigh, Calf, Palm, Shoe, Seat };
	SPIKEELITE_API UStaticMesh* Get(EProfile Profile);
}
