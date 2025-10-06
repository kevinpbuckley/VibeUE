#include "Commands/InputKeyEnumerator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_InputKey.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogInputKeyEnumerator, Log, All);

int32 FInputKeyEnumerator::GetAllInputKeys(
	TArray<FInputKeyInfo>& OutKeys,
	bool bIncludeDeprecated
)
{
	OutKeys.Empty();
	
	// Get all keys from the EKeys system
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	
	for (const FKey& Key : AllKeys)
	{
		// Skip deprecated keys if requested
		if (!bIncludeDeprecated && Key.IsDeprecated())
		{
			continue;
		}
		
		// Create key info struct
		FInputKeyInfo KeyInfo;
		KeyInfo.Key = Key; // CRITICAL: Store the FKey itself!
		KeyInfo.KeyName = Key.GetFName().ToString();
		KeyInfo.DisplayName = Key.GetDisplayName().ToString();
		
		// Get menu category (returns FName in UE 5.6)
		FName MenuCategory = Key.GetMenuCategory();
		KeyInfo.MenuCategory = MenuCategory.ToString();
		
		// Determine category from key flags
		if (Key.IsGamepadKey())
		{
			KeyInfo.Category = TEXT("Gamepad");
		}
		else if (Key.IsMouseButton())
		{
			KeyInfo.Category = TEXT("Mouse");
		}
		else
		{
			KeyInfo.Category = TEXT("Keyboard");
		}
		
		// Set key properties
		KeyInfo.bIsGamepadKey = Key.IsGamepadKey();
		KeyInfo.bIsMouseButton = Key.IsMouseButton();
		KeyInfo.bIsKeyboard = !Key.IsGamepadKey() && !Key.IsMouseButton();
		KeyInfo.bIsModifierKey = Key.IsModifierKey();
		KeyInfo.bIsDigital = Key.IsDigital();
		KeyInfo.bIsAnalog = Key.IsAnalog();
		KeyInfo.bIsBindableInBlueprints = Key.IsBindableInBlueprints();
		
		OutKeys.Add(KeyInfo);
	}
	
	UE_LOG(LogInputKeyEnumerator, Log,
		TEXT("GetAllInputKeys: Found %d keys"), OutKeys.Num());
	
	return OutKeys.Num();
}

int32 FInputKeyEnumerator::GetInputKeysByCategory(
	const FString& Category,
	TArray<FInputKeyInfo>& OutKeys
)
{
	TArray<FInputKeyInfo> AllKeys;
	GetAllInputKeys(AllKeys, false);
	
	OutKeys.Empty();
	
	for (const FInputKeyInfo& KeyInfo : AllKeys)
	{
		if (KeyInfo.Category.Equals(Category, ESearchCase::IgnoreCase) ||
			KeyInfo.MenuCategory.Equals(Category, ESearchCase::IgnoreCase))
		{
			OutKeys.Add(KeyInfo);
		}
	}
	
	UE_LOG(LogInputKeyEnumerator, Log,
		TEXT("GetInputKeysByCategory: Found %d keys in category '%s'"),
		OutKeys.Num(), *Category);
	
	return OutKeys.Num();
}

bool FInputKeyEnumerator::FindInputKey(
	const FString& KeyName,
	FInputKeyInfo& OutKeyInfo
)
{
	TArray<FInputKeyInfo> AllKeys;
	GetAllInputKeys(AllKeys, true);
	
	for (const FInputKeyInfo& KeyInfo : AllKeys)
	{
		if (KeyInfo.KeyName.Equals(KeyName, ESearchCase::IgnoreCase))
		{
			OutKeyInfo = KeyInfo;
			return true;
		}
	}
	
	UE_LOG(LogInputKeyEnumerator, Warning,
		TEXT("FindInputKey: Key not found: %s"), *KeyName);
	
	return false;
}

UK2Node_InputKey* FInputKeyEnumerator::CreateInputKeyNode(
	UBlueprint* Blueprint,
	const FKey& InputKey,
	const FVector2D& Position,
	FString& OutError
)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return nullptr;
	}
	
	if (!InputKey.IsValid())
	{
		OutError = TEXT("Invalid input key");
		return nullptr;
	}
	
	// Get the event graph
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph)
	{
		OutError = TEXT("Could not find event graph");
		return nullptr;
	}
	
	// Create the input key node
	UK2Node_InputKey* InputKeyNode = NewObject<UK2Node_InputKey>(EventGraph);
	if (!InputKeyNode)
	{
		OutError = TEXT("Failed to create input key node");
		return nullptr;
	}
	
	// Initialize the node
	InputKeyNode->InputKey = InputKey;
	InputKeyNode->CreateNewGuid();
	InputKeyNode->PostPlacedNewNode();
	InputKeyNode->NodePosX = Position.X;
	InputKeyNode->NodePosY = Position.Y;
	
	// Add the node to the graph
	EventGraph->AddNode(InputKeyNode, false);
	
	// Allocate default pins
	InputKeyNode->AllocateDefaultPins();
	
	// Reconstruct the node
	InputKeyNode->ReconstructNode();
	
	UE_LOG(LogInputKeyEnumerator, Log,
		TEXT("Created input key node for key '%s' at position (%.0f, %.0f)"),
		*InputKey.ToString(), Position.X, Position.Y);
	
	// Mark Blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	return InputKeyNode;
}

int32 FInputKeyEnumerator::RegisterInputKeySpawners()
{
	// This would register spawners in the Blueprint context menu
	// For now, we just log that it was called
	UE_LOG(LogInputKeyEnumerator, Log,
		TEXT("RegisterInputKeySpawners called - spawner registration not implemented"));
	
	return 0;
}
