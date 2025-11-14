// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/ComponentEventBinder.h"
#include "K2Node_ComponentBoundEvent.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

DEFINE_LOG_CATEGORY_STATIC(LogComponentEventBinder, Log, All);

UK2Node_ComponentBoundEvent* FComponentEventBinder::CreateComponentEvent(
	UBlueprint* Blueprint,
	const FString& ComponentName,
	const FString& DelegateName,
	const FVector2D& Position,
	FString& OutError
)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return nullptr;
	}
	
	// Get the component property from the generated class
	FObjectProperty* ComponentProperty = FindFProperty<FObjectProperty>(
		Blueprint->GeneratedClass, 
		FName(*ComponentName)
	);
	
	if (!ComponentProperty)
	{
		OutError = FString::Printf(TEXT("Component property not found: %s"), *ComponentName);
		return nullptr;
	}
	
	// Get component class
	UClass* ComponentClass = ComponentProperty->PropertyClass;
	if (!ComponentClass)
	{
		OutError = TEXT("Component class is null");
		return nullptr;
	}
	
	// Find the delegate property on the component class
	FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(
		ComponentClass, 
		FName(*DelegateName)
	);
	
	if (!DelegateProperty)
	{
		OutError = FString::Printf(
			TEXT("Delegate '%s' not found on component '%s' (class: %s)"),
			*DelegateName, *ComponentName, *ComponentClass->GetName()
		);
		return nullptr;
	}
	
	// Get the event graph
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph)
	{
		OutError = TEXT("Could not find event graph");
		return nullptr;
	}
	
	// Create the component bound event node at specified position
	UK2Node_ComponentBoundEvent* EventNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_ComponentBoundEvent>(
		EventGraph,
		Position,
		EK2NewNodeFlags::SelectNewNode,
		[ComponentProperty, DelegateProperty](UK2Node_ComponentBoundEvent* NewInstance)
		{
			NewInstance->InitializeComponentBoundEventParams(ComponentProperty, DelegateProperty);
		}
	);
	
	if (!EventNode)
	{
		OutError = TEXT("Failed to create component bound event node");
		return nullptr;
	}
	
	UE_LOG(LogComponentEventBinder, Log,
		TEXT("Created component event: Component='%s', Delegate='%s' at position (%.0f, %.0f)"),
		*ComponentName, *DelegateName, Position.X, Position.Y);
	
	// Mark Blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	return EventNode;
}

bool FComponentEventBinder::GetAvailableComponentEvents(
	UBlueprint* Blueprint,
	const FString& ComponentNameFilter,
	TArray<FComponentEventInfo>& OutEvents
)
{
	OutEvents.Empty();
	
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		UE_LOG(LogComponentEventBinder, Warning, TEXT("Invalid Blueprint or GeneratedClass"));
		return false;
	}
	
	// Iterate through all object properties (components) in the Blueprint
	for (TFieldIterator<FObjectProperty> It(Blueprint->GeneratedClass); It; ++It)
	{
		FObjectProperty* ObjectProperty = *It;
		
		// Get the component name
		FString CurrentComponentName = ObjectProperty->GetName();
		
		// Apply filter if specified
		if (!ComponentNameFilter.IsEmpty() && !CurrentComponentName.Equals(ComponentNameFilter))
		{
			continue;
		}
		
		// Check if this is a component type
		UClass* PropertyClass = ObjectProperty->PropertyClass;
		if (!PropertyClass || !PropertyClass->IsChildOf(UActorComponent::StaticClass()))
		{
			continue;
		}
		
		// Iterate through all delegate properties on this component class
		for (TFieldIterator<FMulticastDelegateProperty> DelegateIt(PropertyClass); DelegateIt; ++DelegateIt)
		{
			FMulticastDelegateProperty* DelegateProperty = *DelegateIt;
			
			// Create event info
			FComponentEventInfo EventInfo;
			EventInfo.ComponentName = CurrentComponentName;
			EventInfo.ComponentClassName = PropertyClass->GetName();
			EventInfo.DelegateName = DelegateProperty->GetFName().ToString();
			EventInfo.DisplayName = DelegateProperty->GetFName().ToString();
			
			// Get delegate signature
			UFunction* SignatureFunction = DelegateProperty->SignatureFunction;
			if (SignatureFunction)
			{
				// Build signature string
				EventInfo.Signature = SignatureFunction->GetName();
				
				// Extract parameter information
				for (TFieldIterator<FProperty> ParamIt(SignatureFunction); ParamIt; ++ParamIt)
				{
					FProperty* Param = *ParamIt;
					
					// Skip return values
					if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						continue;
					}
					
					FParameterInfo ParamInfo;
					ParamInfo.Name = Param->GetFName().ToString();
					ParamInfo.Type = Param->GetClass()->GetName();
					ParamInfo.CPPType = Param->GetCPPType();
					ParamInfo.Direction = Param->HasAnyPropertyFlags(CPF_OutParm) ? TEXT("output") : TEXT("input");
					ParamInfo.bIsOutParam = Param->HasAnyPropertyFlags(CPF_OutParm);
					ParamInfo.bIsReturnParam = false;
					
					EventInfo.Parameters.Add(ParamInfo);
				}
			}
			
			OutEvents.Add(EventInfo);
		}
	}
	
	UE_LOG(LogComponentEventBinder, Log,
		TEXT("Found %d component events in Blueprint '%s'%s"),
		OutEvents.Num(),
		*Blueprint->GetName(),
		ComponentNameFilter.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" for component '%s'"), *ComponentNameFilter));
	
	return true;
}

bool FComponentEventBinder::ValidateComponentDelegate(
	UBlueprint* Blueprint,
	const FString& ComponentName,
	const FString& DelegateName,
	FString& OutError
)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}
	
	if (!Blueprint->GeneratedClass)
	{
		OutError = TEXT("Blueprint GeneratedClass is null");
		return false;
	}
	
	// Get the component property
	FObjectProperty* ComponentProperty = FindFProperty<FObjectProperty>(
		Blueprint->GeneratedClass,
		FName(*ComponentName)
	);
	
	if (!ComponentProperty)
	{
		OutError = FString::Printf(TEXT("Component '%s' not found in Blueprint"), *ComponentName);
		return false;
	}
	
	// Get component class
	UClass* ComponentClass = ComponentProperty->PropertyClass;
	if (!ComponentClass)
	{
		OutError = TEXT("Component class is null");
		return false;
	}
	
	// Check if this is actually a component
	if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		OutError = FString::Printf(
			TEXT("Property '%s' is not a component (class: %s)"),
			*ComponentName, *ComponentClass->GetName()
		);
		return false;
	}
	
	// Find the delegate property
	FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(
		ComponentClass,
		FName(*DelegateName)
	);
	
	if (!DelegateProperty)
	{
		OutError = FString::Printf(
			TEXT("Delegate '%s' not found on component '%s' (class: %s)"),
			*DelegateName, *ComponentName, *ComponentClass->GetName()
		);
		return false;
	}
	
	UE_LOG(LogComponentEventBinder, Log,
		TEXT("Validated component delegate: Component='%s', Delegate='%s'"),
		*ComponentName, *DelegateName);
	
	return true;
}
