#pragma once

#include "CoreMinimal.h"

/**
 * Blueprint Component Type Definitions
 * 
 * This header contains data structures related to Blueprint components,
 * component events, and component hierarchies.
 */

/**
 * Component information structure
 */
struct VIBEUE_API FComponentInfo
{
    FString ComponentName;
    FString ComponentType;
    FString ParentName;
    FTransform RelativeTransform;
    TArray<FString> ChildNames;
    bool bIsSceneComponent;
    
    FComponentInfo()
        : bIsSceneComponent(false)
    {
    }
};

/**
 * Result structure for component event discovery
 */
struct VIBEUE_API FComponentEventsResult
{
    TMap<FString, TArray<struct FComponentEventInfo>> EventsByComponent;  // Component name -> events
    int32 TotalEventCount;
    
    FComponentEventsResult()
        : TotalEventCount(0)
    {
    }
};

/**
 * Result structure for component event creation
 */
struct VIBEUE_API FComponentEventResult
{
    FString NodeId;          // GUID of created event node
    FString ComponentName;   // Component the event is bound to
    FString DelegateName;    // Delegate name that was bound
    int32 PinCount;          // Number of pins on the event node
    FVector2D Position;      // Final node position
};

/**
 * Structure to hold delegate parameter information
 */
struct VIBEUE_API FParameterInfo
{
	FString Name;
	FString Type;
	FString CPPType;
	FString Direction; // "input" or "output"
	bool bIsOutParam;
	bool bIsReturnParam;
};

/**
 * Structure to hold component event information discovered via reflection
 */
struct VIBEUE_API FComponentEventInfo
{
	FString ComponentName;
	FString ComponentClassName;
	FString DelegateName;
	FString DisplayName;
	FString Signature;
	TArray<FParameterInfo> Parameters;
	
	// Metadata from reflection
	class UActorComponent* ComponentTemplate;
	class FMulticastDelegateProperty* DelegateProperty;
};
