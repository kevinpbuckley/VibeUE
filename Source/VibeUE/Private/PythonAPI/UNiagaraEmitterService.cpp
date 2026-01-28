// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UNiagaraEmitterService.h"
#include "PythonAPI/UNiagaraService.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraComponentRendererProperties.h"
#include "NiagaraScriptSourceBase.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

// NiagaraEditor includes for graph traversal
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "EdGraph/EdGraph.h"
#include "NiagaraParameterStore.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

// Color curve data interface for hue shifting
#include "NiagaraDataInterfaceColorCurve.h"
#include "Curves/RichCurve.h"

// =================================================================
// Helper Methods
// =================================================================

UNiagaraSystem* UNiagaraEmitterService::LoadNiagaraSystem(const FString& SystemPath)
{
	if (SystemPath.IsEmpty())
	{
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(SystemPath);
	if (!LoadedObject)
	{
		return nullptr;
	}

	return Cast<UNiagaraSystem>(LoadedObject);
}

FNiagaraEmitterHandle* UNiagaraEmitterService::FindEmitterHandle(UNiagaraSystem* System, const FString& EmitterName)
{
	if (!System)
	{
		return nullptr;
	}

	TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase) ||
			Handle.GetUniqueInstanceName().Equals(EmitterName, ESearchCase::IgnoreCase))
		{
			return &Handle;
		}
	}

	return nullptr;
}

FString UNiagaraEmitterService::GetRendererTypeName(UNiagaraRendererProperties* Renderer)
{
	if (!Renderer)
	{
		return TEXT("Unknown");
	}

	if (Cast<UNiagaraSpriteRendererProperties>(Renderer))
	{
		return TEXT("Sprite");
	}
	else if (Cast<UNiagaraMeshRendererProperties>(Renderer))
	{
		return TEXT("Mesh");
	}
	else if (Cast<UNiagaraRibbonRendererProperties>(Renderer))
	{
		return TEXT("Ribbon");
	}
	else if (Cast<UNiagaraLightRendererProperties>(Renderer))
	{
		return TEXT("Light");
	}
	else if (Cast<UNiagaraComponentRendererProperties>(Renderer))
	{
		return TEXT("Component");
	}

	return Renderer->GetClass()->GetName();
}

// =================================================================
// Module Management Actions
// =================================================================

// Helper to convert ENiagaraScriptUsage to human-readable type string
static FString GetModuleTypeFromUsage(ENiagaraScriptUsage Usage)
{
	switch (Usage)
	{
	case ENiagaraScriptUsage::ParticleSpawnScript:
	case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
		return TEXT("ParticleSpawn");
	case ENiagaraScriptUsage::ParticleUpdateScript:
		return TEXT("ParticleUpdate");
	case ENiagaraScriptUsage::ParticleEventScript:
		return TEXT("ParticleEvent");
	case ENiagaraScriptUsage::ParticleSimulationStageScript:
		return TEXT("ParticleSimulation");
	case ENiagaraScriptUsage::EmitterSpawnScript:
		return TEXT("EmitterSpawn");
	case ENiagaraScriptUsage::EmitterUpdateScript:
		return TEXT("EmitterUpdate");
	case ENiagaraScriptUsage::SystemSpawnScript:
		return TEXT("SystemSpawn");
	case ENiagaraScriptUsage::SystemUpdateScript:
		return TEXT("SystemUpdate");
	default:
		return TEXT("Unknown");
	}
}

// Helper to read a static switch value from a module's function call node
// We iterate through pins directly since FindStaticSwitchInputPin is not exported
static FString GetStaticSwitchValue(UNiagaraNodeFunctionCall* FunctionCall, const FName& SwitchName)
{
	if (!FunctionCall)
	{
		return TEXT("");
	}

	// Look for a pin matching the switch name
	for (UEdGraphPin* Pin : FunctionCall->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input)
		{
			continue;
		}

		// Check if pin name contains the switch name
		FString PinName = Pin->PinName.ToString();
		if (PinName.Contains(SwitchName.ToString(), ESearchCase::IgnoreCase))
		{
			// Return the default value
			if (!Pin->DefaultValue.IsEmpty())
			{
				return Pin->DefaultValue;
			}
		}
	}

	return TEXT("");
}

// Helper to find the EmitterState module and read its lifecycle settings
static void ReadEmitterStateSettings(
	FVersionedNiagaraEmitterData* EmitterData,
	FString& OutLoopBehavior,
	FString& OutLoopDuration,
	FString& OutInactiveResponse)
{
	if (!EmitterData)
	{
		return;
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		return;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		return;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Find all function call nodes
	TArray<UNiagaraNodeFunctionCall*> AllFunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(AllFunctionCalls);

	for (UNiagaraNodeFunctionCall* FunctionCall : AllFunctionCalls)
	{
		if (!FunctionCall)
		{
			continue;
		}

		// Look for EmitterState module
		FString ModuleName = FunctionCall->GetFunctionName();
		if (ModuleName.Equals(TEXT("EmitterState"), ESearchCase::IgnoreCase) ||
			ModuleName.Contains(TEXT("EmitterState")))
		{
			// Read the static switch values
			FString LoopBehavior = GetStaticSwitchValue(FunctionCall, FName("Loop Behavior"));
			if (!LoopBehavior.IsEmpty())
			{
				OutLoopBehavior = LoopBehavior;
			}

			FString InactiveResponse = GetStaticSwitchValue(FunctionCall, FName("Inactive Response"));
			if (!InactiveResponse.IsEmpty())
			{
				OutInactiveResponse = InactiveResponse;
			}

			// Also check for alternative names
			if (OutLoopBehavior.IsEmpty())
			{
				OutLoopBehavior = GetStaticSwitchValue(FunctionCall, FName("LoopBehavior"));
			}
			if (OutInactiveResponse.IsEmpty())
			{
				OutInactiveResponse = GetStaticSwitchValue(FunctionCall, FName("InactiveResponse"));
			}

			// Loop duration is typically a rapid iteration parameter, not a static switch
			// We'll read it from the parameter store if available
			break;
		}
	}
}

// Helper to find the output node that a function call connects to by traversing output pins
static UNiagaraNodeOutput* FindOutputNodeForFunctionCall(UNiagaraNodeFunctionCall* FunctionCall)
{
	if (!FunctionCall)
	{
		return nullptr;
	}

	// Find the output pin that connects to parameter map
	TArray<UEdGraphPin*> OutputPins;
	for (UEdGraphPin* Pin : FunctionCall->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
		{
			OutputPins.Add(Pin);
		}
	}

	// Follow the output chain to find the output node
	TSet<UEdGraphNode*> VisitedNodes;
	TArray<UEdGraphNode*> NodesToCheck;
	
	for (UEdGraphPin* OutputPin : OutputPins)
	{
		for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				NodesToCheck.Add(LinkedPin->GetOwningNode());
			}
		}
	}

	while (NodesToCheck.Num() > 0)
	{
		UEdGraphNode* CurrentNode = NodesToCheck.Pop();
		if (VisitedNodes.Contains(CurrentNode))
		{
			continue;
		}
		VisitedNodes.Add(CurrentNode);

		// Check if this is an output node
		if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(CurrentNode))
		{
			return OutputNode;
		}

		// Continue following output pins
		for (UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						NodesToCheck.Add(LinkedPin->GetOwningNode());
					}
				}
			}
		}
	}

	return nullptr;
}

TArray<FNiagaraModuleInfo_Custom> UNiagaraEmitterService::ListModules(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleType)
{
	TArray<FNiagaraModuleInfo_Custom> Result;

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ListModules - System not found: %s"), *SystemPath);
		return Result;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ListModules - Emitter not found: %s"), *EmitterName);
		return Result;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ListModules - No emitter data found"));
		return Result;
	}

	// Get the graph source to traverse
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ListModules - No graph source found"));
		return Result;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ListModules - Could not cast to UNiagaraScriptSource"));
		return Result;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;
	if (!Graph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ListModules - No NodeGraph found"));
		return Result;
	}

	// Get all function call nodes from the graph using base class method
	TArray<UNiagaraNodeFunctionCall*> AllFunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(AllFunctionCalls);

	int32 ModuleIndex = 0;

	for (UNiagaraNodeFunctionCall* FunctionCall : AllFunctionCalls)
	{
		if (!FunctionCall)
		{
			continue;
		}

		// Find which output node this function call connects to
		UNiagaraNodeOutput* OutputNode = FindOutputNodeForFunctionCall(FunctionCall);
		FString TypeString = TEXT("Unknown");
		
		if (OutputNode)
		{
			ENiagaraScriptUsage Usage = OutputNode->GetUsage();
			TypeString = GetModuleTypeFromUsage(Usage);
		}

		// Filter by module type if specified
		if (!ModuleType.IsEmpty())
		{
			// Allow flexible matching
			if (!TypeString.Contains(ModuleType, ESearchCase::IgnoreCase) &&
				!ModuleType.Contains(TypeString, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		FNiagaraModuleInfo_Custom ModuleInfo;
		ModuleInfo.ModuleName = FunctionCall->GetFunctionName();
		ModuleInfo.ModuleType = TypeString;
		ModuleInfo.ModuleIndex = ModuleIndex++;
		ModuleInfo.bIsEnabled = FunctionCall->IsNodeEnabled();

		Result.Add(ModuleInfo);
	}

	UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::ListModules - Found %d modules in emitter '%s'"), Result.Num(), *EmitterName);
	return Result;
}

bool UNiagaraEmitterService::GetModuleInfo(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName,
	FNiagaraModuleInfo_Custom& OutInfo)
{
	TArray<FNiagaraModuleInfo_Custom> Modules = ListModules(SystemPath, EmitterName, TEXT(""));

	for (const FNiagaraModuleInfo_Custom& Module : Modules)
	{
		if (Module.ModuleName.Equals(ModuleName, ESearchCase::IgnoreCase))
		{
			OutInfo = Module;
			return true;
		}
	}

	return false;
}

bool UNiagaraEmitterService::AddModule(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleScriptPath,
	const FString& ModuleType)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - System not found: %s"), *SystemPath);
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Load the module script
	UObject* ScriptObj = UEditorAssetLibrary::LoadAsset(ModuleScriptPath);
	if (!ScriptObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - Script not found: %s"), *ModuleScriptPath);
		return false;
	}

	UNiagaraScript* ModuleScript = Cast<UNiagaraScript>(ScriptObj);
	if (!ModuleScript)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - Object is not a script: %s"), *ModuleScriptPath);
		return false;
	}

	// Get emitter data
	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - No emitter data found"));
		return false;
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - No graph source found"));
		return false;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - No script source or graph found"));
		return false;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Determine the target script usage from ModuleType
	ENiagaraScriptUsage TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
	UNiagaraScript* TargetScript = EmitterData->UpdateScriptProps.Script;
	
	if (ModuleType.Equals(TEXT("ParticleSpawn"), ESearchCase::IgnoreCase))
	{
		TargetUsage = ENiagaraScriptUsage::ParticleSpawnScript;
		TargetScript = EmitterData->SpawnScriptProps.Script;
	}
	else if (ModuleType.Equals(TEXT("ParticleUpdate"), ESearchCase::IgnoreCase))
	{
		TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
		TargetScript = EmitterData->UpdateScriptProps.Script;
	}
	else if (ModuleType.Equals(TEXT("EmitterSpawn"), ESearchCase::IgnoreCase))
	{
		TargetUsage = ENiagaraScriptUsage::EmitterSpawnScript;
#if WITH_EDITORONLY_DATA
		TargetScript = EmitterData->EmitterSpawnScriptProps.Script;
#endif
	}
	else if (ModuleType.Equals(TEXT("EmitterUpdate"), ESearchCase::IgnoreCase))
	{
		TargetUsage = ENiagaraScriptUsage::EmitterUpdateScript;
#if WITH_EDITORONLY_DATA
		TargetScript = EmitterData->EmitterUpdateScriptProps.Script;
#endif
	}

	if (!TargetScript)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - No target script for type: %s"), *ModuleType);
		return false;
	}

	// Find the output node for this script usage
	UNiagaraNodeOutput* OutputNode = Graph->FindEquivalentOutputNode(TargetUsage, TargetScript->GetUsageId());
	if (!OutputNode)
	{
		// Fallback: manually find output nodes by iterating through all nodes
		// (FindOutputNodes is not exported from NiagaraEditor)
		TArray<UNiagaraNodeOutput*> AllOutputNodes;
		Graph->GetNodesOfClass<UNiagaraNodeOutput>(AllOutputNodes);
		for (UNiagaraNodeOutput* TestNode : AllOutputNodes)
		{
			if (TestNode && TestNode->GetUsage() == TargetUsage)
			{
				OutputNode = TestNode;
				break;
			}
		}
	}

	if (!OutputNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - Could not find output node for usage: %s"), *ModuleType);
		return false;
	}

	// Mark graph for modification
	Graph->Modify();

	// Use the exported overload of AddScriptModuleToStack that takes individual parameters
	// NIAGARAEDITOR_API UNiagaraNodeFunctionCall* AddScriptModuleToStack(UNiagaraScript* ModuleScript, UNiagaraNodeOutput& TargetOutputNode, int32 TargetIndex, FString SuggestedName, const FGuid& VersionGuid)
	UNiagaraNodeFunctionCall* NewModuleNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
		ModuleScript, 
		*OutputNode, 
		INDEX_NONE,  // Add at end
		FString(),   // Use default name
		FGuid());    // Use default version

	if (!NewModuleNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddModule - AddScriptModuleToStack returned null for: %s"), *ModuleScriptPath);
		return false;
	}

	// Mark the system as dirty so changes are saved
	System->MarkPackageDirty();

	// Request a proper recompile (safer than ForceGraphToRecompileOnNextCheck)
	// This avoids crashes when the Niagara editor is open
	System->RequestCompile(false);
	System->WaitForCompilationComplete();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::AddModule - Successfully added module: %s to %s/%s"), 
		*ModuleScriptPath, *EmitterName, *ModuleType);

	return true;
}

bool UNiagaraEmitterService::RemoveModule(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::RemoveModule - System not found: %s"), *SystemPath);
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::RemoveModule - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Get emitter data
	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::RemoveModule - No emitter data found"));
		return false;
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		return false;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		return false;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Find the function call node for this module
	TArray<UNiagaraNodeFunctionCall*> AllFunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(AllFunctionCalls);

	UNiagaraNodeFunctionCall* TargetModule = nullptr;
	for (UNiagaraNodeFunctionCall* FunctionCall : AllFunctionCalls)
	{
		if (!FunctionCall) continue;

		FString FuncName = FunctionCall->GetFunctionName();
		if (FuncName.Equals(ModuleName, ESearchCase::IgnoreCase) ||
			FuncName.Contains(ModuleName, ESearchCase::IgnoreCase))
		{
			TargetModule = FunctionCall;
			break;
		}
	}

	if (!TargetModule)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::RemoveModule - Module not found: %s"), *ModuleName);
		return false;
	}

	// Mark graph for modification
	Graph->Modify();

	// Remove the node from the graph using standard UEdGraph methods
	// First break all pin links
	for (UEdGraphPin* Pin : TargetModule->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}

	// Remove the node from the graph
	Graph->RemoveNode(TargetModule);

	// Mark the system as dirty
	System->MarkPackageDirty();

	// Request a proper recompile (safer than ForceGraphToRecompileOnNextCheck)
	// This avoids crashes when the Niagara editor is open
	System->RequestCompile(false);
	System->WaitForCompilationComplete();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::RemoveModule - Successfully removed module: %s from %s"), 
		*ModuleName, *EmitterName);

	return true;
}

bool UNiagaraEmitterService::EnableModule(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName,
	bool bEnabled)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::EnableModule - System not found: %s"), *SystemPath);
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::EnableModule - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Get emitter data
	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		return false;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		return false;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Find the function call node for this module
	TArray<UNiagaraNodeFunctionCall*> AllFunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(AllFunctionCalls);

	UNiagaraNodeFunctionCall* TargetModule = nullptr;
	for (UNiagaraNodeFunctionCall* FunctionCall : AllFunctionCalls)
	{
		if (!FunctionCall) continue;

		FString FuncName = FunctionCall->GetFunctionName();
		if (FuncName.Equals(ModuleName, ESearchCase::IgnoreCase) ||
			FuncName.Contains(ModuleName, ESearchCase::IgnoreCase))
		{
			TargetModule = FunctionCall;
			break;
		}
	}

	if (!TargetModule)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::EnableModule - Module not found: %s"), *ModuleName);
		return false;
	}

	// Instead of using FNiagaraStackGraphUtilities::SetModuleIsEnabled which requires a valid stack context,
	// we directly modify the graph and recompile. The enabled state is stored in the node's usage bitmask.
	Graph->Modify();
	
	// Get current enabled state and check if we need to change it
	// IsNodeEnabled() returns true when enabled, false when disabled
	bool bCurrentlyEnabled = TargetModule->IsNodeEnabled();
	if (bCurrentlyEnabled == bEnabled)
	{
		UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::EnableModule - Module %s already %s"), 
			*ModuleName, bEnabled ? TEXT("enabled") : TEXT("disabled"));
		return true;  // Already in desired state
	}
	
	// Set the enabled state through the node's enable toggle
	// UNiagaraNodeFunctionCall uses bNodeEnabled property
	TargetModule->Modify();
	TargetModule->SetEnabledState(bEnabled ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled);
	
	// Notify the graph that it changed
	Graph->NotifyGraphChanged();

	// Mark the system as dirty
	System->MarkPackageDirty();

	// Request a proper recompile (safer than ForceGraphToRecompileOnNextCheck)
	// This avoids crashes when the Niagara editor is open
	System->RequestCompile(false);
	System->WaitForCompilationComplete();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::EnableModule - %s module: %s"), 
		bEnabled ? TEXT("Enabled") : TEXT("Disabled"), *ModuleName);

	return true;
}

bool UNiagaraEmitterService::SetModuleInput(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName,
	const FString& InputName,
	const FString& Value)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetModuleInput - System not found: %s"), *SystemPath);
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetModuleInput - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Get emitter data
	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		return false;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		return false;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Find the function call node for this module
	TArray<UNiagaraNodeFunctionCall*> AllFunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(AllFunctionCalls);

	UNiagaraNodeFunctionCall* TargetModule = nullptr;
	for (UNiagaraNodeFunctionCall* FunctionCall : AllFunctionCalls)
	{
		if (!FunctionCall) continue;

		FString FuncName = FunctionCall->GetFunctionName();
		if (FuncName.Equals(ModuleName, ESearchCase::IgnoreCase) ||
			FuncName.Contains(ModuleName, ESearchCase::IgnoreCase))
		{
			TargetModule = FunctionCall;
			break;
		}
	}

	if (!TargetModule)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetModuleInput - Module not found: %s"), *ModuleName);
		return false;
	}

	// Find the input pin matching the InputName
	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : TargetModule->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			FString PinName = Pin->PinName.ToString();
			if (PinName.Equals(InputName, ESearchCase::IgnoreCase) ||
				PinName.Contains(InputName, ESearchCase::IgnoreCase))
			{
				TargetPin = Pin;
				break;
			}
		}
	}

	if (!TargetPin)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetModuleInput - Input pin not found: %s on module %s"), *InputName, *ModuleName);
		return false;
	}

	// Set the pin's default value
	Graph->Modify();
	TargetModule->Modify();
	
	// Use the schema to set the default value properly
	const UEdGraphSchema_Niagara* NiagaraSchema = Cast<UEdGraphSchema_Niagara>(Graph->GetSchema());
	if (NiagaraSchema)
	{
		NiagaraSchema->TrySetDefaultValue(*TargetPin, Value, true);
	}
	else
	{
		// Fallback to direct assignment
		TargetPin->DefaultValue = Value;
	}

	// Mark the system as dirty
	System->MarkPackageDirty();

	// Request a proper recompile (safer than ForceGraphToRecompileOnNextCheck)
	// This avoids crashes when the Niagara editor is open
	System->RequestCompile(false);
	System->WaitForCompilationComplete();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::SetModuleInput - Set %s.%s = %s"), *ModuleName, *InputName, *Value);

	return true;
}

FString UNiagaraEmitterService::GetModuleInput(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName,
	const FString& InputName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return FString();
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return FString();
	}

	// Get emitter data
	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return FString();
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		return FString();
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		return FString();
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Find the function call node for this module
	TArray<UNiagaraNodeFunctionCall*> AllFunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(AllFunctionCalls);

	UNiagaraNodeFunctionCall* TargetModule = nullptr;
	for (UNiagaraNodeFunctionCall* FunctionCall : AllFunctionCalls)
	{
		if (!FunctionCall) continue;

		FString FuncName = FunctionCall->GetFunctionName();
		if (FuncName.Equals(ModuleName, ESearchCase::IgnoreCase) ||
			FuncName.Contains(ModuleName, ESearchCase::IgnoreCase))
		{
			TargetModule = FunctionCall;
			break;
		}
	}

	if (!TargetModule)
	{
		return FString();
	}

	// Check the default pin values on the function call node
	for (UEdGraphPin* Pin : TargetModule->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			FString PinName = Pin->PinName.ToString();
			if (PinName.Equals(InputName, ESearchCase::IgnoreCase) ||
				PinName.Contains(InputName, ESearchCase::IgnoreCase))
			{
				if (!Pin->DefaultValue.IsEmpty())
				{
					return Pin->DefaultValue;
				}
			}
		}
	}

	return FString();
}

bool UNiagaraEmitterService::ReorderModule(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName,
	int32 NewIndex)
{
	// NOTE: ReorderModule requires FNiagaraStackGraphUtilities::GetOrderedModuleNodes() and MoveModule()
	// which are NOT exported from the NiagaraEditor module. Module reordering must be done manually
	// in the Niagara Editor UI for now.
	UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::ReorderModule - Not yet implemented. "
		"The required NiagaraEditor APIs (GetOrderedModuleNodes, MoveModule) are not exported. "
		"Please reorder modules manually in the Niagara Editor UI."));
	return false;
}

bool UNiagaraEmitterService::SetColorTint(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& RGB,
	float Alpha)
{
	// Check if ScaleColor module exists
	TArray<FNiagaraModuleInfo_Custom> Modules = ListModules(SystemPath, EmitterName, TEXT("Update"));
	bool bHasScaleColor = false;
	for (const FNiagaraModuleInfo_Custom& Module : Modules)
	{
		if (Module.ModuleName.Contains(TEXT("ScaleColor")))
		{
			bHasScaleColor = true;
			break;
		}
	}

	// Add ScaleColor module if not present
	if (!bHasScaleColor)
	{
		bool bAdded = AddModule(
			SystemPath,
			EmitterName,
			TEXT("/Niagara/Modules/Update/Color/ScaleColor.ScaleColor"),
			TEXT("Update")
		);
		if (!bAdded)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetColorTint - Failed to add ScaleColor module to %s"), *EmitterName);
			return false;
		}
		UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::SetColorTint - Added ScaleColor module to %s"), *EmitterName);
	}

	// Set Scale RGB via rapid iteration params
	FString RGBParamName = FString::Printf(TEXT("Constants.%s.ScaleColor.Scale RGB"), *EmitterName);
	bool bRGBSet = UNiagaraService::SetRapidIterationParam(SystemPath, EmitterName, RGBParamName, RGB);

	// Set Scale Alpha if not default
	bool bAlphaSet = true;
	if (Alpha != 1.0f)
	{
		FString AlphaParamName = FString::Printf(TEXT("Constants.%s.ScaleColor.Scale Alpha"), *EmitterName);
		FString AlphaStr = FString::Printf(TEXT("%.6f"), Alpha);
		bAlphaSet = UNiagaraService::SetRapidIterationParam(SystemPath, EmitterName, AlphaParamName, AlphaStr);
	}

	if (bRGBSet)
	{
		UE_LOG(LogTemp, Log, TEXT("UNiagaraEmitterService::SetColorTint - Set %s color tint to %s (alpha: %.2f)"), *EmitterName, *RGB, Alpha);
	}

	return bRGBSet && bAlphaSet;
}

// =================================================================
// Color Curve Manipulation (Hue Shifting)
// =================================================================

UNiagaraDataInterfaceColorCurve* UNiagaraEmitterService::FindColorCurveDataInterface(
	UNiagaraSystem* System,
	const FString& EmitterName,
	const FString& ModuleName)
{
	if (!System)
	{
		return nullptr;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindColorCurveDataInterface - Emitter not found: %s"), *EmitterName);
		return nullptr;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return nullptr;
	}

	// Get the graph source
	UNiagaraScriptSourceBase* SourceBase = EmitterData->GraphSource;
	if (!SourceBase)
	{
		return nullptr;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SourceBase);
	if (!ScriptSource || !ScriptSource->NodeGraph)
	{
		return nullptr;
	}

	UNiagaraGraph* Graph = ScriptSource->NodeGraph;

	// Find the ColorFromCurve function call node
	TArray<UNiagaraNodeFunctionCall*> FunctionCalls;
	Graph->GetNodesOfClass<UNiagaraNodeFunctionCall>(FunctionCalls);

	for (UNiagaraNodeFunctionCall* FunctionCall : FunctionCalls)
	{
		if (!FunctionCall)
		{
			continue;
		}

		FString FuncName = FunctionCall->GetFunctionName();
		if (FuncName.Contains(ModuleName, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Log, TEXT("FindColorCurveDataInterface - Found module node: %s"), *FuncName);
			
			// Found the ColorFromCurve node
			// Look for the color curve input pin and follow it to the actual UNiagaraNodeInput
			// which holds the REAL persistent DataInterface
			for (UEdGraphPin* Pin : FunctionCall->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input)
				{
					continue;
				}

				// Check pin name for likely color curve pins
				FString PinName = Pin->PinName.ToString();
				bool bIsColorCurvePin = PinName.Contains(TEXT("Color"), ESearchCase::IgnoreCase) ||
				                        PinName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase);
				
				UE_LOG(LogTemp, Log, TEXT("  Pin: %s, LinkedTo: %d, DefaultObject: %s"), 
					*PinName,
					Pin->LinkedTo.Num(),
					Pin->DefaultObject ? *Pin->DefaultObject->GetName() : TEXT("null"));

				// CRITICAL: Follow the linked pin to find the UNiagaraNodeInput that holds the actual DI
				// This is how the editor accesses the persistent DataInterface!
				if (Pin->LinkedTo.Num() > 0)
				{
					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin || !LinkedPin->GetOwningNode())
						{
							continue;
						}

						UE_LOG(LogTemp, Log, TEXT("    LinkedTo node: %s"), *LinkedPin->GetOwningNode()->GetClass()->GetName());

						// Check if this is a UNiagaraNodeInput - this is where the REAL DI lives!
						if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(LinkedPin->GetOwningNode()))
						{
							// Use reflection to access the private DataInterface member
							// since GetDataInterface() isn't exported from NiagaraEditor module
							FObjectProperty* DIProperty = FindFProperty<FObjectProperty>(UNiagaraNodeInput::StaticClass(), TEXT("DataInterface"));
							UNiagaraDataInterface* DI = nullptr;
							if (DIProperty)
							{
								DI = Cast<UNiagaraDataInterface>(DIProperty->GetObjectPropertyValue_InContainer(InputNode));
							}
							
							UE_LOG(LogTemp, Log, TEXT("    -> InputNode has DataInterface: %s"), 
								DI ? *DI->GetClass()->GetName() : TEXT("null"));
							
							if (UNiagaraDataInterfaceColorCurve* ColorCurveDI = Cast<UNiagaraDataInterfaceColorCurve>(DI))
							{
								UE_LOG(LogTemp, Log, TEXT("    -> FOUND PERSISTENT ColorCurveDI on UNiagaraNodeInput! (Outer: %s)"),
									*ColorCurveDI->GetOuter()->GetName());
								return ColorCurveDI;
							}
						}
					}
				}

				// Fallback: Check if this pin has a default object that's a color curve DI
				if (Pin->DefaultObject)
				{
					if (UNiagaraDataInterfaceColorCurve* ColorCurveDI = Cast<UNiagaraDataInterfaceColorCurve>(Pin->DefaultObject))
					{
						UE_LOG(LogTemp, Log, TEXT("  -> Found ColorCurveDI on pin DefaultObject!"));
						return ColorCurveDI;
					}
				}
			}
		}
	}

	// Fallback: Look for UNiagaraNodeInput nodes directly in the graph that have ColorCurve DIs
	UE_LOG(LogTemp, Log, TEXT("FindColorCurveDataInterface - Searching for UNiagaraNodeInput nodes with ColorCurve DI..."));
	TArray<UNiagaraNodeInput*> InputNodes;
	Graph->GetNodesOfClass<UNiagaraNodeInput>(InputNodes);
	
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (!InputNode)
		{
			continue;
		}
		
		// Use reflection to access the private DataInterface member
		FObjectProperty* DIProperty = FindFProperty<FObjectProperty>(UNiagaraNodeInput::StaticClass(), TEXT("DataInterface"));
		UNiagaraDataInterface* DI = nullptr;
		if (DIProperty)
		{
			DI = Cast<UNiagaraDataInterface>(DIProperty->GetObjectPropertyValue_InContainer(InputNode));
		}
		
		if (UNiagaraDataInterfaceColorCurve* ColorCurveDI = Cast<UNiagaraDataInterfaceColorCurve>(DI))
		{
			// Check if the node name or variable contains our module name
			FString InputName = InputNode->Input.GetName().ToString();
			UE_LOG(LogTemp, Log, TEXT("  Found InputNode with ColorCurveDI: %s"), *InputName);
			
			if (InputName.Contains(ModuleName, ESearchCase::IgnoreCase) ||
				InputName.Contains(TEXT("ColorCurve"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Log, TEXT("  -> MATCH! Using this ColorCurveDI (Outer: %s)"), *ColorCurveDI->GetOuter()->GetName());
				return ColorCurveDI;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("FindColorCurveDataInterface - ColorFromCurve module '%s' not found in emitter '%s'"), *ModuleName, *EmitterName);
	return nullptr;
}

TArray<FNiagaraColorCurveKey> UNiagaraEmitterService::GetColorCurveKeys(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ModuleName)
{
	TArray<FNiagaraColorCurveKey> Result;

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetColorCurveKeys - System not found: %s"), *SystemPath);
		return Result;
	}

	UNiagaraDataInterfaceColorCurve* ColorCurveDI = FindColorCurveDataInterface(System, EmitterName, ModuleName);
	if (!ColorCurveDI)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetColorCurveKeys - Color curve data interface not found for '%s' in '%s'"), *ModuleName, *EmitterName);
		return Result;
	}

	// Get keys from each channel - we need to collect all unique time values
	TSet<float> UniqueTimeValues;

	TArray<FRichCurveKey> RedKeys = ColorCurveDI->RedCurve.GetCopyOfKeys();
	TArray<FRichCurveKey> GreenKeys = ColorCurveDI->GreenCurve.GetCopyOfKeys();
	TArray<FRichCurveKey> BlueKeys = ColorCurveDI->BlueCurve.GetCopyOfKeys();
	TArray<FRichCurveKey> AlphaKeys = ColorCurveDI->AlphaCurve.GetCopyOfKeys();

	for (const FRichCurveKey& Key : RedKeys) UniqueTimeValues.Add(Key.Time);
	for (const FRichCurveKey& Key : GreenKeys) UniqueTimeValues.Add(Key.Time);
	for (const FRichCurveKey& Key : BlueKeys) UniqueTimeValues.Add(Key.Time);
	for (const FRichCurveKey& Key : AlphaKeys) UniqueTimeValues.Add(Key.Time);

	// Sort time values
	TArray<float> SortedTimes = UniqueTimeValues.Array();
	SortedTimes.Sort();

	// Sample the curves at each time to get RGBA values
	for (float Time : SortedTimes)
	{
		FNiagaraColorCurveKey ColorKey;
		ColorKey.Time = Time;
		ColorKey.R = ColorCurveDI->RedCurve.Eval(Time);
		ColorKey.G = ColorCurveDI->GreenCurve.Eval(Time);
		ColorKey.B = ColorCurveDI->BlueCurve.Eval(Time);
		ColorKey.A = ColorCurveDI->AlphaCurve.Eval(Time);
		Result.Add(ColorKey);
	}

	UE_LOG(LogTemp, Log, TEXT("GetColorCurveKeys - Retrieved %d color curve keys from '%s' in '%s'"), Result.Num(), *ModuleName, *EmitterName);
	return Result;
}

bool UNiagaraEmitterService::SetColorCurveKeys(
	const FString& SystemPath,
	const FString& EmitterName,
	const TArray<FNiagaraColorCurveKey>& Keys,
	const FString& ModuleName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetColorCurveKeys - System not found: %s"), *SystemPath);
		return false;
	}

	UNiagaraDataInterfaceColorCurve* ColorCurveDI = FindColorCurveDataInterface(System, EmitterName, ModuleName);
	if (!ColorCurveDI)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetColorCurveKeys - Color curve data interface not found for '%s' in '%s'"), *ModuleName, *EmitterName);
		return false;
	}

	// Mark for modification
	ColorCurveDI->Modify();

	// Reset all curves
	ColorCurveDI->RedCurve.Reset();
	ColorCurveDI->GreenCurve.Reset();
	ColorCurveDI->BlueCurve.Reset();
	ColorCurveDI->AlphaCurve.Reset();

	// Add keys to each curve
	for (const FNiagaraColorCurveKey& ColorKey : Keys)
	{
		ColorCurveDI->RedCurve.AddKey(ColorKey.Time, ColorKey.R);
		ColorCurveDI->GreenCurve.AddKey(ColorKey.Time, ColorKey.G);
		ColorCurveDI->BlueCurve.AddKey(ColorKey.Time, ColorKey.B);
		ColorCurveDI->AlphaCurve.AddKey(ColorKey.Time, ColorKey.A);
	}

	// Auto-set tangents for smooth curves
	ColorCurveDI->RedCurve.AutoSetTangents();
	ColorCurveDI->GreenCurve.AutoSetTangents();
	ColorCurveDI->BlueCurve.AutoSetTangents();
	ColorCurveDI->AlphaCurve.AutoSetTangents();

	// Update LUT if the curve uses it
	ColorCurveDI->UpdateLUT();
	
	// Mark the system package as dirty so changes persist on save
	if (System)
	{
		System->Modify();
		System->MarkPackageDirty();
	}

	UE_LOG(LogTemp, Log, TEXT("SetColorCurveKeys - Set %d color curve keys on '%s' in '%s'"), Keys.Num(), *ModuleName, *EmitterName);
	return true;
}

bool UNiagaraEmitterService::ShiftColorHue(
	const FString& SystemPath,
	const FString& EmitterName,
	float HueShiftDegrees,
	const FString& ModuleName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShiftColorHue - System not found: %s"), *SystemPath);
		return false;
	}

	UNiagaraDataInterfaceColorCurve* ColorCurveDI = FindColorCurveDataInterface(System, EmitterName, ModuleName);
	if (!ColorCurveDI)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShiftColorHue - Color curve data interface not found for '%s' in '%s'"), *ModuleName, *EmitterName);
		return false;
	}

	// Collect all unique time values from all channels
	TSet<float> UniqueTimeValues;
	TArray<FRichCurveKey> RedKeys = ColorCurveDI->RedCurve.GetCopyOfKeys();
	TArray<FRichCurveKey> GreenKeys = ColorCurveDI->GreenCurve.GetCopyOfKeys();
	TArray<FRichCurveKey> BlueKeys = ColorCurveDI->BlueCurve.GetCopyOfKeys();

	for (const FRichCurveKey& Key : RedKeys) UniqueTimeValues.Add(Key.Time);
	for (const FRichCurveKey& Key : GreenKeys) UniqueTimeValues.Add(Key.Time);
	for (const FRichCurveKey& Key : BlueKeys) UniqueTimeValues.Add(Key.Time);

	TArray<float> SortedTimes = UniqueTimeValues.Array();
	SortedTimes.Sort();

	if (SortedTimes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShiftColorHue - No color curve keys found in '%s'"), *ModuleName);
		return false;
	}

	// Mark for modification
	ColorCurveDI->Modify();

	// Store new values
	TArray<TPair<float, FLinearColor>> NewValues;

	for (float Time : SortedTimes)
	{
		// Sample RGB at this time
		float R = ColorCurveDI->RedCurve.Eval(Time);
		float G = ColorCurveDI->GreenCurve.Eval(Time);
		float B = ColorCurveDI->BlueCurve.Eval(Time);

		// Convert to HSV
		FLinearColor OrigColor(R, G, B, 1.0f);
		FLinearColor HSV = OrigColor.LinearRGBToHSV();

		// Shift hue (HSV.R is hue in 0-360 range)
		HSV.R = FMath::Fmod(HSV.R + HueShiftDegrees + 360.0f, 360.0f);

		// Convert back to RGB
		FLinearColor NewColor = HSV.HSVToLinearRGB();

		NewValues.Add(TPair<float, FLinearColor>(Time, NewColor));
	}

	// Reset RGB curves (keep alpha unchanged)
	ColorCurveDI->RedCurve.Reset();
	ColorCurveDI->GreenCurve.Reset();
	ColorCurveDI->BlueCurve.Reset();

	// Add new keys
	for (const TPair<float, FLinearColor>& Pair : NewValues)
	{
		ColorCurveDI->RedCurve.AddKey(Pair.Key, Pair.Value.R);
		ColorCurveDI->GreenCurve.AddKey(Pair.Key, Pair.Value.G);
		ColorCurveDI->BlueCurve.AddKey(Pair.Key, Pair.Value.B);
	}

	// Auto-set tangents for smooth interpolation
	ColorCurveDI->RedCurve.AutoSetTangents();
	ColorCurveDI->GreenCurve.AutoSetTangents();
	ColorCurveDI->BlueCurve.AutoSetTangents();

	// Update LUT
	ColorCurveDI->UpdateLUT();
	
	// Mark the system package as dirty so changes persist on save
	if (System)
	{
		System->Modify();
		System->MarkPackageDirty();
	}

	UE_LOG(LogTemp, Log, TEXT("ShiftColorHue - Shifted hue by %.1f degrees on '%s' in '%s'"), HueShiftDegrees, *ModuleName, *EmitterName);
	return true;
}

// =================================================================
// Renderer Management Actions
// =================================================================

TArray<FNiagaraRendererInfo_Custom> UNiagaraEmitterService::ListRenderers(
	const FString& SystemPath,
	const FString& EmitterName)
{
	TArray<FNiagaraRendererInfo_Custom> Result;

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return Result;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return Result;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return Result;
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	for (int32 i = 0; i < Renderers.Num(); ++i)
	{
		UNiagaraRendererProperties* Renderer = Renderers[i];
		if (Renderer)
		{
			FNiagaraRendererInfo_Custom RendererInfo;
			RendererInfo.RendererName = Renderer->GetName();
			RendererInfo.RendererType = GetRendererTypeName(Renderer);
			RendererInfo.RendererIndex = i;
			RendererInfo.bIsEnabled = Renderer->GetIsEnabled();
			Result.Add(RendererInfo);
		}
	}

	return Result;
}

bool UNiagaraEmitterService::AddRenderer(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& RendererType)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Determine renderer class
	UClass* RendererClass = nullptr;
	if (RendererType.Equals(TEXT("Sprite"), ESearchCase::IgnoreCase))
	{
		RendererClass = UNiagaraSpriteRendererProperties::StaticClass();
	}
	else if (RendererType.Equals(TEXT("Mesh"), ESearchCase::IgnoreCase))
	{
		RendererClass = UNiagaraMeshRendererProperties::StaticClass();
	}
	else if (RendererType.Equals(TEXT("Ribbon"), ESearchCase::IgnoreCase))
	{
		RendererClass = UNiagaraRibbonRendererProperties::StaticClass();
	}
	else if (RendererType.Equals(TEXT("Light"), ESearchCase::IgnoreCase))
	{
		RendererClass = UNiagaraLightRendererProperties::StaticClass();
	}
	else if (RendererType.Equals(TEXT("Component"), ESearchCase::IgnoreCase))
	{
		RendererClass = UNiagaraComponentRendererProperties::StaticClass();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddRenderer - Unknown renderer type: %s"), *RendererType);
		return false;
	}

	// Get the versioned emitter instance to access the emitter and version GUID
	FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
	UNiagaraEmitter* Emitter = VersionedEmitter.Emitter.Get();
	if (!Emitter)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddRenderer - Failed to get emitter instance"));
		return false;
	}

	// Create the renderer with the emitter as outer (proper ownership)
	UNiagaraRendererProperties* NewRenderer = NewObject<UNiagaraRendererProperties>(Emitter, RendererClass, NAME_None, RF_Transactional);
	if (!NewRenderer)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::AddRenderer - Failed to create renderer"));
		return false;
	}

	// Use the proper UE 5.7 API to add the renderer
	Emitter->AddRenderer(NewRenderer, VersionedEmitter.Version);

	System->MarkPackageDirty();
	return true;
}

bool UNiagaraEmitterService::RemoveRenderer(
	const FString& SystemPath,
	const FString& EmitterName,
	int32 RendererIndex)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::RemoveRenderer - Index out of range: %d"), RendererIndex);
		return false;
	}

	UNiagaraRendererProperties* RendererToRemove = Renderers[RendererIndex];
	if (RendererToRemove)
	{
		// Get the versioned emitter instance to access the emitter and version GUID
		FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
		UNiagaraEmitter* Emitter = VersionedEmitter.Emitter.Get();
		if (!Emitter)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::RemoveRenderer - Failed to get emitter instance"));
			return false;
		}

		// Use the proper UE 5.7 API to remove the renderer
		Emitter->RemoveRenderer(RendererToRemove, VersionedEmitter.Version);
	}

	System->MarkPackageDirty();
	return true;
}

bool UNiagaraEmitterService::EnableRenderer(
	const FString& SystemPath,
	const FString& EmitterName,
	int32 RendererIndex,
	bool bEnabled)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		return false;
	}

	UNiagaraRendererProperties* Renderer = Renderers[RendererIndex];
	if (Renderer)
	{
		Renderer->SetIsEnabled(bEnabled);
		System->MarkPackageDirty();
		return true;
	}

	return false;
}

bool UNiagaraEmitterService::SetRendererProperty(
	const FString& SystemPath,
	const FString& EmitterName,
	int32 RendererIndex,
	const FString& PropertyName,
	const FString& Value)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		return false;
	}

	UNiagaraRendererProperties* Renderer = Renderers[RendererIndex];
	if (!Renderer)
	{
		return false;
	}

	// Use reflection to set the property
	FProperty* Property = Renderer->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetRendererProperty - Property not found: %s"), *PropertyName);
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Renderer);
	if (!ValuePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraEmitterService::SetRendererProperty - Failed to get value pointer for property: %s"), *PropertyName);
		return false;
	}

	// Handle common property types
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = Value.ToBool() || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		BoolProp->SetPropertyValue(ValuePtr, BoolValue);
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		FloatProp->SetPropertyValue(ValuePtr, FCString::Atof(*Value));
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		IntProp->SetPropertyValue(ValuePtr, FCString::Atoi(*Value));
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		StrProp->SetPropertyValue(ValuePtr, Value);
	}
	else
	{
		// Try to import from text for complex types
		Property->ImportText_Direct(*Value, ValuePtr, Renderer, 0);
	}

	System->MarkPackageDirty();
	return true;
}

// =================================================================
// Script Discovery Actions
// =================================================================

TArray<FString> UNiagaraEmitterService::SearchModuleScripts(
	const FString& NameFilter,
	const FString& ModuleType)
{
	TArray<FString> Result;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UNiagaraScript::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(TEXT("/Niagara")));
	Filter.PackagePaths.Add(FName(TEXT("/Game")));

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		FString AssetName = Asset.AssetName.ToString();

		// Filter by name
		if (!NameFilter.IsEmpty() && !AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Filter by module type based on path
		FString AssetPath = Asset.GetObjectPathString();
		if (!ModuleType.IsEmpty())
		{
			bool bMatchesType = false;
			if (ModuleType.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase) && AssetPath.Contains(TEXT("Spawn")))
			{
				bMatchesType = true;
			}
			else if (ModuleType.Equals(TEXT("Update"), ESearchCase::IgnoreCase) && AssetPath.Contains(TEXT("Update")))
			{
				bMatchesType = true;
			}
			else if (ModuleType.Equals(TEXT("Event"), ESearchCase::IgnoreCase) && AssetPath.Contains(TEXT("Event")))
			{
				bMatchesType = true;
			}

			if (!bMatchesType)
			{
				continue;
			}
		}

		Result.Add(AssetPath);
	}

	return Result;
}

bool UNiagaraEmitterService::GetScriptInfo(
	const FString& ScriptPath,
	FNiagaraScriptInfo_Custom& OutInfo)
{
	UObject* ScriptObj = UEditorAssetLibrary::LoadAsset(ScriptPath);
	if (!ScriptObj)
	{
		return false;
	}

	UNiagaraScript* Script = Cast<UNiagaraScript>(ScriptObj);
	if (!Script)
	{
		return false;
	}

	OutInfo.ScriptName = Script->GetName();
	OutInfo.ScriptPath = ScriptPath;

	// Get usage type
	ENiagaraScriptUsage Usage = Script->GetUsage();
	switch (Usage)
	{
	case ENiagaraScriptUsage::Module:
		OutInfo.ScriptUsage = TEXT("Module");
		break;
	case ENiagaraScriptUsage::DynamicInput:
		OutInfo.ScriptUsage = TEXT("DynamicInput");
		break;
	case ENiagaraScriptUsage::Function:
		OutInfo.ScriptUsage = TEXT("Function");
		break;
	default:
		OutInfo.ScriptUsage = TEXT("Other");
		break;
	}

	// Description property removed in UE 5.7
	OutInfo.Description = TEXT("");

	return true;
}

TArray<FString> UNiagaraEmitterService::ListBuiltinModules(const FString& ModuleType)
{
	TArray<FString> Result;

	// Common built-in modules by category
	if (ModuleType.IsEmpty() || ModuleType.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase))
	{
		Result.Add(TEXT("/Niagara/Modules/Spawn/Initialization/InitializeParticle"));
		Result.Add(TEXT("/Niagara/Modules/Spawn/Location/SpawnBurst"));
		Result.Add(TEXT("/Niagara/Modules/Spawn/Location/SpawnPerUnit"));
		Result.Add(TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityInCone"));
	}

	if (ModuleType.IsEmpty() || ModuleType.Equals(TEXT("Update"), ESearchCase::IgnoreCase))
	{
		Result.Add(TEXT("/Niagara/Modules/Update/Acceleration/Gravity"));
		Result.Add(TEXT("/Niagara/Modules/Update/Acceleration/Drag"));
		Result.Add(TEXT("/Niagara/Modules/Update/Color/ColorByLife"));
		Result.Add(TEXT("/Niagara/Modules/Update/Color/ColorBySpeed"));
		Result.Add(TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize"));
		Result.Add(TEXT("/Niagara/Modules/Update/Size/ScaleSpriteBySpeed"));
		Result.Add(TEXT("/Niagara/Modules/Update/Lifetime/ParticleLifetime"));
		Result.Add(TEXT("/Niagara/Modules/Update/Forces/PointAttraction"));
		Result.Add(TEXT("/Niagara/Modules/Update/Forces/Vortex"));
	}

	if (ModuleType.IsEmpty() || ModuleType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
	{
		Result.Add(TEXT("/Niagara/Modules/Events/GenerateLocationEvent"));
		Result.Add(TEXT("/Niagara/Modules/Events/GenerateDeathEvent"));
	}

	return Result;
}

// =================================================================
// Diagnostic Actions
// =================================================================

bool UNiagaraEmitterService::GetRendererDetails(
	const FString& SystemPath,
	const FString& EmitterName,
	int32 RendererIndex,
	FNiagaraRendererDetailedInfo& OutInfo)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
	if (RendererIndex < 0 || RendererIndex >= Renderers.Num())
	{
		return false;
	}

	UNiagaraRendererProperties* Renderer = Renderers[RendererIndex];
	if (!Renderer)
	{
		return false;
	}

	// Basic info
	OutInfo.RendererName = Renderer->GetName();
	OutInfo.RendererType = GetRendererTypeName(Renderer);
	OutInfo.RendererIndex = RendererIndex;
	OutInfo.bIsEnabled = Renderer->GetIsEnabled();

	// Material info - access directly from renderer type
	OutInfo.MaterialPath = TEXT("");
	OutInfo.bHasMaterial = false;

	// Type-specific info
	if (UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer))
	{
		// Get material directly from sprite renderer
		if (SpriteRenderer->Material)
		{
			OutInfo.MaterialPath = SpriteRenderer->Material->GetPathName();
			OutInfo.bHasMaterial = true;
		}
		
		FVector2D SubImageSize = SpriteRenderer->SubImageSize;
		OutInfo.SubImageSize = FString::Printf(TEXT("(X=%f,Y=%f)"), SubImageSize.X, SubImageSize.Y);
		OutInfo.Alignment = StaticEnum<ENiagaraSpriteAlignment>()->GetNameStringByValue((int64)SpriteRenderer->Alignment);
		OutInfo.FacingMode = StaticEnum<ENiagaraSpriteFacingMode>()->GetNameStringByValue((int64)SpriteRenderer->FacingMode);
		OutInfo.SortMode = StaticEnum<ENiagaraSortMode>()->GetNameStringByValue((int64)SpriteRenderer->SortMode);
	}
	else if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
	{
		// Get first mesh if available
		const TArray<FNiagaraMeshRendererMeshProperties>& Meshes = MeshRenderer->Meshes;
		if (Meshes.Num() > 0 && Meshes[0].Mesh)
		{
			OutInfo.MeshPath = Meshes[0].Mesh->GetPathName();
		}
		// Get material from override material or mesh default
		if (MeshRenderer->OverrideMaterials.Num() > 0 && MeshRenderer->OverrideMaterials[0].ExplicitMat)
		{
			OutInfo.MaterialPath = MeshRenderer->OverrideMaterials[0].ExplicitMat->GetPathName();
			OutInfo.bHasMaterial = true;
		}
		OutInfo.SortMode = StaticEnum<ENiagaraSortMode>()->GetNameStringByValue((int64)MeshRenderer->SortMode);
	}
	else if (UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Renderer))
	{
		// Get material from ribbon renderer
		if (RibbonRenderer->Material)
		{
			OutInfo.MaterialPath = RibbonRenderer->Material->GetPathName();
			OutInfo.bHasMaterial = true;
		}
		OutInfo.RibbonShape = StaticEnum<ENiagaraRibbonShapeMode>()->GetNameStringByValue((int64)RibbonRenderer->Shape);
	}

	return true;
}

bool UNiagaraEmitterService::GetEmitterProperties(
	const FString& SystemPath,
	const FString& EmitterName,
	FNiagaraEmitterPropertiesInfo& OutInfo)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Basic info
	OutInfo.EmitterName = Handle->GetUniqueInstanceName();
	OutInfo.bIsEnabled = Handle->GetIsEnabled();

	// Simulation settings
	OutInfo.SimTarget = StaticEnum<ENiagaraSimTarget>()->GetNameStringByValue((int64)EmitterData->SimTarget);
	OutInfo.bLocalSpace = EmitterData->bLocalSpace;
	OutInfo.bDeterminism = EmitterData->bDeterminism;
	OutInfo.RandomSeed = EmitterData->RandomSeed;

	// Bounds
	OutInfo.CalculateBoundsMode = StaticEnum<ENiagaraEmitterCalculateBoundMode>()->GetNameStringByValue((int64)EmitterData->CalculateBoundsMode);
	FBox Bounds = EmitterData->FixedBounds;
	OutInfo.FixedBounds = FString::Printf(TEXT("Min(%f,%f,%f) Max(%f,%f,%f)"),
		Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z,
		Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z);

	// Allocation
	OutInfo.AllocationMode = StaticEnum<EParticleAllocationMode>()->GetNameStringByValue((int64)EmitterData->AllocationMode);
	OutInfo.PreAllocationCount = EmitterData->PreAllocationCount;

	// Lifecycle info from EmitterState module - read static switch values
	OutInfo.LoopBehavior = TEXT("Unknown");
	OutInfo.LoopDuration = TEXT("Unknown");
	OutInfo.InactiveResponse = TEXT("Unknown");
	ReadEmitterStateSettings(EmitterData, OutInfo.LoopBehavior, OutInfo.LoopDuration, OutInfo.InactiveResponse);

	return true;
}

// Helper to convert FNiagaraVariable value to string
static FString VariableValueToString(const FNiagaraParameterStore& Store, const FNiagaraVariable& Var)
{
	const FNiagaraTypeDefinition& TypeDef = Var.GetType();
	int32 Offset = Store.IndexOf(Var);
	
	if (Offset == INDEX_NONE)
	{
		return TEXT("(not found)");
	}
	
	const uint8* Data = Store.GetParameterData(Offset, TypeDef);
	if (!Data)
	{
		return TEXT("(no data)");
	}
	
	// Handle common types
	FName TypeName = TypeDef.GetFName();
	int32 Size = TypeDef.GetSize();
	
	// Bool
	if (TypeDef == FNiagaraTypeDefinition::GetBoolDef() || TypeName == FName("bool") || Size == 1)
	{
		return (*Data != 0) ? TEXT("true") : TEXT("false");
	}
	
	// Int32
	if (TypeDef == FNiagaraTypeDefinition::GetIntDef() || TypeName == FName("int32") || TypeName == FName("int"))
	{
		int32 Value = 0;
		FMemory::Memcpy(&Value, Data, sizeof(int32));
		return FString::Printf(TEXT("%d"), Value);
	}
	
	// Float
	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef() || TypeName == FName("float"))
	{
		float Value = 0.0f;
		FMemory::Memcpy(&Value, Data, sizeof(float));
		return FString::Printf(TEXT("%f"), Value);
	}
	
	// FVector (FVector3f internally in Niagara)
	if (TypeDef == FNiagaraTypeDefinition::GetVec3Def() || TypeName.ToString().Contains(TEXT("Vector")))
	{
		if (Size >= sizeof(FVector3f))
		{
			FVector3f Value;
			FMemory::Memcpy(&Value, Data, sizeof(FVector3f));
			return FString::Printf(TEXT("(%f, %f, %f)"), Value.X, Value.Y, Value.Z);
		}
	}
	
	// FLinearColor / FVector4
	if (TypeDef == FNiagaraTypeDefinition::GetColorDef() || TypeName.ToString().Contains(TEXT("Color")) || TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		if (Size >= sizeof(FLinearColor))
		{
			FLinearColor Value;
			FMemory::Memcpy(&Value, Data, sizeof(FLinearColor));
			return FString::Printf(TEXT("(R=%f, G=%f, B=%f, A=%f)"), Value.R, Value.G, Value.B, Value.A);
		}
	}
	
	// FVector2D
	if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		if (Size >= sizeof(FVector2f))
		{
			FVector2f Value;
			FMemory::Memcpy(&Value, Data, sizeof(FVector2f));
			return FString::Printf(TEXT("(%f, %f)"), Value.X, Value.Y);
		}
	}
	
	// For enums and other types, try to represent the raw bytes
	if (Size <= 4)
	{
		int32 IntValue = 0;
		FMemory::Memcpy(&IntValue, Data, FMath::Min(Size, 4));
		return FString::Printf(TEXT("(raw: %d, type: %s, size: %d)"), IntValue, *TypeName.ToString(), Size);
	}
	
	return FString::Printf(TEXT("(type: %s, size: %d bytes)"), *TypeName.ToString(), Size);
}

// Helper to extract parameters from a script
static void ExtractScriptParameters(
	UNiagaraScript* Script, 
	const FString& ScriptTypeName,
	TArray<FNiagaraModuleInputInfo>& OutParams)
{
	if (!Script)
	{
		return;
	}
	
	const FNiagaraParameterStore& Store = Script->RapidIterationParameters;
	
	TArray<FNiagaraVariable> Params;
	Store.GetParameters(Params);
	
	for (const FNiagaraVariable& Var : Params)
	{
		FNiagaraModuleInputInfo Info;
		Info.InputName = FString::Printf(TEXT("[%s] %s"), *ScriptTypeName, *Var.GetName().ToString());
		Info.InputType = Var.GetType().GetFName().ToString();
		Info.CurrentValue = VariableValueToString(Store, Var);
		Info.DefaultValue = TEXT(""); // Would need default script to get this
		Info.bIsLinked = false;
		Info.LinkedSource = TEXT("");
		Info.bIsEditable = true;
		
		OutParams.Add(Info);
	}
}

TArray<FNiagaraModuleInputInfo> UNiagaraEmitterService::GetRapidIterationParameters(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ScriptType)
{
	TArray<FNiagaraModuleInputInfo> Result;
	
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetRapidIterationParameters: Failed to load system: %s"), *SystemPath);
		return Result;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetRapidIterationParameters: Emitter not found: %s"), *EmitterName);
		return Result;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetRapidIterationParameters: No emitter data"));
		return Result;
	}

	// Get parameters from each script type
	bool bFilterEmitterSpawn = ScriptType.IsEmpty() || ScriptType.Equals(TEXT("EmitterSpawn"), ESearchCase::IgnoreCase);
	bool bFilterEmitterUpdate = ScriptType.IsEmpty() || ScriptType.Equals(TEXT("EmitterUpdate"), ESearchCase::IgnoreCase);
	bool bFilterParticleSpawn = ScriptType.IsEmpty() || ScriptType.Equals(TEXT("ParticleSpawn"), ESearchCase::IgnoreCase) || ScriptType.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase);
	bool bFilterParticleUpdate = ScriptType.IsEmpty() || ScriptType.Equals(TEXT("ParticleUpdate"), ESearchCase::IgnoreCase) || ScriptType.Equals(TEXT("Update"), ESearchCase::IgnoreCase);

	if (bFilterEmitterSpawn)
	{
		ExtractScriptParameters(EmitterData->EmitterSpawnScriptProps.Script, TEXT("EmitterSpawn"), Result);
	}
	
	if (bFilterEmitterUpdate)
	{
		ExtractScriptParameters(EmitterData->EmitterUpdateScriptProps.Script, TEXT("EmitterUpdate"), Result);
	}
	
	if (bFilterParticleSpawn)
	{
		ExtractScriptParameters(EmitterData->SpawnScriptProps.Script, TEXT("ParticleSpawn"), Result);
	}
	
	if (bFilterParticleUpdate)
	{
		ExtractScriptParameters(EmitterData->UpdateScriptProps.Script, TEXT("ParticleUpdate"), Result);
	}

	UE_LOG(LogTemp, Log, TEXT("GetRapidIterationParameters: Found %d parameters for emitter %s"), Result.Num(), *EmitterName);
	return Result;
}

