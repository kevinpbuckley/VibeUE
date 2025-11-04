#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "UObject/Class.h"
#include "Dom/JsonObject.h"

// Forward declarations
struct FPropertyInfo;
struct FFunctionInfo;
class UBlueprint;

/**
 * Structure for class information
 */
struct VIBEUE_API FClassInfo
{
    FString ClassName;
    FString ClassPath;
    FString ParentClass;
    bool bIsAbstract;
    bool bIsBlueprint;
    
    FClassInfo()
        : bIsAbstract(false)
        , bIsBlueprint(false)
    {
    }
};

/**
 * Pin information structure for node types
 */
struct VIBEUE_API FPinInfo
{
	FString PinName;
	FString PinType;
	FString Direction;  // "input" or "output"
	bool bIsArray;
	FString DefaultValue;
	
	FPinInfo()
		: bIsArray(false)
	{
	}
};

/**
 * Search criteria for node type discovery
 */
struct VIBEUE_API FNodeTypeSearchCriteria
{
	TOptional<FString> Category;
	TOptional<FString> SearchTerm;
	bool bIncludeFunctions = true;
	bool bIncludeVariables = true;
	bool bIncludeEvents = true;
	int32 MaxResults = 100;
};

/**
 * Node type information returned by discovery
 */
struct VIBEUE_API FNodeTypeInfo
{
	FString SpawnerKey;
	FString NodeTitle;
	FString Category;
	TArray<FPinInfo> ExpectedPins;
	FString NodeType;  // "function_call", "variable_get", etc.
	FString Description;
	TArray<FString> Keywords;
};

/**
 * Blueprint Reflection Service
 * 
 * Consolidates Blueprint type discovery and metadata extraction from BlueprintReflection.cpp
 * into a focused service (~250 lines).
 * 
 * Provides:
 * - Type discovery (parent classes, component types, property types)
 * - Class metadata (properties, functions, class info)
 * - Type validation
 * - Type conversion and resolution
 */
class VIBEUE_API FBlueprintReflectionService : public FServiceBase
{
public:
	explicit FBlueprintReflectionService(TSharedPtr<FServiceContext> Context);
	
	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("BlueprintReflectionService"); }
	
	// ═══════════════════════════════════════════════════════════
	// Type Discovery
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Get list of available parent classes for Blueprint creation
	 * @return Result containing array of class names
	 */
	TResult<TArray<FString>> GetAvailableParentClasses();
	
	/**
	 * Get list of available component types for Blueprint components
	 * @return Result containing array of component type names
	 */
	TResult<TArray<FString>> GetAvailableComponentTypes();
	
	/**
	 * Get list of available property types for Blueprint variables
	 * @return Result containing array of property type names
	 */
	TResult<TArray<FString>> GetAvailablePropertyTypes();
	
	// ═══════════════════════════════════════════════════════════
	// Class Metadata
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Get comprehensive class information
	 * @param Class The class to inspect
	 * @return Result containing class metadata
	 */
	TResult<FClassInfo> GetClassInfo(UClass* Class);
	
	/**
	 * Get all properties of a class
	 * @param Class The class to inspect
	 * @return Result containing array of property info
	 */
	TResult<TArray<FPropertyInfo>> GetClassProperties(UClass* Class);
	
	/**
	 * Get all functions of a class
	 * @param Class The class to inspect
	 * @return Result containing array of function info
	 */
	TResult<TArray<FFunctionInfo>> GetClassFunctions(UClass* Class);
	
	// ═══════════════════════════════════════════════════════════
	// Type Validation
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Validate if a class name is a valid parent for Blueprint creation
	 * @param ClassName The class name to validate
	 * @return Result containing true if valid
	 */
	TResult<bool> IsValidParentClass(const FString& ClassName);
	
	/**
	 * Validate if a component type is valid
	 * @param ComponentType The component type to validate
	 * @return Result containing true if valid
	 */
	TResult<bool> IsValidComponentType(const FString& ComponentType);
	
	/**
	 * Validate if a property type is valid
	 * @param PropertyType The property type to validate
	 * @return Result containing true if valid
	 */
	TResult<bool> IsValidPropertyType(const FString& PropertyType);
	
	// ═══════════════════════════════════════════════════════════
	// Type Conversion
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Resolve a class name or path to a UClass*
	 * @param ClassName The class name or path to resolve
	 * @return Result containing the UClass pointer
	 */
	TResult<UClass*> ResolveClass(const FString& ClassName);
	
	/**
	 * Get the full path of a UClass
	 * @param Class The class to get path for
	 * @return Result containing the class path
	 */
	TResult<FString> GetClassPath(UClass* Class);
	
	// ═══════════════════════════════════════════════════════════
	// Node Type Discovery (Phase 4 Refactoring)
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Get available node types for a Blueprint with filtering
	 * @param Blueprint The Blueprint to discover nodes for
	 * @param Criteria Search and filter criteria
	 * @return Result containing array of available node types
	 */
	TResult<TArray<FNodeTypeInfo>> GetAvailableNodeTypes(
		UBlueprint* Blueprint, 
		const FNodeTypeSearchCriteria& Criteria);

	
private:
	// Helper methods for type cataloging
	void PopulateParentClassCatalog();
	void PopulateComponentTypeCatalog();
	void PopulatePropertyTypeCatalog();
	
	// Helper methods for class introspection
	void ExtractPropertyInfo(FProperty* Property, FPropertyInfo& OutPropertyInfo);
	void ExtractFunctionInfo(UFunction* Function, FFunctionInfo& OutFunctionInfo);
	
	// Validation helpers
	bool IsClassValidForBlueprints(UClass* Class);
	bool IsComponentTypeValid(UClass* ComponentClass);
	
	// Type catalog caches (populated on first use)
	TArray<FString> CachedParentClasses;
	TArray<FString> CachedComponentTypes;
	TArray<FString> CachedPropertyTypes;
	
	// Initialization flags
	bool bParentClassesInitialized;
	bool bComponentTypesInitialized;
	bool bPropertyTypesInitialized;
};
