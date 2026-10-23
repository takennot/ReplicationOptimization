using UnrealBuildTool;

public class ReplicationBudgeter : ModuleRules
{
    public ReplicationBudgeter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore"
        });
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "NetCore"
        });
    }
}
