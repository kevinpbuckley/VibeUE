// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Tools/PythonTools.h"
#include "Tools/PythonDiscoveryService.h"
#include "Core/ToolRegistry.h"
#include "Json.h"

// Helper function to extract a field from ParamsJson
static FString ExtractParamFromJson(const TMap<FString, FString>& Params, const FString& FieldName)
{
	// First check if parameter exists directly
	const FString* DirectParam = Params.Find(FieldName);
	if (DirectParam)
	{
		return *DirectParam;
	}

	// Otherwise, try to extract from ParamsJson
	const FString* ParamsJsonStr = Params.Find(TEXT("ParamsJson"));
	if (!ParamsJsonStr)
	{
		return FString();
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJsonStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return FString();
	}

	FString Value;
	if (JsonObj->TryGetStringField(FieldName, Value))
	{
		return Value;
	}

	return FString();
}

// Helper to extract with default value
static FString ExtractParamWithDefault(const TMap<FString, FString>& Params, const FString& FieldName, const FString& DefaultValue)
{
	FString Value = ExtractParamFromJson(Params, FieldName);
	return Value.IsEmpty() ? DefaultValue : Value;
}

// Helper to extract boolean
static bool ExtractBoolParam(const TMap<FString, FString>& Params, const FString& FieldName, bool DefaultValue)
{
	FString Value = ExtractParamFromJson(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
}

// Helper to extract integer
static int32 ExtractIntParam(const TMap<FString, FString>& Params, const FString& FieldName, int32 DefaultValue)
{
	FString Value = ExtractParamFromJson(Params, FieldName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}
	return FCString::Atoi(*Value);
}

// Register execute_python_code tool
REGISTER_VIBEUE_TOOL(execute_python_code,
	"Execute Python code in Unreal Engine. Returns stdout, stderr, and execution status.",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM("code", "Python code to execute", "string", true)
	),
	{
		FString Code = ExtractParamFromJson(Params, TEXT("code"));
		return UPythonTools::ExecutePythonCode(Code);
	}
);

// Register evaluate_python_expression tool
REGISTER_VIBEUE_TOOL(evaluate_python_expression,
	"Evaluate a Python expression and return its result as a string.",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM("expression", "Python expression to evaluate (e.g. 'len(dir(unreal))')", "string", true)
	),
	{
		FString Expression = ExtractParamFromJson(Params, TEXT("expression"));
		return UPythonTools::EvaluatePythonExpression(Expression);
	}
);

// Register discover_python_module tool
REGISTER_VIBEUE_TOOL(discover_python_module,
	"Discover contents of a Python module with optional filtering to reduce context size.",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM("module_name", "Name of the Python module (e.g. 'unreal')", "string", true),
		TOOL_PARAM("name_filter", "Filter results by name substring (case-insensitive). E.g. 'Blueprint' to find Blueprint-related items", "string", false),
		TOOL_PARAM("max_items", "Maximum items to return (default 100, 0 = unlimited). Use lower values to prevent context blowout", "number", false),
		TOOL_PARAM("include_classes", "Include classes in results (default true)", "boolean", false),
		TOOL_PARAM("include_functions", "Include functions in results (default true)", "boolean", false),
		TOOL_PARAM("case_sensitive", "Whether filtering is case-sensitive (default false)", "boolean", false)
	),
	{
		FString ModuleName = ExtractParamFromJson(Params, TEXT("module_name"));
		FString NameFilter = ExtractParamWithDefault(Params, TEXT("name_filter"), TEXT(""));
		int32 MaxItems = ExtractIntParam(Params, TEXT("max_items"), 100);
		bool IncludeClasses = ExtractBoolParam(Params, TEXT("include_classes"), true);
		bool IncludeFunctions = ExtractBoolParam(Params, TEXT("include_functions"), true);
		bool CaseSensitive = ExtractBoolParam(Params, TEXT("case_sensitive"), false);
		
		auto Service = UPythonTools::GetDiscoveryService();
		if (!Service.IsValid())
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
			ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python discovery service is not available"));
			FString JsonString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return JsonString;
		}
		
		auto Result = Service->DiscoverUnrealModule(1, NameFilter, MaxItems, IncludeClasses, IncludeFunctions, CaseSensitive);
		if (Result.IsError())
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
			ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());
			FString JsonString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return JsonString;
		}
		
		return UPythonTools::ConvertModuleInfoToJson(Result.GetValue());
	}
);

// Register discover_python_class tool
REGISTER_VIBEUE_TOOL(discover_python_class,
	"Discover methods and attributes of a Python class with optional filtering.",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM("class_name", "Fully qualified class name (e.g. 'unreal.BlueprintService')", "string", true),
		TOOL_PARAM("method_filter", "Filter methods by name substring (case-insensitive). E.g. 'variable' to find variable-related methods", "string", false),
		TOOL_PARAM("max_methods", "Maximum methods to return (default 0 = unlimited). Use to limit large class results", "number", false),
		TOOL_PARAM("include_inherited", "Include inherited methods (default true). Set false for class-only methods", "boolean", false),
		TOOL_PARAM("include_private", "Include private methods starting with _ (default false)", "boolean", false)
	),
	{
		FString ClassName = ExtractParamFromJson(Params, TEXT("class_name"));
		FString MethodFilter = ExtractParamWithDefault(Params, TEXT("method_filter"), TEXT(""));
		int32 MaxMethods = ExtractIntParam(Params, TEXT("max_methods"), 0);
		bool IncludeInherited = ExtractBoolParam(Params, TEXT("include_inherited"), true);
		bool IncludePrivate = ExtractBoolParam(Params, TEXT("include_private"), false);
		
		auto Service = UPythonTools::GetDiscoveryService();
		if (!Service.IsValid())
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
			ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python discovery service is not available"));
			FString JsonString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return JsonString;
		}
		
		auto Result = Service->DiscoverClass(ClassName, MethodFilter, MaxMethods, IncludeInherited, IncludePrivate);
		if (Result.IsError())
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
			ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());
			FString JsonString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return JsonString;
		}
		
		return UPythonTools::ConvertClassInfoToJson(Result.GetValue());
	}
);

// Register discover_python_function tool
REGISTER_VIBEUE_TOOL(discover_python_function,
	"Get signature and documentation for a Python function.",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM("function_name", "Fully qualified function name (e.g. 'unreal.load_asset'). Alias: function_path", "string", true)
	),
	{
		// Support both function_name and function_path parameter names
		FString FunctionName = ExtractParamFromJson(Params, TEXT("function_name"));
		if (FunctionName.IsEmpty())
		{
			FunctionName = ExtractParamFromJson(Params, TEXT("function_path"));
		}
		return UPythonTools::DiscoverPythonFunction(FunctionName);
	}
);

// Register list_python_subsystems tool
REGISTER_VIBEUE_TOOL(list_python_subsystems,
	"List all Unreal Engine subsystems accessible from Python.",
	"Python",
	TOOL_PARAMS(),
	{
		return UPythonTools::ListPythonSubsystems();
	}
);
