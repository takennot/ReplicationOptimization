#pragma once

/**
 * Pure policy functions so decisions are unit-testable without a level or a net driver.
 * Hz is SERVER-GLOBAL (nearest connected player); priority is PER-CONNECTION.
 */
namespace RBPolicy
{
    constexpr float BaselineHz = 20.0f;
    constexpr float NearCm = 1800.0f;
    constexpr float MiddleCm = 4500.0f;

    float ChooseUpdateHz(float NearestViewerDistanceCm, bool bCritical, bool bOptimized);

    float PriorityMultiplier(float ViewerDistanceCm, bool bCritical,
                             float SecondsSinceLastReplication);
}
