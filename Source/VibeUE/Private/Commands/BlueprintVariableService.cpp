#include "Commands/BlueprintVariableService.h"
#include "Commands/BlueprintCommands.h"
#include "Commands/CommonUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/TopLevelAssetPath.h"
#include "Editor.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY(LogVibeUEManageVars);

// =============================
// FReflectionCatalogService
// =============================
FReflectionCatalogService::FReflectionCatalogService()
{
}

FReflectionCatalogService::~FReflectionCatalogService()
{
    Shutdown();
}

void FReflectionCatalogService::Initialize()
{
    FScopeLock Lock(&CacheCriticalSection);
    if (bIsInitialized && !bCacheNeedsRebuild)
    {
        return;
    }
    BuildTypeCache();
    BuildHierarchyIndex();
    bIsInitialized = true;
    bCacheNeedsRebuild = false;
    LastRefreshTime = FPlatformTime::Seconds();
}

void FReflectionCatalogService::Shutdown()
{
    FScopeLock Lock(&CacheCriticalSection);
    TypeCache.Reset();
    PathToIndexMap.Reset();
    NameToIndicesMap.Reset();
    HierarchyIndex.Reset();
    bIsInitialized = false;
}

const FReflectedTypeDescriptor* FReflectionCatalogService::FindByPath(const FTopLevelAssetPath& Path) const
{
    FScopeLock Lock(&CacheCriticalSection);
    if (const int32* Index = PathToIndexMap.Find(Path))
    {
        return TypeCache.IsValidIndex(*Index) ? &TypeCache[*Index] : nullptr;
    }
    return nullptr;
}

const FReflectedTypeDescriptor* FReflectionCatalogService::FindByName(const FString& Name) const
{
    FScopeLock Lock(&CacheCriticalSection);
    if (const TArray<int32>* Indices = NameToIndicesMap.Find(Name))
    {
        for (int32 Idx : *Indices)
        {
            if (TypeCache.IsValidIndex(Idx))
            {
                return &TypeCache[Idx];
            }
        }
    }
    return nullptr;
}

TArray<FReflectedTypeDescriptor> FReflectionCatalogService::Query(const FTypeQuery& Criteria) const
{
    FScopeLock Lock(&CacheCriticalSection);
    TArray<FReflectedTypeDescriptor> Results;
    Results.Reserve(256);

    auto Matches = [&Criteria](const FReflectedTypeDescriptor& Desc) -> bool
    {
        if (!Criteria.Category.IsEmpty() && !Desc.Category.Equals(Criteria.Category, ESearchCase::IgnoreCase))
        {
            return false;
        }
        if (!Criteria.SearchText.IsEmpty())
        {
            if (!Desc.Name.Contains(Criteria.SearchText, ESearchCase::IgnoreCase) &&
                !Desc.DisplayName.Contains(Criteria.SearchText, ESearchCase::IgnoreCase))
            {
                return false;
            }
        }
        if (!Criteria.BaseClassPath.IsNull())
        {
            // Simple parent filter: compare Parent path to requested base
            if (!(Desc.Parent == Criteria.BaseClassPath))
            {
                return false;
            }
        }
        if (!Criteria.bIncludeAbstract && Desc.bIsAbstract)
        {
            return false;
        }
        if (!Criteria.bIncludeDeprecated && Desc.bIsDeprecated)
        {
            return false;
        }
        return true;
    };

    int32 Count = 0;
    const int32 Start = FMath::Max(0, Criteria.PageOffset);
    for (int32 i = 0; i < TypeCache.Num(); ++i)
    {
        const FReflectedTypeDescriptor& Desc = TypeCache[i];
        if (!Matches(Desc))
        {
            continue;
        }
        if (Count++ < Start)
        {
            continue;
        }
        Results.Add(Desc);
        if (Criteria.MaxResults > 0 && Results.Num() >= Criteria.MaxResults)
        {
            break;
        }
    }
    return Results;
}

void FReflectionCatalogService::InvalidateCache(const FString& /*Reason*/)
{
    FScopeLock Lock(&CacheCriticalSection);
    bCacheNeedsRebuild = true;
}

bool FReflectionCatalogService::ShouldRefreshCache() const
{
    return bCacheNeedsRebuild || (!bIsInitialized) || ((FPlatformTime::Seconds() - LastRefreshTime) > CacheValiditySeconds);
}

void FReflectionCatalogService::ForceRefresh()
{
    FScopeLock Lock(&CacheCriticalSection);
    BuildTypeCache();
    BuildHierarchyIndex();
    bCacheNeedsRebuild = false;
    bIsInitialized = true;
    LastRefreshTime = FPlatformTime::Seconds();
}

int32 FReflectionCatalogService::GetCachedTypeCount() const
{
    FScopeLock Lock(&CacheCriticalSection);
    return TypeCache.Num();
}

FString FReflectionCatalogService::GetCacheStats() const
{
    FScopeLock Lock(&CacheCriticalSection);
    return FString::Printf(TEXT("TypeCache=%d, NameIndex=%d, HierarchyEntries=%d"), TypeCache.Num(), NameToIndicesMap.Num(), HierarchyIndex.Num());
}

void FReflectionCatalogService::BuildTypeCache()
{
    TypeCache.Reset();
    PathToIndexMap.Reset();
    NameToIndicesMap.Reset();

    TArray<FReflectedTypeDescriptor> Temp;
    Temp.Reserve(2048);
    DiscoverClasses(Temp);
    DiscoverStructs(Temp);
    DiscoverEnums(Temp);
    DiscoverBlueprintClasses(Temp);

    // Trim and clamp size
    if (Temp.Num() > MaxCacheSize)
    {
        Temp.SetNum(MaxCacheSize);
    }

    // Move into cache and build indices
    for (FReflectedTypeDescriptor& D : Temp)
    {
        const int32 NewIdx = TypeCache.Add(MoveTemp(D));
        PathToIndexMap.Add(TypeCache[NewIdx].Path, NewIdx);
        NameToIndicesMap.FindOrAdd(TypeCache[NewIdx].Name).Add(NewIdx);
    }
}

static bool IsTransientTypeName(const FString& InName)
{
    return InName.StartsWith(TEXT("SKEL_")) ||
           InName.StartsWith(TEXT("REINST_")) ||
           InName.StartsWith(TEXT("HOTRELOAD_")) ||
           InName.StartsWith(TEXT("TRASHCLASS_")) ||
           InName.StartsWith(TEXT("TRASHSTRUCT_")) ||
           InName.StartsWith(TEXT("PLACEHOLDER-"));
}

FTopLevelAssetPath FReflectionCatalogService::GetTypePath(UObject* Object) const
{
    if (!Object)
    {
        return FTopLevelAssetPath();
    }
    const UPackage* Pkg = Object->GetOutermost();
    const FString PackageName = Pkg ? Pkg->GetName() : FString();
    const FString ObjName = Object->GetName();
    if (PackageName.StartsWith(TEXT("/Script/")))
    {
        return FTopLevelAssetPath(*PackageName, *ObjName);
    }
    // Blueprint generated types live under /Game or plugin content
    return FTopLevelAssetPath(*PackageName, *ObjName);
}

FString FReflectionCatalogService::GetTypeCategory(UClass* Class) const
{
    if (!Class) return TEXT("Object Types");
    if (Class->HasAnyClassFlags(CLASS_Interface)) return TEXT("Interface");
    return TEXT("Object Types");
}

FString FReflectionCatalogService::GetTypeCategory(UScriptStruct* /*Struct*/) const
{
    return TEXT("Structure");
}

FString FReflectionCatalogService::GetTypeCategory(UEnum* /*Enum*/) const
{
    return TEXT("Enum");
}

bool FReflectionCatalogService::IsValidBlueprintType(UClass* Class) const
{
    if (!Class) return false;
    const FString N = Class->GetName();
    if (IsTransientTypeName(N)) return false;
    if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract)) return false;
    return true;
}

bool FReflectionCatalogService::IsValidBlueprintStruct(UScriptStruct* Struct) const
{
    if (!Struct) return false;
    const FString N = Struct->GetName();
    if (IsTransientTypeName(N)) return false;
    // UE 5.6: Prefer simple metadata presence check for Blueprint visibility
    const bool bBPVisible = Struct->HasMetaData(TEXT("BlueprintType"));
    return bBPVisible;
}

bool FReflectionCatalogService::IsValidBlueprintEnum(UEnum* Enum) const
{
    if (!Enum) return false;
    const FString N = Enum->GetName();
    if (IsTransientTypeName(N)) return false;
    // UE 5.6: Prefer simple metadata presence check for Blueprint visibility
    const bool bBPVisible = Enum->HasMetaData(TEXT("BlueprintType"));
    return bBPVisible;
}

void FReflectionCatalogService::DiscoverClasses(TArray<FReflectedTypeDescriptor>& OutTypes)
{
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Cls = *It;
        if (!IsValidBlueprintType(Cls))
        {
            continue;
        }
        FReflectedTypeDescriptor D;
        D.Name = Cls->GetName();
        D.DisplayName = Cls->GetDisplayNameText().ToString();
        D.Path = GetTypePath(Cls);
        D.Parent = GetTypePath(Cls->GetSuperClass());
        D.Kind = Cls->HasAnyClassFlags(CLASS_Interface) ? EReflectedTypeKind::Interface : EReflectedTypeKind::Class;
        D.bIsAbstract = Cls->HasAnyClassFlags(CLASS_Abstract);
        D.bIsDeprecated = Cls->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
        D.bIsBlueprintGenerated = Cls->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
        D.Tooltip = Cls->GetToolTipText().ToString();
        D.Category = GetTypeCategory(Cls);
        OutTypes.Add(MoveTemp(D));
    }
}

void FReflectionCatalogService::DiscoverStructs(TArray<FReflectedTypeDescriptor>& OutTypes)
{
    for (TObjectIterator<UScriptStruct> It; It; ++It)
    {
        UScriptStruct* S = *It;
        if (!IsValidBlueprintStruct(S))
        {
            continue;
        }
        FReflectedTypeDescriptor D;
        D.Name = S->GetName();
        D.DisplayName = S->GetDisplayNameText().ToString();
        D.Path = GetTypePath(S);
        D.Parent = FTopLevelAssetPath();
        D.Kind = EReflectedTypeKind::Struct;
        D.Tooltip = S->GetToolTipText().ToString();
        D.Category = GetTypeCategory(S);
        OutTypes.Add(MoveTemp(D));
    }
}

void FReflectionCatalogService::DiscoverEnums(TArray<FReflectedTypeDescriptor>& OutTypes)
{
    for (TObjectIterator<UEnum> It; It; ++It)
    {
        UEnum* E = *It;
        if (!IsValidBlueprintEnum(E))
        {
            continue;
        }
        FReflectedTypeDescriptor D;
        D.Name = E->GetName();
        D.DisplayName = E->GetDisplayNameText().ToString();
        D.Path = GetTypePath(E);
        D.Parent = FTopLevelAssetPath();
        D.Kind = EReflectedTypeKind::Enum;
        D.Tooltip = E->GetToolTipText().ToString();
        D.Category = GetTypeCategory(E);
        OutTypes.Add(MoveTemp(D));
    }
}

void FReflectionCatalogService::DiscoverBlueprintClasses(TArray<FReflectedTypeDescriptor>& OutTypes)
{
    // Minimal: Most blueprint generated classes already discovered through UClass iterator
    // Additional asset registry scanning could be added if needed
}

void FReflectionCatalogService::BuildHierarchyIndex()
{
    HierarchyIndex.Reset();
    for (const FReflectedTypeDescriptor& D : TypeCache)
    {
        if (!D.Parent.IsNull())
        {
            HierarchyIndex.FindOrAdd(D.Parent).Add(PathToIndexMap[D.Path]);
        }
    }
}

void FReflectionCatalogService::OnAssetLoaded(UObject* /*Object*/)
{
    InvalidateCache(TEXT("AssetLoaded"));
}

void FReflectionCatalogService::OnAssetDeleted(const FAssetData& /*AssetData*/)
{
    InvalidateCache(TEXT("AssetDeleted"));
}

void FReflectionCatalogService::OnHotReload()
{
    InvalidateCache(TEXT("HotReload"));
}

// =============================
// FPinTypeResolver
// =============================
FPinTypeResolver::FPinTypeResolver() {}
FPinTypeResolver::~FPinTypeResolver() {}

UClass* FPinTypeResolver::ResolveClass(const FTopLevelAssetPath& ClassPath) const
{
    if (TWeakObjectPtr<UClass>* Found = ClassCache.Find(ClassPath))
    {
        if (Found->IsValid()) return Found->Get();
    }
    UClass* Cls = UClass::TryFindTypeSlow<UClass>(ClassPath.ToString());
    ClassCache.Add(ClassPath, Cls);
    return Cls;
}

UScriptStruct* FPinTypeResolver::ResolveStruct(const FTopLevelAssetPath& StructPath) const
{
    if (TWeakObjectPtr<UScriptStruct>* Found = StructCache.Find(StructPath))
    {
        if (Found->IsValid()) return Found->Get();
    }
    UScriptStruct* S = UClass::TryFindTypeSlow<UScriptStruct>(StructPath.ToString());
    StructCache.Add(StructPath, S);
    return S;
}

UEnum* FPinTypeResolver::ResolveEnum(const FTopLevelAssetPath& EnumPath) const
{
    if (TWeakObjectPtr<UEnum>* Found = EnumCache.Find(EnumPath))
    {
        if (Found->IsValid()) return Found->Get();
    }
    UEnum* E = UClass::TryFindTypeSlow<UEnum>(EnumPath.ToString());
    EnumCache.Add(EnumPath, E);
    return E;
}

bool FPinTypeResolver::ResolvePinType(const FReflectedTypeDescriptor& TypeDescriptor, const FContainerDescriptor& Container, FEdGraphPinType& OutPinType, FString& OutError) const
{
    return ResolvePinType(TypeDescriptor.Path, Container, OutPinType, OutError);
}

bool FPinTypeResolver::ResolvePinType(const FTopLevelAssetPath& TypePath, const FContainerDescriptor& Container, FEdGraphPinType& OutPinType, FString& OutError) const
{
    OutPinType = FEdGraphPinType();
    FString PathStr = TypePath.ToString();
    if (PathStr.IsEmpty())
    {
        OutError = TEXT("TYPE_NOT_FOUND: Empty type_path");
        return false;
    }

    // Primitive property types (CoreUObject.*Property) → K2 pin categories
    auto SetPrimitive = [&OutPinType](const FName& Category, const FName& SubCategory)
    {
        OutPinType.PinCategory = Category;
        OutPinType.PinSubCategory = SubCategory;
    };
    if (PathStr.Equals(TEXT("/Script/CoreUObject.BoolProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Boolean, NAME_None);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.IntProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Int, NAME_None);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.Int64Property"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Int64, NAME_None);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.FloatProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.DoubleProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Double);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.StrProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_String, NAME_None);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.NameProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Name, NAME_None);
    }
    else if (PathStr.Equals(TEXT("/Script/CoreUObject.TextProperty"), ESearchCase::IgnoreCase))
    {
        SetPrimitive(UEdGraphSchema_K2::PC_Text, NAME_None);
    }
    else
    {
        // Complex types: struct/enum/class
        if (UScriptStruct* S = ResolveStruct(TypePath))
        {
            OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            OutPinType.PinSubCategoryObject = S;
        }
        else if (UEnum* E = ResolveEnum(TypePath))
        {
            OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
            OutPinType.PinSubCategoryObject = E;
        }
        else if (UClass* C = ResolveClass(TypePath))
        {
            OutPinType.PinCategory = C->HasAnyClassFlags(CLASS_Interface) ? UEdGraphSchema_K2::PC_Interface : UEdGraphSchema_K2::PC_Object;
            OutPinType.PinSubCategoryObject = C;
        }
        else
        {
            OutError = FString::Printf(TEXT("TYPE_NOT_FOUND: %s"), *PathStr);
            return false;
        }
    }

    // Container handling
    if (!Container.Kind.IsEmpty())
    {
        if (Container.Kind.Equals(TEXT("Array"), ESearchCase::IgnoreCase))
        {
            OutPinType.ContainerType = EPinContainerType::Array;
        }
        else if (Container.Kind.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
        {
            OutPinType.ContainerType = EPinContainerType::Set;
        }
        else if (Container.Kind.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
        {
            OutPinType.ContainerType = EPinContainerType::Map;
            // For maps, UE uses PinValueType for value and PinSubCategoryObject for key? Implementation differs by version
            // Minimal approach: leave as map with same subcategory; advanced key/value typing can be added later
        }
    }
    return true;
}

FString FPinTypeResolver::PinTypeToCanonicalPath(const FEdGraphPinType& PinType) const
{
    if (PinType.PinSubCategoryObject.IsValid())
    {
        if (const UObject* Obj = PinType.PinSubCategoryObject.Get())
        {
            const UPackage* Pkg = Obj->GetOutermost();
            return FString::Printf(TEXT("%s.%s"), *Pkg->GetName(), *Obj->GetName());
        }
    }
    return FString();
}

FString FPinTypeResolver::PinTypeToDisplayName(const FEdGraphPinType& PinType) const
{
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
    {
        if (const UScriptStruct* S = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
        {
            return S->GetDisplayNameText().ToString();
        }
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
    {
        if (const UEnum* E = Cast<UEnum>(PinType.PinSubCategoryObject.Get()))
        {
            return E->GetDisplayNameText().ToString();
        }
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object || PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
    {
        if (const UClass* C = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
        {
            return C->GetDisplayNameText().ToString();
        }
    }
    return TEXT("");
}

bool FPinTypeResolver::MakeArrayPinType(const FEdGraphPinType& ElementType, FEdGraphPinType& OutArrayType) const
{
    OutArrayType = ElementType;
    OutArrayType.ContainerType = EPinContainerType::Array;
    return true;
}

bool FPinTypeResolver::MakeSetPinType(const FEdGraphPinType& ElementType, FEdGraphPinType& OutSetType) const
{
    OutSetType = ElementType;
    OutSetType.ContainerType = EPinContainerType::Set;
    return true;
}

bool FPinTypeResolver::MakeMapPinType(const FEdGraphPinType& KeyType, const FEdGraphPinType& ValueType, FEdGraphPinType& OutMapType) const
{
    // Minimal implementation: set container to Map and keep key in subcategory; full key/value support requires newer API
    OutMapType = KeyType;
    OutMapType.ContainerType = EPinContainerType::Map;
    return true;
}

// =============================
// FVariableDefinitionService (minimal)
// =============================
FVariableDefinitionService::FVariableDefinitionService() {}
FVariableDefinitionService::~FVariableDefinitionService() {}

FBPVariableDescription* FVariableDefinitionService::FindVariable(UBlueprint* Blueprint, const FName& VarName) const
{
    if (!Blueprint) return nullptr;
    for (FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == VarName)
        {
            return &Var;
        }
    }
    return nullptr;
}

bool FVariableDefinitionService::CreateOrUpdateVariable(UBlueprint* Blueprint, const FVariableDefinition& Definition, FString& OutError)
{
    if (!Blueprint)
    {
        OutError = TEXT("Invalid Blueprint");
        return false;
    }
    if (Definition.VariableName.IsNone())
    {
        OutError = TEXT("VARIABLE_NAME_MISSING");
        return false;
    }
    // Resolve pin type from canonical path
    FPinTypeResolver Resolver;
    FEdGraphPinType PinType;
    FString ResolveError;
    if (!Resolver.ResolvePinType(Definition.TypePath, Definition.Container, PinType, ResolveError))
    {
        OutError = ResolveError;
        return false;
    }

    const bool bExists = FindVariable(Blueprint, Definition.VariableName) != nullptr;
    if (!bExists)
    {
        if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, Definition.VariableName, PinType, Definition.DefaultValueString))
        {
            OutError = TEXT("FAILED_TO_CREATE_VARIABLE");
            return false;
        }
    }

    if (FBPVariableDescription* Var = FindVariable(Blueprint, Definition.VariableName))
    {
        Var->VarType = PinType;
        Var->Category = FText::FromString(Definition.Category);
        if (!Definition.Tooltip.IsEmpty())
        {
            Var->FriendlyName = Definition.Tooltip;
        }
        // Apply metadata flags roughly
        if (Definition.bPrivate)
        {
            Var->PropertyFlags |= CPF_DisableEditOnInstance;
        }
        if (Definition.bExposeOnSpawn)
        {
            Var->SetMetaData(TEXT("ExposeOnSpawn"), TEXT("true"));
        }
        // Persist metadata map (reset existing and re-apply)
        Var->MetaDataArray.Reset();
        for (const auto& Kvp : Definition.MetadataMap)
        {
            Var->SetMetaData(FName(*Kvp.Key), Kvp.Value);
        }
    }

    return CompileIfNeeded(Blueprint, OutError);
}

bool FVariableDefinitionService::DeleteVariable(UBlueprint* Blueprint, const FName& VarName, FString& OutError)
{
    if (!Blueprint)
    {
        OutError = TEXT("Invalid Blueprint");
        return false;
    }
    // UE 5.6: RemoveMemberVariable returns void; perform removal then verify
    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);
    if (FindVariable(Blueprint, VarName) != nullptr)
    {
        OutError = TEXT("VARIABLE_NOT_FOUND");
        return false;
    }
    return CompileIfNeeded(Blueprint, OutError);
}

bool FVariableDefinitionService::GetVariableMetadata(UBlueprint* Blueprint, const FName& VarName, TMap<FString, FString>& OutMetadata, FString& OutError) const
{
    if (FBPVariableDescription* Var = FindVariable(Blueprint, VarName))
    {
        OutMetadata.Reset();
        for (const FBPVariableMetaDataEntry& Entry : Var->MetaDataArray)
        {
            OutMetadata.Add(Entry.DataKey.ToString(), Entry.DataValue);
        }
        return true;
    }
    OutError = TEXT("VARIABLE_NOT_FOUND");
    return false;
}

bool FVariableDefinitionService::SetVariableMetadata(UBlueprint* Blueprint, const FName& VarName, const TMap<FString, FString>& Metadata, FString& OutError)
{
    if (FBPVariableDescription* Var = FindVariable(Blueprint, VarName))
    {
        Var->MetaDataArray.Reset();
        for (const auto& Kvp : Metadata)
        {
            Var->SetMetaData(FName(*Kvp.Key), Kvp.Value);
        }
        return CompileIfNeeded(Blueprint, OutError);
    }
    OutError = TEXT("VARIABLE_NOT_FOUND");
    return false;
}

TArray<FBPVariableDescription*> FVariableDefinitionService::GetAllVariables(UBlueprint* Blueprint) const
{
    TArray<FBPVariableDescription*> Out;
    if (!Blueprint) return Out;
    for (FBPVariableDescription& V : Blueprint->NewVariables)
    {
        Out.Add(&V);
    }
    return Out;
}

bool FVariableDefinitionService::GetVariableInfo(UBlueprint* Blueprint, const FName& VarName, FVariableDefinition& OutDefinition, FString& OutError) const
{
    if (FBPVariableDescription* Var = FindVariable(Blueprint, VarName))
    {
        OutDefinition = BPVariableToDefinition(*Var);
        return true;
    }
    OutError = TEXT("VARIABLE_NOT_FOUND");
    return false;
}

FVariableDefinition FVariableDefinitionService::BPVariableToDefinition(const FBPVariableDescription& BPVar) const
{
    FVariableDefinition Def;
    Def.VariableName = BPVar.VarName;
    
    // Type path resolution: handle both object types and primitives
    if (const UObject* Obj = BPVar.VarType.PinSubCategoryObject.Get())
    {
        // Object types (UMG widgets, Niagara systems, Blueprint classes, etc.)
        const UPackage* Pkg = Obj->GetOutermost();
        Def.TypePath = FTopLevelAssetPath(*Pkg->GetName(), *Obj->GetName());
    }
    else if (!BPVar.VarType.PinCategory.IsNone())
    {
        // Primitive types: convert pin category to canonical type path
        const FString CategoryStr = BPVar.VarType.PinCategory.ToString();
        FString PropertyTypeName;
        
        if (CategoryStr.Equals(TEXT("float"), ESearchCase::IgnoreCase) || CategoryStr.Equals(TEXT("real"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("FloatProperty");
        }
        else if (CategoryStr.Equals(TEXT("int"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("IntProperty");
        }
        else if (CategoryStr.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("BoolProperty");
        }
        else if (CategoryStr.Equals(TEXT("double"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("DoubleProperty");
        }
        else if (CategoryStr.Equals(TEXT("string"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("StrProperty");
        }
        else if (CategoryStr.Equals(TEXT("name"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("NameProperty");
        }
        else if (CategoryStr.Equals(TEXT("byte"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("ByteProperty");
        }
        else if (CategoryStr.Equals(TEXT("text"), ESearchCase::IgnoreCase))
        {
            PropertyTypeName = TEXT("TextProperty");
        }
        
        if (!PropertyTypeName.IsEmpty())
        {
            Def.TypePath = FTopLevelAssetPath(TEXT("/Script/CoreUObject"), *PropertyTypeName);
        }
    }
    
    Def.Container.Kind = (BPVar.VarType.ContainerType == EPinContainerType::Array) ? TEXT("Array") :
                         (BPVar.VarType.ContainerType == EPinContainerType::Set) ? TEXT("Set") :
                         (BPVar.VarType.ContainerType == EPinContainerType::Map) ? TEXT("Map") : TEXT("");
    
    // Copy default value
    Def.DefaultValueString = BPVar.DefaultValue;
    
    // Copy metadata out
    Def.MetadataMap.Reset();
    for (const FBPVariableMetaDataEntry& Entry : BPVar.MetaDataArray)
    {
        Def.MetadataMap.Add(Entry.DataKey.ToString(), Entry.DataValue);
    }
    Def.Category = BPVar.Category.ToString();
    Def.Tooltip = BPVar.FriendlyName;
    return Def;
}

bool FVariableDefinitionService::DefinitionToBPVariable(const FVariableDefinition& /*Definition*/, FBPVariableDescription& /*OutBPVar*/, FString& /*OutError*/) const
{
    // Not needed currently; using AddMemberVariable pathway instead
    return false;
}

bool FVariableDefinitionService::ValidateVariableDefinition(const FVariableDefinition& Definition, FString& OutError) const
{
    if (Definition.VariableName.IsNone())
    {
        OutError = TEXT("VARIABLE_NAME_MISSING");
        return false;
    }
    if (Definition.TypePath.IsNull())
    {
        OutError = TEXT("TYPE_PATH_REQUIRED");
        return false;
    }
    return true;
}

void FVariableDefinitionService::ApplyDefaultMetadata(FBPVariableDescription& /*BPVar*/, const FVariableDefinition& /*Definition*/) const
{
}

bool FVariableDefinitionService::CompileIfNeeded(UBlueprint* Blueprint, FString& OutError) const
{
    if (!Blueprint) { OutError = TEXT("Invalid Blueprint"); return false; }
    FString CompileError;
    const bool bCompiled = FCommonUtils::SafeCompileBlueprint(Blueprint, CompileError);
    if (!bCompiled && !CompileError.IsEmpty())
    {
        UE_LOG(LogVibeUEManageVars, Warning, TEXT("Compile warning: %s"), *CompileError);
    }
    return true;
}

// =============================
// FPropertyAccessService (stubs)
// =============================
FPropertyAccessService::FPropertyAccessService() {}
FPropertyAccessService::~FPropertyAccessService() {}

// Forward declaration for key token assignment helper (used by ResolveProperty and NavigatePropertyChain)
static bool AssignTokenToPropertyKey(FProperty* KeyProp, void* KeyBuffer, const FString& Token, bool bQuoted, FString& OutError);

FResolvedProperty FPropertyAccessService::ResolveProperty(UBlueprint* Blueprint, const FString& CanonicalPath, FString& OutError)
{
    OutError.Reset();
    if (!Blueprint)
    {
        OutError = TEXT("BLUEPRINT_INVALID");
        return FResolvedProperty();
    }
    // Path convention: variable.property.subProperty...
    TArray<FString> Segments;
    if (!ParsePropertyPath(CanonicalPath, Segments, OutError))
    {
        return FResolvedProperty();
    }
    if (Segments.Num() < 1)
    {
        OutError = TEXT("PATH_EMPTY");
        return FResolvedProperty();
    }
    const FString& FirstSeg = Segments[0];
    // Extract base variable name and optional bracket token from first segment
    FString VarName = FirstSeg;
    bool bFirstHasBracket = false;
    FString FirstBracketToken; bool bFirstTokenQuoted = false;
    {
        const int32 OpenIdx = FirstSeg.Find(TEXT("["));
        if (OpenIdx != INDEX_NONE)
        {
            const int32 CloseIdx = FirstSeg.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenIdx + 1);
            if (CloseIdx == INDEX_NONE || CloseIdx < OpenIdx)
            {
                OutError = FString::Printf(TEXT("PATH_SEGMENT_PARSE_FAILED: %s"), *FirstSeg);
                return FResolvedProperty();
            }
            VarName = FirstSeg.Mid(0, OpenIdx).TrimStartAndEnd();
            FString Inside = FirstSeg.Mid(OpenIdx + 1, CloseIdx - OpenIdx - 1).TrimStartAndEnd();
            bFirstHasBracket = true;
            if (Inside.Len() >= 2 && Inside.StartsWith(TEXT("\"")) && Inside.EndsWith(TEXT("\"")))
            {
                bFirstTokenQuoted = true;
                FirstBracketToken = Inside.Mid(1, Inside.Len() - 2);
            }
            else
            {
                FirstBracketToken = Inside;
            }
        }
    }
    // Find variable description
    FBPVariableDescription* VarDesc = nullptr;
    for (FBPVariableDescription& V : Blueprint->NewVariables)
    {
        if (V.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
        {
            VarDesc = &V;
            break;
        }
    }
    if (!VarDesc)
    {
        OutError = FString::Printf(TEXT("VARIABLE_NOT_FOUND: %s"), *VarName);
        return FResolvedProperty();
    }
    // Resolve property on CDO of generated class
    UClass* GeneratedClass = Blueprint->GeneratedClass;
    if (!GeneratedClass)
    {
        OutError = TEXT("GENERATED_CLASS_MISSING");
        return FResolvedProperty();
    }
    UObject* CDO = GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        OutError = TEXT("CDO_MISSING");
        return FResolvedProperty();
    }
    // Find FProperty by variable name on CDO
    FProperty* RootProp = GeneratedClass->FindPropertyByName(VarDesc->VarName);
    if (!RootProp)
    {
        OutError = FString::Printf(TEXT("PROPERTY_NOT_FOUND: %s"), *VarName);
        return FResolvedProperty();
    }
    void* RootPtr = RootProp->ContainerPtrToValuePtr<void>(CDO);
    if (!RootPtr)
    {
        OutError = TEXT("VALUE_ADDRESS_NULL");
        return FResolvedProperty();
    }
    // Navigate remaining segments
    // If bracket present on the variable itself, handle container element selection at root
    if (bFirstHasBracket)
    {
        if (FArrayProperty* AP = CastField<FArrayProperty>(RootProp))
        {
            if (FirstBracketToken.IsEmpty()) { OutError = TEXT("ARRAY_INDEX_MISSING"); return FResolvedProperty(); }
            if (!FirstBracketToken.IsNumeric()) { OutError = TEXT("ARRAY_INDEX_NOT_NUMERIC"); return FResolvedProperty(); }
            const int32 Index = FCString::Atoi(*FirstBracketToken);
            FScriptArrayHelper Helper(AP, RootPtr);
            if (Index < 0 || Index >= Helper.Num()) { OutError = TEXT("PROPERTY_OUT_OF_RANGE"); return FResolvedProperty(); }
            void* ElemPtr = Helper.GetRawPtr(Index);
            if (Segments.Num() == 1)
            {
                return FResolvedProperty(AP->Inner, CDO, ElemPtr, CanonicalPath);
            }
            // More tail segments: only supported if element is a struct
            if (FStructProperty* ElemSP = CastField<FStructProperty>(AP->Inner))
            {
                TArray<FString> Tail = Segments; Tail.RemoveAt(0);
                FProperty* Terminal = nullptr; void* ValuePtr = nullptr;
                if (!NavigatePropertyChain(ElemSP->Struct, ElemPtr, Tail, Terminal, ValuePtr, OutError))
                {
                    return FResolvedProperty();
                }
                return FResolvedProperty(Terminal, CDO, ValuePtr, CanonicalPath);
            }
            OutError = TEXT("CANNOT_TRAVERSE_NON_STRUCT_ELEMENT_AT_ROOT");
            return FResolvedProperty();
        }
        else if (FMapProperty* MP = CastField<FMapProperty>(RootProp))
        {
            FScriptMapHelper Helper(MP, RootPtr);
            // Temp key buffer
            TArray<uint8> KeyTemp; KeyTemp.SetNumZeroed(MP->KeyProp->GetElementSize());
            MP->KeyProp->InitializeValue(KeyTemp.GetData());
            FString KeyErr;
            if (!AssignTokenToPropertyKey(MP->KeyProp, KeyTemp.GetData(), FirstBracketToken, bFirstTokenQuoted, KeyErr))
            {
                MP->KeyProp->DestroyValue(KeyTemp.GetData());
                OutError = FString::Printf(TEXT("MAP_KEY_CONVERT_FAILED: %s"), *KeyErr);
                return FResolvedProperty();
            }
            int32 FoundIdx = INDEX_NONE; const int32 MaxIdx = Helper.GetMaxIndex();
            for (int32 Slot = 0; Slot < MaxIdx; ++Slot)
            {
                if (!Helper.IsValidIndex(Slot)) continue;
                if (MP->KeyProp->Identical(Helper.GetKeyPtr(Slot), KeyTemp.GetData(), PPF_None)) { FoundIdx = Slot; break; }
            }
            MP->KeyProp->DestroyValue(KeyTemp.GetData());
            if (FoundIdx == INDEX_NONE) { OutError = TEXT("MAP_KEY_NOT_FOUND"); return FResolvedProperty(); }
            void* MapValPtr = Helper.GetValuePtr(FoundIdx);
            if (Segments.Num() == 1)
            {
                return FResolvedProperty(MP->ValueProp, CDO, MapValPtr, CanonicalPath);
            }
            if (FStructProperty* ValSP = CastField<FStructProperty>(MP->ValueProp))
            {
                TArray<FString> Tail = Segments; Tail.RemoveAt(0);
                FProperty* Terminal = nullptr; void* ValuePtr = nullptr;
                if (!NavigatePropertyChain(ValSP->Struct, MapValPtr, Tail, Terminal, ValuePtr, OutError))
                {
                    return FResolvedProperty();
                }
                return FResolvedProperty(Terminal, CDO, ValuePtr, CanonicalPath);
            }
            OutError = TEXT("CANNOT_TRAVERSE_NON_STRUCT_MAP_VALUE_AT_ROOT");
            return FResolvedProperty();
        }
        else if (CastField<FSetProperty>(RootProp))
        {
            OutError = TEXT("SET_INDEX_UNSUPPORTED");
            return FResolvedProperty();
        }
        else
        {
            OutError = TEXT("PROPERTY_NOT_CONTAINER");
            return FResolvedProperty();
        }
    }
    // No bracket on first segment
    if (Segments.Num() == 1)
    {
        return FResolvedProperty(RootProp, CDO, RootPtr, CanonicalPath);
    }
    TArray<FString> Tail = Segments; Tail.RemoveAt(0);
    FProperty* Terminal = nullptr; void* ValuePtr = nullptr;
    if (!NavigatePropertyChain(GeneratedClass, RootPtr, Tail, Terminal, ValuePtr, OutError))
    {
        return FResolvedProperty();
    }
    return FResolvedProperty(Terminal, CDO, ValuePtr, CanonicalPath);
}

bool FPropertyAccessService::GetPropertyValue(const FResolvedProperty& Property, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!Property.bIsValid)
    {
        OutError = TEXT("PROPERTY_INVALID");
        return false;
    }
    OutValue = PropertyToJsonValue(Property.TerminalProperty, Property.ValueAddress, OutError);
    return OutError.IsEmpty();
}

bool FPropertyAccessService::SetPropertyValue(const FResolvedProperty& Property, const TSharedPtr<FJsonValue>& InValue, FString& OutError)
{
    if (!Property.bIsValid)
    {
        OutError = TEXT("PROPERTY_INVALID");
        return false;
    }
    return JsonValueToProperty(Property.TerminalProperty, Property.ValueAddress, InValue, OutError);
}

bool FPropertyAccessService::GetPropertyValueFormatted(const FResolvedProperty& Property, FString& OutFormattedValue, FString& OutError)
{
    TSharedPtr<FJsonValue> Val;
    if (!GetPropertyValue(Property, Val, OutError))
    {
        return false;
    }
    // Simple stringify for now
    if (Val->Type == EJson::String)
    {
        OutFormattedValue = Val->AsString();
    }
    else
    {
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutFormattedValue);
        FJsonSerializer::Serialize(MakeShared<FJsonObject>(), Writer);
    }
    return true;
}

bool FPropertyAccessService::SetPropertyValueFromFormatted(const FResolvedProperty& Property, const FString& FormattedValue, FString& OutError)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FormattedValue);
    TSharedPtr<FJsonValue> Any;
    if (!FJsonSerializer::Deserialize(Reader, Any) || !Any.IsValid())
    {
        // Treat it as a plain string input
        Any = MakeShared<FJsonValueString>(FormattedValue);
    }
    return SetPropertyValue(Property, Any, OutError);
}

bool FPropertyAccessService::ParsePropertyPath(const FString& Path, TArray<FString>& OutSegments, FString& OutError)
{
    OutSegments.Reset();
    OutError.Reset();
    FString Trimmed = Path.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        OutError = TEXT("PATH_EMPTY");
        return false;
    }
    // Bracket/quote-aware split on '.'
    int32 BracketDepth = 0;
    bool bInQuotes = false;
    FString Current;
    for (int32 i = 0; i < Trimmed.Len(); ++i)
    {
        const TCHAR C = Trimmed[i];
        if (C == TEXT('"') && BracketDepth > 0)
        {
            bInQuotes = !bInQuotes;
            Current.AppendChar(C);
            continue;
        }
        if (!bInQuotes)
        {
            if (C == TEXT('[')) { BracketDepth++; Current.AppendChar(C); continue; }
            if (C == TEXT(']')) { BracketDepth = FMath::Max(0, BracketDepth - 1); Current.AppendChar(C); continue; }
            if (C == TEXT('.') && BracketDepth == 0)
            {
                FString Seg = Current.TrimStartAndEnd();
                if (!Seg.IsEmpty()) { OutSegments.Add(Seg); }
                Current.Reset();
                continue;
            }
        }
        Current.AppendChar(C);
    }
    if (!Current.IsEmpty())
    {
        FString Seg = Current.TrimStartAndEnd();
        if (!Seg.IsEmpty()) { OutSegments.Add(Seg); }
    }
    for (int32 i = OutSegments.Num() - 1; i >= 0; --i)
    {
        OutSegments[i] = OutSegments[i].TrimStartAndEnd();
        if (OutSegments[i].IsEmpty())
        {
            OutSegments.RemoveAt(i);
        }
    }
    if (OutSegments.Num() == 0)
    {
        OutError = TEXT("PATH_EMPTY");
        return false;
    }
    return true;
}

FString FPropertyAccessService::CombinePropertyPath(const TArray<FString>& Segments)
{
    return FString::Join(Segments, TEXT("."));
}

// Helper: parse a segment like "Inventory[2]" into base name + optional token
static bool ParseSegmentIndexToken(const FString& Segment, FString& BaseName, bool& bHasBracket, FString& BracketToken, bool& bTokenQuoted)
{
    bHasBracket = false; bTokenQuoted = false; BracketToken.Reset(); BaseName = Segment;
    const int32 OpenIdx = Segment.Find(TEXT("["));
    if (OpenIdx == INDEX_NONE) { return true; }
    const int32 CloseIdx = Segment.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenIdx + 1);
    if (CloseIdx == INDEX_NONE || CloseIdx < OpenIdx) { return false; }
    BaseName = Segment.Mid(0, OpenIdx).TrimStartAndEnd();
    FString Inside = Segment.Mid(OpenIdx + 1, CloseIdx - OpenIdx - 1).TrimStartAndEnd();
    bHasBracket = true;
    if (Inside.Len() >= 2 && Inside.StartsWith(TEXT("\"")) && Inside.EndsWith(TEXT("\"")))
    {
        bTokenQuoted = true;
        BracketToken = Inside.Mid(1, Inside.Len() - 2);
    }
    else
    {
        BracketToken = Inside;
    }
    return true;
}

// Helper: convert a token string into a property value using existing JSON conversion
static bool AssignTokenToPropertyKey(FProperty* KeyProp, void* KeyBuffer, const FString& Token, bool bQuoted, FString& OutError)
{
    if (!KeyProp || !KeyBuffer)
    {
        OutError = TEXT("KEY_ASSIGN_INVALID_INPUT");
        return false;
    }
    // Handle common key types explicitly without relying on private helpers
    if (FBoolProperty* P = CastField<FBoolProperty>(KeyProp))
    {
        if (!(Token.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("false"), ESearchCase::IgnoreCase)))
        {
            OutError = TEXT("MAP_KEY_EXPECTED_BOOL");
            return false;
        }
        P->SetPropertyValue(KeyBuffer, Token.Equals(TEXT("true"), ESearchCase::IgnoreCase));
        return true;
    }
    if (FIntProperty* P = CastField<FIntProperty>(KeyProp))
    {
        if (!bQuoted && (Token.Len() > 0) && (Token[0] == TEXT('-') || FChar::IsDigit(Token[0])))
        {
            P->SetPropertyValue(KeyBuffer, FCString::Atoi(*Token));
            return true;
        }
        OutError = TEXT("MAP_KEY_EXPECTED_INT");
        return false;
    }
    if (FInt64Property* P = CastField<FInt64Property>(KeyProp))
    {
        if (!bQuoted && (Token.Len() > 0) && (Token[0] == TEXT('-') || FChar::IsDigit(Token[0])))
        {
            P->SetPropertyValue(KeyBuffer, FCString::Atoi64(*Token));
            return true;
        }
        OutError = TEXT("MAP_KEY_EXPECTED_INT64");
        return false;
    }
    if (FFloatProperty* P = CastField<FFloatProperty>(KeyProp))
    {
        if (!bQuoted)
        {
            P->SetPropertyValue(KeyBuffer, (float)FCString::Atof(*Token));
            return true;
        }
        OutError = TEXT("MAP_KEY_EXPECTED_FLOAT");
        return false;
    }
    if (FDoubleProperty* P = CastField<FDoubleProperty>(KeyProp))
    {
        if (!bQuoted)
        {
            P->SetPropertyValue(KeyBuffer, FCString::Atod(*Token));
            return true;
        }
        OutError = TEXT("MAP_KEY_EXPECTED_DOUBLE");
        return false;
    }
    if (FStrProperty* P = CastField<FStrProperty>(KeyProp))
    {
        // If token came quoted, it already had quotes stripped by parser
        P->SetPropertyValue(KeyBuffer, Token);
        return true;
    }
    if (FNameProperty* P = CastField<FNameProperty>(KeyProp))
    {
        P->SetPropertyValue(KeyBuffer, FName(*Token));
        return true;
    }
    if (FTextProperty* P = CastField<FTextProperty>(KeyProp))
    {
        P->SetPropertyValue(KeyBuffer, FText::FromString(Token));
        return true;
    }
    if (FEnumProperty* EP = CastField<FEnumProperty>(KeyProp))
    {
        // Only support numeric assignment for enum keys for now
        if (!bQuoted && (Token.Len() > 0) && (Token[0] == TEXT('-') || FChar::IsDigit(Token[0])))
        {
            const int64 Raw = FCString::Atoi64(*Token);
            EP->GetUnderlyingProperty()->SetIntPropertyValue(KeyBuffer, Raw);
            return true;
        }
        OutError = TEXT("MAP_KEY_ENUM_NUMERIC_ONLY");
        return false;
    }
    OutError = FString::Printf(TEXT("MAP_KEY_UNSUPPORTED_TYPE: %s"), *KeyProp->GetClass()->GetName());
    return false;
}

bool FPropertyAccessService::NavigatePropertyChain(UStruct* OwnerStruct, void* OwnerPtr, const TArray<FString>& Segments, FProperty*& OutTerminalProp, void*& OutValuePtr, FString& OutError)
{
    UStruct* CurrentStruct = OwnerStruct;
    void* CurrentPtr = OwnerPtr;
    FProperty* CurrentProp = nullptr;
    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        const FString& RawSegment = Segments[i];
        FString BaseName; bool bHasBracket = false; FString BracketToken; bool bQuoted = false;
        if (!ParseSegmentIndexToken(RawSegment, BaseName, bHasBracket, BracketToken, bQuoted))
        {
            OutError = FString::Printf(TEXT("PATH_SEGMENT_PARSE_FAILED: %s"), *RawSegment);
            return false;
        }
        // Find property by base name
        CurrentProp = CurrentStruct->FindPropertyByName(*BaseName);
        if (!CurrentProp)
        {
            OutError = FString::Printf(TEXT("SUBPROPERTY_NOT_FOUND: %s"), *BaseName);
            return false;
        }
        // Base address of the property value on current container
        void* PropBasePtr = CurrentProp->ContainerPtrToValuePtr<void>(CurrentPtr);

        // Handle container indexers if present
        if (bHasBracket)
        {
            if (FArrayProperty* AP = CastField<FArrayProperty>(CurrentProp))
            {
                // Parse array index
                if (BracketToken.IsEmpty()) { OutError = TEXT("ARRAY_INDEX_MISSING"); return false; }
                if (!BracketToken.IsNumeric()) { OutError = TEXT("ARRAY_INDEX_NOT_NUMERIC"); return false; }
                const int32 Index = FCString::Atoi(*BracketToken);
                FScriptArrayHelper Helper(AP, PropBasePtr);
                if (Index < 0 || Index >= Helper.Num())
                {
                    OutError = TEXT("PROPERTY_OUT_OF_RANGE");
                    return false;
                }
                void* ElemPtr = Helper.GetRawPtr(Index);
                // If there are more segments, the element must be a struct to traverse
                if (i < Segments.Num() - 1)
                {
                    if (FStructProperty* ElemSP = CastField<FStructProperty>(AP->Inner))
                    {
                        CurrentStruct = ElemSP->Struct;
                        CurrentPtr = ElemPtr;
                        continue; // next segment
                    }
                    else
                    {
                        OutError = FString::Printf(TEXT("CANNOT_TRAVERSE_NON_STRUCT_ELEMENT: %s[%d]"), *BaseName, Index);
                        return false;
                    }
                }
                // Terminal = array element itself
                OutTerminalProp = AP->Inner;
                OutValuePtr = ElemPtr;
                return true;
            }
            else if (FMapProperty* MP = CastField<FMapProperty>(CurrentProp))
            {
                // Locate map value by key
                FScriptMapHelper Helper(MP, PropBasePtr);
                // Build a temporary key buffer
                TArray<uint8> KeyTemp; KeyTemp.SetNumZeroed(MP->KeyProp->GetElementSize());
                MP->KeyProp->InitializeValue(KeyTemp.GetData());
                FString KeyErr;
                if (!AssignTokenToPropertyKey(MP->KeyProp, KeyTemp.GetData(), BracketToken, bQuoted, KeyErr))
                {
                    MP->KeyProp->DestroyValue(KeyTemp.GetData());
                    OutError = FString::Printf(TEXT("MAP_KEY_CONVERT_FAILED: %s"), *KeyErr);
                    return false;
                }
                const int32 MaxIdx = Helper.GetMaxIndex();
                int32 FoundIdx = INDEX_NONE;
                for (int32 Slot = 0; Slot < MaxIdx; ++Slot)
                {
                    if (!Helper.IsValidIndex(Slot)) continue;
                    void* ExistingKeyPtr = Helper.GetKeyPtr(Slot);
                    if (MP->KeyProp->Identical(ExistingKeyPtr, KeyTemp.GetData(), PPF_None))
                    {
                        FoundIdx = Slot; break;
                    }
                }
                MP->KeyProp->DestroyValue(KeyTemp.GetData());
                if (FoundIdx == INDEX_NONE)
                {
                    OutError = TEXT("MAP_KEY_NOT_FOUND");
                    return false;
                }
                void* ValuePtr = Helper.GetValuePtr(FoundIdx);
                // If more segments, value must be a struct to traverse
                if (i < Segments.Num() - 1)
                {
                    if (FStructProperty* ValSP = CastField<FStructProperty>(MP->ValueProp))
                    {
                        CurrentStruct = ValSP->Struct;
                        CurrentPtr = ValuePtr;
                        continue;
                    }
                    else
                    {
                        OutError = FString::Printf(TEXT("CANNOT_TRAVERSE_NON_STRUCT_MAP_VALUE: %s[?]"), *BaseName);
                        return false;
                    }
                }
                OutTerminalProp = MP->ValueProp;
                OutValuePtr = ValuePtr;
                return true;
            }
            else if (FSetProperty* SetP = CastField<FSetProperty>(CurrentProp))
            {
                (void)SetP;
                OutError = TEXT("SET_INDEX_UNSUPPORTED");
                return false;
            }
            else
            {
                OutError = TEXT("PROPERTY_NOT_CONTAINER");
                return false;
            }
        }

        // No bracket indexer. If property is a struct, descend; otherwise must be terminal unless more segments would follow
        if (FStructProperty* SP = CastField<FStructProperty>(CurrentProp))
        {
            CurrentStruct = SP->Struct;
            CurrentPtr = PropBasePtr;
        }
        else if (i < Segments.Num() - 1)
        {
            // If this is a container without indexer, returning whole container is only allowed as terminal
            if (CastField<FArrayProperty>(CurrentProp) || CastField<FMapProperty>(CurrentProp) || CastField<FSetProperty>(CurrentProp))
            {
                OutError = TEXT("CONTAINER_REQUIRES_INDEX");
                return false;
            }
            OutError = FString::Printf(TEXT("CANNOT_TRAVERSE_NON_STRUCT: %s"), *BaseName);
            return false;
        }
        else
        {
            // Terminal non-struct property
            OutTerminalProp = CurrentProp;
            OutValuePtr = PropBasePtr;
            return true;
        }
    }
    OutTerminalProp = CurrentProp;
    OutValuePtr = CurrentPtr;
    return (OutTerminalProp != nullptr && OutValuePtr != nullptr);
}

TSharedPtr<FJsonValue> FPropertyAccessService::PropertyToJsonValue(FProperty* Prop, void* ValuePtr, FString& OutError)
{
    if (!Prop || !ValuePtr) { OutError = TEXT("PROP_OR_VALUE_NULL"); return nullptr; }
    // Primitives
    if (FBoolProperty* P = CastField<FBoolProperty>(Prop))
    {
        return MakeShared<FJsonValueBoolean>(P->GetPropertyValue(ValuePtr));
    }
    if (FIntProperty* P = CastField<FIntProperty>(Prop))
    {
        return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    }
    if (FInt64Property* P = CastField<FInt64Property>(Prop))
    {
        return MakeShared<FJsonValueNumber>((double)P->GetPropertyValue(ValuePtr));
    }
    if (FFloatProperty* P = CastField<FFloatProperty>(Prop))
    {
        return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    }
    if (FDoubleProperty* P = CastField<FDoubleProperty>(Prop))
    {
        return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    }
    if (FStrProperty* P = CastField<FStrProperty>(Prop))
    {
        return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr));
    }
    if (FNameProperty* P = CastField<FNameProperty>(Prop))
    {
        return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString());
    }
    if (FTextProperty* P = CastField<FTextProperty>(Prop))
    {
        return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString());
    }
    // Enum
    if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
    {
        int64 Raw = EP->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
        return MakeShared<FJsonValueNumber>((double)Raw);
    }
    // Struct: serialize to JSON object via JsonUtilities
    if (FStructProperty* SP = CastField<FStructProperty>(Prop))
    {
        if (!SP->Struct)
        {
            OutError = TEXT("STRUCT_TYPE_NULL");
            return nullptr;
        }
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        const bool bOk = FJsonObjectConverter::UStructToJsonObject(SP->Struct, ValuePtr, Obj.ToSharedRef(), 0, 0);
        if (!bOk)
        {
            OutError = FString::Printf(TEXT("STRUCT_TO_JSON_FAILED: %s"), *SP->Struct->GetName());
            return nullptr;
        }
        return MakeShared<FJsonValueObject>(Obj);
    }
    // Sets: serialize as JSON array of element values
    if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
    {
        TArray<TSharedPtr<FJsonValue>> JsonArr;
        FScriptSetHelper Helper(SetProp, ValuePtr);
        JsonArr.Reserve(Helper.Num());
        const int32 MaxIdx = Helper.GetMaxIndex();
        for (int32 Idx = 0; Idx < MaxIdx; ++Idx)
        {
            if (!Helper.IsValidIndex(Idx)) continue;
            void* ElemPtr = Helper.GetElementPtr(Idx);
            FString ElemErr;
            TSharedPtr<FJsonValue> ElemVal = PropertyToJsonValue(SetProp->ElementProp, ElemPtr, ElemErr);
            if (!ElemErr.IsEmpty())
            {
                JsonArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("<unsupported:%s>"), *SetProp->ElementProp->GetName())));
            }
            else
            {
                JsonArr.Add(ElemVal);
            }
        }
        return MakeShared<FJsonValueArray>(JsonArr);
    }
    // Maps: serialize as array of { key, value }
    if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
    {
        TArray<TSharedPtr<FJsonValue>> JsonArr;
        FScriptMapHelper Helper(MapProp, ValuePtr);
        JsonArr.Reserve(Helper.Num());
        const int32 MaxIdx = Helper.GetMaxIndex();
        for (int32 Idx = 0; Idx < MaxIdx; ++Idx)
        {
            if (!Helper.IsValidIndex(Idx)) continue;
            void* KeyPtr = Helper.GetKeyPtr(Idx);
            void* ValPtr = Helper.GetValuePtr(Idx);
            FString KeyErr, ValErr;
            TSharedPtr<FJsonValue> KeyVal = PropertyToJsonValue(MapProp->KeyProp, KeyPtr, KeyErr);
            TSharedPtr<FJsonValue> ValVal = PropertyToJsonValue(MapProp->ValueProp, ValPtr, ValErr);
            TSharedPtr<FJsonObject> PairObj = MakeShared<FJsonObject>();
            PairObj->SetField(TEXT("key"), KeyVal.IsValid() ? KeyVal : MakeShared<FJsonValueString>(TEXT("<unsupported>")));
            PairObj->SetField(TEXT("value"), ValVal.IsValid() ? ValVal : MakeShared<FJsonValueString>(TEXT("<unsupported>")));
            JsonArr.Add(MakeShared<FJsonValueObject>(PairObj));
        }
        return MakeShared<FJsonValueArray>(JsonArr);
    }
    // Arrays (best-effort JSON array of element values)
    if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
    {
        TArray<TSharedPtr<FJsonValue>> JsonArr;
        FScriptArrayHelper Helper(AP, ValuePtr);
        const int32 N = Helper.Num();
        JsonArr.Reserve(N);
        for (int32 i = 0; i < N; ++i)
        {
            void* ElemPtr = Helper.GetRawPtr(i);
            FString ElemErr;
            TSharedPtr<FJsonValue> ElemVal = PropertyToJsonValue(AP->Inner, ElemPtr, ElemErr);
            if (!ElemErr.IsEmpty())
            {
                // Fallback: write string error for unsupported element types
                JsonArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("<unsupported:%s>"), *AP->Inner->GetName())));
            }
            else
            {
                JsonArr.Add(ElemVal);
            }
        }
        return MakeShared<FJsonValueArray>(JsonArr);
    }
    // Soft object reference
    if (FSoftObjectProperty* SOP = CastField<FSoftObjectProperty>(Prop))
    {
        FSoftObjectPtr* Ptr = reinterpret_cast<FSoftObjectPtr*>(ValuePtr);
        const FString Path = Ptr ? Ptr->ToSoftObjectPath().ToString() : FString();
        if (Path.IsEmpty()) return MakeShared<FJsonValueNull>();
        return MakeShared<FJsonValueString>(Path);
    }
    // Soft class reference
    if (FSoftClassProperty* SCP = CastField<FSoftClassProperty>(Prop))
    {
        TSoftClassPtr<UObject>* Ptr = reinterpret_cast<TSoftClassPtr<UObject>*>(ValuePtr);
        const FString Path = Ptr ? Ptr->ToSoftObjectPath().ToString() : FString();
        if (Path.IsEmpty()) return MakeShared<FJsonValueNull>();
        return MakeShared<FJsonValueString>(Path);
    }
    // Object reference
    if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
    {
        UObject* Obj = OP->GetObjectPropertyValue(ValuePtr);
        if (!Obj) return MakeShared<FJsonValueNull>();
        const UPackage* Pkg = Obj->GetOutermost();
        return MakeShared<FJsonValueString>(FString::Printf(TEXT("%s.%s"), *Pkg->GetName(), *Obj->GetName()));
    }
    OutError = TEXT("UNSUPPORTED_PROPERTY_TYPE");
    return nullptr;
}

bool FPropertyAccessService::JsonValueToProperty(FProperty* Prop, void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    if (!Prop || !ValuePtr || !JsonValue.IsValid()) { OutError = TEXT("INVALID_INPUT"); return false; }
    // Primitives
    if (FBoolProperty* P = CastField<FBoolProperty>(Prop))
    {
        bool b = (JsonValue->Type == EJson::Boolean) ? JsonValue->AsBool() : JsonValue->AsString().ToBool();
        P->SetPropertyValue(ValuePtr, b);
        return true;
    }
    if (FIntProperty* P = CastField<FIntProperty>(Prop))
    {
        int32 v = (JsonValue->Type == EJson::Number) ? (int32)JsonValue->AsNumber() : FCString::Atoi(*JsonValue->AsString());
        P->SetPropertyValue(ValuePtr, v);
        return true;
    }
    if (FInt64Property* P = CastField<FInt64Property>(Prop))
    {
        int64 v = (JsonValue->Type == EJson::Number) ? (int64)JsonValue->AsNumber() : FCString::Atoi64(*JsonValue->AsString());
        P->SetPropertyValue(ValuePtr, v);
        return true;
    }
    if (FFloatProperty* P = CastField<FFloatProperty>(Prop))
    {
        float v = (JsonValue->Type == EJson::Number) ? (float)JsonValue->AsNumber() : (float)FCString::Atof(*JsonValue->AsString());
        P->SetPropertyValue(ValuePtr, v);
        return true;
    }
    if (FDoubleProperty* P = CastField<FDoubleProperty>(Prop))
    {
        double v = (JsonValue->Type == EJson::Number) ? JsonValue->AsNumber() : FCString::Atod(*JsonValue->AsString());
        P->SetPropertyValue(ValuePtr, v);
        return true;
    }
    if (FStrProperty* P = CastField<FStrProperty>(Prop))
    {
        FString s; JsonValue->TryGetString(s); P->SetPropertyValue(ValuePtr, s); return true;
    }
    if (FNameProperty* P = CastField<FNameProperty>(Prop))
    {
        FString s; JsonValue->TryGetString(s); P->SetPropertyValue(ValuePtr, FName(*s)); return true;
    }
    if (FTextProperty* P = CastField<FTextProperty>(Prop))
    {
        FString s; JsonValue->TryGetString(s); P->SetPropertyValue(ValuePtr, FText::FromString(s)); return true;
    }
    if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
    {
        int64 v = (JsonValue->Type == EJson::Number) ? (int64)JsonValue->AsNumber() : FCString::Atoi64(*JsonValue->AsString());
        EP->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, v);
        return true;
    }
    // Arrays (best-effort: expect JSON array)
    if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
    {
        if (JsonValue->Type != EJson::Array)
        {
            OutError = TEXT("ARRAY_EXPECTED_JSON_ARRAY");
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>& InArr = JsonValue->AsArray();
        FScriptArrayHelper Helper(AP, ValuePtr);
        // Clear existing values (UE 5.6: use RemoveValues to empty)
        if (Helper.Num() > 0)
        {
            const int32 OldNum = Helper.Num();
            Helper.RemoveValues(0, OldNum);
        }
        for (const TSharedPtr<FJsonValue>& ElemVal : InArr)
        {
            const int32 NewIdx = Helper.AddValue();
            void* ElemPtr = Helper.GetRawPtr(NewIdx);
            // Initialize element memory (safe for most property types)
            AP->Inner->InitializeValue(ElemPtr);
            FString ElemErr;
            if (!JsonValueToProperty(AP->Inner, ElemPtr, ElemVal, ElemErr))
            {
                OutError = FString::Printf(TEXT("ARRAY_ELEMENT_SET_FAILED: %s"), *ElemErr);
                return false;
            }
        }
        return true;
    }
    // Struct: expect JSON object and convert via JsonUtilities
    if (FStructProperty* SP = CastField<FStructProperty>(Prop))
    {
        if (!SP->Struct)
        {
            OutError = TEXT("STRUCT_TYPE_NULL");
            return false;
        }
        if (JsonValue->Type != EJson::Object)
        {
            OutError = TEXT("STRUCT_EXPECTED_JSON_OBJECT");
            return false;
        }
        const TSharedPtr<FJsonObject>& Obj = JsonValue->AsObject();
        if (!FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), SP->Struct, ValuePtr, 0, 0))
        {
            OutError = FString::Printf(TEXT("STRUCT_FROM_JSON_FAILED: %s"), *SP->Struct->GetName());
            return false;
        }
        return true;
    }
    // Sets: write unsupported for now (requires careful hashing/equality handling)
    if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
    {
        (void)SetProp;
        OutError = TEXT("SET_SET_UNSUPPORTED");
        return false;
    }
    // Maps: write unsupported for now
    if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
    {
        (void)MapProp;
        OutError = TEXT("MAP_SET_UNSUPPORTED");
        return false;
    }
    // Soft object reference: assign path without loading
    if (FSoftObjectProperty* SOP = CastField<FSoftObjectProperty>(Prop))
    {
        FSoftObjectPtr* Ptr = reinterpret_cast<FSoftObjectPtr*>(ValuePtr);
        if (!Ptr)
        {
            OutError = TEXT("SOFT_OBJECT_PTR_NULL");
            return false;
        }
        if (JsonValue->Type == EJson::Null)
        {
            *Ptr = FSoftObjectPtr();
            return true;
        }
        FString Path; if (!JsonValue->TryGetString(Path)) { OutError = TEXT("SOFT_OBJECT_EXPECTED_STRING"); return false; }
        if (Path.IsEmpty() || Path.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { *Ptr = FSoftObjectPtr(); return true; }
        *Ptr = FSoftObjectPtr(FSoftObjectPath(Path));
        return true;
    }
    // Soft class reference: assign path without loading
    if (FSoftClassProperty* SCP = CastField<FSoftClassProperty>(Prop))
    {
        TSoftClassPtr<UObject>* Ptr = reinterpret_cast<TSoftClassPtr<UObject>*>(ValuePtr);
        if (!Ptr)
        {
            OutError = TEXT("SOFT_CLASS_PTR_NULL");
            return false;
        }
        if (JsonValue->Type == EJson::Null)
        {
            *Ptr = TSoftClassPtr<UObject>();
            return true;
        }
        FString Path; if (!JsonValue->TryGetString(Path)) { OutError = TEXT("SOFT_CLASS_EXPECTED_STRING"); return false; }
        if (Path.IsEmpty() || Path.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { *Ptr = TSoftClassPtr<UObject>(); return true; }
        *Ptr = TSoftClassPtr<UObject>(FSoftObjectPath(Path));
        return true;
    }
    // Object references: not setting by path here (would need load/resolve). Stub as unsupported.
    if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
    {
        // Accept null to clear
        if (JsonValue->Type == EJson::Null)
        {
            OP->SetObjectPropertyValue(ValuePtr, nullptr);
            return true;
        }
        FString Path;
        if (!JsonValue->TryGetString(Path))
        {
            OutError = TEXT("OBJECT_EXPECTED_STRING_PATH");
            return false;
        }
        FString Trimmed = Path.TrimStartAndEnd();
        if (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        {
            OP->SetObjectPropertyValue(ValuePtr, nullptr);
            return true;
        }
        // Use StaticLoadObject with the expected class; require full object path like /Game/Folder/Asset.Asset
        UObject* Loaded = StaticLoadObject(OP->PropertyClass, nullptr, *Trimmed);
        if (!Loaded)
        {
            OutError = FString::Printf(TEXT("OBJECT_LOAD_FAILED: %s"), *Trimmed);
            return false;
        }
        if (!Loaded->IsA(OP->PropertyClass))
        {
            OutError = FString::Printf(TEXT("OBJECT_TYPE_MISMATCH: expected %s"), *OP->PropertyClass->GetName());
            return false;
        }
        OP->SetObjectPropertyValue(ValuePtr, Loaded);
        return true;
    }
    OutError = TEXT("UNSUPPORTED_PROPERTY_TYPE");
    return false;
}

// =============================
// FResponseSerializer
// =============================
TSharedPtr<FJsonObject> FResponseSerializer::SerializeTypeDescriptor(const FReflectedTypeDescriptor& Descriptor)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"), Descriptor.Name);
    Obj->SetStringField(TEXT("display_name"), Descriptor.DisplayName);
    Obj->SetStringField(TEXT("class_path"), Descriptor.Path.ToString());
    Obj->SetStringField(TEXT("type_kind"),
        Descriptor.Kind == EReflectedTypeKind::Class ? TEXT("Class") :
        Descriptor.Kind == EReflectedTypeKind::Struct ? TEXT("Struct") :
        Descriptor.Kind == EReflectedTypeKind::Enum ? TEXT("Enum") :
        Descriptor.Kind == EReflectedTypeKind::Interface ? TEXT("Interface") : TEXT("Unknown"));
    Obj->SetStringField(TEXT("tooltip"), Descriptor.Tooltip);
    Obj->SetStringField(TEXT("category"), Descriptor.Category);
    return Obj;
}

TSharedPtr<FJsonObject> FResponseSerializer::SerializeTypeQuery(const FTypeQuery& Query)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("category"), Query.Category);
    Obj->SetStringField(TEXT("base_class_path"), Query.BaseClassPath.ToString());
    Obj->SetStringField(TEXT("search_text"), Query.SearchText);
    Obj->SetBoolField(TEXT("include_blueprints"), Query.bIncludeBlueprints);
    Obj->SetBoolField(TEXT("include_engine_types"), Query.bIncludeEngine);
    Obj->SetNumberField(TEXT("max_results"), Query.MaxResults);
    Obj->SetNumberField(TEXT("page_offset"), Query.PageOffset);
    return Obj;
}

TSharedPtr<FJsonObject> FResponseSerializer::SerializeContainerDescriptor(const FContainerDescriptor& Container)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("kind"), Container.Kind);
    if (!Container.KeyTypePath.IsEmpty()) Obj->SetStringField(TEXT("key_type_path"), Container.KeyTypePath);
    if (!Container.ValueTypePath.IsEmpty()) Obj->SetStringField(TEXT("value_type_path"), Container.ValueTypePath);
    return Obj;
}

TSharedPtr<FJsonObject> FResponseSerializer::SerializeVariableDefinition(const FVariableDefinition& Definition)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("variable_name"), Definition.VariableName.ToString());
    Obj->SetStringField(TEXT("type_path"), Definition.TypePath.ToString());
    Obj->SetObjectField(TEXT("container"), SerializeContainerDescriptor(Definition.Container));
    Obj->SetStringField(TEXT("category"), Definition.Category);
    Obj->SetStringField(TEXT("tooltip"), Definition.Tooltip);
    Obj->SetStringField(TEXT("default_value"), Definition.DefaultValueString);
    return Obj;
}

TSharedPtr<FJsonObject> FResponseSerializer::SerializePinType(const FEdGraphPinType& PinType)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("pin_category"), PinType.PinCategory.ToString());
    if (PinType.PinSubCategoryObject.IsValid())
    {
        const UObject* ObjType = PinType.PinSubCategoryObject.Get();
        const UPackage* Pkg = ObjType->GetOutermost();
        Obj->SetStringField(TEXT("class_path"), FString::Printf(TEXT("%s.%s"), *Pkg->GetName(), *ObjType->GetName()));
    }
    Obj->SetStringField(TEXT("container"),
        PinType.ContainerType == EPinContainerType::Array ? TEXT("Array") :
        PinType.ContainerType == EPinContainerType::Set ? TEXT("Set") :
        PinType.ContainerType == EPinContainerType::Map ? TEXT("Map") : TEXT("None"));
    return Obj;
}

TSharedPtr<FJsonObject> FResponseSerializer::CreateErrorResponse(const FString& ErrorCode, const FString& Message, const TSharedPtr<FJsonObject>& Details)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("success"), false);
    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
    Err->SetStringField(TEXT("code"), ErrorCode);
    Err->SetStringField(TEXT("message"), Message);
    if (Details.IsValid())
    {
        Root->SetObjectField(TEXT("details"), Details);
    }
    Root->SetObjectField(TEXT("error"), Err);
    return Root;
}

TSharedPtr<FJsonObject> FResponseSerializer::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("success"), true);
    if (Data.IsValid())
    {
        Root->SetObjectField(TEXT("data"), Data);
    }
    return Root;
}

TSharedPtr<FJsonValue> FResponseSerializer::MetadataToJsonValue(const TMap<FString, FString>& Metadata)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    for (const auto& Kvp : Metadata)
    {
        Obj->SetStringField(Kvp.Key, Kvp.Value);
    }
    return MakeShared<FJsonValueObject>(Obj);
}

bool FResponseSerializer::JsonValueToMetadata(const TSharedPtr<FJsonValue>& JsonValue, TMap<FString, FString>& OutMetadata)
{
    if (!JsonValue.IsValid() || JsonValue->Type != EJson::Object)
    {
        return false;
    }
    const TSharedPtr<FJsonObject>& Obj = JsonValue->AsObject();
    for (const auto& Kvp : Obj->Values)
    {
        FString ValStr;
        if (Kvp.Value->TryGetString(ValStr))
        {
            OutMetadata.Add(Kvp.Key, ValStr);
        }
    }
    return true;
}

// =============================
// FBlueprintVariableService (partial)
// =============================
TUniquePtr<FBlueprintVariableService> FBlueprintVariableService::Instance;

FBlueprintVariableService::FBlueprintVariableService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

FBlueprintVariableService::~FBlueprintVariableService() 
{
    Shutdown();
}

void FBlueprintVariableService::Initialize()
{
    FServiceBase::Initialize();
    CatalogService.Initialize();
}

void FBlueprintVariableService::Shutdown()
{
    CatalogService.Shutdown();
    FServiceBase::Shutdown();
}

FBlueprintVariableService& FBlueprintVariableService::Get()
{
    if (!Instance.IsValid())
    {
        // Create a default service context for singleton usage
        TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
        Context->Initialize();
        Instance = MakeUnique<FBlueprintVariableService>(Context);
        Instance->Initialize();
    }
    return *Instance.Get();
}

TSharedPtr<FJsonObject> FBlueprintVariableService::ExecuteCommand(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
    const FString Normalized = Action.TrimStartAndEnd();
    // Lightweight trace for diagnostics
    {
        FString BPN;
        if (Params.IsValid()) { Params->TryGetStringField(TEXT("blueprint_name"), BPN); }
        UE_LOG(LogVibeUEManageVars, Verbose, TEXT("ExecuteCommand Action='%s' BP='%s'"), *Normalized, *BPN);
    }
    if (Normalized.Equals(TEXT("search_types"), ESearchCase::IgnoreCase))
    {
        return HandleSearchTypes(Params);
    }
    if (Normalized.Equals(TEXT("diagnostics"), ESearchCase::IgnoreCase))
    {
        return HandleDiagnostics(Params);
    }
    if (Normalized.Equals(TEXT("get_property"), ESearchCase::IgnoreCase))
    {
        return HandleGetProperty(Params);
    }
    if (Normalized.Equals(TEXT("set_property"), ESearchCase::IgnoreCase))
    {
        return HandleSetProperty(Params);
    }
    if (Normalized.Equals(TEXT("get_property_metadata"), ESearchCase::IgnoreCase))
    {
        return HandleGetPropertyMetadata(Params);
    }
    if (Normalized.Equals(TEXT("set_property_metadata"), ESearchCase::IgnoreCase))
    {
        return HandleSetPropertyMetadata(Params);
    }
    if (Normalized.Equals(TEXT("create"), ESearchCase::IgnoreCase))
    {
        return HandleCreate(Params);
    }
    if (Normalized.Equals(TEXT("delete"), ESearchCase::IgnoreCase))
    {
        return HandleDelete(Params);
    }
    if (Normalized.Equals(TEXT("list"), ESearchCase::IgnoreCase))
    {
        return HandleList(Params);
    }
    if (Normalized.Equals(TEXT("get_info"), ESearchCase::IgnoreCase))
    {
        return HandleGetInfo(Params);
    }
    if (Normalized.Equals(TEXT("modify"), ESearchCase::IgnoreCase))
    {
        return HandleModify(Params);
    }
    return FResponseSerializer::CreateErrorResponse(TEXT("ACTION_UNSUPPORTED"), FString::Printf(TEXT("Action '%s' not implemented in reflection path"), *Action));
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleSearchTypes(const TSharedPtr<FJsonObject>& Params)
{
    if (CatalogService.ShouldRefreshCache())
    {
        CatalogService.ForceRefresh();
    }

    FTypeQuery Query;
    Params->TryGetStringField(TEXT("category"), Query.Category);
    Params->TryGetStringField(TEXT("search_text"), Query.SearchText);
    Params->TryGetBoolField(TEXT("include_blueprints"), Query.bIncludeBlueprints);
    Params->TryGetBoolField(TEXT("include_engine_types"), Query.bIncludeEngine);
    int32 PageOffset = 0; Params->TryGetNumberField(TEXT("page_offset"), PageOffset); Query.PageOffset = PageOffset;
    int32 MaxResults = 100; Params->TryGetNumberField(TEXT("max_results"), MaxResults); Query.MaxResults = MaxResults;
    
    const TArray<FReflectedTypeDescriptor> Results = CatalogService.Query(Query);
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    JsonArray.Reserve(Results.Num());
    for (const FReflectedTypeDescriptor& D : Results)
    {
        JsonArray.Add(MakeShared<FJsonValueObject>(FResponseSerializer::SerializeTypeDescriptor(D)));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("types"), JsonArray);
    Data->SetNumberField(TEXT("total_count"), CatalogService.GetCachedTypeCount());
    return FResponseSerializer::CreateSuccessResponse(Data);
}

// Helper to parse canonical type path string into FTopLevelAssetPath
static bool ParseTopLevelAssetPathString(const FString& In, FTopLevelAssetPath& Out)
{
    FString Trimmed = In.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        Out = FTopLevelAssetPath();
        return false;
    }
    int32 DotIdx = INDEX_NONE;
    if (!Trimmed.FindLastChar(TEXT('.'), DotIdx) || DotIdx <= 0 || DotIdx >= Trimmed.Len() - 1)
    {
        // If no dot, allow full path for native types like /Script/Engine.Actor (which includes a dot)
        // If still no dot, fail
        return false;
    }
    const FString Package = Trimmed.Left(DotIdx);
    const FString Asset = Trimmed.Mid(DotIdx + 1);
    Out = FTopLevelAssetPath(*Package, *Asset);
    return true;
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleCreate(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    }

    const TSharedPtr<FJsonObject>* VariableConfigObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("variable_config"), VariableConfigObj))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'variable_config' object"));
    }

    FVariableDefinition Def;
    FString VarNameStr;
    // variable_name can be inside variable_config or at top-level for convenience
    if (!(*VariableConfigObj)->TryGetStringField(TEXT("variable_name"), VarNameStr))
    {
        Params->TryGetStringField(TEXT("variable_name"), VarNameStr);
    }
    if (VarNameStr.IsEmpty())
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("VARIABLE_NAME_MISSING"), TEXT("'variable_name' is required"));
    }
    Def.VariableName = FName(*VarNameStr);

    // Require canonical 'type_path'
    FString TypePathStr;
    FTopLevelAssetPath TypePath;
    if (!(*VariableConfigObj)->TryGetStringField(TEXT("type_path"), TypePathStr) || TypePathStr.TrimStartAndEnd().IsEmpty())
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("TYPE_PATH_REQUIRED"), TEXT("'variable_config.type_path' must be provided and canonical"));
    }
    if (!ParseTopLevelAssetPathString(TypePathStr, TypePath))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("TYPE_PATH_INVALID"), FString::Printf(TEXT("Invalid type_path '%s'"), *TypePathStr));
    }
    Def.TypePath = TypePath;

    // Optional container
    const TSharedPtr<FJsonObject>* ContainerObj = nullptr;
    if ((*VariableConfigObj)->TryGetObjectField(TEXT("container"), ContainerObj))
    {
        (*ContainerObj)->TryGetStringField(TEXT("kind"), Def.Container.Kind);
        (*ContainerObj)->TryGetStringField(TEXT("key_type_path"), Def.Container.KeyTypePath);
        (*ContainerObj)->TryGetStringField(TEXT("value_type_path"), Def.Container.ValueTypePath);
    }
    (*VariableConfigObj)->TryGetStringField(TEXT("category"), Def.Category);
    (*VariableConfigObj)->TryGetStringField(TEXT("tooltip"), Def.Tooltip);
    (*VariableConfigObj)->TryGetStringField(TEXT("default_value"), Def.DefaultValueString);

    const TSharedPtr<FJsonObject>* MetadataObj = nullptr;
    if ((*VariableConfigObj)->TryGetObjectField(TEXT("metadata"), MetadataObj))
    {
        for (const auto& Pair : (*MetadataObj)->Values)
        {
            FString ValStr;
            if (Pair.Value->TryGetString(ValStr))
            {
                Def.MetadataMap.Add(Pair.Key, ValStr);
            }
        }
    }

    bool bTmp = false;
    if ((*VariableConfigObj)->TryGetBoolField(TEXT("is_private"), bTmp)) Def.bPrivate = bTmp;
    if ((*VariableConfigObj)->TryGetBoolField(TEXT("is_expose_on_spawn"), bTmp)) Def.bExposeOnSpawn = bTmp;

    FString Err;
    UBlueprint* BP = FindBlueprint(BlueprintName, Err);
    if (!BP)
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName); }
        return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err);
    }
    if (!VariableService.CreateOrUpdateVariable(BP, Def, Err))
    {
        if (Err.IsEmpty()) { Err = TEXT("Failed to create/update variable"); }
        return FResponseSerializer::CreateErrorResponse(TEXT("CREATE_FAILED"), Err);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetObjectField(TEXT("variable"), FResponseSerializer::SerializeVariableDefinition(Def));
    return FResponseSerializer::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleDelete(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    }
    FString VarNameStr;
    if (!Params->TryGetStringField(TEXT("variable_name"), VarNameStr) || VarNameStr.IsEmpty())
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("VARIABLE_NAME_MISSING"), TEXT("'variable_name' is required"));
    }
    FString Err;
    UBlueprint* BP = FindBlueprint(BlueprintName, Err);
    if (!BP)
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName); }
        return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err);
    }
    if (!VariableService.DeleteVariable(BP, FName(*VarNameStr), Err))
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Failed to delete variable '%s'"), *VarNameStr); }
        return FResponseSerializer::CreateErrorResponse(TEXT("DELETE_FAILED"), Err);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetStringField(TEXT("variable_name"), VarNameStr);
    return FResponseSerializer::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleList(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    }
    FString Err;
    UBlueprint* BP = FindBlueprint(BlueprintName, Err);
    if (!BP)
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName); }
        return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err);
    }
    const TArray<FBPVariableDescription*> Vars = VariableService.GetAllVariables(BP);
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Reserve(Vars.Num());
    for (const FBPVariableDescription* V : Vars)
    {
        FVariableDefinition D = VariableService.BPVariableToDefinition(*V);
        Arr.Add(MakeShared<FJsonValueObject>(FResponseSerializer::SerializeVariableDefinition(D)));
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetArrayField(TEXT("variables"), Arr);
    return FResponseSerializer::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleGetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    }
    FString VarNameStr;
    if (!Params->TryGetStringField(TEXT("variable_name"), VarNameStr) || VarNameStr.IsEmpty())
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("VARIABLE_NAME_MISSING"), TEXT("'variable_name' is required"));
    }
    FString Err;
    UBlueprint* BP = FindBlueprint(BlueprintName, Err);
    if (!BP)
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err);
    }
    FVariableDefinition Def;
    if (!VariableService.GetVariableInfo(BP, FName(*VarNameStr), Def, Err))
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Variable '%s' not found"), *VarNameStr); }
        return FResponseSerializer::CreateErrorResponse(TEXT("VARIABLE_NOT_FOUND"), Err);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetObjectField(TEXT("variable"), FResponseSerializer::SerializeVariableDefinition(Def));
    return FResponseSerializer::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleModify(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    }
    const TSharedPtr<FJsonObject>* VariableConfigObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("variable_config"), VariableConfigObj))
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'variable_config' object"));
    }
    FString VarNameStr;
    if (!(*VariableConfigObj)->TryGetStringField(TEXT("variable_name"), VarNameStr))
    {
        Params->TryGetStringField(TEXT("variable_name"), VarNameStr);
    }
    if (VarNameStr.IsEmpty())
    {
        return FResponseSerializer::CreateErrorResponse(TEXT("VARIABLE_NAME_MISSING"), TEXT("'variable_name' is required"));
    }

    FString Err;
    UBlueprint* BP = FindBlueprint(BlueprintName, Err);
    if (!BP)
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName); }
        return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err);
    }

    // Start from current info as baseline
    FVariableDefinition Def;
    if (!VariableService.GetVariableInfo(BP, FName(*VarNameStr), Def, Err))
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Variable '%s' not found"), *VarNameStr); }
        return FResponseSerializer::CreateErrorResponse(TEXT("VARIABLE_NOT_FOUND"), Err);
    }

    // Apply incoming changes
    FString TypePathStr;
    if ((*VariableConfigObj)->TryGetStringField(TEXT("type_path"), TypePathStr) && !TypePathStr.TrimStartAndEnd().IsEmpty())
    {
        FTopLevelAssetPath NewTypePath;
        if (!ParseTopLevelAssetPathString(TypePathStr, NewTypePath))
        {
            return FResponseSerializer::CreateErrorResponse(TEXT("TYPE_PATH_INVALID"), FString::Printf(TEXT("Invalid type_path '%s'"), *TypePathStr));
        }
        Def.TypePath = NewTypePath;
    }
    const TSharedPtr<FJsonObject>* ContainerObj = nullptr;
    if ((*VariableConfigObj)->TryGetObjectField(TEXT("container"), ContainerObj))
    {
        (*ContainerObj)->TryGetStringField(TEXT("kind"), Def.Container.Kind);
        (*ContainerObj)->TryGetStringField(TEXT("key_type_path"), Def.Container.KeyTypePath);
        (*ContainerObj)->TryGetStringField(TEXT("value_type_path"), Def.Container.ValueTypePath);
    }
    (*VariableConfigObj)->TryGetStringField(TEXT("category"), Def.Category);
    (*VariableConfigObj)->TryGetStringField(TEXT("tooltip"), Def.Tooltip);
    const TSharedPtr<FJsonObject>* MetadataObj = nullptr;
    if ((*VariableConfigObj)->TryGetObjectField(TEXT("metadata"), MetadataObj))
    {
        for (const auto& Pair : (*MetadataObj)->Values)
        {
            FString ValStr;
            if (Pair.Value->TryGetString(ValStr))
            {
                Def.MetadataMap.Add(Pair.Key, ValStr);
            }
        }
    }
    bool bTmp = false;
    if ((*VariableConfigObj)->TryGetBoolField(TEXT("is_private"), bTmp)) Def.bPrivate = bTmp;
    if ((*VariableConfigObj)->TryGetBoolField(TEXT("is_expose_on_spawn"), bTmp)) Def.bExposeOnSpawn = bTmp;

    if (!VariableService.CreateOrUpdateVariable(BP, Def, Err))
    {
        if (Err.IsEmpty()) { Err = TEXT("Failed to modify variable"); }
        return FResponseSerializer::CreateErrorResponse(TEXT("MODIFY_FAILED"), Err);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetObjectField(TEXT("variable"), FResponseSerializer::SerializeVariableDefinition(Def));
    return FResponseSerializer::CreateSuccessResponse(Data);
}

// Unused handlers (still unimplemented)
TSharedPtr<FJsonObject> FBlueprintVariableService::HandleGetProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName; if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    FString Path; if (!Params->TryGetStringField(TEXT("path"), Path) && !Params->TryGetStringField(TEXT("property_path"), Path)) return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'path' or 'property_path'"));
    FString Err; UBlueprint* BP = FindBlueprint(BlueprintName, Err); if (!BP) { if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName); } return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err); }
    FResolvedProperty RP = PropertyService.ResolveProperty(BP, Path, Err);
    if (!RP.bIsValid)
    {
        if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Failed to resolve property path '%s'"), *Path); }
        return FResponseSerializer::CreateErrorResponse(TEXT("RESOLVE_FAILED"), Err);
    }
    TSharedPtr<FJsonValue> Val; if (!PropertyService.GetPropertyValue(RP, Val, Err)) { if (Err.IsEmpty()) { Err = TEXT("Failed to get property value"); } return FResponseSerializer::CreateErrorResponse(TEXT("GET_FAILED"), Err); }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetStringField(TEXT("path"), Path);
    Data->SetField(TEXT("value"), Val);
    return FResponseSerializer::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> FBlueprintVariableService::HandleSetProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName; if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'blueprint_name'"));
    FString Path; if (!Params->TryGetStringField(TEXT("path"), Path) && !Params->TryGetStringField(TEXT("property_path"), Path)) return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'path' or 'property_path'"));
    TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value")); if (!ValueField.IsValid()) return FResponseSerializer::CreateErrorResponse(TEXT("PARAM_MISSING"), TEXT("Missing 'value'"));
    FString Err; UBlueprint* BP = FindBlueprint(BlueprintName, Err); if (!BP) { if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName); } return FResponseSerializer::CreateErrorResponse(TEXT("BLUEPRINT_NOT_FOUND"), Err); }
    FResolvedProperty RP = PropertyService.ResolveProperty(BP, Path, Err);
    if (!RP.bIsValid)
    {
        // Auto-grow top-level arrays for index write like Var[0]
        if (Err.Contains(TEXT("PROPERTY_OUT_OF_RANGE")))
        {
            // Only handle simple top-level pattern: Var[<index>]
            const FString Trimmed = Path.TrimStartAndEnd();
            if (!Trimmed.Contains(TEXT(".")))
            {
                const int32 OpenIdx = Trimmed.Find(TEXT("["));
                const int32 CloseIdx = (OpenIdx != INDEX_NONE) ? Trimmed.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenIdx + 1) : INDEX_NONE;
                if (OpenIdx != INDEX_NONE && CloseIdx != INDEX_NONE && CloseIdx > OpenIdx)
                {
                    const FString VarName = Trimmed.Mid(0, OpenIdx).TrimStartAndEnd();
                    const FString Inside = Trimmed.Mid(OpenIdx + 1, CloseIdx - OpenIdx - 1).TrimStartAndEnd();
                    if (Inside.IsNumeric() && !VarName.IsEmpty() && BP->GeneratedClass)
                    {
                        int32 Index = FCString::Atoi(*Inside);
                        UObject* CDO = BP->GeneratedClass->GetDefaultObject();
                        if (CDO)
                        {
                            FProperty* RootProp = BP->GeneratedClass->FindPropertyByName(FName(*VarName));
                            if (RootProp)
                            {
                                if (FArrayProperty* AP = CastField<FArrayProperty>(RootProp))
                                {
                                    void* RootPtr = RootProp->ContainerPtrToValuePtr<void>(CDO);
                                    FScriptArrayHelper Helper(AP, RootPtr);
                                    // Grow array to fit Index
                                    while (Helper.Num() <= Index)
                                    {
                                        const int32 NewIdx = Helper.AddValue();
                                        void* ElemPtr = Helper.GetRawPtr(NewIdx);
                                        AP->Inner->InitializeValue(ElemPtr);
                                    }
                                    // Retry resolution after growth
                                    Err.Reset();
                                    RP = PropertyService.ResolveProperty(BP, Path, Err);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!RP.bIsValid)
        {
            if (Err.IsEmpty()) { Err = FString::Printf(TEXT("Failed to resolve property path '%s'"), *Path); }
            return FResponseSerializer::CreateErrorResponse(TEXT("RESOLVE_FAILED"), Err);
        }
    }
    if (!PropertyService.SetPropertyValue(RP, ValueField, Err)) { if (Err.IsEmpty()) { Err = TEXT("Failed to set property value"); } return FResponseSerializer::CreateErrorResponse(TEXT("SET_FAILED"), Err); }
    // Compile after change
    FString CompileErr; FCommonUtils::SafeCompileBlueprint(BP, CompileErr);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Data->SetStringField(TEXT("path"), Path);
    if (!CompileErr.IsEmpty()) Data->SetStringField(TEXT("compile_warning"), CompileErr);
    return FResponseSerializer::CreateSuccessResponse(Data);
}
TSharedPtr<FJsonObject> FBlueprintVariableService::HandleGetPropertyMetadata(const TSharedPtr<FJsonObject>& /*Params*/) { return FResponseSerializer::CreateErrorResponse(TEXT("UNIMPL"), TEXT("get_property_metadata not implemented")); }
TSharedPtr<FJsonObject> FBlueprintVariableService::HandleSetPropertyMetadata(const TSharedPtr<FJsonObject>& /*Params*/) { return FResponseSerializer::CreateErrorResponse(TEXT("UNIMPL"), TEXT("set_property_metadata not implemented")); }
TSharedPtr<FJsonObject> FBlueprintVariableService::HandleDiagnostics(const TSharedPtr<FJsonObject>& /*Params*/) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("stats"), CatalogService.GetCacheStats());
    return FResponseSerializer::CreateSuccessResponse(Data);
}

UBlueprint* FBlueprintVariableService::FindBlueprint(const FString& BlueprintName, FString& OutError)
{
    UBlueprint* BP = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!BP)
    {
        OutError = FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName);
    }
    return BP;
}

bool FBlueprintVariableService::ParseRequestParams(const TSharedPtr<FJsonObject>& Params, FString& OutBlueprintName, FString& OutError)
{
    if (!Params->TryGetStringField(TEXT("blueprint_name"), OutBlueprintName))
    {
        OutError = TEXT("Missing 'blueprint_name'");
        return false;
    }
    return true;
}
