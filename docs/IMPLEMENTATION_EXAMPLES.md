# Implementation Examples

This document provides concrete examples of how the refactored architecture would look and function.

## Example 1: Command Framework Implementation

### Base Command Interface

```cpp
// Source/VibeUE/Public/Framework/ICommand.h
#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Interface for all VibeUE commands
 * Provides common contract for command execution and validation
 */
class VIBEUE_API ICommand
{
public:
    virtual ~ICommand() = default;
    
    /**
     * Execute the command with provided parameters
     * @param Params JSON parameters for the command
     * @return JSON response with success/error status and data
     */
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) = 0;
    
    /**
     * Get the command name for registration
     * @return Unique command identifier
     */
    virtual FString GetCommandName() const = 0;
    
    /**
     * Validate parameters before execution
     * @param Params Parameters to validate
     * @param OutError Error message if validation fails
     * @return True if parameters are valid
     */
    virtual bool ValidateParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const = 0;
    
    /**
     * Get command metadata for documentation/introspection
     * @return Command metadata including description, parameters, examples
     */
    virtual TSharedPtr<FJsonObject> GetMetadata() const = 0;
};
```

### Base Command Implementation

```cpp
// Source/VibeUE/Public/Framework/BaseCommand.h
#pragma once

#include "CoreMinimal.h"
#include "ICommand.h"
#include "Framework/ParameterValidator.h"
#include "Framework/ResponseBuilder.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVibeUECommands, Log, All);

/**
 * Base implementation of ICommand with common functionality
 * All concrete commands should inherit from this class
 */
class VIBEUE_API FBaseCommand : public ICommand
{
public:
    FBaseCommand();
    virtual ~FBaseCommand() = default;

protected:
    // Utility methods for common operations
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr) const;
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Error, const FString& Code = TEXT("GENERIC_ERROR")) const;
    
    void LogCommandStart(const FString& Details = TEXT("")) const;
    void LogCommandEnd(bool bSuccess, const FString& Details = TEXT("")) const;
    
    // Template-based parameter extraction with type safety
    template<typename T>
    bool GetParam(const TSharedPtr<FJsonObject>& Params, const FString& Key, T& OutValue, FString& OutError) const;
    
    template<typename T>
    bool GetOptionalParam(const TSharedPtr<FJsonObject>& Params, const FString& Key, T& OutValue, const T& DefaultValue) const;
    
    // Common validation helpers
    bool ValidateRequiredFields(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& RequiredFields, FString& OutError) const;
    bool ValidateFieldTypes(const TSharedPtr<FJsonObject>& Params, const TMap<FString, FString>& TypeMap, FString& OutError) const;

private:
    FString CommandName;
    FDateTime ExecutionStartTime;
};

// Template implementations
template<typename T>
bool FBaseCommand::GetParam(const TSharedPtr<FJsonObject>& Params, const FString& Key, T& OutValue, FString& OutError) const
{
    return FParameterValidator::ExtractParam<T>(Params, Key, OutValue, OutError);
}

template<typename T>
bool FBaseCommand::GetOptionalParam(const TSharedPtr<FJsonObject>& Params, const FString& Key, T& OutValue, const T& DefaultValue) const
{
    if (!Params->HasField(Key))
    {
        OutValue = DefaultValue;
        return true;
    }
    
    FString Error;
    return GetParam<T>(Params, Key, OutValue, Error);
}
```

## Example 2: Concrete Command Implementation

### Create Blueprint Command

```cpp
// Source/VibeUE/Public/Commands/Blueprint/CreateBlueprintCommand.h
#pragma once

#include "Framework/BaseCommand.h"

namespace BlueprintCommands
{
    /**
     * Command to create a new Blueprint class
     * 
     * Required Parameters:
     * - name: String - Name for the new Blueprint
     * - parent_class: String - Parent class name (default: "Actor")
     * 
     * Optional Parameters:
     * - path: String - Asset path (default: "/Game/Blueprints/")
     * - components: Array - Initial components to add
     */
    class VIBEUE_API FCreateBlueprintCommand : public FBaseCommand
    {
    public:
        FCreateBlueprintCommand();
        
        // ICommand interface
        virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) override;
        virtual FString GetCommandName() const override { return TEXT("create_blueprint"); }
        virtual bool ValidateParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const override;
        virtual TSharedPtr<FJsonObject> GetMetadata() const override;

    private:
        struct FBlueprintCreationParams
        {
            FString Name;
            FString ParentClass;
            FString Path;
            TArray<TSharedPtr<FJsonValue>> Components;
        };
        
        bool ParseParameters(const TSharedPtr<FJsonObject>& Params, FBlueprintCreationParams& OutParams, FString& OutError) const;
        UBlueprint* CreateBlueprint(const FBlueprintCreationParams& Params, FString& OutError) const;
        bool AddInitialComponents(UBlueprint* Blueprint, const TArray<TSharedPtr<FJsonValue>>& Components, FString& OutError) const;
        TSharedPtr<FJsonObject> BuildSuccessResponse(UBlueprint* Blueprint) const;
    };
}
```

```cpp
// Source/VibeUE/Private/Commands/Blueprint/CreateBlueprintCommand.cpp
#include "Commands/Blueprint/CreateBlueprintCommand.h"
#include "Services/BlueprintService.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"

using namespace BlueprintCommands;

FCreateBlueprintCommand::FCreateBlueprintCommand()
{
}

TSharedPtr<FJsonObject> FCreateBlueprintCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
    LogCommandStart();
    
    FString ValidationError;
    if (!ValidateParams(Params, ValidationError))
    {
        LogCommandEnd(false, ValidationError);
        return CreateErrorResponse(ValidationError, TEXT("VALIDATION_ERROR"));
    }
    
    FBlueprintCreationParams CreationParams;
    FString ParsingError;
    if (!ParseParameters(Params, CreationParams, ParsingError))
    {
        LogCommandEnd(false, ParsingError);
        return CreateErrorResponse(ParsingError, TEXT("PARAMETER_ERROR"));
    }
    
    FString CreationError;
    UBlueprint* NewBlueprint = CreateBlueprint(CreationParams, CreationError);
    if (!NewBlueprint)
    {
        LogCommandEnd(false, CreationError);
        return CreateErrorResponse(CreationError, TEXT("CREATION_ERROR"));
    }
    
    // Add initial components if specified
    if (CreationParams.Components.Num() > 0)
    {
        FString ComponentError;
        if (!AddInitialComponents(NewBlueprint, CreationParams.Components, ComponentError))
        {
            UE_LOG(LogVibeUECommands, Warning, TEXT("Blueprint created but component addition failed: %s"), *ComponentError);
            // Continue - blueprint was created successfully, component failure is non-fatal
        }
    }
    
    LogCommandEnd(true, FString::Printf(TEXT("Created blueprint: %s"), *CreationParams.Name));
    return BuildSuccessResponse(NewBlueprint);
}

bool FCreateBlueprintCommand::ValidateParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
    // Define required fields
    TArray<FString> RequiredFields = { TEXT("name") };
    if (!ValidateRequiredFields(Params, RequiredFields, OutError))
    {
        return false;
    }
    
    // Define field types
    TMap<FString, FString> TypeMap;
    TypeMap.Add(TEXT("name"), TEXT("string"));
    TypeMap.Add(TEXT("parent_class"), TEXT("string"));
    TypeMap.Add(TEXT("path"), TEXT("string"));
    TypeMap.Add(TEXT("components"), TEXT("array"));
    
    return ValidateFieldTypes(Params, TypeMap, OutError);
}

bool FCreateBlueprintCommand::ParseParameters(const TSharedPtr<FJsonObject>& Params, FBlueprintCreationParams& OutParams, FString& OutError) const
{
    // Required parameters
    if (!GetParam<FString>(Params, TEXT("name"), OutParams.Name, OutError))
    {
        return false;
    }
    
    // Optional parameters with defaults
    GetOptionalParam<FString>(Params, TEXT("parent_class"), OutParams.ParentClass, TEXT("Actor"));
    GetOptionalParam<FString>(Params, TEXT("path"), OutParams.Path, TEXT("/Game/Blueprints/"));
    
    // Optional components array
    if (Params->HasField(TEXT("components")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ComponentsPtr;
        if (Params->TryGetArrayField(TEXT("components"), ComponentsPtr))
        {
            OutParams.Components = *ComponentsPtr;
        }
    }
    
    return true;
}

UBlueprint* FCreateBlueprintCommand::CreateBlueprint(const FBlueprintCreationParams& Params, FString& OutError) const
{
    return FBlueprintService::CreateBlueprint(Params.Name, Params.ParentClass, Params.Path, OutError);
}

TSharedPtr<FJsonObject> FCreateBlueprintCommand::GetMetadata() const
{
    TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
    
    Metadata->SetStringField(TEXT("name"), GetCommandName());
    Metadata->SetStringField(TEXT("description"), TEXT("Creates a new Blueprint class with optional initial components"));
    Metadata->SetStringField(TEXT("category"), TEXT("blueprint"));
    
    // Required parameters
    TSharedPtr<FJsonObject> RequiredParams = MakeShared<FJsonObject>();
    RequiredParams->SetStringField(TEXT("name"), TEXT("Name for the new Blueprint"));
    Metadata->SetObjectField(TEXT("required_parameters"), RequiredParams);
    
    // Optional parameters
    TSharedPtr<FJsonObject> OptionalParams = MakeShared<FJsonObject>();
    OptionalParams->SetStringField(TEXT("parent_class"), TEXT("Parent class name (default: Actor)"));
    OptionalParams->SetStringField(TEXT("path"), TEXT("Asset path (default: /Game/Blueprints/)"));
    OptionalParams->SetStringField(TEXT("components"), TEXT("Array of initial components to add"));
    Metadata->SetObjectField(TEXT("optional_parameters"), OptionalParams);
    
    // Example usage
    TSharedPtr<FJsonObject> Example = MakeShared<FJsonObject>();
    Example->SetStringField(TEXT("name"), TEXT("MyActor"));
    Example->SetStringField(TEXT("parent_class"), TEXT("Actor"));
    Example->SetStringField(TEXT("path"), TEXT("/Game/MyBlueprints/"));
    Metadata->SetObjectField(TEXT("example"), Example);
    
    return Metadata;
}
```

## Example 3: Command Registry Implementation

```cpp
// Source/VibeUE/Public/Framework/CommandRegistry.h
#pragma once

#include "CoreMinimal.h"
#include "ICommand.h"

/**
 * Central registry for all VibeUE commands
 * Provides command registration, lookup, and metadata services
 */
class VIBEUE_API FCommandRegistry
{
public:
    /**
     * Register a command instance
     * @param Command Command instance to register
     * @return True if registration was successful
     */
    static bool RegisterCommand(TSharedPtr<ICommand> Command);
    
    /**
     * Unregister a command
     * @param CommandName Name of command to unregister
     * @return True if command was found and removed
     */
    static bool UnregisterCommand(const FString& CommandName);
    
    /**
     * Get command instance by name
     * @param CommandName Name of the command
     * @return Command instance or nullptr if not found
     */
    static TSharedPtr<ICommand> GetCommand(const FString& CommandName);
    
    /**
     * Execute a command by name
     * @param CommandName Name of command to execute
     * @param Params Parameters for the command
     * @return Command execution result
     */
    static TSharedPtr<FJsonObject> ExecuteCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Get list of all registered command names
     * @return Array of command names
     */
    static TArray<FString> GetAllCommandNames();
    
    /**
     * Get metadata for all commands or specific category
     * @param Category Optional category filter
     * @return Command metadata
     */
    static TSharedPtr<FJsonObject> GetCommandsMetadata(const FString& Category = TEXT(""));
    
    /**
     * Initialize the command registry and register all built-in commands
     */
    static void Initialize();
    
    /**
     * Clean up the command registry
     */
    static void Shutdown();

private:
    static TMap<FString, TSharedPtr<ICommand>> Commands;
    static FCriticalSection CommandsLock;
    static bool bInitialized;
    
    // Register all built-in command modules
    static void RegisterBlueprintCommands();
    static void RegisterUMGCommands();
    static void RegisterAssetCommands();
    static void RegisterNodeCommands();
    static void RegisterSystemCommands();
};
```

## Example 4: Simplified Bridge Implementation

```cpp
// Source/VibeUE/Private/Bridge.cpp - Simplified version
FString UBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Executing command: %s"), *CommandType);
    
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson;
        
        try
        {
            // Single line command dispatch using registry!
            TSharedPtr<FJsonObject> ResultJson = FCommandRegistry::ExecuteCommand(CommandType, Params);
            
            if (ResultJson.IsValid())
            {
                ResponseJson = MakeShared<FJsonObject>();
                bool bSuccess = ResultJson->GetBoolField(TEXT("success"));
                
                if (bSuccess)
                {
                    ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                    ResponseJson->SetObjectField(TEXT("result"), ResultJson);
                }
                else
                {
                    ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                    ResponseJson->SetStringField(TEXT("error"), ResultJson->GetStringField(TEXT("error")));
                }
            }
            else
            {
                ResponseJson = FResponseBuilder::Error(
                    FString::Printf(TEXT("Unknown command: %s"), *CommandType),
                    TEXT("UNKNOWN_COMMAND")
                );
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson = FResponseBuilder::Error(
                FString::Printf(TEXT("Exception during command execution: %s"), UTF8_TO_TCHAR(e.what())),
                TEXT("EXECUTION_EXCEPTION")
            );
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    return Future.Get();
}
```

## Example 5: Parameter Validator Implementation

```cpp
// Source/VibeUE/Public/Framework/ParameterValidator.h
#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Utility class for validating and extracting command parameters
 * Provides type-safe parameter extraction with detailed error messages
 */
class VIBEUE_API FParameterValidator
{
public:
    // Type-safe parameter extraction
    template<typename T>
    static bool ExtractParam(const TSharedPtr<FJsonObject>& Params, const FString& Key, T& OutValue, FString& OutError);
    
    // Validation helpers
    static bool ValidateRequired(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& RequiredFields, FString& OutError);
    static bool ValidateTypes(const TSharedPtr<FJsonObject>& Params, const TMap<FString, FString>& TypeMap, FString& OutError);
    
    // Specific type extractors with validation
    static bool ExtractString(const TSharedPtr<FJsonObject>& Params, const FString& Key, FString& OutValue, FString& OutError);
    static bool ExtractInt32(const TSharedPtr<FJsonObject>& Params, const FString& Key, int32& OutValue, FString& OutError);
    static bool ExtractFloat(const TSharedPtr<FJsonObject>& Params, const FString& Key, float& OutValue, FString& OutError);
    static bool ExtractBool(const TSharedPtr<FJsonObject>& Params, const FString& Key, bool& OutValue, FString& OutError);
    static bool ExtractVector2D(const TSharedPtr<FJsonObject>& Params, const FString& Key, FVector2D& OutValue, FString& OutError);
    static bool ExtractVector(const TSharedPtr<FJsonObject>& Params, const FString& Key, FVector& OutValue, FString& OutError);
    static bool ExtractRotator(const TSharedPtr<FJsonObject>& Params, const FString& Key, FRotator& OutValue, FString& OutError);
    static bool ExtractArray(const TSharedPtr<FJsonObject>& Params, const FString& Key, TArray<TSharedPtr<FJsonValue>>& OutValue, FString& OutError);
    
private:
    static FString GetTypeName(const TSharedPtr<FJsonValue>& Value);
    static bool IsValidType(const TSharedPtr<FJsonValue>& Value, const FString& ExpectedType);
};

// Template specializations
template<>
bool FParameterValidator::ExtractParam<FString>(const TSharedPtr<FJsonObject>& Params, const FString& Key, FString& OutValue, FString& OutError)
{
    return ExtractString(Params, Key, OutValue, OutError);
}

template<>
bool FParameterValidator::ExtractParam<int32>(const TSharedPtr<FJsonObject>& Params, const FString& Key, int32& OutValue, FString& OutError)
{
    return ExtractInt32(Params, Key, OutValue, OutError);
}

template<>
bool FParameterValidator::ExtractParam<float>(const TSharedPtr<FJsonObject>& Params, const FString& Key, float& OutValue, FString& OutError)
{
    return ExtractFloat(Params, Key, OutValue, OutError);
}
```

This refactored approach demonstrates:

1. **Clean Separation**: Each command is a focused, single-responsibility class
2. **Type Safety**: Template-based parameter extraction with validation
3. **Consistent Patterns**: All commands follow the same structure and error handling
4. **Extensibility**: New commands are easy to add without modifying existing code
5. **Maintainability**: Command logic is isolated and testable
6. **Performance**: O(1) command lookup instead of long if/else chains