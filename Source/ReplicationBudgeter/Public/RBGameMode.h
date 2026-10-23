#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "RBGameMode.generated.h"

class ARBProbeActor;
class ARBStage;

UCLASS()
class REPLICATIONBUDGETER_API ARBGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ARBGameMode();
    virtual void StartPlay() override;
    void SetOptimized(bool bEnabled);
    void ChangeActorCount(int32 NewCount);

private:
    void PopulateScenario(int32 Count);
    void ApplyRates();

    UPROPERTY()
    TArray<TObjectPtr<ARBProbeActor>> Actors;

    UPROPERTY()
    TObjectPtr<ARBStage> Stage;

    FTimerHandle RateTimer;
};
