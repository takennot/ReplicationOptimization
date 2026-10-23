#include "RBGameState.h"
#include "Net/UnrealNetwork.h"

ARBGameState::ARBGameState()
{
    bReplicates = true;
}

void ARBGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARBGameState, bOptimized);
    DOREPLIFETIME(ARBGameState, ScenarioActorCount);
}
