// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"

/**
 * Reliable tinting for runtime basic-shape geometry.
 *
 * Engine BasicShapes materials do not expose a guaranteed runtime "Color"
 * parameter (components without an override material also fall back to the
 * world-grid material), so tinting via the mesh's current material was
 * inconsistent. Instead every tintable component uses the project-authored
 * /Game/Materials/M_Tint material (created by tools/setup_assets.py), which
 * exposes a single "Color" vector parameter.
 */
namespace SEMaterials
{
	inline UMaterialInterface* TintBase()
	{
		static UMaterialInterface* Base = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Materials/M_Tint.M_Tint"));
		return Base;
	}

	/** Create a dynamic M_Tint material in the given colour. Returns null if M_Tint is missing. */
	inline UMaterialInstanceDynamic* MakeTint(UObject* Owner, const FLinearColor& Color)
	{
		UMaterialInterface* Base = TintBase();
		if (!Base) { return nullptr; }
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
		if (MID) { MID->SetVectorParameterValue(TEXT("Color"), Color); }
		return MID;
	}

	/** Assign a freshly created tint MID to slot 0 of a mesh component. */
	inline UMaterialInstanceDynamic* MakeSurface(UObject* Owner, const FLinearColor& Color,
		float Roughness = .78f, float Metallic = 0.f)
	{
		static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Materials/M_ArtSurface.M_ArtSurface"));
		if (!Base) return MakeTint(Owner, Color);
		auto* M = UMaterialInstanceDynamic::Create(Base, Owner);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(Roughness,0.f,1.f));
			M->SetScalarParameterValue(TEXT("Metallic"), FMath::Clamp(Metallic,0.f,1.f));
		}
		return M;
	}

	/** Assign a freshly created tint MID to slot 0 of a mesh component. */
	inline void TintMesh(UMeshComponent* Mesh, UObject* Owner, const FLinearColor& Color)
	{
		if (!Mesh) { return; }
		if (UMaterialInstanceDynamic* MID = MakeTint(Owner, Color))
		{
			Mesh->SetMaterial(0, MID);
		}
	}

	/**
	 * Crowd clothing material: same "Color" parameter but the material feeds a
	 * fraction of it into emissive so distant spectators stay readable in the
	 * dim stands instead of collapsing into black silhouettes.
	 */
	inline UMaterialInterface* CrowdBase()
	{
		static UMaterialInterface* Base = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Materials/M_Crowd.M_Crowd"));
		return Base;
	}

	inline UMaterialInstanceDynamic* MakeCrowd(UObject* Owner, const FLinearColor& Color, float Emissive = 0.06f)
	{
		UMaterialInterface* Base = CrowdBase();
		if (!Base) { return MakeTint(Owner, Color); } // graceful fallback to M_Tint
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetScalarParameterValue(TEXT("Em"), Emissive);
		}
		return MID;
	}
}
