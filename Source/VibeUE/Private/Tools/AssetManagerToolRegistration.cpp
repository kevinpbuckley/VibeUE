// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "PythonAPI/UAssetDiscoveryService.h"
#include "Utils/HelpFileReader.h"
#include "AssetRegistry/AssetData.h"
#include "Json.h"
#include "JsonUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogAssetManagerTool, Log, All);

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

static FString ExtractParam(const TMap<FString, FString>& Params, const FString& FieldName)
{
	const FString* Direct = Params.Find(FieldName);
	if (Direct) { return *Direct; }

	FString Cap = FieldName;
	if (Cap.Len() > 0) { Cap[0] = FChar::ToUpper(Cap[0]); }
	const FString* Capitalized = Params.Find(Cap);
	if (Capitalized) { return *Capitalized; }

	return FString();
}

static bool ExtractBool(const TMap<FString, FString>& Params, const FString& FieldName, bool Default = false)
{
	FString Val = ExtractParam(Params, FieldName);
	if (Val.IsEmpty()) { return Default; }
	return Val.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Val.Equals(TEXT("1"));
}

/**
 * When MCPServer detects an 'action' key it transforms flat params into
 * {Action, ParamsJson} before the tool sees them.  This helper re-expands
 * the nested ParamsJson string back into a flat TMap so the action handlers
 * can use ExtractParam() normally.
 */
static TMap<FString, FString> FlattenParams(const TMap<FString, FString>& Params)
{
	TMap<FString, FString> Flat = Params;
	const FString* ParamsJson = Params.Find(TEXT("ParamsJson"));
	if (!ParamsJson || ParamsJson->IsEmpty()) { return Flat; }

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid()) { return Flat; }

	for (const auto& Pair : JsonObj->Values)
	{
		if (!Pair.Value.IsValid()) { continue; }
		if (Pair.Value->Type == EJson::String)
		{
			Flat.Add(Pair.Key, Pair.Value->AsString());
		}
		else if (Pair.Value->Type == EJson::Boolean)
		{
			Flat.Add(Pair.Key, Pair.Value->AsBool() ? TEXT("true") : TEXT("false"));
		}
		else if (Pair.Value->Type == EJson::Number)
		{
			Flat.Add(Pair.Key, FString::Printf(TEXT("%.10g"), Pair.Value->AsNumber()));
		}
		else
		{
			FString ValStr;
			if (Pair.Value->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> Obj = Pair.Value->AsObject();
				if (Obj.IsValid())
				{
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
						TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ValStr);
					FJsonSerializer::Serialize(Obj.ToSharedRef(), *W);
				}
			}
			else if (Pair.Value->Type == EJson::Array)
			{
				TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
					TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ValStr);
				FJsonSerializer::Serialize(Pair.Value->AsArray(), *W);
			}
			// Null/None → leave ValStr empty
			Flat.Add(Pair.Key, ValStr);
		}
	}
	return Flat;
}

/** Serialize a single FAssetData into a JSON object */
static TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& Asset)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("asset_name"),   Asset.AssetName.ToString());
	Obj->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
	Obj->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
	Obj->SetStringField(TEXT("asset_class"),  Asset.AssetClassPath.GetAssetName().ToString());
	Obj->SetStringField(TEXT("object_path"),  Asset.GetObjectPathString());
	return Obj;
}

/** Serialize an array of FAssetData to a JSON array */
static TArray<TSharedPtr<FJsonValue>> AssetArrayToJson(const TArray<FAssetData>& Assets)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	Out.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		Out.Add(MakeShared<FJsonValueObject>(AssetDataToJson(Asset)));
	}
	return Out;
}

/** Serialize a JSON object or array to a string */
static FString SerializeJson(const TSharedPtr<FJsonObject>& Obj)
{
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return Out;
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

static FString Action_Search(const TMap<FString, FString>& Params)
{
	FString SearchTerm = ExtractParam(Params, TEXT("search_term"));
	FString AssetType  = ExtractParam(Params, TEXT("asset_type"));

	TArray<FAssetData> Results = UAssetDiscoveryService::SearchAssets(SearchTerm, AssetType);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetNumberField(TEXT("count"), Results.Num());
	Response->SetStringField(TEXT("search_term"), SearchTerm);
	Response->SetStringField(TEXT("asset_type"),  AssetType);
	Response->SetArrayField(TEXT("assets"), AssetArrayToJson(Results));
	return SerializeJson(Response);
}

static FString Action_Find(const TMap<FString, FString>& Params)
{
	FString AssetPath = ExtractParam(Params, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'asset_path' is required for action 'find'"));
		return SerializeJson(Err);
	}

	FAssetData FoundAsset;
	bool bFound = UAssetDiscoveryService::FindAssetByPath(AssetPath, FoundAsset);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetBoolField(TEXT("found"),   bFound);
	if (bFound)
	{
		Response->SetObjectField(TEXT("asset"), AssetDataToJson(FoundAsset));
	}
	else
	{
		Response->SetStringField(TEXT("message"), FString::Printf(
			TEXT("No asset found at path: %s"), *AssetPath));
	}
	return SerializeJson(Response);
}

static FString Action_List(const TMap<FString, FString>& Params)
{
	FString Path      = ExtractParam(Params, TEXT("path"));
	FString AssetType = ExtractParam(Params, TEXT("asset_type"));

	if (Path.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'path' is required for action 'list'"));
		return SerializeJson(Err);
	}

	TArray<FAssetData> Results = UAssetDiscoveryService::ListAssetsInPath(Path, AssetType);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetNumberField(TEXT("count"), Results.Num());
	Response->SetStringField(TEXT("path"),       Path);
	Response->SetStringField(TEXT("asset_type"), AssetType);
	Response->SetArrayField(TEXT("assets"), AssetArrayToJson(Results));
	return SerializeJson(Response);
}

static FString Action_Open(const TMap<FString, FString>& Params)
{
	FString AssetPath = ExtractParam(Params, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'asset_path' is required for action 'open'"));
		return SerializeJson(Err);
	}

	bool bOk = UAssetDiscoveryService::OpenAsset(AssetPath);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"),    bOk);
	Response->SetStringField(TEXT("asset_path"), AssetPath);
	if (!bOk)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(
			TEXT("Failed to open asset: %s"), *AssetPath));
	}
	return SerializeJson(Response);
}

static FString Action_Save(const TMap<FString, FString>& Params)
{
	FString AssetPath = ExtractParam(Params, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'asset_path' is required for action 'save'"));
		return SerializeJson(Err);
	}

	bool bOk = UAssetDiscoveryService::SaveAsset(AssetPath);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"),      bOk);
	Response->SetStringField(TEXT("asset_path"), AssetPath);
	if (!bOk)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(
			TEXT("Failed to save asset: %s"), *AssetPath));
	}
	return SerializeJson(Response);
}

static FString Action_SaveAll(const TMap<FString, FString>& Params)
{
	int32 SavedCount = UAssetDiscoveryService::SaveAllAssets();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"),     true);
	Response->SetNumberField(TEXT("saved_count"), SavedCount);
	Response->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Saved %d dirty assets"), SavedCount));
	return SerializeJson(Response);
}

static FString Action_Duplicate(const TMap<FString, FString>& Params)
{
	FString SourcePath      = ExtractParam(Params, TEXT("source_path"));
	FString DestinationPath = ExtractParam(Params, TEXT("destination_path"));

	if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'source_path' and 'destination_path' are required for action 'duplicate'"));
		return SerializeJson(Err);
	}

	bool bOk = UAssetDiscoveryService::DuplicateAsset(SourcePath, DestinationPath);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"),          bOk);
	Response->SetStringField(TEXT("source_path"),      SourcePath);
	Response->SetStringField(TEXT("destination_path"), DestinationPath);
	if (!bOk)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(
			TEXT("Failed to duplicate '%s' to '%s'"), *SourcePath, *DestinationPath));
	}
	return SerializeJson(Response);
}

static FString Action_Move(const TMap<FString, FString>& Params)
{
	FString SourcePath      = ExtractParam(Params, TEXT("source_path"));
	FString DestinationPath = ExtractParam(Params, TEXT("destination_path"));

	if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'source_path' and 'destination_path' are required for action 'move'"));
		return SerializeJson(Err);
	}

	bool bOk = UAssetDiscoveryService::MoveAsset(SourcePath, DestinationPath);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), bOk);
	Response->SetStringField(TEXT("source_path"), SourcePath);
	Response->SetStringField(TEXT("destination_path"), DestinationPath);
	if (!bOk)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(
			TEXT("Failed to move '%s' to '%s'"), *SourcePath, *DestinationPath));
	}
	return SerializeJson(Response);
}

static FString Action_Delete(const TMap<FString, FString>& Params)
{
	FString AssetPath        = ExtractParam(Params, TEXT("asset_path"));
	bool    bSkipRefCheck    = ExtractBool(Params, TEXT("skip_reference_check"), false);

	if (AssetPath.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), TEXT("'asset_path' is required for action 'delete'"));
		return SerializeJson(Err);
	}

	// Safety: check references first unless caller explicitly skips
	if (!bSkipRefCheck)
	{
		TArray<FString> Refs = UAssetDiscoveryService::GetAssetReferencers(AssetPath);
		if (Refs.Num() > 0)
		{
			TSharedPtr<FJsonObject> BlockedResponse = MakeShared<FJsonObject>();
			BlockedResponse->SetBoolField(TEXT("success"), false);
			BlockedResponse->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Asset '%s' is referenced by %d other assets. "
					"Pass skip_reference_check=true to delete anyway, or remove references first."),
					*AssetPath, Refs.Num()));
			TArray<TSharedPtr<FJsonValue>> RefArray;
			for (const FString& Ref : Refs) { RefArray.Add(MakeShared<FJsonValueString>(Ref)); }
			BlockedResponse->SetArrayField(TEXT("referencers"), RefArray);
			return SerializeJson(BlockedResponse);
		}
	}

	bool bOk = UAssetDiscoveryService::DeleteAsset(AssetPath);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"),      bOk);
	Response->SetStringField(TEXT("asset_path"), AssetPath);
	if (!bOk)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(
			TEXT("Failed to delete asset: %s"), *AssetPath));
	}
	return SerializeJson(Response);
}

static FString Action_Help(const TMap<FString, FString>& Params)
{
	FString HelpAction = ExtractParam(Params, TEXT("help_action"));

	TSharedPtr<FJsonObject> HelpResponse;
	if (!HelpAction.IsEmpty())
	{
		HelpResponse = FHelpFileReader::GetActionHelp(TEXT("manage_asset"), HelpAction.ToLower());
	}
	else
	{
		HelpResponse = FHelpFileReader::GetToolHelp(TEXT("manage_asset"));
	}

	return SerializeJson(HelpResponse);
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

REGISTER_VIBEUE_TOOL(manage_asset,
	"PREFERRED tool for all asset operations — use this instead of Python AssetDiscoveryService calls. "
	"Manages Unreal Engine assets: search by name/type, find by exact path, list by folder, open in editor, "
	"save, save_all, duplicate, move, or delete. "
	"\n\nWORKFLOW: Always search/find FIRST to confirm the exact path before editing.\n"
	"  User says 'BP_Player' -> manage_asset(action='search', search_term='BP_Player', asset_type='Blueprint')\n"
	"  Never guess paths. Use the object_path from results as asset_path in subsequent calls.\n"
	"\nCOMMON PATTERNS:\n"
	"  Find by partial name : action='search', search_term='BP_Enemy', asset_type='Blueprint'\n"
	"  Confirm exact path   : action='find',   asset_path='/Game/AI/ST_Cube'\n"
	"  List folder contents : action='list',   path='/Game/AI'\n"
	"  Open in editor       : action='open',   asset_path='/Game/AI/ST_Cube'\n"
	"  Save after edits     : action='save',   asset_path='/Game/AI/ST_Cube'\n"
	"  Save all dirty       : action='save_all'\n"
	"  Duplicate asset      : action='duplicate', source_path='...', destination_path='...'  (for true copies only)\n"
	"  Move or rename asset : action='move', source_path='...', destination_path='...'  (preserves references)\n"
	"  Delete (guarded)     : action='delete', asset_path='...'  (blocked if referenced; use skip_reference_check=true to override)\n"
	"\nNever emulate a move by duplicating then deleting the original. That creates a different asset and can break references.\n"
	"  Per-action docs      : action='help', help_action='search'\n"
	"\nAll paths use Content Browser format: /Game/... or /Engine/...",
	"Assets",
	TOOL_PARAMS(
		TOOL_PARAM("action",
			"Action to perform: 'search', 'find', 'list', 'open', 'save', 'save_all', 'duplicate', 'move', 'delete', 'help'",
			"string", true),
		TOOL_PARAM("asset_path",
			"Content Browser path to a single asset (e.g. /Game/Blueprints/BP_Player). "
			"Required for: find, open, save, delete.",
			"string", false),
		TOOL_PARAM("search_term",
			"Partial name to match against asset names (case-insensitive). Used with action='search'.",
			"string", false),
		TOOL_PARAM("asset_type",
			"Asset class filter: Blueprint, StaticMesh, Texture2D, Material, AnimSequence, etc. "
			"Optional for: search, list.",
			"string", false),
		TOOL_PARAM("path",
			"Content Browser folder path to list recursively (e.g. /Game/AI). Used with action='list'.",
			"string", false),
		TOOL_PARAM("source_path",
			"Source asset path for action='duplicate' or action='move'.",
			"string", false),
		TOOL_PARAM("destination_path",
			"Destination asset path for action='duplicate' or action='move'.",
			"string", false),
		TOOL_PARAM("skip_reference_check",
			"If true, skip the reference check before delete (default false). Used with action='delete'.",
			"boolean", false),
		TOOL_PARAM("help_action",
			"Specific action name to get detailed help for. Used with action='help'.",
			"string", false)
	),
	{
		// MCPServer transforms flat {action, ...} into {Action, ParamsJson} before dispatch.
		// Re-expand ParamsJson so action handlers can use ExtractParam() normally.
		// Note: use 'auto' here — TMap<FString,FString> contains a comma that confuses the
		// C preprocessor's macro argument parser (braces don't count as nesting).
		auto FlatParams = FlattenParams(Params);
		FString Action = ExtractParam(FlatParams, TEXT("action")).ToLower().TrimStartAndEnd();

		if      (Action == TEXT("search"))   { return Action_Search(FlatParams);   }
		else if (Action == TEXT("find"))     { return Action_Find(FlatParams);     }
		else if (Action == TEXT("list"))     { return Action_List(FlatParams);     }
		else if (Action == TEXT("open"))     { return Action_Open(FlatParams);     }
		else if (Action == TEXT("save"))     { return Action_Save(FlatParams);     }
		else if (Action == TEXT("save_all")) { return Action_SaveAll(FlatParams);  }
		else if (Action == TEXT("duplicate")){ return Action_Duplicate(FlatParams);}
		else if (Action == TEXT("move"))     { return Action_Move(FlatParams);     }
		else if (Action == TEXT("delete"))   { return Action_Delete(FlatParams);   }
		else if (Action == TEXT("help"))     { return Action_Help(FlatParams);     }
		else
		{
			TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
			Err->SetBoolField(TEXT("success"), false);
			Err->SetStringField(TEXT("error"), FString::Printf(
				TEXT("Unknown action '%s'. Valid actions: search, find, list, open, save, save_all, duplicate, move, delete, help"),
				*Action));
			FString Out;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
			FJsonSerializer::Serialize(Err.ToSharedRef(), Writer);
			return Out;
		}
	}
);
