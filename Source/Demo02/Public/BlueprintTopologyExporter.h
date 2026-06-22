// BlueprintTopologyExporter.h — 蓝图拓扑导出器
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintTopologyExporter.generated.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

UCLASS()
class DEMO02_API UBlueprintTopologyExporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Blueprint Topology")
	static FString DumpBlueprintLogicToJson(UBlueprint* TargetBP);

private:
	static TSharedPtr<FJsonObject> SerializeGraph(UEdGraph* Graph);
	static TSharedPtr<FJsonObject> SerializeNode(UEdGraphNode* Node, int32 NodeIndex);
	static TSharedPtr<FJsonObject> SerializePin(UEdGraphPin* Pin);
	static FString PinIdToString(const FGuid& PinId);
	static FString GetFunctionName(UEdGraphNode* Node);
	static FString GetEventName(UEdGraphNode* Node);
	static FString GetVariableName(UEdGraphNode* Node);
};
