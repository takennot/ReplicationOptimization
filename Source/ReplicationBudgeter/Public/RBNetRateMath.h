#pragma once

// Standalone net-counter math; deliberately independent of Unreal types so the
// wraparound and sample-duration behavior can be tested with an ordinary C++17 compiler.
#include <cstdint>

struct FRBNetCounterTotals
{
    std::uint32_t InBytes = 0;
    std::uint32_t OutBytes = 0;
    std::uint32_t InPackets = 0;
    std::uint32_t OutPackets = 0;
};

struct FRBNetCounterRates
{
    double InBytesPerSecond = 0.0;
    double OutBytesPerSecond = 0.0;
    double InPacketsPerSecond = 0.0;
    double OutPacketsPerSecond = 0.0;
    bool bValid = false;
};

inline FRBNetCounterRates RBDeriveNetworkRates(
    const FRBNetCounterTotals& Previous,
    const FRBNetCounterTotals& Current,
    double Seconds)
{
    FRBNetCounterRates Result;
    if (!(Seconds > 0.0))
    {
        return Result;
    }

    // uint32 subtraction is deliberately modular, so one 32-bit counter wrap
    // between consecutive (approximately one-second) samples is handled.
    Result.InBytesPerSecond = static_cast<std::uint32_t>(Current.InBytes - Previous.InBytes) / Seconds;
    Result.OutBytesPerSecond = static_cast<std::uint32_t>(Current.OutBytes - Previous.OutBytes) / Seconds;
    Result.InPacketsPerSecond = static_cast<std::uint32_t>(Current.InPackets - Previous.InPackets) / Seconds;
    Result.OutPacketsPerSecond = static_cast<std::uint32_t>(Current.OutPackets - Previous.OutPackets) / Seconds;
    Result.bValid = true;
    return Result;
}
