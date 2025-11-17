// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"

// Forward declarations to avoid including heavy UE headers in public header
class UWidgetBlueprint;

/**
 * Service that extracts comprehensive information from a UWidgetBlueprint.
 * Returns TResult<FWidgetBlueprintInfo> for safe error handling.
 */
class VIBEUE_API FWidgetBlueprintInfoService : public FServiceBase
{
public:
    explicit FWidgetBlueprintInfoService(TSharedPtr<class FServiceContext> Context);

    virtual FString GetServiceName() const override { return TEXT("WidgetBlueprintInfoService"); }

    TResult<FWidgetBlueprintInfo> GetWidgetBlueprintInfo(UWidgetBlueprint* WidgetBlueprint);
};
