#include "Services/Blueprint/BlueprintReflectionService.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SoftObjectPath.h"

// Declare log category
DEFINE_LOG_CATEGORY_STATIC(LogBlueprintReflectionService, Log, All);

FBlueprintReflectionService::FBlueprintReflectionService()
	: bParentClassesInitialized(false)
	, bComponentTypesInitialized(false)
	, bPropertyTypesInitialized(false)
{
}

// ═══════════════════════════════════════════════════════════
// Type Discovery
// ═══════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetAvailableParentClasses()
{
	if (!bParentClassesInitialized)
	{
		PopulateParentClassCatalog();
		bParentClassesInitialized = true;
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> ClassArray;
	for (const FString& ClassName : CachedParentClasses)
	{
		ClassArray.Add(MakeShareable(new FJsonValueString(ClassName)));
	}
	
	Response->SetArrayField(TEXT("parent_classes"), ClassArray);
	Response->SetNumberField(TEXT("count"), CachedParentClasses.Num());
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Returned %d parent classes"), CachedParentClasses.Num());
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetAvailableComponentTypes()
{
	if (!bComponentTypesInitialized)
	{
		PopulateComponentTypeCatalog();
		bComponentTypesInitialized = true;
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	for (const FString& ComponentType : CachedComponentTypes)
	{
		ComponentArray.Add(MakeShareable(new FJsonValueString(ComponentType)));
	}
	
	Response->SetArrayField(TEXT("component_types"), ComponentArray);
	Response->SetNumberField(TEXT("count"), CachedComponentTypes.Num());
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Returned %d component types"), CachedComponentTypes.Num());
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetAvailablePropertyTypes()
{
	if (!bPropertyTypesInitialized)
	{
		PopulatePropertyTypeCatalog();
		bPropertyTypesInitialized = true;
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> PropertyArray;
	for (const FString& PropertyType : CachedPropertyTypes)
	{
		PropertyArray.Add(MakeShareable(new FJsonValueString(PropertyType)));
	}
	
	Response->SetArrayField(TEXT("property_types"), PropertyArray);
	Response->SetNumberField(TEXT("count"), CachedPropertyTypes.Num());
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Returned %d property types"), CachedPropertyTypes.Num());
	
	return Response;
}

// ═══════════════════════════════════════════════════════════
// Class Metadata
// ═══════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetClassInfo(UClass* Class)
{
	if (!Class)
	{
		return CreateErrorResponse(TEXT("INVALID_CLASS"), TEXT("Class is null"));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> ClassInfo = MakeShareable(new FJsonObject);
	
	ClassInfo->SetStringField(TEXT("name"), Class->GetName());
	ClassInfo->SetStringField(TEXT("path"), Class->GetPathName());
	ClassInfo->SetStringField(TEXT("display_name"), Class->GetDisplayNameText().ToString());
	
	if (UClass* SuperClass = Class->GetSuperClass())
	{
		ClassInfo->SetStringField(TEXT("parent_class"), SuperClass->GetName());
		ClassInfo->SetStringField(TEXT("parent_path"), SuperClass->GetPathName());
	}
	
	ClassInfo->SetBoolField(TEXT("is_abstract"), Class->HasAnyClassFlags(CLASS_Abstract));
	ClassInfo->SetBoolField(TEXT("is_blueprint_type"), Class->HasAnyClassFlags(CLASS_BlueprintType));
	ClassInfo->SetBoolField(TEXT("is_placeable"), !Class->HasAnyClassFlags(CLASS_NotPlaceable));
	
	Response->SetObjectField(TEXT("class_info"), ClassInfo);
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetClassProperties(UClass* Class)
{
	if (!Class)
	{
		return CreateErrorResponse(TEXT("INVALID_CLASS"), TEXT("Class is null"));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	
	for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}
		
		TSharedPtr<FJsonObject> PropertyInfo = MakeShareable(new FJsonObject);
		ExtractPropertyInfo(Property, PropertyInfo);
		
		PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropertyInfo)));
	}
	
	Response->SetArrayField(TEXT("properties"), PropertiesArray);
	Response->SetNumberField(TEXT("count"), PropertiesArray.Num());
	Response->SetStringField(TEXT("class_name"), Class->GetName());
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Extracted %d properties from class %s"), 
		PropertiesArray.Num(), *Class->GetName());
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetClassFunctions(UClass* Class)
{
	if (!Class)
	{
		return CreateErrorResponse(TEXT("INVALID_CLASS"), TEXT("Class is null"));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	
	for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (!Function)
		{
			continue;
		}
		
		TSharedPtr<FJsonObject> FunctionInfo = MakeShareable(new FJsonObject);
		ExtractFunctionInfo(Function, FunctionInfo);
		
		FunctionsArray.Add(MakeShareable(new FJsonValueObject(FunctionInfo)));
	}
	
	Response->SetArrayField(TEXT("functions"), FunctionsArray);
	Response->SetNumberField(TEXT("count"), FunctionsArray.Num());
	Response->SetStringField(TEXT("class_name"), Class->GetName());
	
	UE_LOG(LogBlueprintReflectionService, Log, TEXT("Extracted %d functions from class %s"), 
		FunctionsArray.Num(), *Class->GetName());
	
	return Response;
}

// ═══════════════════════════════════════════════════════════
// Type Validation
// ═══════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FBlueprintReflectionService::IsValidParentClass(const FString& ClassName)
{
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	// Try to resolve the class
	TSharedPtr<FJsonObject> ResolveResult = ResolveClass(ClassName);
	bool bIsValid = ResolveResult->GetBoolField(TEXT("success"));
	
	if (bIsValid)
	{
		// Additional validation: check if it's suitable for Blueprints
		FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
		UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
		
		if (Class)
		{
			bIsValid = IsClassValidForBlueprints(Class);
		}
	}
	
	Response->SetBoolField(TEXT("is_valid"), bIsValid);
	Response->SetStringField(TEXT("class_name"), ClassName);
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::IsValidComponentType(const FString& ComponentType)
{
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	// Try to resolve the component class
	TSharedPtr<FJsonObject> ResolveResult = ResolveClass(ComponentType);
	bool bIsValid = ResolveResult->GetBoolField(TEXT("success"));
	
	if (bIsValid)
	{
		FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
		UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
		
		if (Class)
		{
			bIsValid = IsComponentTypeValid(Class);
		}
		else
		{
			bIsValid = false;
		}
	}
	
	Response->SetBoolField(TEXT("is_valid"), bIsValid);
	Response->SetStringField(TEXT("component_type"), ComponentType);
	
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::IsValidPropertyType(const FString& PropertyType)
{
	// Property types include primitives and UObject types
	// Simple validation: check if it's in our catalog or is a known primitive
	
	if (!bPropertyTypesInitialized)
	{
		PopulatePropertyTypeCatalog();
		bPropertyTypesInitialized = true;
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
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
	
	Response->SetBoolField(TEXT("is_valid"), bIsValid);
	Response->SetStringField(TEXT("property_type"), PropertyType);
	
	return Response;
}

// ═══════════════════════════════════════════════════════════
// Type Conversion
// ═══════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FBlueprintReflectionService::ResolveClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return CreateErrorResponse(TEXT("EMPTY_CLASS_NAME"), TEXT("Class name is empty"));
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
		TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
		Response->SetStringField(TEXT("class_path"), ResolvedClass->GetPathName());
		Response->SetStringField(TEXT("class_name"), ResolvedClass->GetName());
		
		return Response;
	}
	
	return CreateErrorResponse(TEXT("CLASS_NOT_FOUND"), 
		FString::Printf(TEXT("Could not resolve class: %s"), *ClassName));
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::GetClassPath(UClass* Class)
{
	if (!Class)
	{
		return CreateErrorResponse(TEXT("INVALID_CLASS"), TEXT("Class is null"));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("class_path"), Class->GetPathName());
	Response->SetStringField(TEXT("class_name"), Class->GetName());
	
	return Response;
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

void FBlueprintReflectionService::ExtractPropertyInfo(FProperty* Property, TSharedPtr<FJsonObject>& OutPropertyInfo)
{
	if (!Property || !OutPropertyInfo.IsValid())
	{
		return;
	}
	
	OutPropertyInfo->SetStringField(TEXT("name"), Property->GetName());
	OutPropertyInfo->SetStringField(TEXT("type"), Property->GetCPPType());
	OutPropertyInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
	OutPropertyInfo->SetStringField(TEXT("tooltip"), Property->GetToolTipText().ToString());
	
	OutPropertyInfo->SetBoolField(TEXT("is_editable"), Property->HasAnyPropertyFlags(CPF_Edit));
	OutPropertyInfo->SetBoolField(TEXT("is_blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
	OutPropertyInfo->SetBoolField(TEXT("is_blueprint_readonly"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
}

void FBlueprintReflectionService::ExtractFunctionInfo(UFunction* Function, TSharedPtr<FJsonObject>& OutFunctionInfo)
{
	if (!Function || !OutFunctionInfo.IsValid())
	{
		return;
	}
	
	OutFunctionInfo->SetStringField(TEXT("name"), Function->GetName());
	OutFunctionInfo->SetStringField(TEXT("category"), Function->GetMetaData(TEXT("Category")));
	OutFunctionInfo->SetStringField(TEXT("tooltip"), Function->GetToolTipText().ToString());
	
	OutFunctionInfo->SetBoolField(TEXT("is_static"), Function->HasAnyFunctionFlags(FUNC_Static));
	OutFunctionInfo->SetBoolField(TEXT("is_const"), Function->HasAnyFunctionFlags(FUNC_Const));
	OutFunctionInfo->SetBoolField(TEXT("is_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
	OutFunctionInfo->SetBoolField(TEXT("is_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	
	// Extract parameters
	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
	{
		FProperty* Param = *PropIt;
		TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject);
		
		ParamInfo->SetStringField(TEXT("name"), Param->GetName());
		ParamInfo->SetStringField(TEXT("type"), Param->GetCPPType());
		ParamInfo->SetBoolField(TEXT("is_return"), Param->HasAnyPropertyFlags(CPF_ReturnParm));
		ParamInfo->SetBoolField(TEXT("is_out"), Param->HasAnyPropertyFlags(CPF_OutParm));
		
		ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
	}
	
	OutFunctionInfo->SetArrayField(TEXT("parameters"), ParamsArray);
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
	if (!Class->HasAnyClassFlags(CLASS_BlueprintType))
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

TSharedPtr<FJsonObject> FBlueprintReflectionService::CreateSuccessResponse()
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}

TSharedPtr<FJsonObject> FBlueprintReflectionService::CreateErrorResponse(const FString& ErrorCode, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}
