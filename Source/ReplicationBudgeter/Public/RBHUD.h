#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RBNetRateMath.h"
#include "RBHUD.generated.h"

class UNetDriver;

USTRUCT()
struct FRBMetricSample
{
    GENERATED_BODY()
    double TimeSeconds = 0.0;
    int32 ActorCount = 0;
    int32 ObservedActorCount = 0;
    int32 MovingActorsWithSamples = 0;
    float RepMovementEventsPerSecond = 0.0f;
    float MeanSecondsSinceMovementEvent = 0.0f;
    float LocalPingMs = 0.0f;
    bool bOptimized = false;

    // Measurements for this world's net driver, across its connections.
    // For the one-client benchmark, client IN and server OUT measure the
    // replicated stream in opposite directions. The first sample has no rates.
    bool bNetRateValid = false;
    double NetSampleDurationSeconds = 0.0;
    double InBytesPerSecond = 0.0;
    double OutBytesPerSecond = 0.0;
    double InPacketsPerSecond = 0.0;
    double OutPacketsPerSecond = 0.0;
    int32 NetActorChannels = -1;
};

/** Per-window movement-receipt observations plus net-driver byte/packet rates. */
UCLASS()
class REPLICATIONBUDGETER_API ARBHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void ResetSamples();
    bool SaveSamplesToCSV(FString& OutFile);

private:
    void Sample(double Now);
    TArray<FRBMetricSample> Samples;
    FRBMetricSample Latest;
    double LastSampleAt = -1.0;
    int32 LastObservedEventCount = 0;
    bool bHasPreviousSample = false;

    // Use cumulative driver counters instead of sampling Unreal's smoothed
    // 'stat net' values. This produces directly exportable interval averages.
    TWeakObjectPtr<UNetDriver> LastNetDriver;
    FRBNetCounterTotals LastNetTotals;
    double LastNetSampleAt = -1.0;
    bool bHasPreviousNetSample = false;
};
