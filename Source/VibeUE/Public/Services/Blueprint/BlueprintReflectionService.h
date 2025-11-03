#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "UObject/Class.h"

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
 * 
 * Note: Currently uses TSharedPtr<FJsonObject> for compatibility with existing codebase.
 * Will be refactored to use TResult<T> when foundation infrastructure is implemented.
 */
class VIBEUE_API FBlueprintReflectionService
{
public:
	FBlueprintReflectionService();
	
	// ═══════════════════════════════════════════════════════════
	// Type Discovery
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Get list of available parent classes for Blueprint creation
	 * @return JSON object with success status and array of class names
	 */
	TSharedPtr<FJsonObject> GetAvailableParentClasses();
	
	/**
	 * Get list of available component types for Blueprint components
	 * @return JSON object with success status and array of component type names
	 */
	TSharedPtr<FJsonObject> GetAvailableComponentTypes();
	
	/**
	 * Get list of available property types for Blueprint variables
	 * @return JSON object with success status and array of property type names
	 */
	TSharedPtr<FJsonObject> GetAvailablePropertyTypes();
	
	// ═══════════════════════════════════════════════════════════
	// Class Metadata
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Get comprehensive class information
	 * @param Class The class to inspect
	 * @return JSON object with class metadata (name, path, parent, etc.)
	 */
	TSharedPtr<FJsonObject> GetClassInfo(UClass* Class);
	
	/**
	 * Get all properties of a class
	 * @param Class The class to inspect
	 * @return JSON object with success status and array of property info
	 */
	TSharedPtr<FJsonObject> GetClassProperties(UClass* Class);
	
	/**
	 * Get all functions of a class
	 * @param Class The class to inspect
	 * @return JSON object with success status and array of function info
	 */
	TSharedPtr<FJsonObject> GetClassFunctions(UClass* Class);
	
	// ═══════════════════════════════════════════════════════════
	// Type Validation
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Validate if a class name is a valid parent for Blueprint creation
	 * @param ClassName The class name to validate
	 * @return JSON object with success status and validation result
	 */
	TSharedPtr<FJsonObject> IsValidParentClass(const FString& ClassName);
	
	/**
	 * Validate if a component type is valid
	 * @param ComponentType The component type to validate
	 * @return JSON object with success status and validation result
	 */
	TSharedPtr<FJsonObject> IsValidComponentType(const FString& ComponentType);
	
	/**
	 * Validate if a property type is valid
	 * @param PropertyType The property type to validate
	 * @return JSON object with success status and validation result
	 */
	TSharedPtr<FJsonObject> IsValidPropertyType(const FString& PropertyType);
	
	// ═══════════════════════════════════════════════════════════
	// Type Conversion
	// ═══════════════════════════════════════════════════════════
	
	/**
	 * Resolve a class name or path to a UClass*
	 * @param ClassName The class name or path to resolve
	 * @return JSON object with success status and class path (or null)
	 */
	TSharedPtr<FJsonObject> ResolveClass(const FString& ClassName);
	
	/**
	 * Get the full path of a UClass
	 * @param Class The class to get path for
	 * @return JSON object with success status and class path
	 */
	TSharedPtr<FJsonObject> GetClassPath(UClass* Class);
	
private:
	// Helper methods for type cataloging
	void PopulateParentClassCatalog();
	void PopulateComponentTypeCatalog();
	void PopulatePropertyTypeCatalog();
	
	// Helper methods for class introspection
	void ExtractPropertyInfo(FProperty* Property, TSharedPtr<FJsonObject>& OutPropertyInfo);
	void ExtractFunctionInfo(UFunction* Function, TSharedPtr<FJsonObject>& OutFunctionInfo);
	
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
	
	// Helper to create standardized responses
	TSharedPtr<FJsonObject> CreateSuccessResponse();
	TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& Message);
};
