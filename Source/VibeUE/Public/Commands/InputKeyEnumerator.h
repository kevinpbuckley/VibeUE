// Copyright VibeUE 2025

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Engine/Blueprint.h"

// Forward declarations for editor-only types
class UK2Node_InputKey;

/**
 * 🔍 REFLECTION-BASED INPUT KEY DISCOVERY
 * 
 * This utility class uses Unreal Engine's reflection system to dynamically
 * discover ALL available input keys without any hardcoded lists or enums.
 * 
 * Key Features:
 * - Dynamic key discovery via EKeys::GetAllKeys() reflection API
 * - Automatic plugin support (Enhanced Input, VR controllers, custom devices)
 * - Cross-version compatibility (UE 5.3, 5.4, 5.6+)
 * - Metadata extraction from FKey reflection methods
 * - Zero maintenance - adapts to new UE versions automatically
 * 
 * Architectural Principle: NO HARDCODED KEY LISTS
 * - ❌ DO NOT iterate static key ranges (EKeys::A to EKeys::Z)
 * - ❌ DO NOT maintain hardcoded key arrays
 * - ✅ DO use EKeys::GetAllKeys() for dynamic discovery
 * - ✅ DO use FKey reflection methods for metadata
 * 
 * Benefits:
 * - Automatically includes new keys added in future UE versions
 * - Discovers keys added by plugins (Enhanced Input, VR, flight sticks)
 * - Works with custom input devices and international keyboards
 * - No code changes needed when Epic adds new input hardware
 */

/**
 * Structure to hold input key information discovered via reflection
 */
struct FInputKeyInfo
{
	FKey Key;                    // Actual FKey object
	FString KeyName;             // Internal name (e.g., "A", "Gamepad_FaceButton_Bottom")
	FString DisplayName;         // User-facing name (e.g., "A", "Gamepad Face Button Bottom")
	FString MenuCategory;        // Category for UI (e.g., "Keys", "Gamepad")
	FString Category;            // Simplified category ("Keyboard", "Mouse", "Gamepad", "Touch")
	
	// Metadata from FKey reflection
	bool bIsGamepadKey;
	bool bIsMouseButton;
	bool bIsModifierKey;
	bool bIsDigital;
	bool bIsAnalog;
	bool bIsAxis1D;
	bool bIsAxis2D;
	bool bIsAxis3D;
	bool bIsButtonAxis;
	bool bIsBindableInBlueprints;
	
	// Additional categorization
	bool bIsKeyboard;            // Derived: !gamepad && !mouse
	bool bIsTouch;               // Touch input
	bool bIsGesture;             // Gesture input
	
	FInputKeyInfo()
		: bIsGamepadKey(false)
		, bIsMouseButton(false)
		, bIsModifierKey(false)
		, bIsDigital(false)
		, bIsAnalog(false)
		, bIsAxis1D(false)
		, bIsAxis2D(false)
		, bIsAxis3D(false)
		, bIsButtonAxis(false)
		, bIsBindableInBlueprints(false)
		, bIsKeyboard(false)
		, bIsTouch(false)
		, bIsGesture(false)
	{
	}
};

/**
 * Utility to enumerate and register all available input keys using reflection
 */
class VIBEUE_API FInputKeyEnumerator
{
public:
	/**
	 * Get all available input keys with complete metadata
	 * 
	 * Uses EKeys::GetAllKeys() reflection to discover every registered FKey.
	 * This automatically includes:
	 * - All keyboard keys (letters, numbers, F-keys, modifiers, international layouts)
	 * - All mouse buttons (Left, Right, Middle, Thumb, X1, X2, scroll wheel)
	 * - All gamepad buttons (Xbox, PlayStation, Switch, generic controllers)
	 * - All VR controller inputs (Touch, Oculus, Vive, if plugins loaded)
	 * - Any plugin-added keys (Enhanced Input, custom input devices)
	 * - Future keys added in new UE versions
	 * 
	 * @param OutKeys - Populated with complete key metadata
	 * @param bIncludeDeprecated - Whether to include deprecated keys (default: false)
	 * @return Number of keys discovered
	 */
	static int32 GetAllInputKeys(
		TArray<FInputKeyInfo>& OutKeys,
		bool bIncludeDeprecated = false
	);

	/**
	 * Get input keys filtered by category
	 * 
	 * @param Category - "Keyboard", "Mouse", "Gamepad", "Touch", or "All"
	 * @param OutKeys - Filtered key list
	 * @return Number of keys found
	 */
	static int32 GetInputKeysByCategory(
		const FString& Category,
		TArray<FInputKeyInfo>& OutKeys
	);

	/**
	 * Find specific input key by name
	 * 
	 * @param KeyName - Name to search for (e.g., "A", "Escape", "Gamepad_FaceButton_Bottom")
	 * @param OutKeyInfo - Populated if found
	 * @return True if key exists
	 */
	static bool FindInputKey(
		const FString& KeyName,
		FInputKeyInfo& OutKeyInfo
	);

	/**
	 * Create input key event node in Blueprint
	 * 
	 * @param Blueprint - Target Blueprint
	 * @param Key - Input key to bind
	 * @param Position - Node position in graph
	 * @param OutError - Error message if creation fails
	 * @return Created node or nullptr
	 */
	static UK2Node_InputKey* CreateInputKeyNode(
		UBlueprint* Blueprint,
		const FKey& Key,
		const FVector2D& Position,
		FString& OutError
	);

	/**
	 * Register all input key spawners for MCP node creation
	 * 
	 * This is called during plugin startup to populate the spawner registry
	 * with ALL discovered input keys, making them available via spawner_key.
	 * 
	 * Uses reflection to discover keys dynamically, so new UE versions and
	 * plugins automatically work without code changes.
	 * 
	 * @return Number of keys registered as spawners
	 */
	static int32 RegisterInputKeySpawners();

	/**
	 * Validate that a key is bindable in Blueprints
	 * 
	 * Some keys may exist but aren't suitable for Blueprint events
	 * (e.g., system keys, deprecated keys)
	 * 
	 * @param Key - Key to validate
	 * @return True if key can be used in Blueprint event nodes
	 */
	static bool IsKeyBindableInBlueprints(const FKey& Key);

private:
	/**
	 * Get all keys from EKeys using reflection (CORE REFLECTION API)
	 * 
	 * This is the fundamental method that enables dynamic discovery.
	 * Uses Unreal's built-in reflection system to enumerate all registered FKeys.
	 * 
	 * @param OutKeys - Raw FKey array from engine
	 */
	static void EnumerateAllKeys(TArray<FKey>& OutKeys);

	/**
	 * Extract complete metadata from FKey using reflection methods
	 * 
	 * Uses FKey's reflection APIs:
	 * - GetFName(), GetDisplayName(), GetMenuCategory()
	 * - IsGamepadKey(), IsMouseButton(), IsModifierKey()
	 * - IsDigital(), IsAnalog(), IsAxis1D/2D/3D()
	 * 
	 * @param Key - Key to analyze
	 * @param OutInfo - Populated metadata structure
	 */
	static void ExtractKeyMetadata(const FKey& Key, FInputKeyInfo& OutInfo);

	/**
	 * Determine simplified category from key characteristics
	 * 
	 * @param Key - Key to categorize
	 * @return "Keyboard", "Mouse", "Gamepad", "Touch", or "Other"
	 */
	static FString DetermineKeyCategory(const FKey& Key);

	/**
	 * Get menu category string for UI organization
	 * 
	 * Uses FKey::GetMenuCategory() reflection
	 * 
	 * @param Key - Key to categorize
	 * @return Category string (e.g., "Input|Keyboard Events")
	 */
	static FString GetKeyMenuCategory(const FKey& Key);

	/**
	 * Check if key should be filtered out (deprecated, invalid, etc.)
	 * 
	 * @param Key - Key to check
	 * @param bIncludeDeprecated - Whether to include deprecated keys
	 * @return True if key should be included
	 */
	static bool ShouldIncludeKey(const FKey& Key, bool bIncludeDeprecated);
};
