#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "UObject/Class.h"

// Forward declarations
struct FPropertyInfo;
struct FFunctionInfo;
class UBlueprintNodeSpawner;

/**
 * Pin information for node descriptors
 */
struct VIBEUE_API FPinInfo
{
	FString Name;
	FString Type;
	FString TypePath;
	FString Direction;  // "input" or "output"
	FString Category;
	bool bIsArray;
	bool bIsReference;
	bool bIsHidden;
	bool bIsAdvanced;
	FString DefaultValue;
	FString Tooltip;
	
	FPinInfo()
		: bIsArray(false)
		, bIsReference(false)
		, bIsHidden(false)
		, bIsAdvanced(false)
	{
	}
};

/**
 * Function metadata for node descriptors
 */
struct VIBEUE_API FFunctionMetadata
{
	FString FunctionName;
	FString FunctionClassName;
	FString FunctionClassPath;
	bool bIsStatic;
	bool bIsConst;
	bool bIsPure;
	FString Module;
	
	FFunctionMetadata()
		: bIsStatic(false)
		, bIsConst(false)
		, bIsPure(false)
	{
	}
};

/**
 * Complete node descriptor with metadata for AI decision-making
 */
struct VIBEUE_API FNodeDescriptor
{
	FString SpawnerKey;
	FString NodeTitle;
	FString Category;
	int32 ExpectedPinCount;
	TArray<FPinInfo> Pins;
	bool bIsStatic;
	TOptional<FFunctionMetadata> FunctionMetadata;
	FString NodeClassName;
	FString NodeClassPath;
	FString DisplayName;
	FString Description;
	FString Tooltip;
	TArray<FString> Keywords;
	
	FNodeDescriptor()
		: ExpectedPinCount(0)
		, bIsStatic(false)
	{
	}
};

/**
 * Search criteria for node descriptor discovery
 */
struct VIBEUE_API FNodeDescriptorSearchCriteria
{
	FString SearchTerm;
	FString CategoryFilter;
	FString ClassFilter;
	int32 MaxResults;
	
	FNodeDescriptorSearchCriteria()
		: MaxResults(100)
	{
	}
};

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
	// Node Discovery
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Discover available Blueprint nodes with complete descriptors
	 * Returns complete node descriptors including expected pin counts, spawner keys,
	 * and full metadata for AI decision-making
	 * 
	 * @param Blueprint The Blueprint to discover nodes for
	 * @param Criteria Search criteria including filters and limits
	 * @return Result containing array of node descriptors
	 */
	TResult<TArray<FNodeDescriptor>> DiscoverNodesWithDescriptors(UBlueprint* Blueprint, const FNodeDescriptorSearchCriteria& Criteria);
	
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
