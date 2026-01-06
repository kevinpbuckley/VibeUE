// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Commands/PythonCommands.h"
#include "Services/Python/PythonExecutionService.h"
#include "Services/Python/PythonDiscoveryService.h"
#include "Services/Python/PythonSchemaService.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"

namespace VibeUE
{

FPythonCommands::FPythonCommands()
{
	// Create service context
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Initialize services
	ExecutionService = MakeShared<FPythonExecutionService>(Context);
	DiscoveryService = MakeShared<FPythonDiscoveryService>(Context, ExecutionService);
	SchemaService = MakeShared<FPythonSchemaService>(Context);

	// Initialize services
	ExecutionService->Initialize();
	DiscoveryService->Initialize();
	SchemaService->Initialize();
}

TSharedPtr<FJsonObject> FPythonCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	// Get action from params
	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		return CreateErrorResponse(
			ErrorCodes::PARAM_MISSING,
			TEXT("Missing required parameter: action")
		);
	}

	// Route to appropriate handler
	if (Action.Equals(TEXT("discover_module"), ESearchCase::IgnoreCase))
	{
		return HandleDiscoverModule(Params);
	}
	else if (Action.Equals(TEXT("discover_class"), ESearchCase::IgnoreCase))
	{
		return HandleDiscoverClass(Params);
	}
	else if (Action.Equals(TEXT("discover_function"), ESearchCase::IgnoreCase))
	{
		return HandleDiscoverFunction(Params);
	}
	else if (Action.Equals(TEXT("list_subsystems"), ESearchCase::IgnoreCase))
	{
		return HandleListSubsystems(Params);
	}
	else if (Action.Equals(TEXT("execute_code"), ESearchCase::IgnoreCase))
	{
		return HandleExecuteCode(Params);
	}
	else if (Action.Equals(TEXT("evaluate_expression"), ESearchCase::IgnoreCase))
	{
		return HandleEvaluateExpression(Params);
	}
	else if (Action.Equals(TEXT("get_examples"), ESearchCase::IgnoreCase))
	{
		return HandleGetExamples(Params);
	}
	else if (Action.Equals(TEXT("read_source_file"), ESearchCase::IgnoreCase))
	{
		return HandleReadSourceFile(Params);
	}
	else if (Action.Equals(TEXT("search_source_files"), ESearchCase::IgnoreCase))
	{
		return HandleSearchSourceFiles(Params);
	}
	else if (Action.Equals(TEXT("list_source_files"), ESearchCase::IgnoreCase))
	{
		return HandleListSourceFiles(Params);
	}
	else if (Action.Equals(TEXT("help"), ESearchCase::IgnoreCase))
	{
		return HandleHelp(Params);
	}

	return CreateErrorResponse(
		ErrorCodes::PARAM_INVALID,
		FString::Printf(TEXT("Unknown action: %s"), *Action)
	);
}

TSharedPtr<FJsonObject> FPythonCommands::HandleDiscoverModule(const TSharedPtr<FJsonObject>& Params)
{
	// Get optional parameters
	int32 MaxDepth = 1;
	FString Filter;

	Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);
	Params->TryGetStringField(TEXT("filter"), Filter);

	// Call service
	auto Result = DiscoveryService->DiscoverUnrealModule(MaxDepth, Filter);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	// Convert to JSON
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("data"), ConvertModuleInfoToJson(Result.GetValue()));

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleDiscoverClass(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName;
	if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing class_name parameter"));
	}

	auto Result = DiscoveryService->DiscoverClass(ClassName);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("data"), ConvertClassInfoToJson(Result.GetValue()));

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleDiscoverFunction(const TSharedPtr<FJsonObject>& Params)
{
	FString FunctionPath;
	if (!Params->TryGetStringField(TEXT("function_path"), FunctionPath))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing function_path parameter"));
	}

	auto Result = DiscoveryService->DiscoverFunction(FunctionPath);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("data"), ConvertFunctionInfoToJson(Result.GetValue()));

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleListSubsystems(const TSharedPtr<FJsonObject>& Params)
{
	auto Result = DiscoveryService->ListEditorSubsystems();

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();

	TArray<TSharedPtr<FJsonValue>> SubsystemsArray;
	for (const FString& Subsystem : Result.GetValue())
	{
		SubsystemsArray.Add(MakeShared<FJsonValueString>(Subsystem));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("subsystems"), SubsystemsArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleExecuteCode(const TSharedPtr<FJsonObject>& Params)
{
	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing code parameter"));
	}

	// Get optional parameters
	FString ScopeStr;
	int32 TimeoutMs = 30000;
	Params->TryGetStringField(TEXT("scope"), ScopeStr);
	Params->TryGetNumberField(TEXT("timeout_ms"), TimeoutMs);

	EPythonFileExecutionScope Scope = EPythonFileExecutionScope::Private;
	if (ScopeStr.Equals(TEXT("public"), ESearchCase::IgnoreCase))
	{
		Scope = EPythonFileExecutionScope::Public;
	}

	auto Result = ExecutionService->ExecuteCode(Code, Scope, TimeoutMs);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("data"), ConvertExecutionResultToJson(Result.GetValue()));

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleEvaluateExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString Expression;
	if (!Params->TryGetStringField(TEXT("expression"), Expression))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing expression parameter"));
	}

	auto Result = ExecutionService->EvaluateExpression(Expression);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("data"), ConvertExecutionResultToJson(Result.GetValue()));

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleGetExamples(const TSharedPtr<FJsonObject>& Params)
{
	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);

	auto Result = SchemaService->GetExampleScripts(Category);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();

	TArray<TSharedPtr<FJsonValue>> ExamplesArray;
	for (const FPythonExampleScript& Example : Result.GetValue())
	{
		ExamplesArray.Add(MakeShared<FJsonValueObject>(ConvertExampleToJson(Example)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("examples"), ExamplesArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleReadSourceFile(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing file_path parameter"));
	}

	int32 StartLine = 0;
	int32 MaxLines = 1000;
	Params->TryGetNumberField(TEXT("start_line"), StartLine);
	Params->TryGetNumberField(TEXT("max_lines"), MaxLines);

	auto Result = DiscoveryService->ReadSourceFile(FilePath, StartLine, MaxLines);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("file_path"), FilePath);
	Data->SetStringField(TEXT("content"), Result.GetValue());
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleSearchSourceFiles(const TSharedPtr<FJsonObject>& Params)
{
	FString Pattern;
	if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
	{
		return CreateErrorResponse(ErrorCodes::PARAM_MISSING, TEXT("Missing pattern parameter"));
	}

	FString FilePattern = TEXT("*.h,*.cpp,*.py");
	int32 ContextLines = 3;
	Params->TryGetStringField(TEXT("file_pattern"), FilePattern);
	Params->TryGetNumberField(TEXT("context_lines"), ContextLines);

	auto Result = DiscoveryService->SearchSourceFiles(Pattern, FilePattern, ContextLines);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	for (const FSourceSearchResult& SearchResult : Result.GetValue())
	{
		ResultsArray.Add(MakeShared<FJsonValueObject>(ConvertSearchResultToJson(SearchResult)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("results"), ResultsArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleListSourceFiles(const TSharedPtr<FJsonObject>& Params)
{
	FString Directory;
	FString FilePattern = TEXT("*");

	Params->TryGetStringField(TEXT("directory"), Directory);
	Params->TryGetStringField(TEXT("pattern"), FilePattern);

	auto Result = DiscoveryService->ListSourceFiles(Directory, FilePattern);

	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FString& File : Result.GetValue())
	{
		FilesArray.Add(MakeShared<FJsonValueString>(File));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("files"), FilesArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("tool"), TEXT("manage_python_execution"));
	Data->SetStringField(TEXT("description"), TEXT("Execute Python code in Unreal Engine with runtime API discovery"));

	TArray<TSharedPtr<FJsonValue>> ActionsArray;

	// Add action descriptions
	TArray<TPair<FString, FString>> Actions = {
		{TEXT("discover_module"), TEXT("Introspect the unreal module to discover classes, functions, signatures")},
		{TEXT("discover_class"), TEXT("Get detailed information about a specific UE class")},
		{TEXT("discover_function"), TEXT("Get signature and documentation for a function")},
		{TEXT("list_subsystems"), TEXT("List available editor subsystems")},
		{TEXT("execute_code"), TEXT("Execute Python code string with output capture")},
		{TEXT("evaluate_expression"), TEXT("Evaluate Python expression and return result")},
		{TEXT("get_examples"), TEXT("Return curated example scripts for common tasks")},
		{TEXT("read_source_file"), TEXT("Read a specific source file from UE Python plugin")},
		{TEXT("search_source_files"), TEXT("Search for patterns in UE Python plugin source")},
		{TEXT("list_source_files"), TEXT("List available source files in UE Python plugin")},
		{TEXT("help"), TEXT("Get help documentation for available actions")}
	};

	for (const auto& Action : Actions)
	{
		TSharedPtr<FJsonObject> ActionObj = MakeShared<FJsonObject>();
		ActionObj->SetStringField(TEXT("action"), Action.Key);
		ActionObj->SetStringField(TEXT("description"), Action.Value);
		ActionsArray.Add(MakeShared<FJsonValueObject>(ActionObj));
	}

	Data->SetArrayField(TEXT("actions"), ActionsArray);
	Response->SetObjectField(TEXT("data"), Data);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::CreateSuccessResponse() const
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::CreateErrorResponse(
	const FString& ErrorCode,
	const FString& ErrorMessage) const
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);

	TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
	Error->SetStringField(TEXT("code"), ErrorCode);
	Error->SetStringField(TEXT("message"), ErrorMessage);

	Response->SetObjectField(TEXT("error"), Error);

	return Response;
}

TSharedPtr<FJsonObject> FPythonCommands::ConvertModuleInfoToJson(const FPythonModuleInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("module_name"), Info.ModuleName);
	Obj->SetNumberField(TEXT("total_members"), Info.TotalMembers);

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FString& Class : Info.Classes)
	{
		ClassesArray.Add(MakeShared<FJsonValueString>(Class));
	}
	Obj->SetArrayField(TEXT("classes"), ClassesArray);

	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (const FString& Function : Info.Functions)
	{
		FunctionsArray.Add(MakeShared<FJsonValueString>(Function));
	}
	Obj->SetArrayField(TEXT("functions"), FunctionsArray);

	TArray<TSharedPtr<FJsonValue>> ConstantsArray;
	for (const FString& Constant : Info.Constants)
	{
		ConstantsArray.Add(MakeShared<FJsonValueString>(Constant));
	}
	Obj->SetArrayField(TEXT("constants"), ConstantsArray);

	return Obj;
}

TSharedPtr<FJsonObject> FPythonCommands::ConvertClassInfoToJson(const FPythonClassInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("name"), Info.Name);
	Obj->SetStringField(TEXT("full_path"), Info.FullPath);
	Obj->SetStringField(TEXT("docstring"), Info.Docstring);
	Obj->SetBoolField(TEXT("is_abstract"), Info.bIsAbstract);

	TArray<TSharedPtr<FJsonValue>> MethodsArray;
	for (const FPythonFunctionInfo& Method : Info.Methods)
	{
		MethodsArray.Add(MakeShared<FJsonValueObject>(ConvertFunctionInfoToJson(Method)));
	}
	Obj->SetArrayField(TEXT("methods"), MethodsArray);

	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FString& Prop : Info.Properties)
	{
		PropertiesArray.Add(MakeShared<FJsonValueString>(Prop));
	}
	Obj->SetArrayField(TEXT("properties"), PropertiesArray);

	return Obj;
}

TSharedPtr<FJsonObject> FPythonCommands::ConvertFunctionInfoToJson(const FPythonFunctionInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("name"), Info.Name);
	Obj->SetStringField(TEXT("signature"), Info.Signature);
	Obj->SetStringField(TEXT("docstring"), Info.Docstring);
	Obj->SetStringField(TEXT("return_type"), Info.ReturnType);
	Obj->SetBoolField(TEXT("is_method"), Info.bIsMethod);
	Obj->SetBoolField(TEXT("is_static"), Info.bIsStatic);

	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	for (const FString& Param : Info.Parameters)
	{
		ParamsArray.Add(MakeShared<FJsonValueString>(Param));
	}
	Obj->SetArrayField(TEXT("parameters"), ParamsArray);

	return Obj;
}

TSharedPtr<FJsonObject> FPythonCommands::ConvertExecutionResultToJson(const FPythonExecutionResult& Result)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetBoolField(TEXT("success"), Result.bSuccess);
	Obj->SetStringField(TEXT("output"), Result.Output);
	Obj->SetStringField(TEXT("result"), Result.Result);
	Obj->SetStringField(TEXT("error_message"), Result.ErrorMessage);
	Obj->SetNumberField(TEXT("execution_time_ms"), Result.ExecutionTimeMs);

	TArray<TSharedPtr<FJsonValue>> LogsArray;
	for (const FString& Log : Result.LogMessages)
	{
		LogsArray.Add(MakeShared<FJsonValueString>(Log));
	}
	Obj->SetArrayField(TEXT("log_messages"), LogsArray);

	return Obj;
}

TSharedPtr<FJsonObject> FPythonCommands::ConvertExampleToJson(const FPythonExampleScript& Example)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("title"), Example.Title);
	Obj->SetStringField(TEXT("description"), Example.Description);
	Obj->SetStringField(TEXT("category"), Example.Category);
	Obj->SetStringField(TEXT("code"), Example.Code);

	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FString& Tag : Example.Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag));
	}
	Obj->SetArrayField(TEXT("tags"), TagsArray);

	return Obj;
}

TSharedPtr<FJsonObject> FPythonCommands::ConvertSearchResultToJson(const FSourceSearchResult& Result)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("file_path"), Result.FilePath);
	Obj->SetNumberField(TEXT("line_number"), Result.LineNumber);
	Obj->SetStringField(TEXT("line_content"), Result.LineContent);

	TArray<TSharedPtr<FJsonValue>> BeforeArray;
	for (const FString& Line : Result.ContextBefore)
	{
		BeforeArray.Add(MakeShared<FJsonValueString>(Line));
	}
	Obj->SetArrayField(TEXT("context_before"), BeforeArray);

	TArray<TSharedPtr<FJsonValue>> AfterArray;
	for (const FString& Line : Result.ContextAfter)
	{
		AfterArray.Add(MakeShared<FJsonValueString>(Line));
	}
	Obj->SetArrayField(TEXT("context_after"), AfterArray);

	return Obj;
}

} // namespace VibeUE
