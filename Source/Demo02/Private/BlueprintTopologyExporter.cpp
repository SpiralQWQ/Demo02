// ============================================================================
// BlueprintTopologyExporter.cpp — 增强版（Gemini 3.5 Flash 审查后改进）
// ============================================================================
// 放置路径：YourProject/Source/YourProject/Private/BlueprintTopologyExporter.cpp
//
// 改进点（2026-06-21 Gemini 3.5 Flash 审查建议）：
//   1. #if WITH_EDITOR 守卫 — 非编辑器打包时提供占位实现，编译不报错
//   2. Writer->Close() 显式调用 — 确保 JSON 缓冲区完全冲刷到 FString
//   3. TPrettyJsonPrintPolicy — 格式化 JSON 输出，便于人工阅读
//   4. 头文件路径精确化 — Dom/JsonObject.h、Serialization/JsonWriter.h
//
// Cast 策略（UE5 蓝图节点类型判定）：
//   UK2Node_CallFunction   → 函数调用节点 → FunctionReference
//   UK2Node_Event          → 引擎原生事件（BeginPlay / Tick / ...） → EventReference
//   UK2Node_CustomEvent    → 用户自定义事件 → CustomFunctionName
//   UK2Node_VariableGet    → 变量读取 → VariableReference
//   UK2Node_VariableSet    → 变量写入 → VariableReference
//   UK2Node_FunctionEntry  → 自定义函数入口 → FunctionReference
//   UK2Node_MacroInstance  → 宏实例（ForLoop / Sequence / ...） → MacroGraphReference
//   UK2Node_Timeline       → Timeline 时间线 → TimelineName
//   UK2Node_DynamicCast    → Cast To 类型转换 → TargetType
// ============================================================================

#include "BlueprintTopologyExporter.h"

// 只在编辑器环境编译完整实现，非编辑器打包仅提供占位空函数
#if WITH_EDITOR

// ---- UE5 核心与编辑器头文件 ----
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

// ---- K2Node 节点类头文件（均属 BlueprintGraph 模块） ----
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Timeline.h"
#include "K2Node_DynamicCast.h"

// ---- JSON 序列化 ----
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/PrettyJsonPrintPolicy.h"

// ============================================================================
// Log 分类
// ============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogBlueprintTopology, Log, All);

// ============================================================================
// 公共入口
// ============================================================================

FString UBlueprintTopologyExporter::DumpBlueprintLogicToJson(UBlueprint* TargetBP)
{
    if (!TargetBP)
    {
        UE_LOG(LogBlueprintTopology, Error, TEXT("DumpBlueprintLogicToJson: TargetBP is null"));
        return TEXT("{\"error\": \"TargetBP is null\"}");
    }

    TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

    RootObj->SetStringField(TEXT("blueprint_name"), TargetBP->GetName());
    RootObj->SetStringField(TEXT("package_name"),  TargetBP->GetPathName());

    if (TargetBP->ParentClass)
    {
        RootObj->SetStringField(TEXT("parent_class"), TargetBP->ParentClass->GetName());
    }

    RootObj->SetStringField(TEXT("blueprint_type"),
        StaticEnum<EBlueprintType>()->GetNameStringByValue(
            static_cast<int64>(TargetBP->BlueprintType)));

    // ---- 遍历所有图表 ----
    TArray<TSharedPtr<FJsonValue>> GraphsArray;

    UE_LOG(LogBlueprintTopology, Log, TEXT("  Scanning UbergraphPages: %d graphs"),
        TargetBP->UbergraphPages.Num());

    // 1. 事件图
    for (UEdGraph* Graph : TargetBP->UbergraphPages)
    {
        if (Graph)
        {
            GraphsArray.Add(MakeShared<FJsonValueObject>(SerializeGraph(Graph)));
        }
    }

    // 2. 自定义函数图
    UE_LOG(LogBlueprintTopology, Log, TEXT("  Scanning FunctionGraphs: %d graphs"),
        TargetBP->FunctionGraphs.Num());

    for (UEdGraph* Graph : TargetBP->FunctionGraphs)
    {
        if (Graph)
        {
            GraphsArray.Add(MakeShared<FJsonValueObject>(SerializeGraph(Graph)));
        }
    }

    RootObj->SetArrayField(TEXT("graphs"), GraphsArray);

    // ---- JSON 序列化（改进点 2+3：PrettyPrint + 显式 Close）----
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);

    if (FJsonSerializer::Serialize(RootObj, Writer))
    {
        Writer->Close(); // 显式关闭，确保缓冲区完全冲刷到 FString
    }
    else
    {
        UE_LOG(LogBlueprintTopology, Error, TEXT("JSON serialization failed for %s"), *TargetBP->GetName());
        return TEXT("{\"error\": \"Serialization failed\"}");
    }

    UE_LOG(LogBlueprintTopology, Log, TEXT("  Dump complete: %d graphs, %d chars"),
        GraphsArray.Num(), OutputString.Len());

    return OutputString;
}

// ============================================================================
// 图表序列化
// ============================================================================

TSharedPtr<FJsonObject> UBlueprintTopologyExporter::SerializeGraph(UEdGraph* Graph)
{
    TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();

    const FString GraphName = Graph->GetName();
    GraphObj->SetStringField(TEXT("graph_name"), GraphName);

    // 图类型判定
    if (GraphName == TEXT("EventGraph"))
    {
        GraphObj->SetStringField(TEXT("graph_type"), TEXT("EventGraph"));
    }
    else if (GraphName.StartsWith(TEXT("Function ")))
    {
        GraphObj->SetStringField(TEXT("graph_type"), TEXT("FunctionGraph"));
    }
    else
    {
        GraphObj->SetStringField(TEXT("graph_type"), TEXT("Unknown"));
    }

    TArray<TSharedPtr<FJsonValue>> NodesArray;
    TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
    TSet<FString> RecordedConnections;
    int32 NodeIndex = 0;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) { continue; }

        // 序列化节点
        TSharedPtr<FJsonObject> NodeObj = SerializeNode(Node, NodeIndex);
        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));

        // 提取连线（去重）
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin) { continue; }

            const FString FromPinId = PinIdToString(Pin->PinId);

            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!LinkedPin) { continue; }

                const FString ToPinId = PinIdToString(LinkedPin->PinId);

                const FString ForwardKey  = FString::Printf(TEXT("%s->%s"), *FromPinId, *ToPinId);
                const FString BackwardKey = FString::Printf(TEXT("%s->%s"), *ToPinId, *FromPinId);

                if (RecordedConnections.Contains(ForwardKey) ||
                    RecordedConnections.Contains(BackwardKey))
                {
                    continue;
                }
                RecordedConnections.Add(ForwardKey);

                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                ConnObj->SetStringField(TEXT("from_pin"),      FromPinId);
                ConnObj->SetStringField(TEXT("to_pin"),        ToPinId);
                ConnObj->SetStringField(TEXT("from_pin_name"), Pin->PinName.ToString());
                ConnObj->SetStringField(TEXT("to_pin_name"),   LinkedPin->PinName.ToString());
                ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
        }

        ++NodeIndex;
    }

    GraphObj->SetArrayField(TEXT("nodes"),       NodesArray);
    GraphObj->SetArrayField(TEXT("connections"), ConnectionsArray);

    UE_LOG(LogBlueprintTopology, Log, TEXT("    Graph [%s]: %d nodes, %d connections"),
        *GraphName, NodesArray.Num(), ConnectionsArray.Num());

    return GraphObj;
}

// ============================================================================
// 节点序列化
// ============================================================================

TSharedPtr<FJsonObject> UBlueprintTopologyExporter::SerializeNode(
    UEdGraphNode* Node, int32 NodeIndex)
{
    TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();

    const FString NodeClass = Node->GetClass()->GetName();
    NodeObj->SetStringField(TEXT("node_class"), NodeClass);
    NodeObj->SetStringField(TEXT("node_id"),
        FString::Printf(TEXT("%s_%d"), *NodeClass, NodeIndex));
    NodeObj->SetStringField(TEXT("node_title"),
        Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
    NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
    NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
    NodeObj->SetStringField(TEXT("comment"), Node->NodeComment);

    // ---- Cast 向下转型提取专属信息 ----
    NodeObj->SetStringField(TEXT("function_name"), GetFunctionName(Node));
    NodeObj->SetStringField(TEXT("event_name"),    GetEventName(Node));
    NodeObj->SetStringField(TEXT("variable_name"), GetVariableName(Node));

    // MacroInstance
    FString MacroName;
    if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
    {
        if (MacroNode->GetMacroGraph())
        {
            MacroName = MacroNode->GetMacroGraph()->GetName();
        }
    }
    NodeObj->SetStringField(TEXT("macro_name"), MacroName);

    // Timeline
    FString TimelineName;
    if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>(Node))
    {
        TimelineName = TimelineNode->TimelineName.ToString();
    }
    NodeObj->SetStringField(TEXT("timeline_name"), TimelineName);

    // DynamicCast
    FString CastTargetType;
    if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
    {
        if (CastNode->TargetType)
        {
            CastTargetType = CastNode->TargetType->GetName();
        }
    }
    NodeObj->SetStringField(TEXT("cast_target_type"), CastTargetType);

    // 纯函数判定 — UE 5.7 IsNodePure 在 UK2Node 上
    bool bIsPure = false;
    if (UK2Node* K2Node = Cast<UK2Node>(Node))
    {
        bIsPure = K2Node->IsNodePure();
    }
    NodeObj->SetBoolField(TEXT("is_pure"), bIsPure);

    // ---- 序列化引脚 ----
    TArray<TSharedPtr<FJsonValue>> PinsArray;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin)
        {
            PinsArray.Add(MakeShared<FJsonValueObject>(SerializePin(Pin)));
        }
    }
    NodeObj->SetArrayField(TEXT("pins"), PinsArray);

    return NodeObj;
}

// ============================================================================
// 引脚序列化
// ============================================================================

TSharedPtr<FJsonObject> UBlueprintTopologyExporter::SerializePin(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();

    PinObj->SetStringField(TEXT("pin_id"),   PinIdToString(Pin->PinId));
    PinObj->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
    PinObj->SetStringField(TEXT("direction"),
        (Pin->Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output"));

    const FString PinCategory = Pin->PinType.PinCategory.ToString();
    PinObj->SetStringField(TEXT("pin_category"), PinCategory);

    // 子类别（Struct 名或 Object Class 名）
    if ((PinCategory == UEdGraphSchema_K2::PC_Struct ||
         PinCategory == UEdGraphSchema_K2::PC_Object ||
         PinCategory == UEdGraphSchema_K2::PC_SoftObject ||
         PinCategory == UEdGraphSchema_K2::PC_Class) &&
        Pin->PinType.PinSubCategoryObject.IsValid())
    {
        PinObj->SetStringField(TEXT("pin_sub_category"),
            Pin->PinType.PinSubCategoryObject->GetName());
    }
    else
    {
        PinObj->SetStringField(TEXT("pin_sub_category"),
            Pin->PinType.PinSubCategory.ToString());
    }

    PinObj->SetBoolField(TEXT("is_array"),     Pin->PinType.IsArray());
    PinObj->SetBoolField(TEXT("is_set"),       Pin->PinType.IsSet());
    PinObj->SetBoolField(TEXT("is_map"),       Pin->PinType.IsMap());
    PinObj->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
    PinObj->SetBoolField(TEXT("is_const"),     Pin->PinType.bIsConst);
    PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);

    // 连线目标
    TArray<TSharedPtr<FJsonValue>> LinkedPinIds;
    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
    {
        if (LinkedPin)
        {
            LinkedPinIds.Add(MakeShared<FJsonValueString>(PinIdToString(LinkedPin->PinId)));
        }
    }
    PinObj->SetArrayField(TEXT("linked_to"), LinkedPinIds);

    return PinObj;
}

// ============================================================================
// 辅助函数
// ============================================================================

FString UBlueprintTopologyExporter::PinIdToString(const FGuid& PinId)
{
    return PinId.ToString(EGuidFormats::Digits);
}

FString UBlueprintTopologyExporter::GetFunctionName(UEdGraphNode* Node)
{
    if (UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(Node))
    {
        const FName MemberName = CallFuncNode->FunctionReference.GetMemberName();
        if (MemberName != NAME_None)
        {
            return MemberName.ToString();
        }
        const FString Fallback = CallFuncNode->GetFunctionName().ToString();
        if (!Fallback.IsEmpty())
        {
            return Fallback;
        }
    }
    return TEXT("");
}

FString UBlueprintTopologyExporter::GetEventName(UEdGraphNode* Node)
{
    if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
    {
        const FName MemberName = EventNode->EventReference.GetMemberName();
        if (MemberName != NAME_None)
        {
            return MemberName.ToString();
        }
    }
    if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
    {
        const FName MemberName = CustomEventNode->CustomFunctionName;
        if (MemberName != NAME_None)
        {
            return MemberName.ToString();
        }
    }
    if (UK2Node_FunctionEntry* FuncEntry = Cast<UK2Node_FunctionEntry>(Node))
    {
        const FName MemberName = FuncEntry->FunctionReference.GetMemberName();
        if (MemberName != NAME_None)
        {
            return MemberName.ToString();
        }
    }
    return TEXT("");
}

FString UBlueprintTopologyExporter::GetVariableName(UEdGraphNode* Node)
{
    if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(Node))
    {
        return VarGet->VariableReference.GetMemberName().ToString();
    }
    if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(Node))
    {
        return VarSet->VariableReference.GetMemberName().ToString();
    }
    return TEXT("");
}

#else
// ============================================================================
// 非编辑器占位实现 — 打包 Shipping 构建时编译
// ============================================================================

FString UBlueprintTopologyExporter::DumpBlueprintLogicToJson(UBlueprint* TargetBP)
{
    return TEXT("{\"error\": \"Only available in Editor (WITH_EDITOR = 1)\"}");
}

#endif // WITH_EDITOR
