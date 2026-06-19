// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/VibeUEMCPToolBridge.h"
#include "Core/ToolRegistry.h"
#include "Core/ToolMetadata.h"

#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	/** Map a VibeUE FToolParameter type string to a JSON Schema type. */
	FString ToJsonSchemaType(const FString& VibeType)
	{
		if (VibeType == TEXT("int"))    { return TEXT("integer"); }
		if (VibeType == TEXT("float"))  { return TEXT("number"); }
		if (VibeType == TEXT("bool"))   { return TEXT("boolean"); }
		if (VibeType == TEXT("object")) { return TEXT("object"); }
		if (VibeType == TEXT("array"))  { return TEXT("array"); }
		return TEXT("string");
	}

	/** Build an MCP JSON Schema object from a tool's parameter metadata. */
	TSharedPtr<FJsonObject> BuildInputSchema(const FToolMetadata& Meta)
	{
		TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Required;

		for (const FToolParameter& Param : Meta.Parameters)
		{
			TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
			Prop->SetStringField(TEXT("type"), ToJsonSchemaType(Param.Type));

			if (!Param.Description.IsEmpty())
			{
				Prop->SetStringField(TEXT("description"), Param.Description);
			}

			if (Param.Type == TEXT("array"))
			{
				TSharedPtr<FJsonObject> Items = MakeShared<FJsonObject>();
				Items->SetStringField(TEXT("type"),
					ToJsonSchemaType(Param.ArrayItemType.IsEmpty() ? TEXT("string") : Param.ArrayItemType));
				Prop->SetObjectField(TEXT("items"), Items);
			}

			if (Param.AllowedValues.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> EnumValues;
				for (const FString& Allowed : Param.AllowedValues)
				{
					EnumValues.Add(MakeShared<FJsonValueString>(Allowed));
				}
				Prop->SetArrayField(TEXT("enum"), EnumValues);
			}

			if (!Param.DefaultValue.IsEmpty())
			{
				Prop->SetStringField(TEXT("default"), Param.DefaultValue);
			}

			Properties->SetObjectField(Param.Name, Prop);

			if (Param.bRequired)
			{
				Required.Add(MakeShared<FJsonValueString>(Param.Name));
			}
		}

		Schema->SetObjectField(TEXT("properties"), Properties);
		if (Required.Num() > 0)
		{
			Schema->SetArrayField(TEXT("required"), Required);
		}
		return Schema;
	}

	/**
	 * Flatten an MCP arguments JSON object into the {name -> string} map VibeUE tools expect.
	 * Scalars become their string form; nested objects/arrays are re-serialized to a JSON string
	 * (VibeUE tools that take structured input expect it as a JSON string, e.g. "ParamsJson").
	 */
	TMap<FString, FString> JsonObjectToArgMap(const TSharedPtr<FJsonObject>& Params)
	{
		TMap<FString, FString> Args;
		if (!Params.IsValid())
		{
			return Args;
		}

		for (const auto& Pair : Params->Values)
		{
			const FString Key = *Pair.Key; // FJsonObject keys are UE::FSharedString in 5.8
			const TSharedPtr<FJsonValue>& Value = Pair.Value;
			if (!Value.IsValid())
			{
				Args.Add(Key, FString());
				continue;
			}

			switch (Value->Type)
			{
			case EJson::String:
				Args.Add(Key, Value->AsString());
				break;
			case EJson::Boolean:
				Args.Add(Key, Value->AsBool() ? TEXT("true") : TEXT("false"));
				break;
			case EJson::Number:
				Args.Add(Key, FString::Printf(TEXT("%.10g"), Value->AsNumber()));
				break;
			case EJson::Null:
				Args.Add(Key, FString());
				break;
			case EJson::Object:
			{
				FString Out;
				const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
					TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
				FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer);
				Args.Add(Key, Out);
				break;
			}
			case EJson::Array:
			{
				FString Out;
				const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
					TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
				FJsonSerializer::Serialize(Value->AsArray(), Writer);
				Args.Add(Key, Out);
				break;
			}
			default:
				Args.Add(Key, FString());
				break;
			}
		}
		return Args;
	}

	/** Adapter exposing one FToolRegistry tool as a top-level MCP tool. */
	struct FVibeUEToolAdapter : IModelContextProtocolTool
	{
		explicit FVibeUEToolAdapter(const FToolMetadata& Meta)
			: Name(Meta.Name)
			, Description(Meta.Description)
			, InputSchema(BuildInputSchema(Meta))
		{
		}

		virtual FString GetName() const override { return Name; }
		virtual FString GetDescription() const override { return Description; }
		virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override { return InputSchema; }

		virtual void RunAsync(const FModelContextProtocolToolRequestId& /*RequestId*/,
			const TSharedPtr<FJsonObject>& Params,
			const FResultCallback& OnComplete) override
		{
			const FString ToolName = Name;
			TMap<FString, FString> Args = JsonObjectToArgMap(Params);

			auto Execute = [ToolName, Args = MoveTemp(Args), OnComplete]()
			{
				const FString Result = FToolRegistry::Get().ExecuteTool(ToolName, Args);

				// VibeUE tools report failure as {"success": false, ...}; surface that as an MCP error.
				bool bIsError = false;
				TSharedPtr<FJsonObject> ResultObj;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result);
				if (FJsonSerializer::Deserialize(Reader, ResultObj) && ResultObj.IsValid())
				{
					bool bSuccess = true;
					if (ResultObj->TryGetBoolField(TEXT("success"), bSuccess) && !bSuccess)
					{
						bIsError = true;
					}
				}

				OnComplete(bIsError
					? UE::ModelContextProtocol::MakeErrorResult(Result)
					: UE::ModelContextProtocol::MakeTextResult(Result));
			};

			// VibeUE tools must run on the game thread.
			if (IsInGameThread())
			{
				Execute();
			}
			else
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(Execute));
			}
		}

	private:
		FString Name;
		FString Description;
		TSharedPtr<FJsonObject> InputSchema;
	};

	/** Tools we registered, so we can remove exactly those on shutdown. */
	TArray<TSharedRef<IModelContextProtocolTool>> GRegisteredTools;
}

namespace VibeUEMCPToolBridge
{
	void RegisterAll()
	{
		IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
		if (!Module)
		{
			UE_LOG(LogToolRegistry, Warning,
				TEXT("VibeUE: ModelContextProtocol module not available; tools not exposed on MCP endpoint."));
			return;
		}

		int32 Registered = 0;
		int32 Skipped = 0;
		for (const FToolMetadata& Meta : FToolRegistry::Get().GetAllTools())
		{
			// Internal-only and editor-testing tools are not exposed to external MCP clients.
			if (Meta.bInternalOnly || Meta.bEditorTestingOnly)
			{
				++Skipped;
				continue;
			}

			TSharedRef<IModelContextProtocolTool> Tool = MakeShared<FVibeUEToolAdapter>(Meta);
			if (Module->AddTool(Tool))
			{
				GRegisteredTools.Add(Tool);
				++Registered;
			}
			else
			{
				UE_LOG(LogToolRegistry, Warning,
					TEXT("VibeUE: MCP tool '%s' was rejected (name collision or invalid name)."), *Meta.Name);
			}
		}

		UE_LOG(LogToolRegistry, Display,
			TEXT("VibeUE: exposed %d tool(s) on Epic's MCP endpoint (%d internal/testing tools skipped)."),
			Registered, Skipped);
	}

	void UnregisterAll()
	{
		if (IModelContextProtocolModule* Module = IModelContextProtocolModule::Get())
		{
			for (const TSharedRef<IModelContextProtocolTool>& Tool : GRegisteredTools)
			{
				Module->RemoveTool(Tool);
			}
		}
		GRegisteredTools.Empty();
	}
}
