#include "RBHUD.h"
#include "RBGameState.h"
#include "RBProbeActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    int32 CountActorChannels(const UNetDriver* Driver)
    {
        if (!Driver) return -1;
        int32 Count = 0;
        const auto CountForConnection = [&Count](const UNetConnection* Connection)
        {
            if (!Connection) return;
            for (const UChannel* Channel : Connection->OpenChannels)
            {
                if (Channel && Channel->IsA<UActorChannel>())
                {
                    ++Count;
                }
            }
        };

        if (Driver->ServerConnection)
        {
            CountForConnection(Driver->ServerConnection);
        }
        else
        {
            // Listen/dedicated server: aggregate channels across remote clients.
            for (const UNetConnection* Connection : Driver->ClientConnections)
            {
                CountForConnection(Connection);
            }
        }
        return Count;
    }
}

void ARBHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!GetWorld() || !GetOwningPlayerController()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (LastSampleAt < 0.0 || (Now - LastSampleAt) >= 1.0)
    {
        Sample(Now);
    }

    const ARBGameState* State = GetWorld()->GetGameState<ARBGameState>();
    const bool bServerLocal = GetNetMode() != NM_Client;
    float Y = 26.0f;
    constexpr float LineHeight = 23.0f;
    const auto Line = [this, &Y, &LineHeight](const FString& S, FLinearColor Color)
    {
        DrawText(S, Color, 22.0f, Y, nullptr, 1.15f);
        Y += LineHeight;
    };

    Line(TEXT("REPLICATION BUDGETER | Unreal C++ / legacy replication"), FLinearColor(0.40f, 0.86f, 0.92f));
    Line(FString::Printf(TEXT("Mode: %s   Actors: %d"),
        (State && State->bOptimized) ? TEXT("PRIORITIZED") : TEXT("BASELINE"),
        State ? State->ScenarioActorCount : 0), FLinearColor::White);
    Line(TEXT("P: toggle policy  |  1/2/3: 50/200/500 actors  |  V: wireframes"), FLinearColor::White);
    Line(TEXT("Console: RBExportCSV, RBClearSamples, stat net, net pktlag 100"), FLinearColor::White);
    Y += 8.0f;

    if (bServerLocal)
    {
        Line(TEXT("HOST: movement-receipt data must be measured on remote client."), FLinearColor::Yellow);
    }
    else
    {
        Line(FString::Printf(TEXT("Client: %d/%d actors arrived | %.1f movement notifications/s"),
            Latest.ObservedActorCount, Latest.ActorCount, Latest.RepMovementEventsPerSecond),
            FLinearColor(0.40f, 0.86f, 0.92f));
        Line(FString::Printf(TEXT("Actors sampled: %d | mean age: %.3f s | ping: %.0f ms"),
            Latest.MovingActorsWithSamples, Latest.MeanSecondsSinceMovementEvent,
            Latest.LocalPingMs), FLinearColor::White);
    }
    if (Latest.bNetRateValid)
    {
        Line(FString::Printf(TEXT("Net %s: IN %.1f KB/s | OUT %.1f KB/s | actor channels: %d"),
            bServerLocal ? TEXT("server") : TEXT("client"),
            Latest.InBytesPerSecond / 1024.0, Latest.OutBytesPerSecond / 1024.0,
            Latest.NetActorChannels), FLinearColor(0.40f, 0.86f, 0.92f));
        Line(FString::Printf(TEXT("Packets: IN %.1f/s | OUT %.1f/s"),
            Latest.InPacketsPerSecond, Latest.OutPacketsPerSecond), FLinearColor::White);
    }
    else
    {
        Line(TEXT("Net: waiting for two valid samples (or a network connection)."), FLinearColor::Yellow);
    }
    Line(TEXT("CSV now includes net-driver bytes/s and packets/s; use Networking Insights for breakdowns."),
         FLinearColor(1.0f, 0.77f, 0.46f));
    Line(TEXT("Wire colors: baseline=neutral; prioritized: cyan=near, gold=mid, gray=far, magenta=critical"), FLinearColor::White);
}

void ARBHUD::Sample(double Now)
{
    const ARBGameState* State = GetWorld()->GetGameState<ARBGameState>();
    FRBMetricSample M;
    M.TimeSeconds = Now;
    M.bOptimized = State && State->bOptimized;
    M.ActorCount = State ? State->ScenarioActorCount : 0;
    if (const APlayerController* PC = GetOwningPlayerController())
    {
        if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
        {
            M.LocalPingMs = PS->GetPingInMilliseconds();
        }
    }

    int32 EventCount = 0;
    double SumAge = 0.0;
    for (TActorIterator<ARBProbeActor> It(GetWorld()); It; ++It)
    {
        const ARBProbeActor* Probe = *It;
        ++M.ObservedActorCount;
        EventCount += Probe->GetReceiveCount();
        if (Probe->GetLastReceiveSeconds() >= 0.0)
        {
            ++M.MovingActorsWithSamples;
            SumAge += FMath::Max(0.0, Now - Probe->GetLastReceiveSeconds());
        }
    }
    if (M.MovingActorsWithSamples > 0)
    {
        M.MeanSecondsSinceMovementEvent = static_cast<float>(SumAge / M.MovingActorsWithSamples);
    }
    if (bHasPreviousSample && LastSampleAt >= 0.0)
    {
        M.RepMovementEventsPerSecond = static_cast<float>(
            FMath::Max(0, EventCount - LastObservedEventCount) / FMath::Max(0.001, Now - LastSampleAt));
    }
    bHasPreviousSample = true;
    LastSampleAt = Now;
    LastObservedEventCount = EventCount;

    if (UNetDriver* Driver = GetWorld()->GetNetDriver())
    {
        // Keep stats collection enabled independently of whether "stat net"
        // happens to be open; otherwise counters can remain zero in PIE.
        Driver->bCollectNetStats = true;

        // These counters are cumulative since driver creation. Differences
        // divided by elapsed seconds give measured (not configured) rates.
        const FRBNetCounterTotals CurrentTotals{
            Driver->InTotalBytes, Driver->OutTotalBytes,
            Driver->InTotalPackets, Driver->OutTotalPackets
        };
        M.NetActorChannels = CountActorChannels(Driver);
        if (bHasPreviousNetSample && LastNetDriver.Get() == Driver)
        {
            const FRBNetCounterRates Rates = RBDeriveNetworkRates(
                LastNetTotals, CurrentTotals, Now - LastNetSampleAt);
            if (Rates.bValid)
            {
                M.bNetRateValid = true;
                M.NetSampleDurationSeconds = Now - LastNetSampleAt;
                M.InBytesPerSecond = Rates.InBytesPerSecond;
                M.OutBytesPerSecond = Rates.OutBytesPerSecond;
                M.InPacketsPerSecond = Rates.InPacketsPerSecond;
                M.OutPacketsPerSecond = Rates.OutPacketsPerSecond;
            }
        }
        LastNetTotals = CurrentTotals;
        LastNetDriver = Driver;
        LastNetSampleAt = Now;
        bHasPreviousNetSample = true;
    }
    else
    {
        bHasPreviousNetSample = false;
        LastNetDriver.Reset();
    }

    Latest = M;
    Samples.Add(M);
}

void ARBHUD::ResetSamples()
{
    Samples.Reset();
    bHasPreviousSample = false;
    LastSampleAt = -1.0;
    LastObservedEventCount = 0;
    bHasPreviousNetSample = false;
    LastNetDriver.Reset();
    LastNetSampleAt = -1.0;
    LastNetTotals = FRBNetCounterTotals();
    Latest = FRBMetricSample();
}

bool ARBHUD::SaveSamplesToCSV(FString& OutFile)
{
    if (Samples.IsEmpty()) return false;
    const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Benchmarks"));
    IFileManager::Get().MakeDirectory(*Folder, true);
    OutFile = FPaths::Combine(Folder,
        FString::Printf(TEXT("RB_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));

    FString Data = TEXT("client_time_s,capture_side,policy,configured_actors,observed_actors,actors_with_movement_samples,movement_events_per_s,mean_age_since_event_s,client_ping_ms,net_sample_duration_s,in_bytes_per_s,out_bytes_per_s,in_packets_per_s,out_packets_per_s,net_actor_channels\n");
    const TCHAR* CaptureSide = GetNetMode() == NM_Client ? TEXT("client") : TEXT("server");
    for (const FRBMetricSample& S : Samples)
    {
        // The first net sample has no preceding interval. Write empty cells,
        // not zero, to avoid biasing benchmark averages.
        const FString NetDuration = S.bNetRateValid ? FString::Printf(TEXT("%.3f"), S.NetSampleDurationSeconds) : TEXT("");
        const FString InBytes = S.bNetRateValid ? FString::Printf(TEXT("%.1f"), S.InBytesPerSecond) : TEXT("");
        const FString OutBytes = S.bNetRateValid ? FString::Printf(TEXT("%.1f"), S.OutBytesPerSecond) : TEXT("");
        const FString InPackets = S.bNetRateValid ? FString::Printf(TEXT("%.1f"), S.InPacketsPerSecond) : TEXT("");
        const FString OutPackets = S.bNetRateValid ? FString::Printf(TEXT("%.1f"), S.OutPacketsPerSecond) : TEXT("");
        const FString ActorChannels = S.NetActorChannels >= 0 ? FString::FromInt(S.NetActorChannels) : TEXT("");
        Data += FString::Printf(TEXT("%.3f,%s,%s,%d,%d,%d,%.3f,%.4f,%.1f,%s,%s,%s,%s,%s,%s\n"),
            S.TimeSeconds, CaptureSide,
            S.bOptimized ? TEXT("prioritized") : TEXT("baseline"),
            S.ActorCount, S.ObservedActorCount, S.MovingActorsWithSamples,
            S.RepMovementEventsPerSecond, S.MeanSecondsSinceMovementEvent,
            S.LocalPingMs, *NetDuration, *InBytes, *OutBytes, *InPackets,
            *OutPackets, *ActorChannels);
    }
    return FFileHelper::SaveStringToFile(Data, *OutFile,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
