// Copyright VibeUE 2025

#include "Services/Blueprint/BlueprintReflectionService.h"
#include "Services/Blueprint/BlueprintPropertyService.h"
#include "Services/Blueprint/BlueprintFunctionService.h"
#include "Commands/BlueprintReflection.h"
#include "Commands/InputKeyEnumerator.h"
#include "Core/ErrorCodes.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SoftObjectPath.h"
#include "Commands/BlueprintReflection.h"  // For FBlueprintReflection static methods

// Declare log category
DEFINE_LOG_CATEGORY_STATIC(LogBlueprintReflectionService, Log, All);

FBlueprintReflectionService::FBlueprintReflectionService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
	, bParentClassesInitialized(false)
	, bComponentTypesInitialized(false)
	, bPropertyTypesInitialized(false)
{
}

// ═══════════════════════════════════════════════════════════
// Type Discovery
// ═══════════════════════════════════════════════════════════

TResult<TArray<FString>> FBlueprintReflectionService::GetAvailableParentClasses()
{
	if (!bParentClassesInitialized)
	{
		PopulateParentClassCatalog();
		bParentClassesInitialized = true;
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Returned %d parent classes"), CachedParentClasses.Num());
	return TResult<TArray<FString>>::Success(CachedParentClasses);
}

TResult<TArray<FString>> FBlueprintReflectionService::GetAvailableComponentTypes()
{
	if (!bComponentTypesInitialized)
	{
		PopulateComponentTypeCatalog();
		bComponentTypesInitialized = true;
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Returned %d component types"), CachedComponentTypes.Num());
	return TResult<TArray<FString>>::Success(CachedComponentTypes);
}

TResult<TArray<FString>> FBlueprintReflectionService::GetAvailablePropertyTypes()
{
	if (!bPropertyTypesInitialized)
	{
		PopulatePropertyTypeCatalog();
		bPropertyTypesInitialized = true;
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Returned %d property types"), CachedPropertyTypes.Num());
	return TResult<TArray<FString>>::Success(CachedPropertyTypes);
}

TResult<TArray<FNodeTypeInfo>> FBlueprintReflectionService::GetAvailableNodeTypes(UBlueprint* Blueprint, const FNodeTypeSearchCriteria& Criteria)
{
	// Validate Blueprint
	auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint"));
	if (ValidationResult.IsError())
	{
		return TResult<TArray<FNodeTypeInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Discovering available node types for Blueprint: %s"), *Blueprint->GetName());
	
	// Use descriptor-based discovery from FBlueprintReflection
	FString SearchTerm = Criteria.SearchTerm.Get(TEXT(""));
	FString CategoryFilter = Criteria.Category.Get(TEXT(""));
	FString ClassFilter = Criteria.ClassFilter.Get(TEXT(""));
	
	TArray<FBlueprintReflection::FNodeSpawnerDescriptor> Descriptors = 
		FBlueprintReflection::DiscoverNodesWithDescriptors(Blueprint, SearchTerm, CategoryFilter, ClassFilter, Criteria.MaxResults);
	
	// Convert descriptors to FNodeTypeInfo
	TArray<FNodeTypeInfo> NodeTypes;
	
	for (const FBlueprintReflection::FNodeSpawnerDescriptor& Desc : Descriptors)
	{
		// Apply type filters
		if (!Criteria.bIncludeFunctions && Desc.NodeType == TEXT("function_call")) continue;
		if (!Criteria.bIncludeVariables && (Desc.NodeType == TEXT("variable_get") || Desc.NodeType == TEXT("variable_set"))) continue;
		if (!Criteria.bIncludeEvents && Desc.NodeType == TEXT("event")) continue;
		
		FNodeTypeInfo Info;
		Info.SpawnerKey = Desc.SpawnerKey;
		Info.NodeTitle = Desc.DisplayName;
		Info.Category = Desc.Category;
		Info.NodeType = Desc.NodeType;
		Info.Description = Desc.Description;
		
		// Convert Keywords array to comma-separated string
		Info.Keywords = FString::Join(Desc.Keywords, TEXT(", "));
		
		Info.ExpectedPinCount = Desc.ExpectedPinCount;
		Info.bIsStatic = Desc.bIsStatic;
		
		NodeTypes.Add(Info);
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Discovered %d node types"), NodeTypes.Num());
	return TResult<TArray<FNodeTypeInfo>>::Success(NodeTypes);
}

// ═══════════════════════════════════════════════════════════
// Class Metadata
// ═══════════════════════════════════════════════════════════

TResult<FClassInfo> FBlueprintReflectionService::GetClassInfo(UClass* Class)
{
	if (!Class)
	{
		return TResult<FClassInfo>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Class is null"));
	}
	
	FClassInfo Info;
	Info.ClassName = Class->GetName();
	Info.ClassPath = Class->GetPathName();
	
	if (UClass* SuperClass = Class->GetSuperClass())
	{
		Info.ParentClass = SuperClass->GetName();
	}
	
	Info.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
	Info.bIsBlueprint = Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
	
	return TResult<FClassInfo>::Success(Info);
}

TResult<TArray<FPropertyInfo>> FBlueprintReflectionService::GetClassProperties(UClass* Class)
{
	if (!Class)
	{
		return TResult<TArray<FPropertyInfo>>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Class is null"));
	}
	
	TArray<FPropertyInfo> Properties;
	
	for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}
		
		FPropertyInfo PropertyInfo;
		ExtractPropertyInfo(Property, PropertyInfo);
		Properties.Add(PropertyInfo);
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Extracted %d properties from class %s"), 
		Properties.Num(), *Class->GetName());
	
	return TResult<TArray<FPropertyInfo>>::Success(Properties);
}

TResult<TArray<FFunctionInfo>> FBlueprintReflectionService::GetClassFunctions(UClass* Class)
{
	if (!Class)
	{
		return TResult<TArray<FFunctionInfo>>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Class is null"));
	}
	
	TArray<FFunctionInfo> Functions;
	
	for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (!Function)
		{
			continue;
		}
		
		FFunctionInfo FunctionInfo;
		ExtractFunctionInfo(Function, FunctionInfo);
		Functions.Add(FunctionInfo);
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Extracted %d functions from class %s"), 
		Functions.Num(), *Class->GetName());
	
	return TResult<TArray<FFunctionInfo>>::Success(Functions);
}

// ═══════════════════════════════════════════════════════════
// Type Validation
// ═══════════════════════════════════════════════════════════

TResult<bool> FBlueprintReflectionService::IsValidParentClass(const FString& ClassName)
{
	// Try to resolve the class
	TResult<UClass*> ResolveResult = ResolveClass(ClassName);
	if (!ResolveResult.IsSuccess())
	{
		return TResult<bool>::Error(ResolveResult.GetErrorCode(), ResolveResult.GetErrorMessage());
	}
	
	UClass* Class = ResolveResult.GetValue();
	bool bIsValid = IsClassValidForBlueprints(Class);
	
	return TResult<bool>::Success(bIsValid);
}

TResult<bool> FBlueprintReflectionService::IsValidComponentType(const FString& ComponentType)
{
	// Try to resolve the component class
	TResult<UClass*> ResolveResult = ResolveClass(ComponentType);
	if (!ResolveResult.IsSuccess())
	{
		return TResult<bool>::Error(ResolveResult.GetErrorCode(), ResolveResult.GetErrorMessage());
	}
	
	UClass* Class = ResolveResult.GetValue();
	bool bIsValid = IsComponentTypeValid(Class);
	
	return TResult<bool>::Success(bIsValid);
}

TResult<bool> FBlueprintReflectionService::IsValidPropertyType(const FString& PropertyType)
{
	// Property types include primitives and UObject types
	// Simple validation: check if it's in our catalog or is a known primitive
	
	if (!bPropertyTypesInitialized)
	{
		PopulatePropertyTypeCatalog();
		bPropertyTypesInitialized = true;
	}
	
	bool bIsValid = CachedPropertyTypes.Contains(PropertyType);
	
	// Check for primitive types
	if (!bIsValid)
	{
		TArray<FString> PrimitiveTypes = {
			TEXT("bool"), TEXT("int32"), TEXT("float"), TEXT("FString"), TEXT("FName"),
			TEXT("FText"), TEXT("FVector"), TEXT("FRotator"), TEXT("FTransform")
		};
		
		bIsValid = PrimitiveTypes.Contains(PropertyType);
	}
	
	return TResult<bool>::Success(bIsValid);
}

// ═══════════════════════════════════════════════════════════
// Type Conversion
// ═══════════════════════════════════════════════════════════

TResult<UClass*> FBlueprintReflectionService::ResolveClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return TResult<UClass*>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Class name is empty"));
	}
	
	UClass* ResolvedClass = nullptr;
	
	// Try direct load
	ResolvedClass = FindObject<UClass>(nullptr, *ClassName);
	
	// Try with /Script/ prefix
	if (!ResolvedClass && !ClassName.StartsWith(TEXT("/Script/")))
	{
		FString ScriptPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
		ResolvedClass = FindObject<UClass>(nullptr, *ScriptPath);
	}
	
	// Try loading as soft object path
	if (!ResolvedClass)
	{
		FSoftClassPath SoftClassPath(ClassName);
		if (SoftClassPath.IsValid())
		{
			ResolvedClass = SoftClassPath.TryLoadClass<UObject>();
		}
	}
	
	if (ResolvedClass)
	{
		return TResult<UClass*>::Success(ResolvedClass);
	}
	
	return TResult<UClass*>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
		FString::Printf(TEXT("Could not resolve class: %s"), *ClassName));
}

TResult<FString> FBlueprintReflectionService::GetClassPath(UClass* Class)
{
	if (!Class)
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Class is null"));
	}
	
	return TResult<FString>::Success(Class->GetPathName());
}

// ═══════════════════════════════════════════════════════════
// Private Helper Methods
// ═══════════════════════════════════════════════════════════

void FBlueprintReflectionService::PopulateParentClassCatalog()
{
	CachedParentClasses.Empty();
	
	// Common Blueprint parent classes
	TArray<UClass*> CommonParents = {
		AActor::StaticClass(),
		UActorComponent::StaticClass(),
		USceneComponent::StaticClass(),
		UObject::StaticClass()
	};
	
	for (UClass* ParentClass : CommonParents)
	{
		if (ParentClass && IsClassValidForBlueprints(ParentClass))
		{
			CachedParentClasses.Add(ParentClass->GetName());
		}
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Populated %d parent classes"), 
		CachedParentClasses.Num());
}

void FBlueprintReflectionService::PopulateComponentTypeCatalog()
{
	CachedComponentTypes.Empty();
	
	// Iterate through all UActorComponent subclasses
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		
		if (Class->IsChildOf(UActorComponent::StaticClass()))
		{
			CachedComponentTypes.Add(Class->GetName());
		}
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Populated %d component types"), 
		CachedComponentTypes.Num());
}

void FBlueprintReflectionService::PopulatePropertyTypeCatalog()
{
	CachedPropertyTypes.Empty();
	
	// Add common property types
	TArray<FString> CommonTypes = {
		TEXT("bool"), TEXT("int32"), TEXT("float"), TEXT("double"),
		TEXT("FString"), TEXT("FName"), TEXT("FText"),
		TEXT("FVector"), TEXT("FRotator"), TEXT("FTransform"),
		TEXT("FLinearColor"), TEXT("FColor")
	};
	
	CachedPropertyTypes.Append(CommonTypes);
	
	// Add common UObject types
	TArray<UClass*> CommonObjectTypes = {
		UObject::StaticClass(),
		AActor::StaticClass(),
		UActorComponent::StaticClass()
	};
	
	for (UClass* ObjectType : CommonObjectTypes)
	{
		if (ObjectType)
		{
			CachedPropertyTypes.Add(ObjectType->GetName());
		}
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Populated %d property types"), 
		CachedPropertyTypes.Num());
}

void FBlueprintReflectionService::ExtractPropertyInfo(FProperty* Property, FPropertyInfo& OutPropertyInfo)
{
	if (!Property)
	{
		return;
	}
	
	OutPropertyInfo.PropertyName = Property->GetName();
	OutPropertyInfo.PropertyType = Property->GetCPPType();
	OutPropertyInfo.Category = Property->GetMetaData(TEXT("Category"));
	OutPropertyInfo.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
}

void FBlueprintReflectionService::ExtractFunctionInfo(UFunction* Function, FFunctionInfo& OutFunctionInfo)
{
	if (!Function)
	{
		return;
	}
	
	// FFunctionInfo uses Name, GraphGuid, and NodeCount fields
	OutFunctionInfo.Name = Function->GetName();
	OutFunctionInfo.GraphGuid = TEXT(""); // Not populated here
	OutFunctionInfo.NodeCount = 0; // Not populated here
}

bool FBlueprintReflectionService::IsClassValidForBlueprints(UClass* Class)
{
	if (!Class)
	{
		return false;
	}
	
	// Check if class is abstract or deprecated
	if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}
	
	// Check if it's a blueprint type
	if (!Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return false;
	}
	
	return true;
}

bool FBlueprintReflectionService::IsComponentTypeValid(UClass* ComponentClass)
{
	if (!ComponentClass)
	{
		return false;
	}
	
	// Must be derived from UActorComponent
	if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return false;
	}
	
	// Check if class is abstract or deprecated
	if (ComponentClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}
	
	return true;
}

// ═══════════════════════════════════════════════════════════
// Input Key Discovery
// ═══════════════════════════════════════════════════════════

TResult<FInputKeyResult> FBlueprintReflectionService::GetAllInputKeys(
	const FString& Category, 
	bool bIncludeDeprecated)
{
	FInputKeyResult Result;
	Result.Category = Category;
	
	// Get input keys using FInputKeyEnumerator
	TArray<FInputKeyInfo> Keys;
	
	if (Category == TEXT("All"))
	{
		Result.TotalCount = FInputKeyEnumerator::GetAllInputKeys(Keys, bIncludeDeprecated);
	}
	else
	{
		Result.TotalCount = FInputKeyEnumerator::GetInputKeysByCategory(Category, Keys);
	}
	
	// Convert to JSON objects
	for (const FInputKeyInfo& KeyInfo : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
		KeyObj->SetStringField(TEXT("key_name"), KeyInfo.KeyName);
		KeyObj->SetStringField(TEXT("display_name"), KeyInfo.DisplayName);
		KeyObj->SetStringField(TEXT("menu_category"), KeyInfo.MenuCategory);
		KeyObj->SetStringField(TEXT("category"), KeyInfo.Category);
		KeyObj->SetBoolField(TEXT("is_gamepad"), KeyInfo.bIsGamepadKey);
		KeyObj->SetBoolField(TEXT("is_mouse"), KeyInfo.bIsMouseButton);
		KeyObj->SetBoolField(TEXT("is_keyboard"), KeyInfo.bIsKeyboard);
		KeyObj->SetBoolField(TEXT("is_modifier"), KeyInfo.bIsModifierKey);
		KeyObj->SetBoolField(TEXT("is_digital"), KeyInfo.bIsDigital);
		KeyObj->SetBoolField(TEXT("is_analog"), KeyInfo.bIsAnalog);
		KeyObj->SetBoolField(TEXT("is_bindable"), KeyInfo.bIsBindableInBlueprints);
		
		Result.Keys.Add(KeyObj);
		
		// Calculate statistics
		if (KeyInfo.bIsGamepadKey)
		{
			Result.GamepadCount++;
		}
		else if (KeyInfo.bIsMouseButton)
		{
			Result.MouseCount++;
		}
		else if (KeyInfo.bIsKeyboard)
		{
			Result.KeyboardCount++;
		}
		else
		{
			Result.OtherCount++;
		}
	}
	
	UE_LOG(LogBlueprintReflectionService, Log, 
		TEXT("Discovered %d input keys (Category: %s, Keyboard: %d, Mouse: %d, Gamepad: %d, Other: %d)"),
		Result.TotalCount, *Category, Result.KeyboardCount, Result.MouseCount, Result.GamepadCount, Result.OtherCount);
	
	return TResult<FInputKeyResult>::Success(Result);
}
