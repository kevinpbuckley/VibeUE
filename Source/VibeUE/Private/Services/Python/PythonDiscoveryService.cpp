// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/Python/PythonDiscoveryService.h"
#include "Services/Python/PythonExecutionService.h"
#include "Core/ErrorCodes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace VibeUE
{

FPythonDiscoveryService::FPythonDiscoveryService(
	TSharedPtr<FServiceContext> Context,
	TSharedPtr<FPythonExecutionService> InExecutionService)
	: FServiceBase(Context)
	, ExecutionService(InExecutionService)
{
}

TResult<FPythonModuleInfo> FPythonDiscoveryService::DiscoverUnrealModule(
	int32 MaxDepth,
	const FString& Filter)
{
	// Check cache first
	FString CacheKey = FString::Printf(TEXT("unreal_%d_%s"), MaxDepth, *Filter);
	if (ModuleCache.Contains(CacheKey))
	{
		return TResult<FPythonModuleInfo>::Success(ModuleCache[CacheKey]);
	}

	// Build introspection script
	FString FilterCondition = Filter.IsEmpty() ?
		TEXT("True") :
		FString::Printf(TEXT("'%s'.lower() in name.lower()"), *Filter);

	FString IntrospectionCode = FString::Printf(TEXT(
		"import unreal\n"
		"import inspect\n"
		"import json\n"
		"\n"
		"result = {'module_name': 'unreal', 'classes': [], 'functions': [], 'constants': [], 'total_members': 0}\n"
		"\n"
		"for name, obj in inspect.getmembers(unreal):\n"
		"    if not %s:\n"
		"        continue\n"
		"    result['total_members'] += 1\n"
		"    if inspect.isclass(obj):\n"
		"        result['classes'].append(name)\n"
		"    elif inspect.isfunction(obj) or inspect.isbuiltin(obj):\n"
		"        result['functions'].append(name)\n"
		"    elif not name.startswith('_'):\n"
		"        result['constants'].append(name)\n"
		"\n"
		"print(json.dumps(result))\n"
	), *FilterCondition);

	// Execute introspection
	auto ExecResult = ExecuteIntrospectionScript(IntrospectionCode);
	if (ExecResult.IsError())
	{
		return TResult<FPythonModuleInfo>::Error(
			ExecResult.GetErrorCode(),
			ExecResult.GetErrorMessage()
		);
	}

	// Parse result
	FPythonModuleInfo ModuleInfo;
	if (!ParseModuleInfo(ExecResult.GetValue(), ModuleInfo))
	{
		return TResult<FPythonModuleInfo>::Error(
			ErrorCodes::PYTHON_INTROSPECTION_FAILED,
			TEXT("Failed to parse module introspection results")
		);
	}

	// Cache result
	ModuleCache.Add(CacheKey, ModuleInfo);

	return TResult<FPythonModuleInfo>::Success(ModuleInfo);
}

TResult<FPythonClassInfo> FPythonDiscoveryService::DiscoverClass(const FString& ClassName)
{
	// Check cache first
	if (ClassCache.Contains(ClassName))
	{
		return TResult<FPythonClassInfo>::Success(ClassCache[ClassName]);
	}

	// Normalize class name (remove "unreal." prefix if present)
	FString NormalizedName = ClassName;
	if (ClassName.StartsWith(TEXT("unreal.")))
	{
		NormalizedName = ClassName.RightChop(7); // Remove "unreal."
	}

	// Build introspection script
	FString IntrospectionCode = FString::Printf(TEXT(
		"import unreal\n"
		"import inspect\n"
		"import json\n"
		"\n"
		"try:\n"
		"    cls = getattr(unreal, '%s')\n"
		"    if not inspect.isclass(cls):\n"
		"        raise ValueError('Not a class')\n"
		"\n"
		"    result = {\n"
		"        'name': '%s',\n"
		"        'full_path': 'unreal.%s',\n"
		"        'docstring': inspect.getdoc(cls) or '',\n"
		"        'base_classes': [b.__name__ for b in inspect.getmro(cls)[1:]],\n"
		"        'methods': [],\n"
		"        'properties': [],\n"
		"        'is_abstract': inspect.isabstract(cls)\n"
		"    }\n"
		"\n"
		"    for name, obj in inspect.getmembers(cls):\n"
		"        if name.startswith('_'):\n"
		"            continue\n"
		"        if inspect.ismethod(obj) or inspect.isfunction(obj):\n"
		"            try:\n"
		"                sig = str(inspect.signature(obj))\n"
		"            except:\n"
		"                sig = '(...)'\n"
		"            result['methods'].append({\n"
		"                'name': name,\n"
		"                'signature': sig,\n"
		"                'docstring': inspect.getdoc(obj) or ''\n"
		"            })\n"
		"        elif not callable(obj):\n"
		"            result['properties'].append(name)\n"
		"\n"
		"    print(json.dumps(result))\n"
		"except AttributeError:\n"
		"    print(json.dumps({'error': 'Class not found'}))\n"
		"except Exception as e:\n"
		"    print(json.dumps({'error': str(e)}))\n"
	), *NormalizedName, *NormalizedName, *NormalizedName);

	// Execute introspection
	auto ExecResult = ExecuteIntrospectionScript(IntrospectionCode);
	if (ExecResult.IsError())
	{
		return TResult<FPythonClassInfo>::Error(
			ExecResult.GetErrorCode(),
			ExecResult.GetErrorMessage()
		);
	}

	// Parse result
	FPythonClassInfo ClassInfo;
	if (!ParseClassInfo(ExecResult.GetValue(), ClassInfo))
	{
		return TResult<FPythonClassInfo>::Error(
			ErrorCodes::PYTHON_CLASS_NOT_FOUND,
			FString::Printf(TEXT("Class '%s' not found in unreal module"), *ClassName)
		);
	}

	// Cache result
	ClassCache.Add(ClassName, ClassInfo);

	return TResult<FPythonClassInfo>::Success(ClassInfo);
}

TResult<FPythonFunctionInfo> FPythonDiscoveryService::DiscoverFunction(const FString& FunctionPath)
{
	// Normalize function name
	FString NormalizedName = FunctionPath;
	if (FunctionPath.StartsWith(TEXT("unreal.")))
	{
		NormalizedName = FunctionPath.RightChop(7);
	}

	// Build introspection script
	FString IntrospectionCode = FString::Printf(TEXT(
		"import unreal\n"
		"import inspect\n"
		"import json\n"
		"\n"
		"try:\n"
		"    func = getattr(unreal, '%s')\n"
		"    if not (inspect.isfunction(func) or inspect.isbuiltin(func)):\n"
		"        raise ValueError('Not a function')\n"
		"\n"
		"    result = {\n"
		"        'name': '%s',\n"
		"        'docstring': inspect.getdoc(func) or '',\n"
		"        'is_method': False,\n"
		"        'is_static': False,\n"
		"        'is_class_method': False\n"
		"    }\n"
		"\n"
		"    try:\n"
		"        sig = inspect.signature(func)\n"
		"        result['signature'] = str(sig)\n"
		"        result['parameters'] = [p.name for p in sig.parameters.values()]\n"
		"        result['param_types'] = [str(p.annotation) if p.annotation != inspect.Parameter.empty else 'Any' for p in sig.parameters.values()]\n"
		"        result['return_type'] = str(sig.return_annotation) if sig.return_annotation != inspect.Signature.empty else 'Any'\n"
		"    except:\n"
		"        result['signature'] = '(...)'\n"
		"        result['parameters'] = []\n"
		"        result['param_types'] = []\n"
		"        result['return_type'] = 'Any'\n"
		"\n"
		"    print(json.dumps(result))\n"
		"except AttributeError:\n"
		"    print(json.dumps({'error': 'Function not found'}))\n"
		"except Exception as e:\n"
		"    print(json.dumps({'error': str(e)}))\n"
	), *NormalizedName, *NormalizedName);

	// Execute introspection
	auto ExecResult = ExecuteIntrospectionScript(IntrospectionCode);
	if (ExecResult.IsError())
	{
		return TResult<FPythonFunctionInfo>::Error(
			ExecResult.GetErrorCode(),
			ExecResult.GetErrorMessage()
		);
	}

	// Parse result
	FPythonFunctionInfo FuncInfo;
	if (!ParseFunctionInfo(ExecResult.GetValue(), FuncInfo))
	{
		return TResult<FPythonFunctionInfo>::Error(
			ErrorCodes::PYTHON_FUNCTION_NOT_FOUND,
			FString::Printf(TEXT("Function '%s' not found in unreal module"), *FunctionPath)
		);
	}

	return TResult<FPythonFunctionInfo>::Success(FuncInfo);
}

TResult<TArray<FString>> FPythonDiscoveryService::ListEditorSubsystems()
{
	// Build script to find all editor subsystems
	FString IntrospectionCode = TEXT(
		"import unreal\n"
		"import inspect\n"
		"import json\n"
		"\n"
		"result = {'subsystems': []}\n"
		"\n"
		"for name, obj in inspect.getmembers(unreal):\n"
		"    if inspect.isclass(obj) and 'Subsystem' in name and 'Editor' in name:\n"
		"        result['subsystems'].append(name)\n"
		"\n"
		"print(json.dumps(result))\n"
	);

	// Execute introspection
	auto ExecResult = ExecuteIntrospectionScript(IntrospectionCode);
	if (ExecResult.IsError())
	{
		return TResult<TArray<FString>>::Error(
			ExecResult.GetErrorCode(),
			ExecResult.GetErrorMessage()
		);
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExecResult.GetValue());

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return TResult<TArray<FString>>::Error(
			ErrorCodes::PYTHON_INTROSPECTION_FAILED,
			TEXT("Failed to parse subsystem list")
		);
	}

	TArray<FString> Subsystems;
	const TArray<TSharedPtr<FJsonValue>>* SubsystemsArray;
	if (JsonObject->TryGetArrayField(TEXT("subsystems"), SubsystemsArray))
	{
		for (const auto& Value : *SubsystemsArray)
		{
			Subsystems.Add(Value->AsString());
		}
	}

	return TResult<TArray<FString>>::Success(Subsystems);
}

TResult<TArray<FString>> FPythonDiscoveryService::SearchAPI(
	const FString& SearchPattern,
	const FString& SearchType)
{
	// First discover the module
	auto ModuleResult = DiscoverUnrealModule(1, SearchPattern);
	if (ModuleResult.IsError())
	{
		return TResult<TArray<FString>>::Error(
			ModuleResult.GetErrorCode(),
			ModuleResult.GetErrorMessage()
		);
	}

	const FPythonModuleInfo& ModuleInfo = ModuleResult.GetValue();
	TArray<FString> Results;

	// Add classes if requested
	if (SearchType.Equals(TEXT("all"), ESearchCase::IgnoreCase) ||
		SearchType.Equals(TEXT("class"), ESearchCase::IgnoreCase))
	{
		for (const FString& ClassName : ModuleInfo.Classes)
		{
			Results.Add(FString::Printf(TEXT("class: %s"), *ClassName));
		}
	}

	// Add functions if requested
	if (SearchType.Equals(TEXT("all"), ESearchCase::IgnoreCase) ||
		SearchType.Equals(TEXT("function"), ESearchCase::IgnoreCase))
	{
		for (const FString& FunctionName : ModuleInfo.Functions)
		{
			Results.Add(FString::Printf(TEXT("function: %s"), *FunctionName));
		}
	}

	return TResult<TArray<FString>>::Success(Results);
}

TResult<FString> FPythonDiscoveryService::ReadSourceFile(
	const FString& RelativePath,
	int32 StartLine,
	int32 MaxLines)
{
	// Validate path
	if (!IsValidSourcePath(RelativePath))
	{
		return TResult<FString>::Error(
			ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Invalid source path: %s"), *RelativePath)
		);
	}

	// Get full path
	FString FullPath = GetFullSourcePath(RelativePath);

	// Check if file exists
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
	{
		return TResult<FString>::Error(
			ErrorCodes::PYTHON_MODULE_NOT_FOUND,
			FString::Printf(TEXT("Source file not found: %s"), *RelativePath)
		);
	}

	// Read file
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FullPath))
	{
		return TResult<FString>::Error(
			ErrorCodes::OPERATION_FAILED,
			FString::Printf(TEXT("Failed to read source file: %s"), *RelativePath)
		);
	}

	// Extract requested lines
	int32 EndLine = FMath::Min(StartLine + MaxLines, Lines.Num());
	FString Result;

	for (int32 i = StartLine; i < EndLine; ++i)
	{
		if (i > StartLine)
		{
			Result += TEXT("\n");
		}
		Result += FString::Printf(TEXT("%5d: %s"), i + 1, *Lines[i]);
	}

	return TResult<FString>::Success(Result);
}

TResult<TArray<FSourceSearchResult>> FPythonDiscoveryService::SearchSourceFiles(
	const FString& Pattern,
	const FString& FilePattern,
	int32 ContextLines)
{
	TArray<FSourceSearchResult> Results;

	// Get list of files matching the file pattern
	TArray<FString> FilePatterns;
	FilePattern.ParseIntoArray(FilePatterns, TEXT(","), true);

	FString SearchPath = GetPluginSourceRoot();
	TArray<FString> AllFiles;

	// Find all files matching the patterns
	for (const FString& SinglePattern : FilePatterns)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *SearchPath, *SinglePattern.TrimStartAndEnd(), true, false);
		AllFiles.Append(Files);
	}

	// Search each file for the pattern
	for (const FString& FilePath : AllFiles)
	{
		// Read file contents
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
		{
			continue;
		}

		// Search for pattern in each line
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			if (Lines[LineIndex].Contains(Pattern))
			{
				FSourceSearchResult Result;

				// Make path relative to plugin root
				Result.FilePath = FilePath;
				FString PluginRoot = GetPluginSourceRoot();
				if (!PluginRoot.EndsWith(TEXT("/")))
				{
					PluginRoot += TEXT("/");
				}
				FPaths::MakePathRelativeTo(Result.FilePath, *PluginRoot);

				Result.LineNumber = LineIndex + 1;
				Result.LineContent = Lines[LineIndex];

				// Add context lines before
				int32 ContextStart = FMath::Max(0, LineIndex - ContextLines);
				for (int32 i = ContextStart; i < LineIndex; ++i)
				{
					Result.ContextBefore.Add(Lines[i]);
				}

				// Add context lines after
				int32 ContextEnd = FMath::Min(Lines.Num(), LineIndex + ContextLines + 1);
				for (int32 i = LineIndex + 1; i < ContextEnd; ++i)
				{
					Result.ContextAfter.Add(Lines[i]);
				}

				Results.Add(Result);
			}
		}
	}

	return TResult<TArray<FSourceSearchResult>>::Success(Results);
}

TResult<TArray<FString>> FPythonDiscoveryService::ListSourceFiles(
	const FString& SubDirectory,
	const FString& FilePattern)
{
	FString SearchPath = GetPluginSourceRoot();
	if (!SubDirectory.IsEmpty())
	{
		SearchPath = FPaths::Combine(SearchPath, SubDirectory);
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *SearchPath, *FilePattern, true, false);

	// Make paths relative to plugin source root
	FString PluginRoot = GetPluginSourceRoot();
	// Ensure trailing slash for proper relative path calculation
	if (!PluginRoot.EndsWith(TEXT("/")))
	{
		PluginRoot += TEXT("/");
	}

	for (FString& File : Files)
	{
		FPaths::MakePathRelativeTo(File, *PluginRoot);
	}

	return TResult<TArray<FString>>::Success(Files);
}

TResult<FString> FPythonDiscoveryService::ExecuteIntrospectionScript(const FString& PythonCode)
{
	if (!ExecutionService.IsValid())
	{
		return TResult<FString>::Error(
			ErrorCodes::PYTHON_NOT_AVAILABLE,
			TEXT("PythonExecutionService not initialized")
		);
	}

	auto ExecResult = ExecutionService->ExecuteCode(PythonCode, EPythonFileExecutionScope::Private);
	if (ExecResult.IsError())
	{
		return TResult<FString>::Error(
			ExecResult.GetErrorCode(),
			ExecResult.GetErrorMessage()
		);
	}

	return TResult<FString>::Success(ExecResult.GetValue().Output.TrimStartAndEnd());
}

bool FPythonDiscoveryService::ParseModuleInfo(const FString& JsonResult, FPythonModuleInfo& OutInfo)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResult);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	JsonObject->TryGetStringField(TEXT("module_name"), OutInfo.ModuleName);
	JsonObject->TryGetNumberField(TEXT("total_members"), OutInfo.TotalMembers);

	const TArray<TSharedPtr<FJsonValue>>* ClassesArray;
	if (JsonObject->TryGetArrayField(TEXT("classes"), ClassesArray))
	{
		for (const auto& Value : *ClassesArray)
		{
			OutInfo.Classes.Add(Value->AsString());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* FunctionsArray;
	if (JsonObject->TryGetArrayField(TEXT("functions"), FunctionsArray))
	{
		for (const auto& Value : *FunctionsArray)
		{
			OutInfo.Functions.Add(Value->AsString());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ConstantsArray;
	if (JsonObject->TryGetArrayField(TEXT("constants"), ConstantsArray))
	{
		for (const auto& Value : *ConstantsArray)
		{
			OutInfo.Constants.Add(Value->AsString());
		}
	}

	return true;
}

bool FPythonDiscoveryService::ParseClassInfo(const FString& JsonResult, FPythonClassInfo& OutInfo)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResult);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Check for error
	FString Error;
	if (JsonObject->TryGetStringField(TEXT("error"), Error))
	{
		return false;
	}

	JsonObject->TryGetStringField(TEXT("name"), OutInfo.Name);
	JsonObject->TryGetStringField(TEXT("full_path"), OutInfo.FullPath);
	JsonObject->TryGetStringField(TEXT("docstring"), OutInfo.Docstring);
	JsonObject->TryGetBoolField(TEXT("is_abstract"), OutInfo.bIsAbstract);

	const TArray<TSharedPtr<FJsonValue>>* BaseClassesArray;
	if (JsonObject->TryGetArrayField(TEXT("base_classes"), BaseClassesArray))
	{
		for (const auto& Value : *BaseClassesArray)
		{
			OutInfo.BaseClasses.Add(Value->AsString());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* MethodsArray;
	if (JsonObject->TryGetArrayField(TEXT("methods"), MethodsArray))
	{
		for (const auto& Value : *MethodsArray)
		{
			const TSharedPtr<FJsonObject>* MethodObj;
			if (Value->TryGetObject(MethodObj))
			{
				FPythonFunctionInfo FuncInfo;
				(*MethodObj)->TryGetStringField(TEXT("name"), FuncInfo.Name);
				(*MethodObj)->TryGetStringField(TEXT("signature"), FuncInfo.Signature);
				(*MethodObj)->TryGetStringField(TEXT("docstring"), FuncInfo.Docstring);
				FuncInfo.bIsMethod = true;
				OutInfo.Methods.Add(FuncInfo);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* PropertiesArray;
	if (JsonObject->TryGetArrayField(TEXT("properties"), PropertiesArray))
	{
		for (const auto& Value : *PropertiesArray)
		{
			OutInfo.Properties.Add(Value->AsString());
		}
	}

	return true;
}

bool FPythonDiscoveryService::ParseFunctionInfo(const FString& JsonResult, FPythonFunctionInfo& OutInfo)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResult);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Check for error
	FString Error;
	if (JsonObject->TryGetStringField(TEXT("error"), Error))
	{
		return false;
	}

	JsonObject->TryGetStringField(TEXT("name"), OutInfo.Name);
	JsonObject->TryGetStringField(TEXT("signature"), OutInfo.Signature);
	JsonObject->TryGetStringField(TEXT("docstring"), OutInfo.Docstring);
	JsonObject->TryGetStringField(TEXT("return_type"), OutInfo.ReturnType);
	JsonObject->TryGetBoolField(TEXT("is_method"), OutInfo.bIsMethod);
	JsonObject->TryGetBoolField(TEXT("is_static"), OutInfo.bIsStatic);
	JsonObject->TryGetBoolField(TEXT("is_class_method"), OutInfo.bIsClassMethod);

	const TArray<TSharedPtr<FJsonValue>>* ParamsArray;
	if (JsonObject->TryGetArrayField(TEXT("parameters"), ParamsArray))
	{
		for (const auto& Value : *ParamsArray)
		{
			OutInfo.Parameters.Add(Value->AsString());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ParamTypesArray;
	if (JsonObject->TryGetArrayField(TEXT("param_types"), ParamTypesArray))
	{
		for (const auto& Value : *ParamTypesArray)
		{
			OutInfo.ParamTypes.Add(Value->AsString());
		}
	}

	return true;
}

FString FPythonDiscoveryService::GetPluginSourceRoot() const
{
	// Get the engine installation path
	FString EngineDir = FPaths::EngineDir();
	return FPaths::Combine(EngineDir, TEXT("Plugins/Experimental/PythonScriptPlugin"));
}

bool FPythonDiscoveryService::IsValidSourcePath(const FString& Path) const
{
	// Prevent directory traversal
	if (Path.Contains(TEXT("..")) || Path.Contains(TEXT("~")))
	{
		return false;
	}

	// Must not be absolute path
	if (FPaths::IsRelative(Path) == false && !Path.StartsWith(TEXT("Source/")) &&
		!Path.StartsWith(TEXT("Content/")) && !Path.StartsWith(TEXT("Public/")) &&
		!Path.StartsWith(TEXT("Private/")))
	{
		return false;
	}

	return true;
}

FString FPythonDiscoveryService::GetFullSourcePath(const FString& RelativePath) const
{
	FString PluginRoot = GetPluginSourceRoot();

	// Handle special directories
	if (RelativePath.StartsWith(TEXT("Public/")) || RelativePath.StartsWith(TEXT("Private/")))
	{
		return FPaths::Combine(PluginRoot, TEXT("Source/PythonScriptPlugin"), RelativePath);
	}
	else if (RelativePath.StartsWith(TEXT("Content/")))
	{
		return FPaths::Combine(PluginRoot, RelativePath);
	}
	else if (RelativePath.StartsWith(TEXT("Source/")))
	{
		return FPaths::Combine(PluginRoot, RelativePath);
	}

	// Default: assume it's in Source/PythonScriptPlugin
	return FPaths::Combine(PluginRoot, TEXT("Source/PythonScriptPlugin"), RelativePath);
}

} // namespace VibeUE
