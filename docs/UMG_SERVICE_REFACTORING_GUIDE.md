# UMG Service Layer Refactoring Guide

This document explains how to refactor UMGCommands and UMGReflectionCommands to use the new service layer.

## Architecture Overview

The Phase 4 service layer follows a clear separation of concerns:

```
Commands Layer (UMGCommands.cpp)
    ↓ delegates to
Service Layer (WidgetDiscoveryService, WidgetComponentService, etc.)
    ↓ uses
Type Layer (WidgetTypes.h, WidgetPropertyTypes.h, etc.)
```

## Refactoring Pattern

### Before (Monolithic)
```cpp
TSharedPtr<FJsonObject> FUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("name"), WidgetName))
    {
        return CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'name' parameter"));
    }
    
    // Validation
    if (WidgetName.IsEmpty())
    {
        return CreateErrorResponse(TEXT("PARAM_INVALID"), TEXT("Widget name is empty"));
    }
    
    // Business logic - creating widget blueprint
    FString PackagePath = TEXT("/Game/UI/");
    UWidgetBlueprint* NewWidgetBP = Cast<UWidgetBlueprint>(
        FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(),
            CreatePackage(*PackagePath),
            FName(*WidgetName),
            BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(),
            UWidgetBlueprintGeneratedClass::StaticClass(),
            NAME_None
        )
    );
    
    if (!NewWidgetBP)
    {
        return CreateErrorResponse(TEXT("WIDGET_CREATE_FAILED"), TEXT("Failed to create widget blueprint"));
    }
    
    // Format response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("name"), NewWidgetBP->GetName());
    Response->SetStringField(TEXT("path"), NewWidgetBP->GetPathName());
    return CreateSuccessResponse(Response);
}
```

### After (Service-Based)
```cpp
TSharedPtr<FJsonObject> FUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("name"), WidgetName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'name' parameter"));
    }
    
    FString PackagePath = TEXT("/Game/UI/");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    
    FString ParentClass = TEXT("UserWidget");
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Delegate to service
    TResult<UWidgetBlueprint*> Result = WidgetLifecycleService->CreateWidgetBlueprint(
        WidgetName,
        PackagePath,
        ParentClass
    );
    
    // Handle service result
    if (Result.IsError())
    {
        return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
    }
    
    // Format response
    UWidgetBlueprint* NewWidgetBP = Result.GetValue();
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("name"), NewWidgetBP->GetName());
    Response->SetStringField(TEXT("path"), NewWidgetBP->GetPathName());
    return CreateSuccessResponse(Response);
}
```

## Key Changes

### 1. Service Initialization
Add service members to command class:

```cpp
class VIBEUE_API FUMGCommands
{
public:
    FUMGCommands();
    
private:
    // Service instances
    TSharedPtr<FServiceContext> ServiceContext;
    TSharedPtr<FWidgetDiscoveryService> WidgetDiscoveryService;
    TSharedPtr<FWidgetComponentService> WidgetComponentService;
    TSharedPtr<FWidgetPropertyService> WidgetPropertyService;
    TSharedPtr<FWidgetReflectionService> WidgetReflectionService;
    TSharedPtr<FWidgetHierarchyService> WidgetHierarchyService;
    TSharedPtr<FWidgetLifecycleService> WidgetLifecycleService;
    TSharedPtr<FWidgetEventService> WidgetEventService;
};
```

Initialize in constructor:

```cpp
FUMGCommands::FUMGCommands()
{
    ServiceContext = MakeShared<FServiceContext>();
    WidgetDiscoveryService = MakeShared<FWidgetDiscoveryService>(ServiceContext);
    WidgetComponentService = MakeShared<FWidgetComponentService>(ServiceContext);
    WidgetPropertyService = MakeShared<FWidgetPropertyService>(ServiceContext);
    WidgetReflectionService = MakeShared<FWidgetReflectionService>(ServiceContext);
    WidgetHierarchyService = MakeShared<FWidgetHierarchyService>(ServiceContext);
    WidgetLifecycleService = MakeShared<FWidgetLifecycleService>(ServiceContext);
    WidgetEventService = MakeShared<FWidgetEventService>(ServiceContext);
}
```

### 2. Command Handler Pattern
Each handler should:
1. Extract and validate parameters
2. Delegate to appropriate service(s)
3. Handle TResult response
4. Format JSON response

### 3. Service Mapping
Map command handlers to services:

| Command Handler | Primary Service |
|----------------|----------------|
| HandleCreateUMGWidgetBlueprint | WidgetLifecycleService |
| HandleDeleteWidgetBlueprint | WidgetLifecycleService |
| HandleSearchItems | WidgetDiscoveryService |
| HandleGetWidgetBlueprintInfo | WidgetDiscoveryService |
| HandleListWidgetComponents | WidgetHierarchyService |
| HandleAddTextBlockToWidget | WidgetComponentService |
| HandleAddButtonToWidget | WidgetComponentService |
| HandleAddChildToPanel | WidgetComponentService |
| HandleRemoveUMGComponent | WidgetComponentService |
| HandleSetWidgetProperty | WidgetPropertyService |
| HandleGetWidgetProperty | WidgetPropertyService |
| HandleListWidgetProperties | WidgetPropertyService |
| HandleGetAvailableWidgetTypes | WidgetReflectionService |
| HandleValidateWidgetHierarchy | WidgetHierarchyService |
| HandleBindInputEvents | WidgetEventService |
| HandleGetAvailableEvents | WidgetEventService |

## Benefits

1. **Separation of Concerns**: Commands handle JSON, services handle business logic
2. **Testability**: Services can be tested independently
3. **Reusability**: Services can be used by multiple commands or other services
4. **Type Safety**: TResult<T> provides compile-time type checking
5. **Error Handling**: Consistent error codes and messages via ErrorCodes.h
6. **Maintainability**: Clear structure makes code easier to understand and modify

## Next Steps

1. Add service includes to UMGCommands.h
2. Add service member variables
3. Initialize services in constructor
4. Refactor each command handler one at a time
5. Test each refactored handler
6. Remove old monolithic business logic
7. Repeat for UMGReflectionCommands
