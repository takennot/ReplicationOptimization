#include "RBPolicy.h"
#include <algorithm>

float RBPolicy::ChooseUpdateHz(float D, bool bCritical, bool bOptimized)
{
    if (!bOptimized)
    {
        return BaselineHz;
    }

    const float DistanceHz = D < NearCm ? 20.0f : (D < MiddleCm ? 8.0f : 2.0f);
    // Important actors remain readable even when no nearby viewer exists.
    return bCritical ? std::max(12.0f, DistanceHz) : DistanceHz;
}

float RBPolicy::PriorityMultiplier(float D, bool bCritical, float Time)
{
    const float RangeWeight = D < NearCm ? 3.0f : (D < MiddleCm ? 1.5f : 0.45f);
    const float CriticalWeight = bCritical ? 2.5f : 1.0f;
    // Increase weight for actors that have not replicated recently; cap it to
    // prevent a very stale actor from dominating the entire priority order.
    const float AgeWeight = std::clamp(Time / 0.2f, 1.0f, 4.0f);
    return RangeWeight * CriticalWeight * AgeWeight;
}
