#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RBStage.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UDirectionalLightComponent;

/** Simple replicated scene light + floor; every mesh exists in engine content. */
UCLASS()
class REPLICATIONBUDGETER_API ARBStage : public AActor
{
    GENERATED_BODY()
public:
    ARBStage();
private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Floor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDirectionalLightComponent> Sun;
};
