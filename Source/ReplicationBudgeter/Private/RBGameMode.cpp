#include "RBGameMode.h"
#include "RBGameState.h"
#include "RBProbeActor.h"
#include "RBStage.h"
#include "RBPolicy.h"
#include "RBPlayerController.h"
#include "RBHUD.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ARBGameMode::ARBGameMode()
{
    GameStateClass = ARBGameState::StaticClass();
    PlayerControllerClass = ARBPlayerController::StaticClass();
    HUDClass = ARBHUD::StaticClass();
    DefaultPawnClass = ADefaultPawn::StaticClass();
}

void ARBGameMode::StartPlay()
{
    Super::StartPlay();
    Stage = GetWorld()->SpawnActor<ARBStage>();

    int32 Requested = 200;
    FParse::Value(FCommandLine::Get(), TEXT("RBActors="), Requested);
    PopulateScenario(FMath::Clamp(Requested, 10, 1000));
    GetWorldTimerManager().SetTimer(RateTimer, this, &ARBGameMode::ApplyRates,
                                    0.5f, true);
    ApplyRates();
}

void ARBGameMode::PopulateScenario(int32 Count)
{
    for (ARBProbeActor* Actor : Actors)
    {
        if (IsValid(Actor)) Actor->Destroy();
    }
    Actors.Reset();
    Actors.Reserve(Count);

    const int32 Columns = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count)));
    const int32 Rows = FMath::CeilToInt(static_cast<float>(Count) / Columns);
    const float SpacingX = 340.0f;
    const float SpacingY = 350.0f;

    for (int32 I = 0; I < Count; ++I)
    {
        const int32 Column = I % Columns;
        const int32 Row = I / Columns;
        const FVector Center(600.0f + Column * SpacingX,
                             (Row - (Rows - 1) * 0.5f) * SpacingY,
                             180.0f + (I % 4) * 95.0f);

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ARBProbeActor* Actor = GetWorld()->SpawnActor<ARBProbeActor>(
            ARBProbeActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
        if (Actor)
        {
            Actor->Configure(I, Center);
            Actors.Add(Actor);
        }
    }

    if (ARBGameState* State = GetGameState<ARBGameState>())
    {
        State->ScenarioActorCount = Actors.Num();
        State->ForceNetUpdate();
    }
    ApplyRates();
    UE_LOG(LogTemp, Display, TEXT("[ReplicationBudgeter] Spawned %d probes"), Actors.Num());
}

void ARBGameMode::SetOptimized(bool bEnabled)
{
    if (ARBGameState* State = GetGameState<ARBGameState>())
    {
        State->bOptimized = bEnabled;
        State->ForceNetUpdate();
    }
    ApplyRates();
    UE_LOG(LogTemp, Display, TEXT("[ReplicationBudgeter] Policy %s"),
            bEnabled ? TEXT("OPTIMIZED") : TEXT("BASELINE"));
}

void ARBGameMode::ChangeActorCount(int32 NewCount)
{
    PopulateScenario(FMath::Clamp(NewCount, 10, 1000));
}

void ARBGameMode::ApplyRates()
{
    const ARBGameState* State = GetGameState<ARBGameState>();
    const bool bOptimized = State && State->bOptimized;

    TArray<FVector, TInlineAllocator<8>> ViewerPositions;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (const APlayerController* PC = It->Get())
        {
            if (const APawn* Pawn = PC->GetPawn())
            {
                ViewerPositions.Add(Pawn->GetActorLocation());
            }
        }
    }

    for (ARBProbeActor* Actor : Actors)
    {
        if (!IsValid(Actor)) continue;
        float Nearest = TNumericLimits<float>::Max();
        for (const FVector& Pos : ViewerPositions)
        {
            Nearest = FMath::Min(Nearest, FVector::Dist(Actor->GetActorLocation(), Pos));
        }
        Actor->SetUpdateHz(RBPolicy::ChooseUpdateHz(Nearest, Actor->IsCritical(), bOptimized));
    }
}
