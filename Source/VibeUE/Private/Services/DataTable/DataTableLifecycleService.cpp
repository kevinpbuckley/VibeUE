// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/DataTable/DataTableLifecycleService.h"
#include "Services/DataTable/DataTableDiscoveryService.h"
#include "Core/ErrorCodes.h"
#include "Engine/DataTable.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataTableLifecycle, Log, All);

FDataTableLifecycleService::FDataTableLifecycleService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

TResult<UDataTable*> FDataTableLifecycleService::CreateDataTable(
	UScriptStruct* RowStruct,
	const FString& AssetPath,
	const FString& AssetName)
{
	if (!RowStruct)
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row struct is required"));
	}
	
	if (AssetName.IsEmpty())
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Asset name is required"));
	}
	
	// Verify row struct is valid (inherits from FTableRowBase)
	UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
	if (!RowStruct->IsChildOf(TableRowBase))
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::ROW_STRUCT_INVALID,
			FString::Printf(TEXT("%s is not a valid row struct (must inherit from FTableRowBase)"), *RowStruct->GetName()));
	}
	
	// Normalize asset path
	FString NormalizedPath = AssetPath;
	if (NormalizedPath.IsEmpty())
	{
		NormalizedPath = TEXT("/Game/Data");
	}
	
	if (!NormalizedPath.StartsWith(TEXT("/Game")) && 
		!NormalizedPath.StartsWith(TEXT("/Engine")))
	{
		if (NormalizedPath.StartsWith(TEXT("/")))
		{
			NormalizedPath = TEXT("/Game") + NormalizedPath;
		}
		else
		{
			NormalizedPath = TEXT("/Game/") + NormalizedPath;
		}
	}
	
	// Create the data table using asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	// Create factory with the specified row struct
	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;
	
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, NormalizedPath, UDataTable::StaticClass(), Factory);
	
	if (!NewAsset)
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::OPERATION_FAILED,
			FString::Printf(TEXT("Failed to create data table at %s/%s"), *NormalizedPath, *AssetName));
	}
	
	UDataTable* DataTable = Cast<UDataTable>(NewAsset);
	if (!DataTable)
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Created asset is not a data table"));
	}
	
	// Mark dirty
	DataTable->MarkPackageDirty();
	
	UE_LOG(LogDataTableLifecycle, Display, TEXT("Created data table: %s with row struct: %s"), 
		*DataTable->GetPathName(), *RowStruct->GetName());
	
	return TResult<UDataTable*>::Success(DataTable);
}

TResult<UDataTable*> FDataTableLifecycleService::CreateDataTableByStructName(
	const FString& RowStructName,
	const FString& AssetPath,
	const FString& AssetName)
{
	// Use discovery service to find the struct
	FDataTableDiscoveryService DiscoveryService(GetContext());
	
	auto StructResult = DiscoveryService.FindRowStruct(RowStructName);
	if (StructResult.IsError())
	{
		return TResult<UDataTable*>::Error(StructResult.GetErrorCode(), StructResult.GetErrorMessage());
	}
	
	return CreateDataTable(StructResult.GetValue(), AssetPath, AssetName);
}
