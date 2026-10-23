#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RBGameState.generated.h"

UCLASS()
class REPLICATIONBUDGETER_API ARBGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    ARBGameState();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Benchmark")
    bool bOptimized = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Benchmark")
    int32 ScenarioActorCount = 0;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};
