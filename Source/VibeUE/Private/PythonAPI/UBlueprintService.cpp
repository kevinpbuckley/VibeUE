// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlueprintService.h"
#include "PythonAPI/BlueprintTypeParser.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Factories/BlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SubobjectDataSubsystem.h"

UBlueprint* UBlueprintService::LoadBlueprint(const FString& BlueprintPath)
{
	if (BlueprintPath.IsEmpty())
	{
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(BlueprintPath);
	return Cast<UBlueprint>(LoadedObject);
}

bool UBlueprintService::GetBlueprintInfo(const FString& BlueprintPath, FBlueprintDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBlueprintService::GetBlueprintInfo: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	OutInfo.BlueprintName = Blueprint->GetName();
	OutInfo.BlueprintPath = BlueprintPath;
	OutInfo.bIsWidgetBlueprint = Blueprint->IsA<UWidgetBlueprint>();

	// Get parent class
	if (UClass* ParentClass = Blueprint->ParentClass)
	{
		OutInfo.ParentClass = ParentClass->GetName();
	}

	// Get variables
	OutInfo.Variables = ListVariables(BlueprintPath);

	// Get functions
	OutInfo.Functions = ListFunctions(BlueprintPath);

	// Get components
	OutInfo.Components = ListComponents(BlueprintPath);

	return true;
}

TArray<FBlueprintVariableInfo> UBlueprintService::ListVariables(const FString& BlueprintPath)
{
	TArray<FBlueprintVariableInfo> Variables;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Variables;
	}

	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		FBlueprintVariableInfo VarInfo;
		VarInfo.VariableName = VarDesc.VarName.ToString();
		VarInfo.VariableType = VarDesc.VarType.PinCategory.ToString();
		VarInfo.Category = VarDesc.Category.ToString();
		VarInfo.bIsPublic = (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) == 0;
		VarInfo.bIsExposed = (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0;
		VarInfo.DefaultValue = VarDesc.DefaultValue;

		Variables.Add(VarInfo);
	}

	return Variables;
}

TArray<FBlueprintFunctionInfo> UBlueprintService::ListFunctions(const FString& BlueprintPath)
{
	TArray<FBlueprintFunctionInfo> Functions;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Functions;
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return Functions;
	}

	// Iterate through functions
	for (TFieldIterator<UFunction> FuncIt(GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (!Function)
		{
			continue;
		}

		FBlueprintFunctionInfo FuncInfo;
		FuncInfo.FunctionName = Function->GetName();

		// Check if pure
		FuncInfo.bIsPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

		// Check if override
		UFunction* SuperFunc = Function->GetSuperFunction();
		FuncInfo.bIsOverride = (SuperFunc != nullptr);

		// Get parameters and return type
		for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				FuncInfo.ReturnType = Prop->GetCPPType();
			}
			else if (Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				FString ParamStr = FString::Printf(TEXT("%s: %s"), *Prop->GetName(), *Prop->GetCPPType());
				FuncInfo.Parameters.Add(ParamStr);
			}
		}

		if (FuncInfo.ReturnType.IsEmpty())
		{
			FuncInfo.ReturnType = TEXT("void");
		}

		Functions.Add(FuncInfo);
	}

	return Functions;
}

TArray<FBlueprintComponentInfo> UBlueprintService::ListComponents(const FString& BlueprintPath)
{
	TArray<FBlueprintComponentInfo> Components;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Components;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return Components;
	}

	const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		FBlueprintComponentInfo CompInfo;
		CompInfo.ComponentName = Node->GetVariableName().ToString();

		if (UClass* ComponentClass = Node->ComponentClass)
		{
			CompInfo.ComponentClass = ComponentClass->GetName();
			CompInfo.bIsSceneComponent = ComponentClass->IsChildOf<USceneComponent>();
		}

		if (Node->ParentComponentOrVariableName != NAME_None)
		{
			CompInfo.AttachParent = Node->ParentComponentOrVariableName.ToString();
		}

		CompInfo.bIsRootComponent = (Node == SCS->GetDefaultSceneRootNode());

		// Get children
		for (USCS_Node* ChildNode : Node->GetChildNodes())
		{
			if (ChildNode)
			{
				CompInfo.Children.Add(ChildNode->GetVariableName().ToString());
			}
		}

		Components.Add(CompInfo);
	}

	return Components;
}

TArray<FBlueprintComponentInfo> UBlueprintService::GetComponentHierarchy(const FString& BlueprintPath)
{
	// For simplicity, return the same as ListComponents
	// Could be enhanced to build a proper hierarchy tree
	return ListComponents(BlueprintPath);
}

FString UBlueprintService::GetParentClass(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint || !Blueprint->ParentClass)
	{
		return FString();
	}

	return Blueprint->ParentClass->GetName();
}

bool UBlueprintService::IsWidgetBlueprint(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	return Blueprint->IsA<UWidgetBlueprint>();
}

// ============================================================================
// COMPONENT MANAGEMENT (manage_blueprint_component actions)
// ============================================================================

TArray<FComponentTypeInfo> UBlueprintService::GetAvailableComponents(const FString& SearchFilter, int32 MaxResults)
{
	TArray<FComponentTypeInfo> Results;
	
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		
		// Only include ActorComponent classes
		if (!Class->IsChildOf<UActorComponent>())
		{
			continue;
		}
		
		// Skip abstract, deprecated, hidden classes
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden))
		{
			continue;
		}
		
		// Apply search filter
		if (!SearchFilter.IsEmpty())
		{
			FString ClassName = Class->GetName();
			FString DisplayName = Class->GetDisplayNameText().ToString();
			if (!ClassName.Contains(SearchFilter, ESearchCase::IgnoreCase) &&
				!DisplayName.Contains(SearchFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		
		FComponentTypeInfo Info;
		Info.Name = Class->GetName();
		Info.DisplayName = Class->GetDisplayNameText().ToString();
		Info.ClassPath = Class->GetPathName();
		Info.bIsSceneComponent = Class->IsChildOf<USceneComponent>();
		Info.bIsPrimitiveComponent = Class->IsChildOf<UPrimitiveComponent>();
		Info.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
		
		// Get category from metadata
		if (const FString* CategoryMeta = Class->FindMetaData(TEXT("Category")))
		{
			Info.Category = *CategoryMeta;
		}
		else
		{
			Info.Category = TEXT("Miscellaneous");
		}
		
		// Get base class
		if (UClass* SuperClass = Class->GetSuperClass())
		{
			Info.BaseClass = SuperClass->GetName();
		}
		
		Results.Add(Info);
		
		// Limit results
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	
	// Sort by name
	Results.Sort([](const FComponentTypeInfo& A, const FComponentTypeInfo& B) {
		return A.Name < B.Name;
	});
	
	return Results;
}

bool UBlueprintService::GetComponentInfo(const FString& ComponentType, FComponentDetailedInfo& OutInfo)
{
	// Find the class
	UClass* ComponentClass = nullptr;
	
	// Try to find by exact name first
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf<UActorComponent>())
		{
			if (Class->GetName() == ComponentType || Class->GetName() == ComponentType + TEXT("Component"))
			{
				ComponentClass = Class;
				break;
			}
		}
	}
	
	// Try by path
	if (!ComponentClass)
	{
		ComponentClass = FindObject<UClass>(nullptr, *ComponentType);
	}
	
	if (!ComponentClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetComponentInfo: Component type not found: %s"), *ComponentType);
		return false;
	}
	
	OutInfo.Name = ComponentClass->GetName();
	OutInfo.DisplayName = ComponentClass->GetDisplayNameText().ToString();
	OutInfo.ClassPath = ComponentClass->GetPathName();
	OutInfo.bIsSceneComponent = ComponentClass->IsChildOf<USceneComponent>();
	OutInfo.bIsPrimitiveComponent = ComponentClass->IsChildOf<UPrimitiveComponent>();
	
	// Get category
	if (const FString* CategoryMeta = ComponentClass->FindMetaData(TEXT("Category")))
	{
		OutInfo.Category = *CategoryMeta;
	}
	
	// Get parent class
	if (UClass* SuperClass = ComponentClass->GetSuperClass())
	{
		OutInfo.ParentClass = SuperClass->GetName();
	}
	
	// Count properties and functions
	OutInfo.PropertyCount = 0;
	OutInfo.FunctionCount = 0;
	
	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		if (PropIt->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			OutInfo.PropertyCount++;
		}
	}
	
	for (TFieldIterator<UFunction> FuncIt(ComponentClass); FuncIt; ++FuncIt)
	{
		if (FuncIt->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			OutInfo.FunctionCount++;
		}
	}
	
	return true;
}

bool UBlueprintService::AddComponent(
	const FString& BlueprintPath,
	const FString& ComponentType,
	const FString& ComponentName,
	const FString& ParentName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find component class
	UClass* ComponentClass = nullptr;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf<UActorComponent>())
		{
			if (Class->GetName() == ComponentType || 
				Class->GetName() == ComponentType + TEXT("Component") ||
				Class->GetPathName() == ComponentType)
			{
				if (!Class->HasAnyClassFlags(CLASS_Abstract))
				{
					ComponentClass = Class;
					break;
				}
			}
		}
	}
	
	if (!ComponentClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Component type not found or abstract: %s"), *ComponentType);
		return false;
	}
	
	// Check for duplicate name
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			UE_LOG(LogTemp, Warning, TEXT("AddComponent: Component '%s' already exists"), *ComponentName);
			return false;
		}
	}
	
	// Create new SCS node
	USCS_Node* NewNode = SCS->CreateNode(ComponentClass, FName(*ComponentName));
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddComponent: Failed to create SCS node for %s"), *ComponentType);
		return false;
	}
	
	// Attach to parent if specified
	if (!ParentName.IsEmpty())
	{
		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ParentName)
			{
				ParentNode = Node;
				break;
			}
		}
		
		if (ParentNode)
		{
			ParentNode->AddChildNode(NewNode);
			// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
			// AddChildNode only manages the ChildNodes array, it does NOT set the parent reference
			NewNode->SetParent(ParentNode);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AddComponent: Parent '%s' not found, adding to root"), *ParentName);
			SCS->AddNode(NewNode);
		}
	}
	else
	{
		// Add to root
		SCS->AddNode(NewNode);
	}
	
	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("AddComponent: Added '%s' of type '%s' to %s"), *ComponentName, *ComponentType, *BlueprintPath);
	return true;
}

bool UBlueprintService::RemoveComponent(
	const FString& BlueprintPath,
	const FString& ComponentName,
	bool bRemoveChildren)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find the node to remove
	USCS_Node* NodeToRemove = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			NodeToRemove = Node;
			break;
		}
	}
	
	if (!NodeToRemove)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveComponent: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// If removing children, recursively remove them first
	if (bRemoveChildren)
	{
		TArray<USCS_Node*> ChildNodes = NodeToRemove->GetChildNodes();
		for (USCS_Node* Child : ChildNodes)
		{
			if (Child)
			{
				FString ChildName = Child->GetVariableName().ToString();
				// Recursively remove each child with its descendants
				RemoveComponent(BlueprintPath, ChildName, true);
			}
		}
	}
	else
	{
		// If not removing children, reparent them first
		TArray<USCS_Node*> ChildNodes = NodeToRemove->GetChildNodes();
		USCS_Node* ParentNode = nullptr;
		
		// Find parent of the node being removed
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node)
			{
				for (USCS_Node* Child : Node->GetChildNodes())
				{
					if (Child == NodeToRemove)
					{
						ParentNode = Node;
						break;
					}
				}
			}
		}
		
		// Move children to parent or root
		for (USCS_Node* Child : ChildNodes)
		{
			NodeToRemove->RemoveChildNode(Child);
			if (ParentNode)
			{
				ParentNode->AddChildNode(Child);
			}
			else
			{
				SCS->AddNode(Child);
			}
		}
	}
	
	// Remove the node
	SCS->RemoveNode(NodeToRemove);
	
	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("RemoveComponent: Removed '%s' from %s"), *ComponentName, *BlueprintPath);
	return true;
}

// Helper to find component template in blueprint
static UActorComponent* FindComponentTemplate(UBlueprint* Blueprint, const FString& ComponentName)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return nullptr;
	}
	
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			return Node->ComponentTemplate;
		}
	}
	
	return nullptr;
}

bool UBlueprintService::GetComponentProperty(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& PropertyName,
	FString& OutValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetComponentProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetComponentProperty: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Find the property
	FProperty* Property = Component->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetComponentProperty: Property '%s' not found on component '%s'"), *PropertyName, *ComponentName);
		return false;
	}
	
	// Get value as string
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
	Property->ExportTextItem_Direct(OutValue, ValuePtr, nullptr, Component, PPF_None);
	
	return true;
}

bool UBlueprintService::SetComponentProperty(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetComponentProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Find the property
	FProperty* Property = Component->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty: Property '%s' not found on component '%s'"), *PropertyName, *ComponentName);
		return false;
	}
	
	// Set value from string
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
	
	// Mark component and blueprint as modified before making changes
	Component->Modify();
	Blueprint->Modify();
	
	if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, Component, PPF_None))
	{
		UE_LOG(LogTemp, Error, TEXT("SetComponentProperty: Failed to set property '%s' to '%s'"), *PropertyName, *PropertyValue);
		return false;
	}
	
	// Mark blueprint as modified and recompile
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("SetComponentProperty: Set '%s.%s' = '%s'"), *ComponentName, *PropertyName, *PropertyValue);
	return true;
}

TArray<FComponentPropertyInfo> UBlueprintService::GetAllComponentProperties(
	const FString& BlueprintPath,
	const FString& ComponentName,
	bool bIncludeInherited)
{
	TArray<FComponentPropertyInfo> Results;
	
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetAllComponentProperties: Failed to load blueprint: %s"), *BlueprintPath);
		return Results;
	}
	
	UActorComponent* Component = FindComponentTemplate(Blueprint, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetAllComponentProperties: Component '%s' not found"), *ComponentName);
		return Results;
	}
	
	UClass* ComponentClass = Component->GetClass();
	
	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		// Skip if not including inherited
		if (!bIncludeInherited && Property->GetOwnerClass() != ComponentClass)
		{
			continue;
		}
		
		// Skip transient properties
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}
		
		FComponentPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		Info.bIsInherited = (Property->GetOwnerClass() != ComponentClass);
		
		// Get category
		if (Property->HasMetaData(TEXT("Category")))
		{
			Info.Category = Property->GetMetaData(TEXT("Category"));
		}
		
		// Get current value
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
		Property->ExportTextItem_Direct(Info.Value, ValuePtr, nullptr, Component, PPF_None);
		
		Results.Add(Info);
	}
	
	return Results;
}

TArray<FComponentPropertyInfo> UBlueprintService::ListComponentProperties(
	const FString& BlueprintPath,
	const FString& ComponentName,
	bool bIncludeInherited)
{
	// This is an alias for GetAllComponentProperties with a more intuitive name
	return GetAllComponentProperties(BlueprintPath, ComponentName, bIncludeInherited);
}

bool UBlueprintService::SetRootComponent(
	const FString& BlueprintPath,
	const FString& ComponentName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetRootComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("SetRootComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find the component node to make root
	USCS_Node* NewRootNode = nullptr;
	USCS_Node* CurrentRootNode = SCS->GetDefaultSceneRootNode();
	
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			NewRootNode = Node;
			break;
		}
	}
	
	if (!NewRootNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetRootComponent: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Check if it's already the root
	if (NewRootNode == CurrentRootNode)
	{
		UE_LOG(LogTemp, Log, TEXT("SetRootComponent: '%s' is already the root component"), *ComponentName);
		return true;
	}
	
	// Ensure the new root is a SceneComponent
	if (!NewRootNode->ComponentTemplate || !NewRootNode->ComponentTemplate->IsA<USceneComponent>())
	{
		UE_LOG(LogTemp, Error, TEXT("SetRootComponent: '%s' is not a SceneComponent and cannot be root"), *ComponentName);
		return false;
	}
	
	// Store children of the current root (if any) to reparent them
	TArray<USCS_Node*> ChildrenToReparent;
	if (CurrentRootNode)
	{
		ChildrenToReparent = CurrentRootNode->GetChildNodes();
	}
	
	// Find and remove the new root from its current parent
	USCS_Node* NewRootCurrentParent = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node)
		{
			for (USCS_Node* Child : Node->GetChildNodes())
			{
				if (Child == NewRootNode)
				{
					NewRootCurrentParent = Node;
					break;
				}
			}
			if (NewRootCurrentParent)
			{
				break;
			}
		}
	}
	
	// Mark blueprint as modifying
	Blueprint->Modify();
	
	// Remove new root from its current parent
	if (NewRootCurrentParent)
	{
		NewRootCurrentParent->RemoveChildNode(NewRootNode);
	}
	else
	{
		// It might be a root node itself, remove it from root nodes
		SCS->RemoveNode(NewRootNode);
	}
	
	// If there was a current root, we need to handle it
	if (CurrentRootNode && CurrentRootNode != NewRootNode)
	{
		// Remove children from current root first (we'll add them to new root)
		for (USCS_Node* Child : ChildrenToReparent)
		{
			if (Child && Child != NewRootNode)
			{
				CurrentRootNode->RemoveChildNode(Child);
			}
		}
		
		// Detach the current root from being THE root  
		// Make the old root a child of the new root
		SCS->RemoveNode(CurrentRootNode);
		NewRootNode->AddChildNode(CurrentRootNode);
		// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
		CurrentRootNode->SetParent(NewRootNode);
	}
	
	// Add new root as a root node
	SCS->AddNode(NewRootNode);
	
	// Reparent the old children (except the new root) to the new root
	for (USCS_Node* Child : ChildrenToReparent)
	{
		if (Child && Child != NewRootNode && Child != CurrentRootNode)
		{
			NewRootNode->AddChildNode(Child);
			// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
			Child->SetParent(NewRootNode);
		}
	}
	
	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("SetRootComponent: Set '%s' as root component in %s"), *ComponentName, *BlueprintPath);
	return true;
}

bool UBlueprintService::CompareComponents(
	const FString& BlueprintPathA,
	const FString& ComponentNameA,
	const FString& BlueprintPathB,
	const FString& ComponentNameB,
	FString& OutDifferences)
{
	// Get properties from both components
	TArray<FComponentPropertyInfo> PropsA = GetAllComponentProperties(BlueprintPathA, ComponentNameA, true);
	TArray<FComponentPropertyInfo> PropsB = GetAllComponentProperties(BlueprintPathB, ComponentNameB, true);
	
	if (PropsA.Num() == 0)
	{
		OutDifferences = FString::Printf(TEXT("Component '%s' not found in blueprint A or has no properties"), *ComponentNameA);
		return false;
	}
	
	if (PropsB.Num() == 0)
	{
		OutDifferences = FString::Printf(TEXT("Component '%s' not found in blueprint B or has no properties"), *ComponentNameB);
		return false;
	}
	
	TArray<FString> Differences;
	
	// Build map of properties from A
	TMap<FString, FComponentPropertyInfo> MapA;
	for (const FComponentPropertyInfo& Prop : PropsA)
	{
		MapA.Add(Prop.PropertyName, Prop);
	}
	
	// Build map of properties from B
	TMap<FString, FComponentPropertyInfo> MapB;
	for (const FComponentPropertyInfo& Prop : PropsB)
	{
		MapB.Add(Prop.PropertyName, Prop);
	}
	
	// Find properties only in A
	for (const auto& Pair : MapA)
	{
		if (!MapB.Contains(Pair.Key))
		{
			Differences.Add(FString::Printf(TEXT("Property '%s' only in A (%s)"), *Pair.Key, *Pair.Value.PropertyType));
		}
	}
	
	// Find properties only in B
	for (const auto& Pair : MapB)
	{
		if (!MapA.Contains(Pair.Key))
		{
			Differences.Add(FString::Printf(TEXT("Property '%s' only in B (%s)"), *Pair.Key, *Pair.Value.PropertyType));
		}
	}
	
	// Compare matching properties
	for (const auto& PairA : MapA)
	{
		if (MapB.Contains(PairA.Key))
		{
			const FComponentPropertyInfo& PropA = PairA.Value;
			const FComponentPropertyInfo& PropB = MapB[PairA.Key];
			
			// Check type difference
			if (PropA.PropertyType != PropB.PropertyType)
			{
				Differences.Add(FString::Printf(TEXT("Property '%s' type differs: '%s' vs '%s'"), 
					*PairA.Key, *PropA.PropertyType, *PropB.PropertyType));
			}
			// Check value difference (only for same types)
			else if (PropA.Value != PropB.Value)
			{
				// Truncate long values
				FString ValA = PropA.Value.Len() > 50 ? PropA.Value.Left(47) + TEXT("...") : PropA.Value;
				FString ValB = PropB.Value.Len() > 50 ? PropB.Value.Left(47) + TEXT("...") : PropB.Value;
				Differences.Add(FString::Printf(TEXT("Property '%s' value differs: '%s' vs '%s'"), 
					*PairA.Key, *ValA, *ValB));
			}
		}
	}
	
	if (Differences.Num() == 0)
	{
		OutDifferences = TEXT("Components are identical");
	}
	else
	{
		OutDifferences = FString::Join(Differences, TEXT("\n"));
	}
	
	return true;
}

bool UBlueprintService::ReparentComponent(
	const FString& BlueprintPath,
	const FString& ComponentName,
	const FString& NewParentName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Blueprint has no SCS: %s"), *BlueprintPath);
		return false;
	}
	
	// Find the component to reparent
	USCS_Node* NodeToReparent = nullptr;
	USCS_Node* CurrentParent = nullptr;
	
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node)
		{
			if (Node->GetVariableName().ToString() == ComponentName)
			{
				NodeToReparent = Node;
			}
			
			// Check if this node is the current parent
			for (USCS_Node* Child : Node->GetChildNodes())
			{
				if (Child && Child->GetVariableName().ToString() == ComponentName)
				{
					CurrentParent = Node;
				}
			}
		}
	}
	
	if (!NodeToReparent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReparentComponent: Component '%s' not found"), *ComponentName);
		return false;
	}
	
	// Find new parent
	USCS_Node* NewParent = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == NewParentName)
		{
			NewParent = Node;
			break;
		}
	}
	
	if (!NewParent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReparentComponent: New parent '%s' not found"), *NewParentName);
		return false;
	}
	
	// Prevent circular parenting
	if (NodeToReparent == NewParent)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Cannot parent component to itself"));
		return false;
	}
	
	// Check for circular reference (NewParent can't be a descendant of NodeToReparent)
	TArray<USCS_Node*> Descendants;
	TFunction<void(USCS_Node*)> CollectDescendants = [&](USCS_Node* Node) {
		for (USCS_Node* Child : Node->GetChildNodes())
		{
			if (Child)
			{
				Descendants.Add(Child);
				CollectDescendants(Child);
			}
		}
	};
	CollectDescendants(NodeToReparent);
	
	if (Descendants.Contains(NewParent))
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentComponent: Circular reference - new parent is a descendant"));
		return false;
	}
	
	// Remove from current parent
	if (CurrentParent)
	{
		CurrentParent->RemoveChildNode(NodeToReparent);
	}
	else
	{
		// It's a root node
		SCS->RemoveNode(NodeToReparent);
	}
	
	// Add to new parent
	NewParent->AddChildNode(NodeToReparent);
	
	// CRITICAL: Call SetParent to properly set ParentComponentOrVariableName
	// AddChildNode only manages the ChildNodes array, it does NOT set the parent reference
	NodeToReparent->SetParent(NewParent);
	
	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("ReparentComponent: Moved '%s' to parent '%s'"), *ComponentName, *NewParentName);
	return true;
}

// ============================================================================
// VARIABLE MANAGEMENT (Phase 1)
// ============================================================================

bool UBlueprintService::AddVariable(
	const FString& BlueprintPath,
	const FString& VariableName,
	const FString& VariableType,
	const FString& DefaultValue,
	bool bIsArray,
	const FString& ContainerType)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Check if variable already exists
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName.ToString() == VariableName)
		{
			UE_LOG(LogTemp, Warning, TEXT("AddVariable: Variable '%s' already exists in %s"), *VariableName, *BlueprintPath);
			return false;
		}
	}

	// Parse the type string
	FEdGraphPinType PinType;
	FString ErrorMessage;
	if (!FBlueprintTypeParser::ParseTypeString(VariableType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AddVariable: Failed to parse type '%s': %s"), *VariableType, *ErrorMessage);
		return false;
	}

	// Create variable description
	FBPVariableDescription NewVar;
	NewVar.VarName = FName(*VariableName);
	NewVar.VarGuid = FGuid::NewGuid();
	NewVar.VarType = PinType;
	NewVar.FriendlyName = VariableName;
	NewVar.Category = FText::FromString(TEXT("Default"));
	NewVar.DefaultValue = DefaultValue;
	NewVar.PropertyFlags = CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance;

	// Add variable to blueprint
	Blueprint->NewVariables.Add(NewVar);

	// Mark blueprint as modified and refresh
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("AddVariable: Added variable '%s' of type '%s' to %s"), *VariableName, *VariableType, *BlueprintPath);
	return true;
}

bool UBlueprintService::SetVariableDefaultValue(
	const FString& BlueprintPath,
	const FString& VariableName,
	const FString& DefaultValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetVariableDefaultValue: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the variable
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName.ToString() == VariableName)
		{
			Var.DefaultValue = DefaultValue;
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			UE_LOG(LogTemp, Log, TEXT("SetVariableDefaultValue: Set '%s' default to '%s'"), *VariableName, *DefaultValue);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("SetVariableDefaultValue: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
	return false;
}

bool UBlueprintService::RemoveVariable(
	const FString& BlueprintPath,
	const FString& VariableName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find and remove the variable
	for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
	{
		if (Blueprint->NewVariables[i].VarName.ToString() == VariableName)
		{
			FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, Blueprint->NewVariables[i].VarName);
			UE_LOG(LogTemp, Log, TEXT("RemoveVariable: Removed variable '%s' from %s"), *VariableName, *BlueprintPath);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("RemoveVariable: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
	return false;
}

bool UBlueprintService::GetVariableInfo(
	const FString& BlueprintPath,
	const FString& VariableName,
	FBlueprintVariableDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetVariableInfo: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the variable
	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		if (VarDesc.VarName.ToString() == VariableName)
		{
			OutInfo.VariableName = VarDesc.VarName.ToString();
			OutInfo.VariableType = VarDesc.VarType.PinCategory.ToString();
			OutInfo.Category = VarDesc.Category.ToString();
			OutInfo.DefaultValue = VarDesc.DefaultValue;

			// Get tooltip from metadata
			if (VarDesc.HasMetaData(TEXT("tooltip")))
			{
				OutInfo.Tooltip = VarDesc.GetMetaData(TEXT("tooltip"));
			}

			// Build type path from pin type
			if (VarDesc.VarType.PinSubCategoryObject.IsValid())
			{
				const UObject* TypeObj = VarDesc.VarType.PinSubCategoryObject.Get();
				if (TypeObj)
				{
					OutInfo.TypePath = TypeObj->GetPathName();
				}
			}
			else
			{
				// Primitive types
				OutInfo.TypePath = FString::Printf(TEXT("/Script/CoreUObject.%sProperty"), *VarDesc.VarType.PinCategory.ToString());
			}

			// Property flags
			OutInfo.bIsInstanceEditable = (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) == 0;
			OutInfo.bIsExposeOnSpawn = (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0;
			OutInfo.bIsPrivate = VarDesc.VarType.bIsConst;
			OutInfo.bIsBlueprintReadOnly = (VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0;
			OutInfo.bIsExposeToCinematics = (VarDesc.PropertyFlags & CPF_Interp) != 0;

			// Container type
			OutInfo.bIsArray = (VarDesc.VarType.ContainerType == EPinContainerType::Array);
			OutInfo.bIsSet = (VarDesc.VarType.ContainerType == EPinContainerType::Set);
			OutInfo.bIsMap = (VarDesc.VarType.ContainerType == EPinContainerType::Map);

			// Replication
			if (VarDesc.RepNotifyFunc != NAME_None)
			{
				OutInfo.ReplicationCondition = TEXT("RepNotify");
			}
			else if (VarDesc.PropertyFlags & CPF_Net)
			{
				OutInfo.ReplicationCondition = TEXT("Replicated");
			}
			else
			{
				OutInfo.ReplicationCondition = TEXT("None");
			}

			UE_LOG(LogTemp, Log, TEXT("GetVariableInfo: Got info for '%s' in %s"), *VariableName, *BlueprintPath);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("GetVariableInfo: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
	return false;
}

bool UBlueprintService::ModifyVariable(
	const FString& BlueprintPath,
	const FString& VariableName,
	const FString& NewName,
	const FString& NewCategory,
	const FString& NewTooltip,
	const FString& NewDefaultValue,
	int32 bSetInstanceEditable,
	int32 bSetExposeOnSpawn,
	int32 bSetPrivate,
	int32 bSetBlueprintReadOnly,
	const FString& NewReplicationCondition)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ModifyVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the variable
	FBPVariableDescription* FoundVar = nullptr;
	for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		if (VarDesc.VarName.ToString() == VariableName)
		{
			FoundVar = &VarDesc;
			break;
		}
	}

	if (!FoundVar)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModifyVariable: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return false;
	}

	bool bModified = false;

	// Rename if specified
	if (!NewName.IsEmpty() && NewName != VariableName)
	{
		FBlueprintEditorUtils::RenameMemberVariable(Blueprint, FoundVar->VarName, FName(*NewName));
		bModified = true;
	}

	// Update category
	if (!NewCategory.IsEmpty())
	{
		FoundVar->Category = FText::FromString(NewCategory);
		bModified = true;
	}

	// Update tooltip (stored as metadata)
	if (!NewTooltip.IsEmpty())
	{
		FoundVar->SetMetaData(TEXT("tooltip"), NewTooltip);
		bModified = true;
	}

	// Update default value
	if (!NewDefaultValue.IsEmpty())
	{
		FoundVar->DefaultValue = NewDefaultValue;
		bModified = true;
	}

	// Update property flags
	if (bSetInstanceEditable >= 0)
	{
		if (bSetInstanceEditable > 0)
		{
			FoundVar->PropertyFlags &= ~CPF_DisableEditOnInstance;
		}
		else
		{
			FoundVar->PropertyFlags |= CPF_DisableEditOnInstance;
		}
		bModified = true;
	}

	if (bSetExposeOnSpawn >= 0)
	{
		if (bSetExposeOnSpawn > 0)
		{
			FoundVar->PropertyFlags |= CPF_ExposeOnSpawn;
		}
		else
		{
			FoundVar->PropertyFlags &= ~CPF_ExposeOnSpawn;
		}
		bModified = true;
	}

	if (bSetBlueprintReadOnly >= 0)
	{
		if (bSetBlueprintReadOnly > 0)
		{
			FoundVar->PropertyFlags |= CPF_BlueprintReadOnly;
		}
		else
		{
			FoundVar->PropertyFlags &= ~CPF_BlueprintReadOnly;
		}
		bModified = true;
	}

	// Update replication
	if (!NewReplicationCondition.IsEmpty())
	{
		if (NewReplicationCondition.Equals(TEXT("Replicated"), ESearchCase::IgnoreCase))
		{
			FoundVar->PropertyFlags |= CPF_Net;
			FoundVar->RepNotifyFunc = NAME_None;
		}
		else if (NewReplicationCondition.Equals(TEXT("RepNotify"), ESearchCase::IgnoreCase))
		{
			FoundVar->PropertyFlags |= CPF_Net;
			// Create OnRep function name
			FString OnRepName = FString::Printf(TEXT("OnRep_%s"), *FoundVar->VarName.ToString());
			FoundVar->RepNotifyFunc = FName(*OnRepName);
		}
		else if (NewReplicationCondition.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			FoundVar->PropertyFlags &= ~CPF_Net;
			FoundVar->RepNotifyFunc = NAME_None;
		}
		bModified = true;
	}

	if (bModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("ModifyVariable: Modified variable '%s' in %s"), *VariableName, *BlueprintPath);
	}

	return true;
}

TArray<FVariableTypeInfo> UBlueprintService::SearchVariableTypes(
	const FString& SearchTerm,
	const FString& Category,
	int32 MaxResults)
{
	TArray<FVariableTypeInfo> Results;

	// Pre-defined basic types (always available)
	struct FBuiltInType
	{
		FString TypeName;
		FString TypePath;
		FString Category;
		FString Description;
	};

	TArray<FBuiltInType> BuiltInTypes = {
		// Basic types
		{ TEXT("Boolean"), TEXT("bool"), TEXT("Basic"), TEXT("True or false value") },
		{ TEXT("Byte"), TEXT("byte"), TEXT("Basic"), TEXT("8-bit unsigned integer (0-255)") },
		{ TEXT("Integer"), TEXT("int"), TEXT("Basic"), TEXT("32-bit signed integer") },
		{ TEXT("Integer64"), TEXT("int64"), TEXT("Basic"), TEXT("64-bit signed integer") },
		{ TEXT("Float"), TEXT("float"), TEXT("Basic"), TEXT("Single precision floating point (32-bit)") },
		{ TEXT("Double"), TEXT("double"), TEXT("Basic"), TEXT("Double precision floating point (64-bit)") },
		{ TEXT("Name"), TEXT("FName"), TEXT("Basic"), TEXT("Unique identifier name") },
		{ TEXT("String"), TEXT("FString"), TEXT("Basic"), TEXT("Text string") },
		{ TEXT("Text"), TEXT("FText"), TEXT("Basic"), TEXT("Localizable text") },

		// Common structures
		{ TEXT("Vector"), TEXT("FVector"), TEXT("Structure"), TEXT("3D vector (X, Y, Z)") },
		{ TEXT("Vector2D"), TEXT("FVector2D"), TEXT("Structure"), TEXT("2D vector (X, Y)") },
		{ TEXT("Vector4"), TEXT("FVector4"), TEXT("Structure"), TEXT("4D vector (X, Y, Z, W)") },
		{ TEXT("Rotator"), TEXT("FRotator"), TEXT("Structure"), TEXT("Rotation in 3D space (Pitch, Yaw, Roll)") },
		{ TEXT("Transform"), TEXT("FTransform"), TEXT("Structure"), TEXT("Location, rotation, and scale") },
		{ TEXT("Quat"), TEXT("FQuat"), TEXT("Structure"), TEXT("Quaternion rotation") },
		{ TEXT("Color"), TEXT("FColor"), TEXT("Structure"), TEXT("RGBA color (0-255)") },
		{ TEXT("LinearColor"), TEXT("FLinearColor"), TEXT("Structure"), TEXT("Linear RGBA color (0.0-1.0)") },
		{ TEXT("DateTime"), TEXT("FDateTime"), TEXT("Structure"), TEXT("Date and time") },
		{ TEXT("Timespan"), TEXT("FTimespan"), TEXT("Structure"), TEXT("Time duration") },
		{ TEXT("Guid"), TEXT("FGuid"), TEXT("Structure"), TEXT("Globally unique identifier") },
		{ TEXT("IntPoint"), TEXT("FIntPoint"), TEXT("Structure"), TEXT("2D integer point") },
		{ TEXT("IntVector"), TEXT("FIntVector"), TEXT("Structure"), TEXT("3D integer vector") },
		{ TEXT("Box"), TEXT("FBox"), TEXT("Structure"), TEXT("3D axis-aligned bounding box") },
		{ TEXT("Box2D"), TEXT("FBox2D"), TEXT("Structure"), TEXT("2D axis-aligned bounding box") },

		// Common object types
		{ TEXT("Object"), TEXT("UObject"), TEXT("Object"), TEXT("Base Unreal object reference") },
		{ TEXT("Actor"), TEXT("AActor"), TEXT("Object"), TEXT("Actor reference") },
		{ TEXT("Pawn"), TEXT("APawn"), TEXT("Object"), TEXT("Pawn reference") },
		{ TEXT("Character"), TEXT("ACharacter"), TEXT("Object"), TEXT("Character reference") },
		{ TEXT("PlayerController"), TEXT("APlayerController"), TEXT("Object"), TEXT("Player controller reference") },
		{ TEXT("ActorComponent"), TEXT("UActorComponent"), TEXT("Object"), TEXT("Actor component reference") },
		{ TEXT("SceneComponent"), TEXT("USceneComponent"), TEXT("Object"), TEXT("Scene component reference") },
		{ TEXT("StaticMeshComponent"), TEXT("UStaticMeshComponent"), TEXT("Object"), TEXT("Static mesh component") },
		{ TEXT("SkeletalMeshComponent"), TEXT("USkeletalMeshComponent"), TEXT("Object"), TEXT("Skeletal mesh component") },
		{ TEXT("Texture2D"), TEXT("UTexture2D"), TEXT("Object"), TEXT("2D texture reference") },
		{ TEXT("Material"), TEXT("UMaterialInterface"), TEXT("Object"), TEXT("Material reference") },
		{ TEXT("SoundBase"), TEXT("USoundBase"), TEXT("Object"), TEXT("Sound reference") },
		{ TEXT("ParticleSystem"), TEXT("UParticleSystem"), TEXT("Object"), TEXT("Particle system reference") },
		{ TEXT("DataTable"), TEXT("UDataTable"), TEXT("Object"), TEXT("Data table reference") },
		{ TEXT("CurveFloat"), TEXT("UCurveFloat"), TEXT("Object"), TEXT("Float curve reference") },
		{ TEXT("AnimMontage"), TEXT("UAnimMontage"), TEXT("Object"), TEXT("Animation montage reference") },
		{ TEXT("AnimSequence"), TEXT("UAnimSequence"), TEXT("Object"), TEXT("Animation sequence reference") },
		{ TEXT("Blueprint"), TEXT("UBlueprint"), TEXT("Object"), TEXT("Blueprint asset reference") },
		{ TEXT("UserWidget"), TEXT("UUserWidget"), TEXT("Object"), TEXT("User widget reference") },
		{ TEXT("World"), TEXT("UWorld"), TEXT("Object"), TEXT("World reference") },
	};

	// Filter and add matching types
	for (const FBuiltInType& Type : BuiltInTypes)
	{
		// Check category filter
		if (!Category.IsEmpty() && !Type.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Check search term
		if (!SearchTerm.IsEmpty())
		{
			if (!Type.TypeName.Contains(SearchTerm, ESearchCase::IgnoreCase) &&
				!Type.TypePath.Contains(SearchTerm, ESearchCase::IgnoreCase) &&
				!Type.Description.Contains(SearchTerm, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		FVariableTypeInfo Info;
		Info.TypeName = Type.TypeName;
		Info.TypePath = Type.TypePath;
		Info.Category = Type.Category;
		Info.Description = Type.Description;
		Results.Add(Info);

		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SearchVariableTypes: Found %d types matching '%s' (category: '%s')"),
		Results.Num(), *SearchTerm, *Category);

	return Results;
}

// ============================================================================
// FUNCTION MANAGEMENT (Phase 2)
// ============================================================================

bool UBlueprintService::CreateFunction(
	const FString& BlueprintPath,
	const FString& FunctionName,
	bool bIsPure)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateFunction: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Check if function already exists
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateFunction: Function '%s' already exists in %s"), *FunctionName, *BlueprintPath);
			return true;  // Idempotent - not an error
		}
	}

	// Create new function graph
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateFunction: Failed to create graph for '%s'"), *FunctionName);
		return false;
	}

	// Set up the function graph
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated=*/true, /*SignatureFromClass=*/nullptr);

	// Find the function entry node and set pure flag
	if (bIsPure)
	{
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		NewGraph->GetNodesOfClass(EntryNodes);
		if (EntryNodes.Num() > 0 && EntryNodes[0])
		{
			EntryNodes[0]->AddExtraFlags(FUNC_BlueprintPure);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("CreateFunction: Created function '%s' in %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::AddFunctionParameter(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	const FString& ParameterType,
	bool bIsOutput,
	bool bIsReference,
	const FString& DefaultValue,
	bool bIsArray,
	const FString& ContainerType)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Parse the type string
	FEdGraphPinType PinType;
	FString ErrorMessage;
	if (!FBlueprintTypeParser::ParseTypeString(ParameterType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: Failed to parse type '%s': %s"), *ParameterType, *ErrorMessage);
		return false;
	}

	// Set reference flag
	if (bIsReference)
	{
		PinType.bIsReference = true;
	}

	// Find the appropriate node (entry for inputs, result for outputs)
	if (bIsOutput)
	{
		// Add to function result node
		TArray<UK2Node_FunctionResult*> ResultNodes;
		FunctionGraph->GetNodesOfClass(ResultNodes);

		if (ResultNodes.Num() == 0)
		{
			// Create result node if it doesn't exist
			UK2Node_FunctionResult* ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
			FunctionGraph->AddNode(ResultNode, false, false);
			ResultNode->CreateNewGuid();
			ResultNode->PostPlacedNewNode();
			ResultNode->AllocateDefaultPins();
			ResultNodes.Add(ResultNode);
		}

		if (ResultNodes.Num() > 0 && ResultNodes[0])
		{
			ResultNodes[0]->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Input);
		}
	}
	else
	{
		// Add to function entry node
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		FunctionGraph->GetNodesOfClass(EntryNodes);

		if (EntryNodes.Num() > 0 && EntryNodes[0])
		{
			EntryNodes[0]->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Output);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionParameter: No entry node found in function '%s'"), *FunctionName);
			return false;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddFunctionParameter: Added parameter '%s' (%s) to function '%s'"), *ParameterName, bIsOutput ? TEXT("output") : TEXT("input"), *FunctionName);
	return true;
}

bool UBlueprintService::AddFunctionLocalVariable(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName,
	const FString& VariableType,
	const FString& DefaultValue,
	bool bIsArray,
	const FString& ContainerType)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Parse the type string
	FEdGraphPinType PinType;
	FString ErrorMessage;
	if (!FBlueprintTypeParser::ParseTypeString(VariableType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: Failed to parse type '%s': %s"), *VariableType, *ErrorMessage);
		return false;
	}

	// Find the function entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() == 0 || !EntryNodes[0])
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionLocalVariable: No entry node found in function '%s'"), *FunctionName);
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

	// Create local variable description
	FBPVariableDescription LocalVar;
	LocalVar.VarName = FName(*VariableName);
	LocalVar.VarGuid = FGuid::NewGuid();
	LocalVar.VarType = PinType;
	LocalVar.FriendlyName = VariableName;
	LocalVar.DefaultValue = DefaultValue;
	LocalVar.Category = FText::FromString(TEXT("Local Variables"));

	// Add to entry node's local variables
	EntryNode->LocalVariables.Add(LocalVar);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddFunctionLocalVariable: Added local variable '%s' to function '%s'"), *VariableName, *FunctionName);
	return true;
}

TArray<FBlueprintFunctionParameterInfo> UBlueprintService::GetFunctionParameters(
	const FString& BlueprintPath,
	const FString& FunctionName)
{
	TArray<FBlueprintFunctionParameterInfo> Parameters;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Parameters;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return Parameters;
	}

	// Get parameters from entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0 && EntryNodes[0])
	{
		UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
		for (UEdGraphPin* Pin : EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = false;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				Parameters.Add(ParamInfo);
			}
		}
	}

	// Get output parameters from result node
	TArray<UK2Node_FunctionResult*> ResultNodes;
	FunctionGraph->GetNodesOfClass(ResultNodes);

	if (ResultNodes.Num() > 0 && ResultNodes[0])
	{
		UK2Node_FunctionResult* ResultNode = ResultNodes[0];
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = true;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				Parameters.Add(ParamInfo);
			}
		}
	}

	return Parameters;
}

bool UBlueprintService::DeleteFunction(
	const FString& BlueprintPath,
	const FString& FunctionName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteFunction: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeleteFunction: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Remove the function graph
	FBlueprintEditorUtils::RemoveGraph(Blueprint, FunctionGraph, EGraphRemoveFlags::Recompile);
	UE_LOG(LogTemp, Log, TEXT("DeleteFunction: Deleted function '%s' from %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::GetFunctionInfo(
	const FString& BlueprintPath,
	const FString& FunctionName,
	FBlueprintFunctionDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetFunctionInfo: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetFunctionInfo: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	OutInfo.FunctionName = FunctionGraph->GetName();
	OutInfo.GraphGuid = FunctionGraph->GraphGuid.ToString();
	OutInfo.NodeCount = FunctionGraph->Nodes.Num();

	// Get entry node for parameters and local variables
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0 && EntryNodes[0])
	{
		UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

		// Check if pure
		OutInfo.bIsPure = EntryNode->HasAnyExtraFlags(FUNC_BlueprintPure);

		// Get input parameters
		for (UEdGraphPin* Pin : EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = false;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				OutInfo.InputParameters.Add(ParamInfo);
			}
		}

		// Get local variables
		for (const FBPVariableDescription& VarDesc : EntryNode->LocalVariables)
		{
			FBlueprintLocalVariableInfo LocalInfo;
			LocalInfo.VariableName = VarDesc.VarName.ToString();
			LocalInfo.FriendlyName = VarDesc.FriendlyName;
			LocalInfo.VariableType = FBlueprintTypeParser::GetFriendlyTypeName(VarDesc.VarType);
			LocalInfo.DisplayType = UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString();
			LocalInfo.DefaultValue = VarDesc.DefaultValue;
			LocalInfo.Category = VarDesc.Category.ToString();
			LocalInfo.Guid = VarDesc.VarGuid.ToString();
			LocalInfo.bIsConst = VarDesc.VarType.bIsConst || ((VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
			LocalInfo.bIsReference = VarDesc.VarType.bIsReference;
			LocalInfo.bIsArray = (VarDesc.VarType.ContainerType == EPinContainerType::Array);
			LocalInfo.bIsSet = (VarDesc.VarType.ContainerType == EPinContainerType::Set);
			LocalInfo.bIsMap = (VarDesc.VarType.ContainerType == EPinContainerType::Map);
			OutInfo.LocalVariables.Add(LocalInfo);
		}
	}

	// Get output parameters from result node
	TArray<UK2Node_FunctionResult*> ResultNodes;
	FunctionGraph->GetNodesOfClass(ResultNodes);

	if (ResultNodes.Num() > 0 && ResultNodes[0])
	{
		UK2Node_FunctionResult* ResultNode = ResultNodes[0];
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
			{
				FBlueprintFunctionParameterInfo ParamInfo;
				ParamInfo.ParameterName = Pin->PinName.ToString();
				ParamInfo.ParameterType = FBlueprintTypeParser::GetFriendlyTypeName(Pin->PinType);
				ParamInfo.bIsOutput = true;
				ParamInfo.bIsReference = Pin->PinType.bIsReference;
				ParamInfo.DefaultValue = Pin->DefaultValue;
				OutInfo.OutputParameters.Add(ParamInfo);
			}
		}
	}

	// Check if this is an override
	if (Blueprint->GeneratedClass)
	{
		UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(FName(*FunctionName));
		if (Function)
		{
			OutInfo.bIsOverride = (Function->GetSuperFunction() != nullptr);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("GetFunctionInfo: Got info for function '%s' in %s"), *FunctionName, *BlueprintPath);
	return true;
}

bool UBlueprintService::AddFunctionInput(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	const FString& ParameterType)
{
	return AddFunctionParameter(BlueprintPath, FunctionName, ParameterName, ParameterType, /*bIsOutput=*/false);
}

bool UBlueprintService::AddFunctionOutput(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	const FString& ParameterType)
{
	return AddFunctionParameter(BlueprintPath, FunctionName, ParameterName, ParameterType, /*bIsOutput=*/true);
}

bool UBlueprintService::RemoveFunctionParameter(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ParameterName,
	bool bIsOutput)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionParameter: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionParameter: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	bool bFound = false;

	if (!bIsOutput)
	{
		// Remove from entry node (input parameters)
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		FunctionGraph->GetNodesOfClass(EntryNodes);

		if (EntryNodes.Num() > 0 && EntryNodes[0])
		{
			UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
			for (int32 i = EntryNode->Pins.Num() - 1; i >= 0; --i)
			{
				UEdGraphPin* Pin = EntryNode->Pins[i];
				if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
				{
					Pin->BreakAllPinLinks();
					EntryNode->Pins.RemoveAt(i);
					bFound = true;
					break;
				}
			}
		}
	}
	else
	{
		// Remove from result node (output parameters)
		TArray<UK2Node_FunctionResult*> ResultNodes;
		FunctionGraph->GetNodesOfClass(ResultNodes);

		for (UK2Node_FunctionResult* ResultNode : ResultNodes)
		{
			if (ResultNode)
			{
				for (int32 i = ResultNode->Pins.Num() - 1; i >= 0; --i)
				{
					UEdGraphPin* Pin = ResultNode->Pins[i];
					if (Pin && Pin->Direction == EGPD_Input && Pin->PinName.ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
					{
						Pin->BreakAllPinLinks();
						ResultNode->Pins.RemoveAt(i);
						bFound = true;
						break;
					}
				}
			}
			if (bFound) break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveFunctionParameter: Parameter '%s' not found in function '%s'"), *ParameterName, *FunctionName);
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveFunctionParameter: Removed parameter '%s' from function '%s'"), *ParameterName, *FunctionName);
	return true;
}

bool UBlueprintService::RemoveFunctionLocalVariable(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionLocalVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionLocalVariable: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	FName VarFName(*VariableName);

	// Try to find and remove the local variable
	UK2Node_FunctionEntry* EntryNode = nullptr;
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0)
	{
		EntryNode = EntryNodes[0];
	}

	if (!EntryNode)
	{
		UE_LOG(LogTemp, Error, TEXT("RemoveFunctionLocalVariable: No entry node found in function '%s'"), *FunctionName);
		return false;
	}

	// Find the local variable
	bool bFound = false;
	for (int32 Index = 0; Index < EntryNode->LocalVariables.Num(); ++Index)
	{
		if (EntryNode->LocalVariables[Index].VarName == VarFName)
		{
			EntryNode->LocalVariables.RemoveAt(Index);
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveFunctionLocalVariable: Local variable '%s' not found in function '%s'"), *VariableName, *FunctionName);
		return false;
	}

	// Remove any variable nodes referencing this local
	FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, VarFName, true, FunctionGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveFunctionLocalVariable: Removed local variable '%s' from function '%s'"), *VariableName, *FunctionName);
	return true;
}

bool UBlueprintService::UpdateFunctionLocalVariable(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName,
	const FString& NewName,
	const FString& NewType,
	const FString& NewDefaultValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateFunctionLocalVariable: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateFunctionLocalVariable: Function '%s' not found in %s"), *FunctionName, *BlueprintPath);
		return false;
	}

	// Get entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() == 0 || !EntryNodes[0])
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateFunctionLocalVariable: No entry node found in function '%s'"), *FunctionName);
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

	// Find the local variable
	FBPVariableDescription* VarDesc = nullptr;
	for (FBPVariableDescription& Var : EntryNode->LocalVariables)
	{
		if (Var.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
		{
			VarDesc = &Var;
			break;
		}
	}

	if (!VarDesc)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateFunctionLocalVariable: Local variable '%s' not found in function '%s'"), *VariableName, *FunctionName);
		return false;
	}

	bool bModified = false;

	// Update type if specified
	if (!NewType.IsEmpty())
	{
		FEdGraphPinType NewPinType;
		FString ErrorMessage;
		if (FBlueprintTypeParser::ParseTypeString(NewType, NewPinType, false, TEXT(""), ErrorMessage))
		{
			VarDesc->VarType = NewPinType;
			VarDesc->DefaultValue.Empty(); // Clear default when type changes
			bModified = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UpdateFunctionLocalVariable: Failed to parse type '%s': %s"), *NewType, *ErrorMessage);
		}
	}

	// Update default value if specified
	if (!NewDefaultValue.IsEmpty())
	{
		VarDesc->DefaultValue = NewDefaultValue;
		bModified = true;
	}

	// Update name if specified
	if (!NewName.IsEmpty() && !NewName.Equals(VariableName, ESearchCase::CaseSensitive))
	{
		VarDesc->VarName = FName(*NewName);
		VarDesc->FriendlyName = FName::NameToDisplayString(NewName, VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean);
		bModified = true;
	}

	if (bModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("UpdateFunctionLocalVariable: Updated local variable '%s' in function '%s'"), *VariableName, *FunctionName);
	}

	return true;
}

TArray<FBlueprintLocalVariableInfo> UBlueprintService::ListFunctionLocalVariables(
	const FString& BlueprintPath,
	const FString& FunctionName)
{
	TArray<FBlueprintLocalVariableInfo> LocalVariables;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return LocalVariables;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return LocalVariables;
	}

	// Get entry node
	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass(EntryNodes);

	if (EntryNodes.Num() > 0 && EntryNodes[0])
	{
		UK2Node_FunctionEntry* EntryNode = EntryNodes[0];

		for (const FBPVariableDescription& VarDesc : EntryNode->LocalVariables)
		{
			FBlueprintLocalVariableInfo LocalInfo;
			LocalInfo.VariableName = VarDesc.VarName.ToString();
			LocalInfo.FriendlyName = VarDesc.FriendlyName;
			LocalInfo.VariableType = FBlueprintTypeParser::GetFriendlyTypeName(VarDesc.VarType);
			LocalInfo.DisplayType = UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString();
			LocalInfo.DefaultValue = VarDesc.DefaultValue;
			LocalInfo.Category = VarDesc.Category.ToString();
			LocalInfo.Guid = VarDesc.VarGuid.ToString();
			LocalInfo.bIsConst = VarDesc.VarType.bIsConst || ((VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
			LocalInfo.bIsReference = VarDesc.VarType.bIsReference;
			LocalInfo.bIsArray = (VarDesc.VarType.ContainerType == EPinContainerType::Array);
			LocalInfo.bIsSet = (VarDesc.VarType.ContainerType == EPinContainerType::Set);
			LocalInfo.bIsMap = (VarDesc.VarType.ContainerType == EPinContainerType::Map);
			LocalVariables.Add(LocalInfo);
		}
	}

	return LocalVariables;
}

// ============================================================================
// NODE MANAGEMENT (Phase 3)
// ============================================================================

UEdGraph* UBlueprintService::FindGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	return nullptr;
}

UEdGraphNode* UBlueprintService::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph)
	{
		return nullptr;
	}

	FGuid SearchGuid;
	if (!FGuid::Parse(NodeId, SearchGuid))
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == SearchGuid)
		{
			return Node;
		}
	}

	return nullptr;
}

FString UBlueprintService::AddGetVariableNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& VariableName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddGetVariableNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// Special handling for Animation Blueprints and EventGraph
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		// AnimBPs may store EventGraph in UbergraphPages
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddGetVariableNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the variable property
	FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VariableName));
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("AddGetVariableNode: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return FString();
	}

	// Create the get variable node
	UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
	GetNode->VariableReference.SetSelfMember(FName(*VariableName));

	// Add to graph
	Graph->AddNode(GetNode, false, false);
	GetNode->CreateNewGuid();
	GetNode->PostPlacedNewNode();
	GetNode->AllocateDefaultPins();

	// Set position
	GetNode->NodePosX = PosX;
	GetNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddGetVariableNode: Added get node for '%s' in %s"), *VariableName, *GraphName);

	return GetNode->NodeGuid.ToString();
}

FString UBlueprintService::AddSetVariableNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& VariableName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSetVariableNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// Special handling for Animation Blueprints and EventGraph
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		// AnimBPs may store EventGraph in UbergraphPages
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSetVariableNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the variable property
	FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VariableName));
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSetVariableNode: Variable '%s' not found in %s"), *VariableName, *BlueprintPath);
		return FString();
	}

	// Create the set variable node
	UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
	SetNode->VariableReference.SetSelfMember(FName(*VariableName));

	// Add to graph
	Graph->AddNode(SetNode, false, false);
	SetNode->CreateNewGuid();
	SetNode->PostPlacedNewNode();
	SetNode->AllocateDefaultPins();

	// Set position
	SetNode->NodePosX = PosX;
	SetNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddSetVariableNode: Added set node for '%s' in %s"), *VariableName, *GraphName);

	return SetNode->NodeGuid.ToString();
}

FString UBlueprintService::AddBranchNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBranchNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBranchNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Create the branch node
	UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);

	// Add to graph
	Graph->AddNode(BranchNode, false, false);
	BranchNode->CreateNewGuid();
	BranchNode->PostPlacedNewNode();
	BranchNode->AllocateDefaultPins();

	// Set position
	BranchNode->NodePosX = PosX;
	BranchNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddBranchNode: Added branch node to %s"), *GraphName);

	return BranchNode->NodeGuid.ToString();
}

FString UBlueprintService::AddCastNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& TargetClass,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCastNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCastNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the target class
	UClass* TargetUClass = FindFirstObject<UClass>(*TargetClass, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("AddCastNode"));
	if (!TargetUClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddCastNode: Class '%s' not found"), *TargetClass);
		return FString();
	}

	// Create the cast node
	UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(Graph);
	CastNode->TargetType = TargetUClass;

	// Add to graph
	Graph->AddNode(CastNode, false, false);
	CastNode->CreateNewGuid();
	CastNode->PostPlacedNewNode();
	CastNode->AllocateDefaultPins();

	// Set position
	CastNode->NodePosX = PosX;
	CastNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddCastNode: Added cast to '%s' in %s"), *TargetClass, *GraphName);

	return CastNode->NodeGuid.ToString();
}

FString UBlueprintService::AddEventNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& EventName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEventNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	// For AnimBlueprints, check UbergraphPages for EventGraph first
	UEdGraph* Graph = nullptr;
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (AnimBP && GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			if (UberGraph && UberGraph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				Graph = UberGraph;
				break;
			}
		}
	}
	
	if (!Graph)
	{
		Graph = FindGraph(Blueprint, GraphName);
	}

	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEventNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Try to find the event function
	UFunction* EventFunction = nullptr;
	
	// Check parent class for the event function
	if (Blueprint->ParentClass)
	{
		EventFunction = Blueprint->ParentClass->FindFunctionByName(FName(*EventName));
	}

	if (!EventFunction)
	{
		UE_LOG(LogTemp, Error, TEXT("AddEventNode: Event function '%s' not found in parent class"), *EventName);
		return FString();
	}

	// Create the event node
	UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
	EventNode->EventReference.SetExternalMember(FName(*EventName), Blueprint->ParentClass);
	EventNode->bOverrideFunction = true;

	// Add to graph
	Graph->AddNode(EventNode, false, false);
	EventNode->CreateNewGuid();
	EventNode->PostPlacedNewNode();
	EventNode->AllocateDefaultPins();

	// Set position
	EventNode->NodePosX = PosX;
	EventNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddEventNode: Added event '%s' in %s"), *EventName, *GraphName);

	return EventNode->NodeGuid.ToString();
}

FString UBlueprintService::AddPrintStringNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddPrintStringNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddPrintStringNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Create the print string node
	UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);

	// Set the function to call
	UFunction* PrintStringFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	if (PrintStringFunc)
	{
		PrintNode->SetFromFunction(PrintStringFunc);
	}

	// Add to graph
	Graph->AddNode(PrintNode, false, false);
	PrintNode->CreateNewGuid();
	PrintNode->PostPlacedNewNode();
	PrintNode->AllocateDefaultPins();

	// Set position
	PrintNode->NodePosX = PosX;
	PrintNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddPrintStringNode: Added print string node to %s"), *GraphName);

	return PrintNode->NodeGuid.ToString();
}

bool UBlueprintService::ConnectNodes(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return false;
	}

	// Find source node
	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	if (!SourceNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Source node '%s' not found"), *SourceNodeId);
		return false;
	}

	// Find target node
	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!TargetNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Target node '%s' not found"), *TargetNodeId);
		return false;
	}

	// Find source pin (output)
	UEdGraphPin* SourcePin = nullptr;
	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output &&
			(Pin->PinName.ToString().Equals(SourcePinName, ESearchCase::IgnoreCase) ||
			 Pin->PinName == FName(*SourcePinName)))
		{
			SourcePin = Pin;
			break;
		}
	}

	if (!SourcePin)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Source pin '%s' not found on node '%s'"), *SourcePinName, *SourceNodeId);
		return false;
	}

	// Find target pin (input)
	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input &&
			(Pin->PinName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase) ||
			 Pin->PinName == FName(*TargetPinName)))
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Target pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeId);
		return false;
	}

	// Make the connection
	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
	if (Schema)
	{
		Schema->TryCreateConnection(SourcePin, TargetPin);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("ConnectNodes: Connected '%s'.'%s' to '%s'.'%s'"),
			*SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("ConnectNodes: Failed to get schema for graph '%s'"), *GraphName);
	return false;
}

TArray<FBlueprintNodeInfo> UBlueprintService::GetNodesInGraph(
	const FString& BlueprintPath,
	const FString& GraphName)
{
	TArray<FBlueprintNodeInfo> NodeInfos;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return NodeInfos;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return NodeInfos;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		FBlueprintNodeInfo NodeInfo;
		NodeInfo.NodeId = Node->NodeGuid.ToString();
		NodeInfo.NodeType = Node->GetClass()->GetName();
		NodeInfo.NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		NodeInfo.PosX = Node->NodePosX;
		NodeInfo.PosY = Node->NodePosY;

		// Get pin names (for quick reference)
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				NodeInfo.PinNames.Add(Pin->PinName.ToString());

				// Also add detailed pin info
				FBlueprintPinInfo PinInfo;
				PinInfo.PinName = Pin->PinName.ToString();
				PinInfo.PinType = Pin->PinType.PinCategory.ToString();
				PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
				PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
				PinInfo.DefaultValue = Pin->DefaultValue;
				NodeInfo.Pins.Add(PinInfo);
			}
		}

		NodeInfos.Add(NodeInfo);
	}

	return NodeInfos;
}

bool UBlueprintService::CompileBlueprint(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CompileBlueprint: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("CompileBlueprint: Compiled %s"), *BlueprintPath);
	return true;
}

// ============================================================================
// ADVANCED NODE OPERATIONS (Phase 4)
// ============================================================================

FString UBlueprintService::AddFunctionCallNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& FunctionOwnerClass,
	const FString& FunctionName,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Graph '%s' not found in %s"), *GraphName, *BlueprintPath);
		return FString();
	}

	// Find the class that owns the function
	UClass* OwnerClass = nullptr;
	UFunction* Function = nullptr;
	
	// Check if this is a self-function call
	if (FunctionOwnerClass.IsEmpty() || FunctionOwnerClass.Equals(TEXT("Self"), ESearchCase::IgnoreCase))
	{
		// First check the generated class for compiled functions
		if (Blueprint->GeneratedClass)
		{
			Function = Blueprint->GeneratedClass->FindFunctionByName(FName(*FunctionName));
		}
		
		// If not found, check function graphs for user-defined functions
		if (!Function)
		{
			for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
			{
				if (FuncGraph && FuncGraph->GetFName() == FName(*FunctionName))
				{
					// Found the function graph, use the generated class function
					if (Blueprint->GeneratedClass)
					{
						Function = Blueprint->GeneratedClass->FindFunctionByName(FName(*FunctionName));
					}
					break;
				}
			}
		}
		
		if (Function)
		{
			OwnerClass = Blueprint->GeneratedClass;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Self function '%s' not found in blueprint"), *FunctionName);
			return FString();
		}
	}
	else
	{
		// Map common class names to their actual classes
		if (FunctionOwnerClass.Equals(TEXT("KismetMathLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetMathLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("KismetSystemLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetSystemLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("KismetStringLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetStringLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("KismetArrayLibrary"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UKismetArrayLibrary::StaticClass();
		}
		else if (FunctionOwnerClass.Equals(TEXT("GameplayStatics"), ESearchCase::IgnoreCase))
		{
			OwnerClass = UGameplayStatics::StaticClass();
		}
		else
		{
			// Try to find the class by name using FindFirstObject (UE5 replacement for FindObject with ANY_PACKAGE)
			OwnerClass = FindFirstObject<UClass>(*FunctionOwnerClass, EFindFirstObjectOptions::ExactClass);
			if (!OwnerClass)
			{
				// Try with U prefix
				OwnerClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *FunctionOwnerClass), EFindFirstObjectOptions::ExactClass);
			}
		}

		if (!OwnerClass)
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Class '%s' not found"), *FunctionOwnerClass);
			return FString();
		}
		
		// Find the function
		Function = OwnerClass->FindFunctionByName(FName(*FunctionName));
		if (!Function)
		{
			UE_LOG(LogTemp, Error, TEXT("AddFunctionCallNode: Function '%s' not found in class '%s'"), *FunctionName, *FunctionOwnerClass);
			return FString();
		}
	}

	// Create the call function node
	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	CallNode->SetFromFunction(Function);

	// Add to graph
	Graph->AddNode(CallNode, false, false);
	CallNode->CreateNewGuid();
	CallNode->PostPlacedNewNode();
	CallNode->AllocateDefaultPins();

	// Set position
	CallNode->NodePosX = PosX;
	CallNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UE_LOG(LogTemp, Log, TEXT("AddFunctionCallNode: Added %s::%s to %s"), *FunctionOwnerClass, *FunctionName, *GraphName);

	return CallNode->NodeGuid.ToString();
}

FString UBlueprintService::AddComparisonNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& ComparisonType,
	const FString& ValueType,
	float PosX,
	float PosY)
{
	// Build the function name based on comparison type and value type
	FString FunctionName;
	
	// UE 5.7: Float operations are now Double - normalize the type
	FString NormalizedType = ValueType;
	if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		NormalizedType = TEXT("Double");
	}
	
	if (ComparisonType.Equals(TEXT("Greater"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Greater_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("Less"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Less_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("GreaterEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("GreaterEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("LessEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("LessEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("Equal"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("EqualEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (ComparisonType.Equals(TEXT("NotEqual"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("NotEqual_%s%s"), *NormalizedType, *NormalizedType);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddComparisonNode: Unknown comparison type '%s'"), *ComparisonType);
		return FString();
	}

	return AddFunctionCallNode(BlueprintPath, GraphName, TEXT("KismetMathLibrary"), FunctionName, PosX, PosY);
}

FString UBlueprintService::AddMathNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& MathOperation,
	const FString& ValueType,
	float PosX,
	float PosY)
{
	// Build the function name based on operation and value type
	FString FunctionName;
	
	// UE 5.7: Float operations are now Double - normalize the type
	FString NormalizedType = ValueType;
	if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		NormalizedType = TEXT("Double");
	}
	
	if (MathOperation.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Add_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Subtract_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Multiply_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Divide"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Divide_%s%s"), *NormalizedType, *NormalizedType);
	}
	else if (MathOperation.Equals(TEXT("Clamp"), ESearchCase::IgnoreCase))
	{
		// Clamp has a different naming convention
		if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FClamp");
		}
		else if (ValueType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("Clamp");
		}
		else if (ValueType.Equals(TEXT("Double"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FClamp64");
		}
		else
		{
			FunctionName = TEXT("FClamp");
		}
	}
	else if (MathOperation.Equals(TEXT("Min"), ESearchCase::IgnoreCase))
	{
		if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FMin");
		}
		else
		{
			FunctionName = TEXT("Min");
		}
	}
	else if (MathOperation.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
	{
		if (ValueType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			FunctionName = TEXT("FMax");
		}
		else
		{
			FunctionName = TEXT("Max");
		}
	}
	else if (MathOperation.Equals(TEXT("Abs"), ESearchCase::IgnoreCase))
	{
		FunctionName = TEXT("Abs");
	}
	else if (MathOperation.Equals(TEXT("Negate"), ESearchCase::IgnoreCase))
	{
		FunctionName = FString::Printf(TEXT("Negate_%s"), *NormalizedType);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddMathNode: Unknown math operation '%s'"), *MathOperation);
		return FString();
	}

	return AddFunctionCallNode(BlueprintPath, GraphName, TEXT("KismetMathLibrary"), FunctionName, PosX, PosY);
}

TArray<FBlueprintConnectionInfo> UBlueprintService::GetConnections(
	const FString& BlueprintPath,
	const FString& GraphName)
{
	TArray<FBlueprintConnectionInfo> Connections;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return Connections;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return Connections;
	}

	// Track which connections we've already added to avoid duplicates
	TSet<FString> AddedConnections;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode())
				{
					continue;
				}

				// Create a unique key for this connection to avoid duplicates
				FString ConnectionKey = FString::Printf(TEXT("%s.%s->%s.%s"),
					*Node->NodeGuid.ToString(),
					*Pin->PinName.ToString(),
					*LinkedPin->GetOwningNode()->NodeGuid.ToString(),
					*LinkedPin->PinName.ToString());

				if (AddedConnections.Contains(ConnectionKey))
				{
					continue;
				}
				AddedConnections.Add(ConnectionKey);

				FBlueprintConnectionInfo Connection;
				Connection.SourceNodeId = Node->NodeGuid.ToString();
				Connection.SourceNodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				Connection.SourcePinName = Pin->PinName.ToString();
				Connection.TargetNodeId = LinkedPin->GetOwningNode()->NodeGuid.ToString();
				Connection.TargetNodeTitle = LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				Connection.TargetPinName = LinkedPin->PinName.ToString();

				Connections.Add(Connection);
			}
		}
	}

	return Connections;
}

TArray<FBlueprintPinInfo> UBlueprintService::GetNodePins(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId)
{
	TArray<FBlueprintPinInfo> PinInfos;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return PinInfos;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return PinInfos;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodePins: Node '%s' not found"), *NodeId);
		return PinInfos;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		FBlueprintPinInfo PinInfo;
		PinInfo.PinName = Pin->PinName.ToString();
		PinInfo.PinType = Pin->PinType.PinCategory.ToString();
		PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
		PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
		PinInfo.DefaultValue = Pin->DefaultValue;

		PinInfos.Add(PinInfo);
	}

	return PinInfos;
}

bool UBlueprintService::DisconnectPin(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin
	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("DisconnectPin: Pin '%s' not found on node '%s'"), *PinName, *NodeId);
		return false;
	}

	if (TargetPin->LinkedTo.Num() == 0)
	{
		return true; // Already disconnected
	}

	// Break all connections
	TargetPin->BreakAllPinLinks();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("DisconnectPin: Disconnected pin '%s' on node '%s'"), *PinName, *NodeId);
	return true;
}

bool UBlueprintService::DeleteNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Node '%s' not found"), *NodeId);
		return false;
	}

	// Don't delete entry or result nodes
	if (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_FunctionResult>())
	{
		UE_LOG(LogTemp, Error, TEXT("DeleteNode: Cannot delete function entry or result nodes"));
		return false;
	}

	// Break all connections first
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}

	// Remove the node
	Graph->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("DeleteNode: Deleted node '%s' from graph '%s'"), *NodeId, *GraphName);
	return true;
}

bool UBlueprintService::SetNodePosition(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePosition: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePosition: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePosition: Node '%s' not found"), *NodeId);
		return false;
	}

	// Set the position
	Node->NodePosX = static_cast<int32>(PosX);
	Node->NodePosY = static_cast<int32>(PosY);
	
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SetNodePosition: Moved node '%s' to (%d, %d)"), *NodeId, Node->NodePosX, Node->NodePosY);
	return true;
}

FString UBlueprintService::CreateBlueprint(
	const FString& BlueprintName,
	const FString& ParentClass,
	const FString& BlueprintPath)
{
	if (BlueprintName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Blueprint name is empty"));
		return FString();
	}

	// Determine parent class
	UClass* ParentClassPtr = AActor::StaticClass(); // Default to Actor
	if (!ParentClass.IsEmpty())
	{
		// Try common class names directly
		if (ParentClass.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = AActor::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("Pawn"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = APawn::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("Character"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = ACharacter::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase))
		{
			ParentClassPtr = APlayerController::StaticClass();
		}
		else
		{
			// Try to find the class by name
			ParentClassPtr = FindObject<UClass>(nullptr, *ParentClass);
			if (!ParentClassPtr)
			{
				// Try with /Script/Engine. prefix
				FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ParentClass);
				ParentClassPtr = FindObject<UClass>(nullptr, *FullPath);
			}
			if (!ParentClassPtr)
			{
				UE_LOG(LogTemp, Warning, TEXT("CreateBlueprint: Parent class '%s' not found, defaulting to Actor"), *ParentClass);
				ParentClassPtr = AActor::StaticClass();
			}
		}
	}

	// Build proper package path
	FString PackagePath = BlueprintPath;
	PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
	PackagePath.TrimStartAndEndInline();
	while (PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath.LeftChopInline(1);
	}
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/Blueprints");
	}

	const FString FullAssetPath = PackagePath + TEXT("/") + BlueprintName;

	// Check if blueprint already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBlueprint: Blueprint already exists at '%s', returning existing path"), *FullAssetPath);
		return FullAssetPath;
	}

	// Create package
	UPackage* Package = CreatePackage(*FullAssetPath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Failed to create package for '%s'"), *FullAssetPath);
		return FString();
	}

	// Create blueprint using BlueprintFactory
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClassPtr;

	UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
		UBlueprint::StaticClass(),
		Package,
		*BlueprintName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn
	));

	if (!NewBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprint: Factory failed to create blueprint '%s'"), *BlueprintName);
		return FString();
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewBlueprint);

	// Mark package dirty
	Package->MarkPackageDirty();

	// Save the asset
	if (!UEditorAssetLibrary::SaveAsset(NewBlueprint->GetPathName(), false))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBlueprint: Created blueprint but failed to save"));
	}

	UE_LOG(LogTemp, Log, TEXT("CreateBlueprint: Created blueprint '%s' at '%s'"), *BlueprintName, *NewBlueprint->GetPathName());
	return NewBlueprint->GetPathName();
}

bool UBlueprintService::GetProperty(
	const FString& BlueprintPath,
	const FString& PropertyName,
	FString& OutValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Blueprint has no generated class"));
		return false;
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Failed to get CDO"));
		return false;
	}

	// Find property
	FProperty* Property = GeneratedClass->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("GetProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Export property value to string
	const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(CDO);
	Property->ExportTextItem_Direct(OutValue, PropertyValue, nullptr, nullptr, PPF_None);

	UE_LOG(LogTemp, Log, TEXT("GetProperty: Got property '%s' = '%s'"), *PropertyName, *OutValue);
	return true;
}

bool UBlueprintService::SetProperty(
	const FString& BlueprintPath,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Blueprint has no generated class"));
		return false;
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Failed to get CDO"));
		return false;
	}

	// Find property
	FProperty* Property = GeneratedClass->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("SetProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Import property value from string
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(CDO);
	Property->ImportText_Direct(*PropertyValue, PropertyAddr, nullptr, PPF_None);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	UE_LOG(LogTemp, Log, TEXT("SetProperty: Set property '%s' = '%s'"), *PropertyName, *PropertyValue);
	return true;
}

bool UBlueprintService::ReparentBlueprint(
	const FString& BlueprintPath,
	const FString& NewParentClass)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentBlueprint: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	if (NewParentClass.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentBlueprint: New parent class is empty"));
		return false;
	}

	// Find the new parent class - try common class names first
	UClass* NewParent = nullptr;
	if (NewParentClass.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
	{
		NewParent = AActor::StaticClass();
	}
	else if (NewParentClass.Equals(TEXT("Pawn"), ESearchCase::IgnoreCase))
	{
		NewParent = APawn::StaticClass();
	}
	else if (NewParentClass.Equals(TEXT("Character"), ESearchCase::IgnoreCase))
	{
		NewParent = ACharacter::StaticClass();
	}
	else if (NewParentClass.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase))
	{
		NewParent = APlayerController::StaticClass();
	}
	else
	{
		// Try to find by name
		NewParent = FindObject<UClass>(nullptr, *NewParentClass);
		if (!NewParent)
		{
			// Try with /Script/Engine. prefix
			FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *NewParentClass);
			NewParent = FindObject<UClass>(nullptr, *FullPath);
		}
	}

	if (!NewParent)
	{
		UE_LOG(LogTemp, Error, TEXT("ReparentBlueprint: New parent class '%s' not found"), *NewParentClass);
		return false;
	}

	// Perform reparenting (UE5 doesn't have FBlueprintEditorUtils::ReparentBlueprint)
	FString OldParentName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None");
	
	// Directly set the parent class
	Blueprint->ParentClass = NewParent;
	
	// Mark for recompilation and save
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);
	
	UE_LOG(LogTemp, Log, TEXT("ReparentBlueprint: Reparented '%s' from '%s' to '%s'"),
		*Blueprint->GetName(), *OldParentName, *NewParent->GetName());

	return true;
}

bool UBlueprintService::DiffBlueprints(
	const FString& BlueprintPathA,
	const FString& BlueprintPathB,
	FString& OutDifferences)
{
	UBlueprint* BlueprintA = LoadBlueprint(BlueprintPathA);
	UBlueprint* BlueprintB = LoadBlueprint(BlueprintPathB);

	if (!BlueprintA || !BlueprintB)
	{
		UE_LOG(LogTemp, Error, TEXT("DiffBlueprints: Failed to load one or both blueprints"));
		return false;
	}

	TArray<FString> Differences;

	// Compare parent classes
	FString ParentA = BlueprintA->ParentClass ? BlueprintA->ParentClass->GetName() : TEXT("None");
	FString ParentB = BlueprintB->ParentClass ? BlueprintB->ParentClass->GetName() : TEXT("None");
	if (ParentA != ParentB)
	{
		Differences.Add(FString::Printf(TEXT("Parent Class: '%s' vs '%s'"), *ParentA, *ParentB));
	}

	// Compare variables
	TSet<FName> VarsA, VarsB;
	for (const FBPVariableDescription& Var : BlueprintA->NewVariables)
	{
		VarsA.Add(Var.VarName);
	}
	for (const FBPVariableDescription& Var : BlueprintB->NewVariables)
	{
		VarsB.Add(Var.VarName);
	}

	TSet<FName> OnlyInA = VarsA.Difference(VarsB);
	TSet<FName> OnlyInB = VarsB.Difference(VarsA);

	if (OnlyInA.Num() > 0)
	{
		TArray<FString> VarNames;
		for (FName VarName : OnlyInA)
		{
			VarNames.Add(VarName.ToString());
		}
		Differences.Add(FString::Printf(TEXT("Variables only in A: %s"), *FString::Join(VarNames, TEXT(", "))));
	}

	if (OnlyInB.Num() > 0)
	{
		TArray<FString> VarNames;
		for (FName VarName : OnlyInB)
		{
			VarNames.Add(VarName.ToString());
		}
		Differences.Add(FString::Printf(TEXT("Variables only in B: %s"), *FString::Join(VarNames, TEXT(", "))));
	}

	// Compare components
	TArray<FBlueprintComponentInfo> CompsA = ListComponents(BlueprintPathA);
	TArray<FBlueprintComponentInfo> CompsB = ListComponents(BlueprintPathB);

	if (CompsA.Num() != CompsB.Num())
	{
		Differences.Add(FString::Printf(TEXT("Component count: %d vs %d"), CompsA.Num(), CompsB.Num()));
	}

	// Build output
	if (Differences.Num() == 0)
	{
		OutDifferences = TEXT("Blueprints are identical");
		return true; // Return true even when identical so Python gets the output string
	}

	OutDifferences = FString::Join(Differences, TEXT("\n"));
	return true; // Has differences
}

// ============================================================================
// NODE MANAGEMENT - Advanced Operations
// ============================================================================

TArray<FBlueprintNodeTypeInfo> UBlueprintService::DiscoverNodes(
	const FString& BlueprintPath,
	const FString& SearchTerm,
	const FString& Category,
	int32 MaxResults)
{
	TArray<FBlueprintNodeTypeInfo> Results;

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("DiscoverNodes: Failed to load blueprint: %s"), *BlueprintPath);
		return Results;
	}

	FString SearchLower = SearchTerm.ToLower();
	FString CategoryLower = Category.ToLower();
	
	// Search through common library functions
	TArray<TPair<UClass*, FString>> FunctionLibraries = {
		{UKismetMathLibrary::StaticClass(), TEXT("Math")},
		{UKismetSystemLibrary::StaticClass(), TEXT("Utilities")},
		{UKismetStringLibrary::StaticClass(), TEXT("String")},
		{UKismetArrayLibrary::StaticClass(), TEXT("Array")},
		{UGameplayStatics::StaticClass(), TEXT("Game")},
	};
	
	for (const auto& [LibClass, LibCategory] : FunctionLibraries)
	{
		if (Results.Num() >= MaxResults)
		{
			break;
		}
		
		// Filter by category if specified
		if (!CategoryLower.IsEmpty() && !LibCategory.ToLower().Contains(CategoryLower))
		{
			continue;
		}
		
		// Iterate functions in this class
		for (TFieldIterator<UFunction> It(LibClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (Results.Num() >= MaxResults)
			{
				break;
			}
			
			UFunction* Func = *It;
			if (!Func)
			{
				continue;
			}
			
			// Only include BlueprintCallable functions
			if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			{
				continue;
			}
			
			FString FuncName = Func->GetName();
			FString DisplayName = Func->GetDisplayNameText().ToString();
			if (DisplayName.IsEmpty())
			{
				DisplayName = FuncName;
			}
			
			// Filter by search term
			if (!SearchLower.IsEmpty())
			{
				bool bMatches = DisplayName.ToLower().Contains(SearchLower) ||
				                FuncName.ToLower().Contains(SearchLower);
				
				// Also check keywords
				FString Keywords = Func->GetMetaData(TEXT("Keywords"));
				if (!Keywords.IsEmpty())
				{
					bMatches = bMatches || Keywords.ToLower().Contains(SearchLower);
				}
				
				if (!bMatches)
				{
					continue;
				}
			}
			
			FBlueprintNodeTypeInfo Info;
			Info.DisplayName = DisplayName;
			Info.Category = LibCategory;
			Info.NodeClass = TEXT("K2Node_CallFunction");
			Info.SpawnerKey = FString::Printf(TEXT("FUNC %s::%s"), *LibClass->GetName(), *FuncName);
			Info.bIsPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
			Info.Tooltip = Func->GetMetaData(TEXT("ToolTip"));
			
			// Get keywords
			FString Keywords = Func->GetMetaData(TEXT("Keywords"));
			if (!Keywords.IsEmpty())
			{
				Keywords.ParseIntoArray(Info.Keywords, TEXT(","), true);
			}
			
			Results.Add(Info);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("DiscoverNodes: Found %d nodes matching '%s' in category '%s'"), 
		Results.Num(), *SearchTerm, *Category);
	
	return Results;
}

bool UBlueprintService::GetNodeDetails(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	FBlueprintNodeDetailedInfo& OutInfo)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodeDetails: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodeDetails: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("GetNodeDetails: Node '%s' not found"), *NodeId);
		return false;
	}

	// Basic info
	OutInfo.NodeId = Node->NodeGuid.ToString();
	OutInfo.NodeClass = Node->GetClass()->GetName();
	OutInfo.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	OutInfo.FullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	OutInfo.GraphName = Graph->GetName();
	OutInfo.Tooltip = Node->GetTooltipText().ToString();
	OutInfo.PosX = Node->NodePosX;
	OutInfo.PosY = Node->NodePosY;

	// Determine graph scope
	if (Blueprint->UbergraphPages.Contains(Graph))
	{
		OutInfo.GraphScope = TEXT("event");
	}
	else if (Blueprint->FunctionGraphs.Contains(Graph))
	{
		OutInfo.GraphScope = TEXT("function");
	}
	else if (Blueprint->MacroGraphs.Contains(Graph))
	{
		OutInfo.GraphScope = TEXT("macro");
	}
	else
	{
		OutInfo.GraphScope = TEXT("unknown");
	}

	// Check if pure (K2Node has this)
	if (UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		OutInfo.bIsPure = K2Node->IsNodePure();
	}

	// Function call specific info
	if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
	{
		if (UFunction* Func = FuncNode->GetTargetFunction())
		{
			OutInfo.FunctionName = Func->GetName();
			OutInfo.FunctionClass = Func->GetOuterUClass()->GetName();
			OutInfo.bIsLatent = Func->HasMetaData(TEXT("Latent"));
		}
	}

	// Variable node specific info
	if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
	{
		OutInfo.VariableName = VarGetNode->GetVarName().ToString();
	}
	else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
	{
		OutInfo.VariableName = VarSetNode->GetVarName().ToString();
	}

	// Get the schema for pin operations
	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());

	// Process pins
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->bHidden)
		{
			continue;
		}

		FBlueprintPinDetailedInfo PinInfo;
		PinInfo.PinName = Pin->PinName.ToString();
		PinInfo.DisplayName = Pin->GetDisplayName().ToString();
		PinInfo.PinCategory = Pin->PinType.PinCategory.ToString();
		PinInfo.PinSubCategory = Pin->PinType.PinSubCategory.ToString();
		
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PinInfo.TypePath = Pin->PinType.PinSubCategoryObject->GetPathName();
		}
		
		PinInfo.bIsInput = (Pin->Direction == EGPD_Input);
		PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
		PinInfo.bIsHidden = Pin->bHidden;
		PinInfo.bIsArray = Pin->PinType.ContainerType == EPinContainerType::Array;
		PinInfo.bIsReference = Pin->PinType.bIsReference;
		PinInfo.DefaultValue = Pin->DefaultValue;
		PinInfo.Tooltip = Pin->PinToolTip;

		// Check if can split
		if (Schema && Pin)
		{
			PinInfo.bCanSplit = Schema->CanSplitStructPin(*Pin);
			PinInfo.bIsSplit = Pin->SubPins.Num() > 0;
		}

		// Get connections
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				FString Connection = FString::Printf(TEXT("%s:%s"),
					*LinkedPin->GetOwningNode()->NodeGuid.ToString(),
					*LinkedPin->PinName.ToString());
				PinInfo.Connections.Add(Connection);
			}
		}

		if (PinInfo.bIsInput)
		{
			OutInfo.InputPins.Add(PinInfo);
		}
		else
		{
			OutInfo.OutputPins.Add(PinInfo);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("GetNodeDetails: Got details for node '%s' (%s)"), *NodeId, *OutInfo.NodeTitle);
	return true;
}

bool UBlueprintService::SetNodePinValue(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* TestPin : Node->Pins)
	{
		if (TestPin && TestPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			Pin = TestPin;
			break;
		}
	}

	if (!Pin)
	{
		// Try display name
		for (UEdGraphPin* TestPin : Node->Pins)
		{
			if (TestPin && TestPin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				Pin = TestPin;
				break;
			}
		}
	}

	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Pin '%s' not found on node"), *PinName);
		return false;
	}

	if (Pin->Direction != EGPD_Input)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodePinValue: Pin '%s' is not an input pin"), *PinName);
		return false;
	}

	// Set the default value
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema)
	{
		Schema->TrySetDefaultValue(*Pin, Value);
	}
	else
	{
		Pin->DefaultValue = Value;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("SetNodePinValue: Set pin '%s' on node '%s' to '%s'"), *PinName, *NodeId, *Value);
	return true;
}

bool UBlueprintService::SplitPin(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* TestPin : Node->Pins)
	{
		if (TestPin && TestPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			Pin = TestPin;
			break;
		}
	}

	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Pin '%s' not found on node"), *PinName);
		return false;
	}

	// Get schema
	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());
	if (!Schema)
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Failed to get K2 schema"));
		return false;
	}

	// Check if can split
	if (!Schema->CanSplitStructPin(*Pin))
	{
		UE_LOG(LogTemp, Error, TEXT("SplitPin: Pin '%s' cannot be split (not a splittable struct type)"), *PinName);
		return false;
	}

	// Already split?
	if (Pin->SubPins.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SplitPin: Pin '%s' is already split"), *PinName);
		return true; // Already in desired state
	}

	// Perform split
	Schema->SplitPin(Pin);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("SplitPin: Split pin '%s' on node '%s'"), *PinName, *NodeId);
	return true;
}

bool UBlueprintService::RecombinePin(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PinName)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the pin (or its parent if already split)
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* TestPin : Node->Pins)
	{
		if (TestPin && TestPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			Pin = TestPin;
			break;
		}
	}

	// Check parent if pin is a sub-pin (e.g., ReturnValue_X -> ReturnValue)
	if (!Pin)
	{
		for (UEdGraphPin* TestPin : Node->Pins)
		{
			if (TestPin && TestPin->ParentPin)
			{
				if (TestPin->ParentPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
				{
					Pin = TestPin->ParentPin;
					break;
				}
			}
		}
	}

	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Pin '%s' not found on node"), *PinName);
		return false;
	}

	// Make sure we have the parent pin
	if (Pin->ParentPin)
	{
		Pin = Pin->ParentPin;
	}

	// Check if already recombined
	if (Pin->SubPins.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RecombinePin: Pin '%s' is already recombined"), *PinName);
		return true;
	}

	// Get schema
	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());
	if (!Schema)
	{
		UE_LOG(LogTemp, Error, TEXT("RecombinePin: Failed to get K2 schema"));
		return false;
	}

	// Perform recombine
	Schema->RecombinePin(Pin);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("RecombinePin: Recombined pin '%s' on node '%s'"), *PinName, *NodeId);
	return true;
}

bool UBlueprintService::RefreshNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	bool bCompile)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshNode: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshNode: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshNode: Node '%s' not found"), *NodeId);
		return false;
	}

	// Reconstruct the node (refreshes pins based on current function signature)
	Node->ReconstructNode();
	
	// Mark as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Compile if requested
	if (bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	UE_LOG(LogTemp, Log, TEXT("RefreshNode: Refreshed node '%s' in graph '%s'"), *NodeId, *GraphName);
	return true;
}

bool UBlueprintService::ConfigureNode(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeId,
	const FString& PropertyName,
	const FString& Value)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Failed to load blueprint: %s"), *BlueprintPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Node '%s' not found"), *NodeId);
		return false;
	}

	// Find the property on the node
	FProperty* Property = Node->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Property '%s' not found on node"), *PropertyName);
		return false;
	}

	// Try to set the property value
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Node);
	
	// Handle special cases for class/object references
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		// Load class from path
		UClass* LoadedClass = LoadObject<UClass>(nullptr, *Value);
		if (!LoadedClass)
		{
			// Try finding by name
			LoadedClass = FindObject<UClass>(nullptr, *Value);
		}
		if (LoadedClass)
		{
			ClassProp->SetPropertyValue(PropertyAddr, LoadedClass);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ConfigureNode: Failed to load class '%s'"), *Value);
			return false;
		}
	}
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		// Use generic import for soft class references
		Property->ImportText_Direct(*Value, PropertyAddr, nullptr, PPF_None);
	}
	else
	{
		// Use generic import
		Property->ImportText_Direct(*Value, PropertyAddr, nullptr, PPF_None);
	}

	// Reconstruct node to apply changes
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("ConfigureNode: Set property '%s' = '%s' on node '%s'"), *PropertyName, *Value, *NodeId);
	return true;
}

FString UBlueprintService::CreateNodeByKey(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& SpawnerKey,
	float PosX,
	float PosY)
{
	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Failed to load blueprint: %s"), *BlueprintPath);
		return FString();
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// Parse spawner key - format: "FUNC ClassName::FunctionName" or "NODE NodeClassName"
	FString KeyType, KeyValue;
	if (!SpawnerKey.Split(TEXT(" "), &KeyType, &KeyValue))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Invalid spawner key format: %s"), *SpawnerKey);
		return FString();
	}

	UEdGraphNode* NewNode = nullptr;

	if (KeyType.Equals(TEXT("FUNC"), ESearchCase::IgnoreCase))
	{
		// Function call node
		FString ClassName, FunctionName;
		if (!KeyValue.Split(TEXT("::"), &ClassName, &FunctionName))
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Invalid function key format: %s"), *KeyValue);
			return FString();
		}

		// Find the function
		UClass* OwnerClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
		if (!OwnerClass)
		{
			// Try more search paths
			OwnerClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::ExactClass);
		}

		if (!OwnerClass)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Class '%s' not found"), *ClassName);
			return FString();
		}

		UFunction* Function = OwnerClass->FindFunctionByName(*FunctionName);
		if (!Function)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Function '%s' not found in class '%s'"), *FunctionName, *ClassName);
			return FString();
		}

		// Create the function call node
		UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
		FuncNode->SetFromFunction(Function);
		FuncNode->NodePosX = PosX;
		FuncNode->NodePosY = PosY;
		FuncNode->AllocateDefaultPins();
		Graph->AddNode(FuncNode, true, false);
		NewNode = FuncNode;
	}
	else if (KeyType.Equals(TEXT("NODE"), ESearchCase::IgnoreCase))
	{
		// Generic node creation - find the node class
		UClass* NodeClass = FindFirstObject<UClass>(*KeyValue, EFindFirstObjectOptions::ExactClass);
		if (!NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Node class '%s' not found"), *KeyValue);
			return FString();
		}

		NewNode = NewObject<UEdGraphNode>(Graph, NodeClass);
		NewNode->NodePosX = PosX;
		NewNode->NodePosY = PosY;
		NewNode->AllocateDefaultPins();
		Graph->AddNode(NewNode, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNodeByKey: Unknown key type: %s"), *KeyType);
		return FString();
	}

	if (NewNode)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("CreateNodeByKey: Created node with key '%s' at (%f, %f)"), *SpawnerKey, PosX, PosY);
		return NewNode->NodeGuid.ToString();
	}

	return FString();
}

// ============================================================================
// EXISTENCE CHECKS - Fast boolean checks before creation (Idempotency)
// ============================================================================

bool UBlueprintService::BlueprintExists(const FString& BlueprintPath)
{
	if (BlueprintPath.IsEmpty())
	{
		return false;
	}

	// Fast path: use DoesAssetExist which doesn't load the asset
	return UEditorAssetLibrary::DoesAssetExist(BlueprintPath);
}

bool UBlueprintService::VariableExists(const FString& BlueprintPath, const FString& VariableName)
{
	if (VariableName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::FunctionExists(const FString& BlueprintPath, const FString& FunctionName)
{
	if (FunctionName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	// Check function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// Also check generated class for functions (including inherited/overridden)
	if (UClass* GeneratedClass = Blueprint->GeneratedClass)
	{
		if (GeneratedClass->FindFunctionByName(FName(*FunctionName)))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::ComponentExists(const FString& BlueprintPath, const FString& ComponentName)
{
	if (ComponentName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return false;
	}

	const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::LocalVariableExists(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& VariableName)
{
	if (FunctionName.IsEmpty() || VariableName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return false;
	}

	// Get the entry node which contains local variables
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
			{
				if (LocalVar.VarName.ToString().Equals(VariableName, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			break;
		}
	}

	return false;
}

bool UBlueprintService::NodeExists(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& NodeTitle)
{
	if (GraphName.IsEmpty() || NodeTitle.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		// Check full title
		FString FullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		if (FullTitle.Equals(NodeTitle, ESearchCase::IgnoreCase))
		{
			return true;
		}

		// Also check compact title (shorter version)
		FString CompactTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (CompactTitle.Equals(NodeTitle, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UBlueprintService::FunctionCallExists(
	const FString& BlueprintPath,
	const FString& GraphName,
	const FString& FunctionName)
{
	if (GraphName.IsEmpty() || FunctionName.IsEmpty())
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(BlueprintPath);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			FName FuncName = CallNode->FunctionReference.GetMemberName();
			if (FuncName.ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}

	return false;
}
