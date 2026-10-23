using UnrealBuildTool;
using System.Collections.Generic;

public class ReplicationBudgeterEditorTarget : TargetRules
{
    public ReplicationBudgeterEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("ReplicationBudgeter");
    }
}
