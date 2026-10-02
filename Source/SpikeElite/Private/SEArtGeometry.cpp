// SPDX-License-Identifier: MIT
#include "SEArtGeometry.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/StrongObjectPtr.h"
#include "SEMaterials.h"

namespace SEArtGeometry
{
	struct FRing { float Z, X, Y, OffsetX = 0.f; };
	UStaticMesh* Get(EProfile Profile)
	{
		// Strong references are bounded by the nine profiles. All characters/crowd share them.
		static TMap<EProfile, TStrongObjectPtr<UStaticMesh>> Cache;
		if (const auto* Found = Cache.Find(Profile)) { return Found->Get(); }
		check(IsInGameThread());
		TArray<FRing> Rings;
		switch (Profile)
		{
		case EProfile::Torso: Rings = {{-.5f,.36f,.36f},{-.4f,.40f,.38f},{-.15f,.44f,.40f},{.05f,.49f,.46f},{.28f,.47f,.50f},{.38f,.40f,.48f},{.5f,.22f,.22f}}; break;
		case EProfile::Head: Rings = {{-.5f,.20f,.23f},{-.4f,.35f,.33f},{-.2f,.46f,.43f},{.05f,.50f,.49f},{.28f,.47f,.50f},{.42f,.34f,.37f},{.5f,.10f,.13f}}; break;
		case EProfile::UpperArm: Rings = {{-.5f,.33f,.33f},{-.4f,.39f,.39f},{-.1f,.48f,.46f},{.2f,.50f,.50f},{.4f,.45f,.44f},{.5f,.32f,.32f}}; break;
		case EProfile::Forearm: Rings = {{-.5f,.27f,.30f},{-.3f,.34f,.34f},{0,.43f,.40f},{.25f,.50f,.46f},{.45f,.42f,.42f},{.5f,.34f,.34f}}; break;
		case EProfile::Thigh: Rings = {{-.5f,.32f,.32f},{-.35f,.39f,.38f},{-.1f,.47f,.46f},{.22f,.50f,.50f},{.42f,.45f,.45f},{.5f,.36f,.36f}}; break;
		case EProfile::Calf: Rings = {{-.5f,.24f,.26f},{-.3f,.28f,.30f},{0,.42f,.43f},{.20f,.50f,.50f},{.38f,.39f,.39f},{.5f,.32f,.32f}}; break;
		case EProfile::Palm: Rings = {{-.5f,.28f,.34f},{-.35f,.42f,.46f},{0,.50f,.50f},{.3f,.40f,.45f},{.5f,.25f,.30f}}; break;
		case EProfile::Shoe: Rings = {{-.5f,.42f,.44f,.04f},{-.36f,.50f,.50f,0},{-.1f,.49f,.48f,0},{.1f,.42f,.43f,-.03f},{.32f,.29f,.38f,-.12f},{.5f,.21f,.30f,-.18f}}; break;
		default: Rings = {{-.5f,.43f,.43f},{-.35f,.50f,.50f},{.35f,.50f,.50f},{.5f,.43f,.43f}}; break;
		}
		FMeshDescription Description;
		FStaticMeshAttributes A(Description);
		A.Register();
		auto Positions = A.GetVertexPositions();
		auto Normals = A.GetVertexInstanceNormals();
		auto Tangents = A.GetVertexInstanceTangents();
		auto Signs = A.GetVertexInstanceBinormalSigns();
		auto UVs = A.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		auto Colors = A.GetVertexInstanceColors();
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		A.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Surface");
		constexpr int32 Sides = 32;
		TArray<FVertexID> Vertices;
		TArray<FVector3f> SmoothNormals;
		TArray<FVector3f> SmoothTangents;
		for (int32 R = 0; R < Rings.Num(); ++R)
		{
			const FRing& Ring = Rings[R];
			const FRing& Before = Rings[FMath::Max(0, R - 1)];
			const FRing& After = Rings[FMath::Min(Rings.Num() - 1, R + 1)];
			for (int32 J = 0; J <= Sides; ++J)
			{
				const float Angle = 2.f * PI * J / Sides;
				const float C = FMath::Cos(Angle), S = FMath::Sin(Angle);
				const FVertexID V = Description.CreateVertex();
				Positions[V] = FVector3f((Ring.X * C + Ring.OffsetX) * 100.f, Ring.Y * S * 100.f, Ring.Z * 100.f);
				Vertices.Add(V);
				const FVector3f Around(-Ring.X * S, Ring.Y * C, 0.f);
				const FVector3f Along((After.X - Before.X) * C + After.OffsetX - Before.OffsetX, (After.Y - Before.Y) * S, After.Z - Before.Z);
				SmoothNormals.Add(FVector3f::CrossProduct(Around, Along).GetSafeNormal());
				SmoothTangents.Add(Around.GetSafeNormal());
			}
		}
		auto Tri = [&](int32 I, int32 J, int32 K)
		{
			TArray<FVertexInstanceID, TInlineAllocator<3>> Instances;
			// UE's left-handed mesh convention uses Cross(edge2, edge1).
			// Reverse the mathematical ring winding to show the OUTSIDE, not
			// the hollow interior (also essential for correct shadow culling).
			for (int32 Index : {I, K, J})
			{
				const FVertexInstanceID VI = Description.CreateVertexInstance(Vertices[Index]);
				Normals[VI] = SmoothNormals[Index]; Tangents[VI] = SmoothTangents[Index]; Signs[VI] = 1.f;
				Colors[VI] = FVector4f(1.f,1.f,1.f,1.f);
				UVs.Set(VI, 0, FVector2f(float(Index % (Sides + 1)) / Sides, float(Index / (Sides + 1)) / (Rings.Num() - 1)));
				Instances.Add(VI);
			}
			Description.CreatePolygon(Group, Instances);
		};
		for (int32 R = 0; R < Rings.Num() - 1; ++R)
		{
			for (int32 J = 0; J < Sides; ++J)
			{
				const int32 I = R * (Sides + 1) + J;
				Tri(I, I + 1, I + Sides + 2); Tri(I, I + Sides + 2, I + Sides + 1);
			}
		}
		for (int32 End = 0; End < 2; ++End)
		{
			const int32 R = End ? Rings.Num() - 1 : 0;
			const FVertexID V = Description.CreateVertex();
			Positions[V] = FVector3f(Rings[R].OffsetX * 100.f, 0.f, Rings[R].Z * 100.f);
			const int32 Centre = Vertices.Add(V);
			SmoothNormals.Add(FVector3f(0,0,End ? 1.f : -1.f)); SmoothTangents.Add(FVector3f(1,0,0));
			for (int32 J = 0; J < Sides; ++J)
			{
				const int32 I = R * (Sides + 1) + J;
				if (End) { Tri(Centre,I,I+1); } else { Tri(Centre,I+1,I); }
			}
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(SEMaterials::TintBase(), TEXT("Surface")));
		// Fast runtime builds do not run the editor's UV-density calculation.
		// Supply finite streaming metadata rather than disabling streaming or
		// ignoring a packaged-only GetUVChannelData ensure.
		FMeshUVChannelInfo& UVInfo = Mesh->GetStaticMaterials()[0].UVChannelData;
		UVInfo.bInitialized = true;
		UVInfo.bOverrideDensities = true;
		UVInfo.LocalUVDensities[0] = 100.f;
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bFastBuild = true; Params.bBuildSimpleCollision = false;
		Params.bMarkPackageDirty = false; Params.bCommitMeshDescription = false;
		if (!Mesh->BuildFromMeshDescriptions({ &Description }, Params)) { return nullptr; }
		Cache.Add(Profile, TStrongObjectPtr<UStaticMesh>(Mesh));
		return Mesh;
	}
}
