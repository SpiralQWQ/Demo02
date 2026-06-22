using UnrealBuildTool;

public class Demo02 : ModuleRules
{
    public Demo02(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",          // UBlueprint 编辑器反射
            "BlueprintGraph",    // K2Node_CallFunction / K2Node_Event 等
            "KismetCompiler",    // FKismetCompilerContext — Qwen 审查建议添加
            "Kismet",            // 蓝图编辑器核心
            "Json",              // FJsonObject
            "JsonUtilities",     // FJsonObjectConverter
            "Slate",
            "SlateCore",
        });
    }
}
