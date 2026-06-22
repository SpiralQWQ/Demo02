using UnrealBuildTool;
using System.Collections.Generic;

public class Demo02EditorTarget : TargetRules
{
    public Demo02EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        bOverrideBuildEnvironment = true;
        ExtraModuleNames.Add("Demo02");
    }
}
