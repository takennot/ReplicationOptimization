#include "RBPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRBPolicyTest,
    "ReplicationBudgeter.Policy.RatesAndFairness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRBPolicyTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("baseline ignores distance"),
        RBPolicy::ChooseUpdateHz(90000.0f, false, false), 20.0f);
    TestEqual(TEXT("optimized near"),
        RBPolicy::ChooseUpdateHz(800.0f, false, true), 20.0f);
    TestEqual(TEXT("optimized middle"),
        RBPolicy::ChooseUpdateHz(2800.0f, false, true), 8.0f);
    TestEqual(TEXT("optimized far"),
        RBPolicy::ChooseUpdateHz(8000.0f, false, true), 2.0f);
    TestEqual(TEXT("critical far gets higher cadence"),
        RBPolicy::ChooseUpdateHz(8000.0f, true, true), 12.0f);
    TestTrue(TEXT("near has higher relative priority than far"),
        RBPolicy::PriorityMultiplier(800, false, 0.2f) >
        RBPolicy::PriorityMultiplier(8000, false, 0.2f));
    TestTrue(TEXT("critical gets higher relative priority"),
        RBPolicy::PriorityMultiplier(8000, true, 0.2f) >
        RBPolicy::PriorityMultiplier(8000, false, 0.2f));
    TestTrue(TEXT("age mitigates starvation"),
        RBPolicy::PriorityMultiplier(8000, false, 0.9f) >
        RBPolicy::PriorityMultiplier(8000, false, 0.1f));
    return true;
}
#endif
