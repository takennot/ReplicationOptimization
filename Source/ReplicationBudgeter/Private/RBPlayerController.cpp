#include "RBPlayerController.h"
#include "RBGameMode.h"
#include "RBGameState.h"
#include "RBHUD.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void ARBPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) return;
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ARBPlayerController::TogglePolicy);
    InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ARBPlayerController::ToggleVisuals);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ARBPlayerController::Choose50);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ARBPlayerController::Choose200);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ARBPlayerController::Choose500);
}

void ARBPlayerController::TogglePolicy()
{
    const ARBGameState* State = GetWorld()->GetGameState<ARBGameState>();
    ServerSetMode(!(State && State->bOptimized));
}

void ARBPlayerController::ToggleVisuals() { bDebugEnabled = !bDebugEnabled; }
void ARBPlayerController::Choose50() { ServerSetCount(50); }
void ARBPlayerController::Choose200() { ServerSetCount(200); }
void ARBPlayerController::Choose500() { ServerSetCount(500); }

void ARBPlayerController::ServerSetMode_Implementation(bool bEnabled)
{
    if (ARBGameMode* GM = GetWorld()->GetAuthGameMode<ARBGameMode>())
    {
        GM->SetOptimized(bEnabled);
    }
}

void ARBPlayerController::ServerSetCount_Implementation(int32 Count)
{
    if (ARBGameMode* GM = GetWorld()->GetAuthGameMode<ARBGameMode>())
    {
        GM->ChangeActorCount(FMath::Clamp(Count, 10, 1000));
    }
}

void ARBPlayerController::RBSetMode(int32 Optimized)
{
    ServerSetMode(Optimized != 0);
}

void ARBPlayerController::RBSetCount(int32 Count)
{
    ServerSetCount(FMath::Clamp(Count, 10, 1000));
}

void ARBPlayerController::RBClearSamples()
{
    if (ARBHUD* BenchmarkHUD = Cast<ARBHUD>(GetHUD()))
    {
        BenchmarkHUD->ResetSamples();
    }
}

void ARBPlayerController::RBExportCSV()
{
    if (ARBHUD* BenchmarkHUD = Cast<ARBHUD>(GetHUD()))
    {
        FString Path;
        if (BenchmarkHUD->SaveSamplesToCSV(Path))
        {
            UE_LOG(LogTemp, Display, TEXT("[ReplicationBudgeter] CSV saved to %s"), *Path);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green,
                    FString::Printf(TEXT("CSV saved: %s"), *Path));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[ReplicationBudgeter] CSV export failed or no samples"));
        }
    }
}
