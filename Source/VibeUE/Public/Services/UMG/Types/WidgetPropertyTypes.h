#pragma once

#include "CoreMinimal.h"

/**
 * UMG Widget Property Type Definitions
 * 
 * This header contains data structures related to UMG widget properties
 * and events. Note: UMG's FPropertyInfo is in a separate namespace from
 * Blueprint's FPropertyInfo to avoid naming conflicts.
 */

/**
 * @struct FEventInfo
 * @brief Structure holding widget event information
 * 
 * Contains metadata about a widget event including its name, signature,
 * and parameter details.
 */
struct VIBEUE_API FEventInfo
{
	/** The name of the event (e.g., "OnClicked", "OnTextChanged") */
	FString EventName;
	
	/** The component class that owns this event */
	FString ComponentClassName;
	
	/** Human-readable signature of the event */
	FString Signature;
	
	/** Event category (e.g., "Interaction", "Visual", "Data") */
	FString Category;
	
	/** Whether this is a custom user-created event */
	bool bIsCustomEvent;

	FEventInfo()
		: bIsCustomEvent(false)
	{}
};


