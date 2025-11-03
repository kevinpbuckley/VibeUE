#include "Services/UMG/WidgetLifecycleService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"

FWidgetLifecycleService::FWidgetLifecycleService(TSharedPtr<FServiceContext> Context) : FServiceBase(Context) {}

TResult<void> FWidgetLifecycleService::OpenWidgetInEditor(const FString& WidgetName)
{
    return TResult<void>::Error(VibeUE::ErrorCodes::OPERATION_NOT_SUPPORTED, TEXT("Widget editor operations not yet implemented"));
}

TResult<bool> FWidgetLifecycleService::IsWidgetOpen(const FString& WidgetName) { return TResult<bool>::Success(false); }

TResult<void> FWidgetLifecycleService::CloseWidget(const FString& WidgetName)
{
    return TResult<void>::Error(VibeUE::ErrorCodes::OPERATION_NOT_SUPPORTED, TEXT("Widget editor operations not yet implemented"));
}

TResult<TArray<FString>> FWidgetLifecycleService::ValidateWidget(UWidgetBlueprint* Widget)
{
    if (!Widget) return TResult<TArray<FString>>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Widget blueprint cannot be null"));

    TArray<FString> Errors;

    if (!Widget->WidgetTree)
    {
        Errors.Add(TEXT("Widget blueprint has no WidgetTree"));
        return TResult<TArray<FString>>::Success(Errors);
    }

    if (!Widget->WidgetTree->RootWidget)
    {
        Errors.Add(TEXT("Widget tree has no root widget"));
        return TResult<TArray<FString>>::Success(Errors);
    }

    ValidateWidgetRecursive(Widget->WidgetTree->RootWidget, Errors);

    TSet<UWidget*> Visited;
    if (DetectCircularReference(Widget->WidgetTree->RootWidget, Visited))
        Errors.Add(TEXT("Circular reference detected in widget hierarchy"));

    return TResult<TArray<FString>>::Success(Errors);
}

TResult<bool> FWidgetLifecycleService::IsWidgetValid(UWidgetBlueprint* Widget)
{
    if (!Widget) return TResult<bool>::Success(false);
    
    bool bHasWidgetTree = Widget->WidgetTree != nullptr;
    bool bHasRootWidget = bHasWidgetTree && Widget->WidgetTree->RootWidget != nullptr;
    bool bIsValidLowLevel = Widget->IsValidLowLevel();
    bool bIsValidObject = IsValid(Widget);
    bool bIsValid = bHasWidgetTree && bHasRootWidget && bIsValidLowLevel && bIsValidObject;
    
    return TResult<bool>::Success(bIsValid);
}

TResult<void> FWidgetLifecycleService::ValidateHierarchy(UWidgetBlueprint* Widget)
{
    if (!Widget) return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Widget blueprint cannot be null"));

    auto ValidationResult = ValidateWidget(Widget);
    if (!ValidationResult.IsSuccess())
        return TResult<void>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());

    const TArray<FString>& Errors = ValidationResult.GetValue();
    if (Errors.Num() > 0)
        return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Widget hierarchy validation failed: %s"), *FString::Join(Errors, TEXT("; "))));

    return TResult<void>::Success();
}

TResult<FWidgetInfo> FWidgetLifecycleService::GetWidgetInfo(UWidgetBlueprint* Widget)
{
    if (!Widget) return TResult<FWidgetInfo>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Widget blueprint cannot be null"));

    FWidgetInfo Info;
    Info.Name = Widget->GetName();
    Info.Path = Widget->GetPathName();
    Info.PackagePath = Widget->GetPackage() ? Widget->GetPackage()->GetPathName() : TEXT("");
    Info.ParentClass = Widget->ParentClass ? Widget->ParentClass->GetName() : TEXT("UserWidget");
    Info.WidgetType = Widget->GetClass()->GetName();
    return TResult<FWidgetInfo>::Success(Info);
}

TResult<TArray<FString>> FWidgetLifecycleService::GetWidgetCategories(UWidgetBlueprint* Widget)
{
    if (!Widget) return TResult<TArray<FString>>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Widget blueprint cannot be null"));

    TArray<FString> Categories;
    if (Widget->GetPackage())
    {
        TArray<FString> PathParts;
        Widget->GetPackage()->GetPathName().ParseIntoArray(PathParts, TEXT("/"), true);
        for (int32 i = 1; i < PathParts.Num() - 1; ++i)
            if (!PathParts[i].IsEmpty()) Categories.Add(PathParts[i]);
    }
    return TResult<TArray<FString>>::Success(Categories);
}

void FWidgetLifecycleService::ValidateWidgetRecursive(UWidget* Widget, TArray<FString>& Errors)
{
    if (!Widget) { Errors.Add(TEXT("Null widget found in hierarchy")); return; }
    if (!Widget->IsValidLowLevel() || !IsValid(Widget))
        Errors.Add(FString::Printf(TEXT("Invalid widget: %s"), *Widget->GetName()));
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
            ValidateWidgetRecursive(PanelWidget->GetChildAt(i), Errors);
}

bool FWidgetLifecycleService::DetectCircularReference(UWidget* Widget, TSet<UWidget*>& Visited)
{
    if (!Widget) return false;
    if (Visited.Contains(Widget)) return true;
    
    Visited.Add(Widget);
    
    bool bCircularRefFound = false;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            if (DetectCircularReference(PanelWidget->GetChildAt(i), Visited))
            {
                bCircularRefFound = true;
                break;
            }
        }
    }
    
    // Remove from visited set to allow widget in different branches
    Visited.Remove(Widget);
    
    return bCircularRefFound;
}
