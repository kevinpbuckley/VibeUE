#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"
#include "Core/Result.h"

class UWidgetBlueprint;

/**
 * Service responsible for asset-level widget blueprint operations (delete, reference scanning, etc.)
 */
class VIBEUE_API FWidgetAssetService : public FServiceBase
{
public:
    explicit FWidgetAssetService(TSharedPtr<FServiceContext> Context);

    virtual FString GetServiceName() const override { return TEXT("WidgetAssetService"); }

    /**
     * Delete a widget blueprint asset from the content browser, optionally collecting reference info.
     */
    TResult<FWidgetDeleteResult> DeleteWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint, bool bCheckReferences);
};
