// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/PythonSchemaService.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace VibeUE
{

FPythonSchemaService::FPythonSchemaService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FPythonSchemaService::Initialize()
{
	if (!bExamplesInitialized)
	{
		InitializeExamples();
		bExamplesInitialized = true;
	}
}

TResult<FString> FPythonSchemaService::GenerateClassSchema(const FPythonClassInfo& ClassInfo)
{
	TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();

	Schema->SetStringField(TEXT("type"), TEXT("object"));
	Schema->SetStringField(TEXT("name"), ClassInfo.Name);
	Schema->SetStringField(TEXT("full_path"), ClassInfo.FullPath);
	Schema->SetStringField(TEXT("description"), ClassInfo.Docstring);

	// Add methods
	TArray<TSharedPtr<FJsonValue>> MethodsArray;
	for (const FPythonFunctionInfo& Method : ClassInfo.Methods)
	{
		TSharedPtr<FJsonObject> MethodObj = MakeShared<FJsonObject>();
		MethodObj->SetStringField(TEXT("name"), Method.Name);
		MethodObj->SetStringField(TEXT("signature"), Method.Signature);
		MethodObj->SetStringField(TEXT("description"), Method.Docstring);
		MethodsArray.Add(MakeShared<FJsonValueObject>(MethodObj));
	}
	Schema->SetArrayField(TEXT("methods"), MethodsArray);

	// Add properties
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FString& Prop : ClassInfo.Properties)
	{
		PropsArray.Add(MakeShared<FJsonValueString>(Prop));
	}
	Schema->SetArrayField(TEXT("properties"), PropsArray);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(Schema.ToSharedRef(), Writer))
	{
		return TResult<FString>::Error(
			ErrorCodes::OPERATION_FAILED,
			TEXT("Failed to serialize class schema to JSON")
		);
	}

	return TResult<FString>::Success(OutputString);
}

TResult<FString> FPythonSchemaService::GenerateFunctionSignature(const FPythonFunctionInfo& FuncInfo)
{
	// If we already have a signature, use it
	if (!FuncInfo.Signature.IsEmpty())
	{
		return TResult<FString>::Success(
			FString::Printf(TEXT("%s%s"), *FuncInfo.Name, *FuncInfo.Signature)
		);
	}

	// Build signature from parameters
	FString ParamsStr;
	for (int32 i = 0; i < FuncInfo.Parameters.Num(); ++i)
	{
		if (i > 0)
		{
			ParamsStr += TEXT(", ");
		}

		ParamsStr += FuncInfo.Parameters[i];

		// Add type hint if available
		if (i < FuncInfo.ParamTypes.Num() && !FuncInfo.ParamTypes[i].IsEmpty())
		{
			ParamsStr += FString::Printf(TEXT(": %s"), *FuncInfo.ParamTypes[i]);
		}
	}

	FString Signature = FString::Printf(TEXT("%s(%s)"), *FuncInfo.Name, *ParamsStr);

	// Add return type if available
	if (!FuncInfo.ReturnType.IsEmpty() && FuncInfo.ReturnType != TEXT("Any"))
	{
		Signature += FString::Printf(TEXT(" -> %s"), *FuncInfo.ReturnType);
	}

	return TResult<FString>::Success(Signature);
}

TResult<FString> FPythonSchemaService::GenerateAPIDocumentation(
	const FPythonModuleInfo& ModuleInfo,
	bool bIncludeExamples)
{
	TSharedPtr<FJsonObject> Doc = MakeShared<FJsonObject>();

	Doc->SetStringField(TEXT("module"), ModuleInfo.ModuleName);
	Doc->SetNumberField(TEXT("total_members"), ModuleInfo.TotalMembers);

	// Add classes
	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FString& ClassName : ModuleInfo.Classes)
	{
		ClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	Doc->SetArrayField(TEXT("classes"), ClassesArray);

	// Add functions
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (const FString& FunctionName : ModuleInfo.Functions)
	{
		FunctionsArray.Add(MakeShared<FJsonValueString>(FunctionName));
	}
	Doc->SetArrayField(TEXT("functions"), FunctionsArray);

	// Add examples if requested
	if (bIncludeExamples)
	{
		auto ExamplesResult = GetExampleScripts();
		if (ExamplesResult.IsSuccess())
		{
			TArray<TSharedPtr<FJsonValue>> ExamplesArray;
			for (const FPythonExampleScript& Example : ExamplesResult.GetValue())
			{
				TSharedPtr<FJsonObject> ExampleObj = MakeShared<FJsonObject>();
				ExampleObj->SetStringField(TEXT("title"), Example.Title);
				ExampleObj->SetStringField(TEXT("description"), Example.Description);
				ExampleObj->SetStringField(TEXT("category"), Example.Category);
				ExampleObj->SetStringField(TEXT("code"), Example.Code);
				ExamplesArray.Add(MakeShared<FJsonValueObject>(ExampleObj));
			}
			Doc->SetArrayField(TEXT("examples"), ExamplesArray);
		}
	}

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(Doc.ToSharedRef(), Writer))
	{
		return TResult<FString>::Error(
			ErrorCodes::OPERATION_FAILED,
			TEXT("Failed to serialize API documentation to JSON")
		);
	}

	return TResult<FString>::Success(OutputString);
}

TResult<TArray<FPythonExampleScript>> FPythonSchemaService::GetExampleScripts(const FString& Category)
{
	// Ensure examples are initialized
	if (!bExamplesInitialized)
	{
		InitializeExamples();
		bExamplesInitialized = true;
	}

	// Filter by category if specified
	if (!Category.IsEmpty())
	{
		TArray<FPythonExampleScript> FilteredExamples;
		for (const FPythonExampleScript& Example : ExampleScripts)
		{
			if (Example.Category.Equals(Category, ESearchCase::IgnoreCase))
			{
				FilteredExamples.Add(Example);
			}
		}
		return TResult<TArray<FPythonExampleScript>>::Success(FilteredExamples);
	}

	return TResult<TArray<FPythonExampleScript>>::Success(ExampleScripts);
}

FString FPythonSchemaService::ConvertPythonTypeToJsonType(const FString& PythonType)
{
	if (PythonType.Equals(TEXT("str"), ESearchCase::IgnoreCase))
	{
		return TEXT("string");
	}
	else if (PythonType.Equals(TEXT("int"), ESearchCase::IgnoreCase))
	{
		return TEXT("integer");
	}
	else if (PythonType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		return TEXT("number");
	}
	else if (PythonType.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
	{
		return TEXT("boolean");
	}
	else if (PythonType.Equals(TEXT("list"), ESearchCase::IgnoreCase) ||
	         PythonType.Equals(TEXT("tuple"), ESearchCase::IgnoreCase))
	{
		return TEXT("array");
	}
	else if (PythonType.Equals(TEXT("dict"), ESearchCase::IgnoreCase))
	{
		return TEXT("object");
	}

	return TEXT("any");
}

void FPythonSchemaService::InitializeExamples()
{
	ExampleScripts.Empty();

	// Asset Management Examples
	{
		FPythonExampleScript Example;
		Example.Title = TEXT("Load and Modify Asset");
		Example.Description = TEXT("Load a blueprint asset and modify a component property");
		Example.Category = TEXT("Asset Management");
		Example.Code = TEXT(
			"import unreal\n"
			"\n"
			"# Load blueprint asset\n"
			"asset = unreal.load_asset('/Game/Blueprints/BP_MyActor')\n"
			"if asset:\n"
			"    # Get the default object (CDO)\n"
			"    default_obj = asset.get_default_object()\n"
			"    \n"
			"    # Get a component by class\n"
			"    component = default_obj.get_component_by_class(unreal.StaticMeshComponent)\n"
			"    if component:\n"
			"        # Modify property\n"
			"        component.set_editor_property('Mass', 100.0)\n"
			"        print(f'Updated {asset.get_name()}')\n"
			"    \n"
			"    # Save the asset\n"
			"    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())\n"
		);
		Example.Tags.Add(TEXT("asset"));
		Example.Tags.Add(TEXT("blueprint"));
		Example.Tags.Add(TEXT("property"));
		ExampleScripts.Add(Example);
	}

	// Level Editing Examples
	{
		FPythonExampleScript Example;
		Example.Title = TEXT("Spawn Actor in Level");
		Example.Description = TEXT("Spawn an actor in the current level at a specific location");
		Example.Category = TEXT("Level Editing");
		Example.Code = TEXT(
			"import unreal\n"
			"\n"
			"# Get editor actor subsystem\n"
			"subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
			"\n"
			"# Load the actor class\n"
			"actor_class = unreal.load_class(None, '/Game/Blueprints/BP_MyActor.BP_MyActor_C')\n"
			"\n"
			"if actor_class:\n"
			"    # Define spawn location and rotation\n"
			"    location = unreal.Vector(0, 0, 100)\n"
			"    rotation = unreal.Rotator(0, 0, 0)\n"
			"    \n"
			"    # Spawn the actor\n"
			"    actor = subsys.spawn_actor_from_class(actor_class, location, rotation)\n"
			"    if actor:\n"
			"        actor.set_actor_label('SpawnedActor')\n"
			"        print(f'Spawned: {actor.get_actor_label()}')\n"
		);
		Example.Tags.Add(TEXT("level"));
		Example.Tags.Add(TEXT("actor"));
		Example.Tags.Add(TEXT("spawn"));
		ExampleScripts.Add(Example);
	}

	// Discovery Examples
	{
		FPythonExampleScript Example;
		Example.Title = TEXT("List All Level Actors");
		Example.Description = TEXT("Get all actors in the current level and print their names");
		Example.Category = TEXT("Discovery");
		Example.Code = TEXT(
			"import unreal\n"
			"\n"
			"# Get editor actor subsystem\n"
			"subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
			"\n"
			"# Get all level actors\n"
			"actors = subsys.get_all_level_actors()\n"
			"\n"
			"print(f'Found {len(actors)} actors in level:')\n"
			"for actor in actors:\n"
			"    print(f'  - {actor.get_actor_label()} ({actor.get_class().get_name()})')\n"
		);
		Example.Tags.Add(TEXT("discovery"));
		Example.Tags.Add(TEXT("actors"));
		ExampleScripts.Add(Example);
	}

	// Asset Library Examples
	{
		FPythonExampleScript Example;
		Example.Title = TEXT("Find Assets by Type");
		Example.Description = TEXT("Search for all assets of a specific type in Content Browser");
		Example.Category = TEXT("Asset Management");
		Example.Code = TEXT(
			"import unreal\n"
			"\n"
			"# Get asset registry\n"
			"asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()\n"
			"\n"
			"# Search for all Blueprint assets\n"
			"filter = unreal.ARFilter(\n"
			"    class_names=['Blueprint'],\n"
			"    package_paths=['/Game'],\n"
			"    recursive_paths=True\n"
			")\n"
			"\n"
			"assets = asset_registry.get_assets(filter)\n"
			"\n"
			"print(f'Found {len(assets)} Blueprint assets:')\n"
			"for asset_data in assets:\n"
			"    print(f'  - {asset_data.asset_name}')\n"
		);
		Example.Tags.Add(TEXT("asset"));
		Example.Tags.Add(TEXT("search"));
		Example.Tags.Add(TEXT("discovery"));
		ExampleScripts.Add(Example);
	}

	LogInfo(FString::Printf(TEXT("Initialized %d Python example scripts"), ExampleScripts.Num()));
}

} // namespace VibeUE
