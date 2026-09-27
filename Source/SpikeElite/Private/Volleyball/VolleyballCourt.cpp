// SPDX-License-Identifier: MIT
#include "Volleyball/VolleyballCourt.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVolleyballCourt::AVolleyballCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
	Root->SetBoxExtent(FVector(HalfCourtLength, HalfCourtWidth, 10.0f));
	Root->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Root;

	// Floor: 18m x 9m box, 10cm thick.
	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	FloorMesh->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FloorMesh->SetStaticMesh(CubeMesh.Object);
		FloorMesh->SetRelativeScale3D(FVector(HalfCourtLength * 2.0f / 100.0f, HalfCourtWidth * 2.0f / 100.0f, 0.1f));
		FloorMesh->SetRelativeLocation(FVector(0, 0, -5.0f));
	}

	// Net: thin box across the middle, at net height.
	NetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Net"));
	NetMesh->SetupAttachment(Root);
	if (CubeMesh.Succeeded())
	{
		NetMesh->SetStaticMesh(CubeMesh.Object);
		// 9m wide (across width), 1m tall, 5cm thick
		NetMesh->SetRelativeScale3D(FVector(0.05f, HalfCourtWidth * 2.0f / 100.0f, NetHeight / 100.0f));
		NetMesh->SetRelativeLocation(FVector(0, 0, NetHeight / 2.0f));
	}

	// Posts: cylinders at the net edges.
	auto MakePost = [&](const TCHAR* Name, float Y)
	{
		UStaticMeshComponent* Post = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Post->SetupAttachment(Root);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylMesh.Succeeded())
		{
			Post->SetStaticMesh(CylMesh.Object);
			Post->SetRelativeScale3D(FVector(0.15f, 0.15f, (NetHeight + 20.0f) / 100.0f));
			Post->SetRelativeLocation(FVector(0, Y, (NetHeight + 20.0f) / 2.0f));
		}
		return Post;
	};
	PostLeft = MakePost(TEXT("PostLeft"), HalfCourtWidth + 20.0f);
	PostRight = MakePost(TEXT("PostRight"), -HalfCourtWidth - 20.0f);
}

void AVolleyballCourt::BeginPlay()
{
	Super::BeginPlay();
	BuildCourt();
}

void AVolleyballCourt::BuildCourt()
{
	// Geometry is already built in constructor. Hook for M1 (line decals, crowd, lighting).
}
