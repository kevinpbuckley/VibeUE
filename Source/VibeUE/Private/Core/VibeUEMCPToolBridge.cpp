// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/VibeUEMCPToolBridge.h"
#include "Core/ToolRegistry.h"
#include "Core/ToolMetadata.h"

#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"
#include "ModelContextProtocolSession.h" // FModelContextProtocolToolRequestId, for CancelAsync

#include "Async/Async.h"
#include "Core/VibeUEToolCancel.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// One current-call cancel token per thread (see Core/VibeUEToolCancel.h)
FVibeUEToolCancel*& FVibeUEToolCancel::CurrentSlot()
{
	static thread_local FVibeUEToolCancel* Current = nullptr;
	return Current;
}

namespace
{
	/** Map a VibeUE FToolParameter type string to a JSON Schema type. */
	FString ToJsonSchemaType(const FString& VibeType)
	{
		if (VibeType == TEXT("int"))    { return TEXT("integer"); }
		if (VibeType == TEXT("float"))  { return TEXT("number"); }
		// Tools also declare the JSON Schema names themselves (capture_image max_width, deep_research lat/lng)
		if (VibeType == TEXT("number") || VibeType == TEXT("integer") || VibeType == TEXT("boolean")) { return VibeType; }
		if (VibeType == TEXT("bool"))   { return TEXT("boolean"); }
		if (VibeType == TEXT("object")) { return TEXT("object"); }
		if (VibeType == TEXT("array"))  { return TEXT("array"); }
		return TEXT("string");
	}

	/**
	 * Tools that may run on a worker thread. Only tools that touch no UObject and no editor
	 * state qualify: deep_research and terrain_data make HTTP requests (started on the game thread, see
	 * DeepResearchTools.cpp and TerrainDataTools.cpp), parse text and write files. On the game thread they blocked
	 * the editor for up to 45 s.
	 */
	bool RunsOffGameThread(const FString& ToolName)
	{
		return ToolName == TEXT("deep_research") || ToolName == TEXT("terrain_data");
	}

	/**
	 * The running off-game-thread calls, by request id, for CancelAsync and the exit sweep. A multimap:
	 * the id carries no session, so two clients' calls can share one, and neither may drop out of the table (a cancel
	 * with that id reaches both).
	 */
	FCriticalSection GRunningCallsLock;
	TMultiMap<FString, TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe>> GRunningCalls;

	FString RequestKey(const FModelContextProtocolToolRequestId& RequestId)
	{
		// The id's own operator== and hash are not exported; its JSON text is what Epic hashes too.
		return RequestId.RequestId.IsValid() ? RequestId.RequestId->AsString() : FString();
	}

	/** Turn a tool's JSON string into the MCP result and hand it over (safe from any thread). */
	void DeliverResult(const FString& Result, const IModelContextProtocolTool::FResultCallback& OnComplete)
	{
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

		// A tool can return one image alongside its JSON by embedding a reserved
		// "vibeue_image": {"mime_type", "base64"} field — surfaced here as a real MCP image
		// content block so clients render the picture instead of receiving a megabyte of
		// base64 inside a text block (issue #544).
		if (!bIsError && ResultObj.IsValid())
		{
			const TSharedPtr<FJsonObject>* ImageObj = nullptr;
			FString MimeType, Base64;
			if (ResultObj->TryGetObjectField(TEXT("vibeue_image"), ImageObj) && ImageObj && ImageObj->IsValid() &&
				(*ImageObj)->TryGetStringField(TEXT("mime_type"), MimeType) &&
				(*ImageObj)->TryGetStringField(TEXT("base64"), Base64) && !Base64.IsEmpty())
			{
				ResultObj->RemoveField(TEXT("vibeue_image"));
				FString TextPart;
				const TSharedRef<TJsonWriter<>> TextWriter = TJsonWriterFactory<>::Create(&TextPart);
				FJsonSerializer::Serialize(ResultObj.ToSharedRef(), TextWriter);

				TSharedPtr<FJsonObject> ImageContent = MakeShared<FJsonObject>();
				ImageContent->SetStringField(TEXT("type"), TEXT("image"));
				ImageContent->SetStringField(TEXT("data"), Base64);
				ImageContent->SetStringField(TEXT("mimeType"), MimeType);

				TArray<TSharedPtr<FJsonValue>> Content;
				Content.Add(MakeShared<FJsonValueObject>(UE::ModelContextProtocol::MakeTextContentObject(TextPart)));
				Content.Add(MakeShared<FJsonValueObject>(ImageContent));

				TSharedPtr<FJsonObject> ResultRoot = MakeShared<FJsonObject>();
				ResultRoot->SetArrayField(TEXT("content"), Content);
				OnComplete(FModelContextProtocolToolResult(ResultRoot));
				return;
			}
		}

		OnComplete(bIsError
			? UE::ModelContextProtocol::MakeErrorResult(Result)
			: UE::ModelContextProtocol::MakeTextResult(Result));
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

		virtual void RunAsync(const FModelContextProtocolToolRequestId& RequestId,
			const TSharedPtr<FJsonObject>& Params,
			const FResultCallback& OnComplete) override
		{
			const FString ToolName = Name;
			TMap<FString, FString> Args = JsonObjectToArgMap(Params);

			// Checks on the game thread, the work on a worker thread, the result from there
			// (OnComplete may be called from any thread, IModelContextProtocolTool.h). The editor keeps ticking
			// while the tool waits, and CancelAsync can wake it.
			if (RunsOffGameThread(ToolName))
			{
				auto Launch = [ToolName, Args = MoveTemp(Args), OnComplete, Key = RequestKey(RequestId)]() mutable
				{
					FToolExecuteFunc Func;
					FString ErrorJson;
					if (!FToolRegistry::Get().PrepareToolCall(ToolName, Args, Func, ErrorJson))
					{
						DeliverResult(ErrorJson, OnComplete);
						return;
					}
					TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe> Cancel = MakeShared<FVibeUEToolCancel, ESPMode::ThreadSafe>();
					{
						FScopeLock Guard(&GRunningCallsLock);
						GRunningCalls.Add(Key, Cancel);
					}
					// The queued thread pool, not a task-graph worker: the tool may wait up to 45 s.
					Async(EAsyncExecution::ThreadPool,
						[Func = MoveTemp(Func), Args = MoveTemp(Args), OnComplete, Key, Cancel]()
					{
						FString Result;
						{
							FVibeUEToolCancel::FScope CurrentCall(&Cancel.Get());
							Result = Func(Args);
						}
						{
							FScopeLock Guard(&GRunningCallsLock);
							GRunningCalls.RemoveSingle(Key, Cancel); // Only this call's entry
						}
						DeliverResult(Result, OnComplete);
					});
				};
				if (IsInGameThread())
				{
					Launch();
				}
				else
				{
					AsyncTask(ENamedThreads::GameThread, MoveTemp(Launch));
				}
				return;
			}

			auto Execute = [ToolName, Args = MoveTemp(Args), OnComplete]()
			{
				DeliverResult(FToolRegistry::Get().ExecuteTool(ToolName, Args), OnComplete);
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

		// Epic's server calls this on notifications/cancelled; it wakes an off-game-thread call.
		virtual void CancelAsync(const FModelContextProtocolToolRequestId& RequestId) override
		{
			TArray<TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe>> Cancels;
			{
				FScopeLock Guard(&GRunningCallsLock);
				GRunningCalls.MultiFind(RequestKey(RequestId), Cancels); // Every call with this id
			}
			for (const TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe>& Cancel : Cancels)
			{
				Cancel->Cancel();
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

	// At editor exit, wake every worker still waiting on a request, so the thread pool's shutdown
	// does not wait out each request's timeout, and no worker queues a request start after HTTP is gone. Not from
	// UnregisterAll: that also runs on every ModelContextProtocol.RefreshTools, which must not cancel live calls.
	void CancelAllRunning()
	{
		TArray<TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe>> Cancels;
		{
			FScopeLock Guard(&GRunningCallsLock);
			GRunningCalls.GenerateValueArray(Cancels);
		}
		for (const TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe>& Cancel : Cancels)
		{
			Cancel->Cancel();
		}
	}
}

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

// Tools declare parameter types both in VibeUE's own names ("int", "float", "bool") and in JSON Schema's
// ("number", "integer", "boolean"). The schema names used to fall through to "string", so capture_image max_width,
// deep_research lat/lng and execute_python_code auto_save were advertised as strings. BuildInputSchema lives in this
// file's anonymous namespace, so the test does too. Test path prefix VibeUE.Bridge.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUEBridgeSchemaTypesTest, "VibeUE.Bridge.SchemaTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUEBridgeSchemaTypesTest::RunTest(const FString& Parameters)
{
	FToolMetadata Meta;
	Meta.Name = TEXT("schema_types_test");
	Meta.Parameters.Add(FToolParameter(TEXT("count"), TEXT("VibeUE int"), TEXT("int"), false));
	Meta.Parameters.Add(FToolParameter(TEXT("scale"), TEXT("VibeUE float"), TEXT("float"), false));
	Meta.Parameters.Add(FToolParameter(TEXT("flag"), TEXT("VibeUE bool"), TEXT("bool"), false));
	Meta.Parameters.Add(FToolParameter(TEXT("lat"), TEXT("JSON Schema number"), TEXT("number"), false));
	Meta.Parameters.Add(FToolParameter(TEXT("max_items"), TEXT("JSON Schema integer"), TEXT("integer"), false));
	Meta.Parameters.Add(FToolParameter(TEXT("auto_save"), TEXT("JSON Schema boolean"), TEXT("boolean"), false));
	Meta.Parameters.Add(FToolParameter(TEXT("label"), TEXT("string"), TEXT("string"), true));

	const TSharedPtr<FJsonObject> Schema = BuildInputSchema(Meta);
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!TestTrue(TEXT("the schema has properties"), Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), Properties)))
	{
		return false;
	}
	auto TypeOf = [Properties](const TCHAR* ParamName)
	{
		const TSharedPtr<FJsonObject>* Prop = nullptr;
		FString Type;
		if ((*Properties)->TryGetObjectField(ParamName, Prop))
		{
			(*Prop)->TryGetStringField(TEXT("type"), Type);
		}
		return Type;
	};
	TestEqual(TEXT("int -> integer"), TypeOf(TEXT("count")), FString(TEXT("integer")));
	TestEqual(TEXT("float -> number"), TypeOf(TEXT("scale")), FString(TEXT("number")));
	TestEqual(TEXT("bool -> boolean"), TypeOf(TEXT("flag")), FString(TEXT("boolean")));
	TestEqual(TEXT("number stays number"), TypeOf(TEXT("lat")), FString(TEXT("number")));
	TestEqual(TEXT("integer stays integer"), TypeOf(TEXT("max_items")), FString(TEXT("integer")));
	TestEqual(TEXT("boolean stays boolean"), TypeOf(TEXT("auto_save")), FString(TEXT("boolean")));
	TestEqual(TEXT("string stays string"), TypeOf(TEXT("label")), FString(TEXT("string")));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
