// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UNiagaraEmitterService.h"
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

// =================================================================
// Internal Helpers (required by SetColorTint)
// =================================================================

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
	
	// Enum types - resolve to display name
	if (TypeDef.IsEnum() && Size <= 4)
	{
		int32 IntValue = 0;
		FMemory::Memcpy(&IntValue, Data, FMath::Min(Size, 4));
		if (UEnum* Enum = TypeDef.GetEnum())
		{
			FText DisplayName = Enum->GetDisplayNameTextByValue(IntValue);
			if (!DisplayName.IsEmpty())
			{
				return DisplayName.ToString();
			}
			FString EnumName = Enum->GetNameStringByValue(IntValue);
			if (!EnumName.IsEmpty())
			{
				return EnumName;
			}
		}
		return FString::Printf(TEXT("%d"), IntValue);
	}

	// For other types, try to represent the raw bytes
	if (Size <= 4)
	{
		int32 IntValue = 0;
		FMemory::Memcpy(&IntValue, Data, FMath::Min(Size, 4));
		return FString::Printf(TEXT("(raw: %d, type: %s, size: %d)"), IntValue, *TypeName.ToString(), Size);
	}

	return FString::Printf(TEXT("(type: %s, size: %d bytes)"), *TypeName.ToString(), Size);
}


