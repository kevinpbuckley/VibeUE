// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphSchema.h"
#include "AssetRegistry/AssetRegistryModule.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVibeUEManageVars, Log, All);

// Forward declarations
class FObjectPropertyNode;
struct FBPVariableDescription;

// Describes different type categories for reflection-based discovery (no UENUM needed)
enum class EReflectedTypeKind : uint8
{
    Class,
    Struct,
    Enum,
    BlueprintGeneratedClass,
    Interface,
    Basic
};

// Container descriptor for array/set/map variable types
struct FContainerDescriptor
{
    FString Kind; // "Array", "Set", "Map"
    FString KeyTypePath; // for maps only
    FString ValueTypePath; // for maps, or element type for arrays/sets
};

// Complete type descriptor from reflection system
struct VIBEUE_API FReflectedTypeDescriptor
{
    FString Name;         // Short name (e.g., "UserWidget", "WBP_HUD_C")
    FString DisplayName;  // FText display name converted to string
    FTopLevelAssetPath Path; // Canonical path (e.g., "/Script/UMG.UserWidget")
    EReflectedTypeKind Kind = EReflectedTypeKind::Class;
    FTopLevelAssetPath Parent; // For hierarchy filtering
    bool bIsBlueprintGenerated = false;
    bool bIsDeprecated = false;
    bool bIsAbstract = false;
    FString Tooltip;
    FString Category; // Logical grouping category
};

// Query parameters for type catalog search
struct FTypeQuery
{
    FString Category;
    FTopLevelAssetPath BaseClassPath;
    FString SearchText;
    bool bIncludeBlueprints = true;
    bool bIncludeEngine = true;
    bool bIncludeAbstract = false;
    bool bIncludeDeprecated = false;
    int32 MaxResults = 100;
    int32 PageOffset = 0;
};

// Variable definition structure for create/modify operations
struct FVariableDefinition
{
    FName VariableName;
    FTopLevelAssetPath TypePath; // Canonical type path
    FContainerDescriptor Container; // Optional container info
    TMap<FString, FString> MetadataMap; // Raw metadata from engine
    bool bExposeOnSpawn = false;
    bool bPrivate = false;
    bool bExposeToCinematics = false;
    FString Category = TEXT("Default");
    FString Tooltip;
    FString DefaultValueString; // Serialized default value
};

// Resolved property for get/set operations (FProperty-based)
struct FResolvedProperty
{
    // Terminal property and container pointers
    FProperty* TerminalProperty = nullptr;
    UObject* TargetObject = nullptr;     // Typically class default object (CDO)
    void* ValueAddress = nullptr;        // Address of the terminal property value within the target object
    FString CanonicalPath;
    bool bIsValid = false;
    FString ErrorMessage;

    FResolvedProperty() = default;
    FResolvedProperty(FProperty* InProp, UObject* InTarget, void* InAddr, const FString& InPath)
        : TerminalProperty(InProp), TargetObject(InTarget), ValueAddress(InAddr), CanonicalPath(InPath), bIsValid(InProp != nullptr && InTarget != nullptr && InAddr != nullptr) {}
};

/**
 * Service for discovering and caching all Blueprint-usable types via reflection
 */
class VIBEUE_API FReflectionCatalogService
{
public:
    FReflectionCatalogService();
    ~FReflectionCatalogService();

    // Lifecycle
    void Initialize();
    void Shutdown();
    bool IsInitialized() const { return bIsInitialized; }

    // Type discovery
    const FReflectedTypeDescriptor* FindByPath(const FTopLevelAssetPath& Path) const;
    const FReflectedTypeDescriptor* FindByName(const FString& Name) const;
    TArray<FReflectedTypeDescriptor> Query(const FTypeQuery& Criteria) const;
    
    // Cache management
    void InvalidateCache(const FString& Reason = TEXT("Manual"));
    bool ShouldRefreshCache() const;
    void ForceRefresh();

    // Diagnostics
    int32 GetCachedTypeCount() const;
    FString GetCacheStats() const;

private:
    // Core discovery functions
    void BuildTypeCache();
    void DiscoverClasses(TArray<FReflectedTypeDescriptor>& OutTypes);
    void DiscoverStructs(TArray<FReflectedTypeDescriptor>& OutTypes);
    void DiscoverEnums(TArray<FReflectedTypeDescriptor>& OutTypes);
    void DiscoverBlueprintClasses(TArray<FReflectedTypeDescriptor>& OutTypes);
    void BuildHierarchyIndex();

    // Validation
    bool IsValidBlueprintType(UClass* Class) const;
    bool IsValidBlueprintStruct(UScriptStruct* Struct) const;
    bool IsValidBlueprintEnum(UEnum* Enum) const;
    
    // Helper functions
    FString GetTypeCategory(UClass* Class) const;
    FString GetTypeCategory(UScriptStruct* Struct) const;
    FString GetTypeCategory(UEnum* Enum) const;
    FTopLevelAssetPath GetTypePath(UObject* Object) const;

    // Event handlers
    void OnAssetLoaded(UObject* Object);
    void OnAssetDeleted(const FAssetData& AssetData);
    void OnHotReload();

    // Cache data
    TArray<FReflectedTypeDescriptor> TypeCache;
    TMap<FTopLevelAssetPath, int32> PathToIndexMap;
    TMap<FString, TArray<int32>> NameToIndicesMap;
    TMap<FTopLevelAssetPath, TArray<int32>> HierarchyIndex; // Parent -> Children
    
    // State
    bool bIsInitialized = false;
    bool bCacheNeedsRebuild = true;
    double LastRefreshTime = 0.0;
    int32 ModuleLoadId = 0;
    
    // Config
    static constexpr double CacheValiditySeconds = 300.0; // 5 minutes
    static constexpr int32 MaxCacheSize = 10000;
    
    // Critical section for thread safety
    mutable FCriticalSection CacheCriticalSection;
};

/**
 * Service for resolving canonical type paths to FEdGraphPinType
 */
class VIBEUE_API FPinTypeResolver
{
public:
    FPinTypeResolver();
    ~FPinTypeResolver();

    // Core resolution
    bool ResolvePinType(const FReflectedTypeDescriptor& TypeDescriptor, const FContainerDescriptor& Container, 
                       FEdGraphPinType& OutPinType, FString& OutError) const;
    bool ResolvePinType(const FTopLevelAssetPath& TypePath, const FContainerDescriptor& Container,
                       FEdGraphPinType& OutPinType, FString& OutError) const;
    
    // Conversion utilities
    FString PinTypeToCanonicalPath(const FEdGraphPinType& PinType) const;
    FString PinTypeToDisplayName(const FEdGraphPinType& PinType) const;
    
    // Container support
    bool MakeArrayPinType(const FEdGraphPinType& ElementType, FEdGraphPinType& OutArrayType) const;
    bool MakeSetPinType(const FEdGraphPinType& ElementType, FEdGraphPinType& OutSetType) const;
    bool MakeMapPinType(const FEdGraphPinType& KeyType, const FEdGraphPinType& ValueType, FEdGraphPinType& OutMapType) const;

private:
    // Internal resolution
    UClass* ResolveClass(const FTopLevelAssetPath& ClassPath) const;
    UScriptStruct* ResolveStruct(const FTopLevelAssetPath& StructPath) const;
    UEnum* ResolveEnum(const FTopLevelAssetPath& EnumPath) const;
    
    // Object caches (weak references)
    mutable TMap<FTopLevelAssetPath, TWeakObjectPtr<UClass>> ClassCache;
    mutable TMap<FTopLevelAssetPath, TWeakObjectPtr<UScriptStruct>> StructCache;
    mutable TMap<FTopLevelAssetPath, TWeakObjectPtr<UEnum>> EnumCache;
    
    mutable FCriticalSection ResolverCriticalSection;
};

/**
 * Service for CRUD operations on Blueprint variable definitions  
 */
class VIBEUE_API FVariableDefinitionService
{
public:
    FVariableDefinitionService();
    ~FVariableDefinitionService();

    // Variable CRUD
    FBPVariableDescription* FindVariable(UBlueprint* Blueprint, const FName& VarName) const;
    bool CreateOrUpdateVariable(UBlueprint* Blueprint, const FVariableDefinition& Definition, FString& OutError);
    bool DeleteVariable(UBlueprint* Blueprint, const FName& VarName, FString& OutError);
    
    // Metadata operations
    bool GetVariableMetadata(UBlueprint* Blueprint, const FName& VarName, TMap<FString, FString>& OutMetadata, FString& OutError) const;
    bool SetVariableMetadata(UBlueprint* Blueprint, const FName& VarName, const TMap<FString, FString>& Metadata, FString& OutError);
    
    // Listing and info
    TArray<FBPVariableDescription*> GetAllVariables(UBlueprint* Blueprint) const;
    bool GetVariableInfo(UBlueprint* Blueprint, const FName& VarName, FVariableDefinition& OutDefinition, FString& OutError) const;
    
    // Conversion utilities
    FVariableDefinition BPVariableToDefinition(const FBPVariableDescription& BPVar) const;
    bool DefinitionToBPVariable(const FVariableDefinition& Definition, FBPVariableDescription& OutBPVar, FString& OutError) const;

private:
    // Internal helpers
    bool ValidateVariableDefinition(const FVariableDefinition& Definition, FString& OutError) const;
    void ApplyDefaultMetadata(FBPVariableDescription& BPVar, const FVariableDefinition& Definition) const;
    bool CompileIfNeeded(UBlueprint* Blueprint, FString& OutError) const;
};

/**
 * Service for property value get/set operations using property handles
 */
class VIBEUE_API FPropertyAccessService  
{
public:
    FPropertyAccessService();
    ~FPropertyAccessService();

    // Property resolution
    FResolvedProperty ResolveProperty(UBlueprint* Blueprint, const FString& CanonicalPath, FString& OutError);
    
    // Value operations
    bool GetPropertyValue(const FResolvedProperty& Property, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
    bool SetPropertyValue(const FResolvedProperty& Property, const TSharedPtr<FJsonValue>& InValue, FString& OutError);
    
    // Formatted string operations (for complex types)
    bool GetPropertyValueFormatted(const FResolvedProperty& Property, FString& OutFormattedValue, FString& OutError);
    bool SetPropertyValueFromFormatted(const FResolvedProperty& Property, const FString& FormattedValue, FString& OutError);
    
    // Path utilities
    bool ParsePropertyPath(const FString& Path, TArray<FString>& OutSegments, FString& OutError);
    FString CombinePropertyPath(const TArray<FString>& Segments);

private:
    // Internal: reflection navigation helpers
    bool NavigatePropertyChain(UStruct* OwnerStruct, void* OwnerPtr, const TArray<FString>& Segments, FProperty*& OutTerminalProp, void*& OutValuePtr, FString& OutError);
    TSharedPtr<FJsonValue> PropertyToJsonValue(FProperty* Prop, void* ValuePtr, FString& OutError);
    bool JsonValueToProperty(FProperty* Prop, void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError);
};

/**
 * Response serialization utilities  
 */
class VIBEUE_API FResponseSerializer
{
public:
    // Type descriptors to JSON
    static TSharedPtr<FJsonObject> SerializeTypeDescriptor(const FReflectedTypeDescriptor& Descriptor);
    static TSharedPtr<FJsonObject> SerializeTypeQuery(const FTypeQuery& Query);
    static TSharedPtr<FJsonObject> SerializeContainerDescriptor(const FContainerDescriptor& Container);
    
    // Variable definitions to JSON
    static TSharedPtr<FJsonObject> SerializeVariableDefinition(const FVariableDefinition& Definition);
    static TSharedPtr<FJsonObject> SerializePinType(const FEdGraphPinType& PinType);
    
    // Error responses
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& Message, 
                                                      const TSharedPtr<FJsonObject>& Details = nullptr);
    
    // Success responses  
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    
private:
    static TSharedPtr<FJsonValue> MetadataToJsonValue(const TMap<FString, FString>& Metadata);
    static bool JsonValueToMetadata(const TSharedPtr<FJsonValue>& JsonValue, TMap<FString, FString>& OutMetadata);
};

/**
 * Main coordinator for blueprint variable management operations
 */
class VIBEUE_API FBlueprintVariableCommandContext
{
public:
    FBlueprintVariableCommandContext();
    ~FBlueprintVariableCommandContext();
    void Initialize();
    void Shutdown();

    // Command routing
    TSharedPtr<FJsonObject> ExecuteCommand(const FString& Action, const TSharedPtr<FJsonObject>& Params);

    // Service accessors
    FReflectionCatalogService& GetCatalogService() { return CatalogService; }
    FPinTypeResolver& GetPinTypeResolver() { return PinTypeResolver; }
    FVariableDefinitionService& GetVariableService() { return VariableService; }
    FPropertyAccessService& GetPropertyService() { return PropertyService; }

private:
    // Action handlers
    TSharedPtr<FJsonObject> HandleSearchTypes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleModify(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDelete(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleList(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetPropertyMetadata(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPropertyMetadata(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDiagnostics(const TSharedPtr<FJsonObject>& Params);

    // Utilities
    UBlueprint* FindBlueprint(const FString& BlueprintName, FString& OutError);
    bool ParseRequestParams(const TSharedPtr<FJsonObject>& Params, FString& OutBlueprintName, FString& OutError);

    // Services
    FReflectionCatalogService CatalogService;
    FPinTypeResolver PinTypeResolver;
    FVariableDefinitionService VariableService;
    FPropertyAccessService PropertyService;
    
    // State
    bool bIsInitialized = false;
    
    // Static instance
    static TUniquePtr<FBlueprintVariableCommandContext> Instance;
    
public:
    static FBlueprintVariableCommandContext& Get();
};