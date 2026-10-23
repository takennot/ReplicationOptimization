using UnrealBuildTool;
using System.Collections.Generic;

public class ReplicationBudgeterTarget : TargetRules
{
    public ReplicationBudgeterTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("ReplicationBudgeter");
    }
}
