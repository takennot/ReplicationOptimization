#include "RBStage.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/DirectionalLightComponent.h"
#include "UObject/ConstructorHelpers.h"

ARBStage::ARBStage()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
    Floor->SetupAttachment(Root);
    Floor->SetRelativeLocation(FVector(0.0f, 0.0f, -280.0f));
    Floor->SetRelativeScale3D(FVector(160.f, 130.f, 1.f));
    Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) Floor->SetStaticMesh(Cube.Object);

    Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLight"));
    Sun->SetupAttachment(Root);
    Sun->SetRelativeRotation(FRotator(-48.0f, -35.0f, 0.0f));
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetIntensity(7.0f);
}
