// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Blueprint/Types/ComponentTypes.h"
#include "Engine/Blueprint.h"

// Forward declarations for editor-only types
class UK2Node_ComponentBoundEvent;

/**
 * 🔍 REFLECTION-BASED COMPONENT EVENT BINDING
 * 
 * This utility class enables MCP tools to create component delegate event nodes
 * using Unreal's reflection system to discover and bind delegates dynamically.
 * 
 * Key Features:
 * - Automatic component discovery via Blueprint reflection
 * - Dynamic delegate enumeration using UClass property iteration
 * - Signature matching via FMulticastDelegateProperty reflection
 * - No hardcoded component or delegate lists
 * 
 * Architectural Principle: Uses reflection exclusively for future-proofing
 * and plugin compatibility. Works with any component type, any delegate.
 */

/**
 * Utility class for creating and binding component delegate events
 * using Unreal Engine's reflection system.
 */
class VIBEUE_API FComponentEventBinder
{
public:
	/**
	 * Create a component bound event node in Blueprint
	 * 
	 * Uses reflection to:
	 * 1. Find component in Blueprint's SCS hierarchy
	 * 2. Discover delegate property via UClass reflection
	 * 3. Extract delegate signature dynamically
	 * 4. Create properly configured event node
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param ComponentName - Name of component with delegate (from Blueprint variables)
	 * @param DelegateName - Name of delegate property (e.g., "OnHealthChanged")
	 * @param Position - Node position in graph (X, Y coordinates)
	 * @param OutError - Error message if creation fails
	 * @return Created event node or nullptr on failure
	 */
	static UK2Node_ComponentBoundEvent* CreateComponentEvent(
		UBlueprint* Blueprint,
		const FString& ComponentName,
		const FString& DelegateName,
		const FVector2D& Position,
		FString& OutError
	);

	/**
	 * Get all available component events for a Blueprint using reflection
	 * 
	 * Discovers:
	 * - All components in Blueprint (SCS nodes + inherited components)
	 * - All multicast delegate properties per component via reflection
	 * - Complete delegate signatures and parameters
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param ComponentNameFilter - Optional filter for specific component (empty = all)
	 * @param OutEvents - Array of discovered event descriptors
	 * @return True if successful (even if no events found)
	 */
	static bool GetAvailableComponentEvents(
		UBlueprint* Blueprint,
		const FString& ComponentNameFilter,
		TArray<FComponentEventInfo>& OutEvents
	);

	/**
	 * Validate that component exists and has the specified delegate
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param ComponentName - Component to check
	 * @param DelegateName - Delegate to find
	 * @param OutError - Detailed error if validation fails
	 * @return True if valid
	 */
	static bool ValidateComponentDelegate(
		UBlueprint* Blueprint,
		const FString& ComponentName,
		const FString& DelegateName,
		FString& OutError
	);

private:
	/**
	 * Find component in Blueprint's component hierarchy using reflection
	 * 
	 * Searches:
	 * - SimpleConstructionScript (SCS) nodes
	 * - Inherited components from parent classes
	 * - Component variables
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param ComponentName - Name to search for
	 * @return Component template or nullptr
	 */
	static UActorComponent* FindComponentByName(
		UBlueprint* Blueprint,
		const FString& ComponentName
	);

	/**
	 * Find delegate property in component class using reflection
	 * 
	 * Uses TFieldIterator<FProperty> to enumerate all properties
	 * and filters for FMulticastDelegateProperty instances.
	 * 
	 * @param ComponentClass - Component's UClass
	 * @param DelegateName - Delegate property name to find
	 * @return Delegate property or nullptr
	 */
	static FMulticastDelegateProperty* FindDelegateProperty(
		UClass* ComponentClass,
		const FString& DelegateName
	);

	/**
	 * Create event node and bind to component delegate
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param Component - Component instance
	 * @param DelegateProperty - Delegate to bind
	 * @param DelegateName - Delegate name for node labeling
	 * @param Position - Graph position
	 * @return Created node or nullptr
	 */
	static UK2Node_ComponentBoundEvent* CreateBoundEventNode(
		UBlueprint* Blueprint,
		UActorComponent* Component,
		FMulticastDelegateProperty* DelegateProperty,
		const FString& DelegateName,
		const FVector2D& Position
	);

	/**
	 * Extract parameter information from delegate signature using reflection
	 * 
	 * @param SignatureFunction - Delegate's signature function
	 * @param OutParameters - Populated parameter descriptors
	 */
	static void ExtractDelegateParameters(
		UFunction* SignatureFunction,
		TArray<FParameterInfo>& OutParameters
	);

	/**
	 * Build user-friendly signature string from reflection metadata
	 * 
	 * @param SignatureFunction - Delegate signature
	 * @return Formatted signature (e.g., "void(float OldValue, float NewValue)")
	 */
	static FString BuildSignatureString(UFunction* SignatureFunction);

	/**
	 * Get all components from Blueprint including inherited ones
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param OutComponents - Component templates with names
	 */
	static void GetAllBlueprintComponents(
		UBlueprint* Blueprint,
		TArray<TPair<FString, UActorComponent*>>& OutComponents
	);
};
