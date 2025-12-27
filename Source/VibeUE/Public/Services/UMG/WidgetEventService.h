// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidgetBlueprint;

class VIBEUE_API FWidgetEventService : public FServiceBase
{
public:
    explicit FWidgetEventService(TSharedPtr<FServiceContext> Context);
    virtual FString GetServiceName() const override { return TEXT("WidgetEventService"); }

    /**
     * Discover available events and callable functions for a widget blueprint or specific component.
     * If ComponentName is provided, returns delegate events for that specific component (e.g., Button's OnClicked).
     * Otherwise returns widget-level events.
     */
    TResult<TArray<FWidgetEventInfo>> GetAvailableEvents(UWidgetBlueprint* WidgetBlueprint, const FString& ComponentName = TEXT(""), const FString& WidgetType = TEXT(""));

    /**
     * Bind input/event mappings to functions in the widget blueprint.
     * Returns the number of mappings successfully applied (best-effort).
     */
    TResult<int32> BindInputEvents(UWidgetBlueprint* WidgetBlueprint, const TArray<FWidgetInputMapping>& Mappings);
};
