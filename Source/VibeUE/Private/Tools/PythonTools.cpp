// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/PythonTools.h"
#include "Tools/PythonTypes.h"
#include "Core/ServiceContext.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Chat/ChatSession.h"
#include "FileHelpers.h"

// Include service headers after PythonTypes
#include "Tools/PythonExecutionService.h"
#include "Tools/PythonDiscoveryService.h"
#include "Tools/PythonSchemaService.h"

DEFINE_LOG_CATEGORY_STATIC(LogPythonTools, Log, All);

using namespace VibeUE;

// Static service instances
static TSharedPtr<FPythonExecutionService> ExecutionServiceInstance;
static TSharedPtr<FPythonDiscoveryService> DiscoveryServiceInstance;
static TSharedPtr<FPythonSchemaService> SchemaServiceInstance;
static TSharedPtr<FServiceContext> ServiceContextInstance;

UPythonTools::UPythonTools()
{
}

TSharedPtr<FPythonExecutionService> UPythonTools::GetExecutionService()
{
	if (!ExecutionServiceInstance.IsValid())
	{
		// Ensure service context exists
		if (!ServiceContextInstance.IsValid())
		{
			ServiceContextInstance = MakeShared<FServiceContext>();
		}
		ExecutionServiceInstance = MakeShared<FPythonExecutionService>(ServiceContextInstance);
		ExecutionServiceInstance->Initialize();
	}
	return ExecutionServiceInstance;
}

TSharedPtr<FPythonDiscoveryService> UPythonTools::GetDiscoveryService()
{
	if (!DiscoveryServiceInstance.IsValid())
	{
		// Ensure service context exists
		if (!ServiceContextInstance.IsValid())
		{
			ServiceContextInstance = MakeShared<FServiceContext>();
		}
		DiscoveryServiceInstance = MakeShared<FPythonDiscoveryService>(ServiceContextInstance, GetExecutionService());
		DiscoveryServiceInstance->Initialize();
	}
	return DiscoveryServiceInstance;
}

TSharedPtr<FPythonSchemaService> UPythonTools::GetSchemaService()
{
	if (!SchemaServiceInstance.IsValid())
	{
		// Ensure service context exists
		if (!ServiceContextInstance.IsValid())
		{
			ServiceContextInstance = MakeShared<FServiceContext>();
		}
		SchemaServiceInstance = MakeShared<FPythonSchemaService>(ServiceContextInstance);
		SchemaServiceInstance->Initialize();
	}
	return SchemaServiceInstance;
}

void UPythonTools::Shutdown()
{
	UE_LOG(LogPythonTools, Log, TEXT("UPythonTools::Shutdown - Releasing Python service instances"));
	
	// Release all Python service instances BEFORE Python shuts down
	// Order matters - discovery depends on execution
	SchemaServiceInstance.Reset();
	DiscoveryServiceInstance.Reset();
	ExecutionServiceInstance.Reset();
	ServiceContextInstance.Reset();
	
	UE_LOG(LogPythonTools, Log, TEXT("UPythonTools::Shutdown - All service instances released"));
}

FString UPythonTools::ExecutePythonCode(const FString& Code)
{
	// Efficient engine readiness check - once ready, never check again
	static bool bEngineReady = false;
	if (!bEngineReady)
	{
		// Check engine, editor, and that we're past initial load phase
		if (!GEngine || !GEditor || GIsInitialLoad)
		{
			return TEXT("Unreal Engine Loading");
		}
		bEngineReady = true;
	}

	// Auto-save all dirty packages before executing Python code if enabled
	if (FChatSession::IsAutoSaveBeforePythonExecutionEnabled())
	{
		// Validate GEditor is available before attempting to save
		if (!GEditor)
		{
			UE_LOG(LogPythonTools, Warning, TEXT("Cannot auto-save: GEditor is not available"));
		}
		else if (GIsPlayInEditorWorld)
		{
			UE_LOG(LogPythonTools, Warning, TEXT("Cannot auto-save: Currently in PIE mode"));
		}
		else
		{
			UE_LOG(LogPythonTools, Log, TEXT("Auto-saving dirty packages before Python execution..."));

			const bool bPromptUserToSave = false;
			const bool bSaveMapPackages = true;
			const bool bSaveContentPackages = true;
			const bool bFastSave = false;
			const bool bNotifyNoPackagesSaved = false;
			const bool bCanBeDeclined = false;

			bool bSaveSuccess = FEditorFileUtils::SaveDirtyPackages(
				bPromptUserToSave,
				bSaveMapPackages,
				bSaveContentPackages,
				bFastSave,
				bNotifyNoPackagesSaved,
				bCanBeDeclined
			);

			if (bSaveSuccess)
			{
				UE_LOG(LogPythonTools, Log, TEXT("Auto-save completed successfully"));
			}
			else
			{
				UE_LOG(LogPythonTools, Warning, TEXT("Auto-save completed with warnings or errors"));
			}
		}
	}

	auto Service = GetExecutionService();
	if (!Service.IsValid() || !Service.Get())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python execution service is not available"));
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	// Extra safety: verify service context is valid
	if (!ServiceContextInstance.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("SERVICE_CONTEXT_INVALID"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Service context is not properly initialized"));
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	auto Result = Service->ExecuteCode(Code);

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

	return ConvertExecutionResultToJson(Result.GetValue());
}

FString UPythonTools::DiscoverPythonModule(const FString& ModuleName)
{
	auto Service = GetDiscoveryService();
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

	// Use max_depth=1 and empty filter as defaults
	auto Result = Service->DiscoverUnrealModule(1, TEXT(""));

	if (Result.IsError())
	{
		// Return error as JSON
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
		ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());

		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	return ConvertModuleInfoToJson(Result.GetValue());
}

FString UPythonTools::DiscoverPythonClass(const FString& ClassName)
{
	auto Service = GetDiscoveryService();
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
	
	auto Result = Service->DiscoverClass(ClassName);

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

	return ConvertClassInfoToJson(Result.GetValue());
}

FString UPythonTools::DiscoverPythonFunction(const FString& FunctionName)
{
	auto Service = GetDiscoveryService();
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
	
	auto Result = Service->DiscoverFunction(FunctionName);

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

	return ConvertFunctionInfoToJson(Result.GetValue());
}

FString UPythonTools::ListPythonSubsystems()
{
	auto Service = GetDiscoveryService();
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
	
	auto Result = Service->ListEditorSubsystems();

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

	// Convert subsystems array to JSON
	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetBoolField(TEXT("success"), true);

	TArray<TSharedPtr<FJsonValue>> SubsystemsArray;
	for (const FString& Subsystem : Result.GetValue())
	{
		SubsystemsArray.Add(MakeShared<FJsonValueString>(Subsystem));
	}
	ResponseObj->SetArrayField(TEXT("subsystems"), SubsystemsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertExecutionResultToJson(const VibeUE::FPythonExecutionResult& Result)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), Result.bSuccess);
	JsonObj->SetStringField(TEXT("output"), Result.Output);
	JsonObj->SetStringField(TEXT("result"), Result.Result);
	
	// Only include error field if there's an actual error message
	if (!Result.ErrorMessage.IsEmpty())
	{
		JsonObj->SetStringField(TEXT("error"), Result.ErrorMessage);
	}
	
	JsonObj->SetNumberField(TEXT("execution_time_ms"), Result.ExecutionTimeMs);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertModuleInfoToJson(const VibeUE::FPythonModuleInfo& Info)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), true);
	JsonObj->SetStringField(TEXT("module_name"), Info.ModuleName);

	// Convert classes array
	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FString& ClassName : Info.Classes)
	{
		ClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	JsonObj->SetArrayField(TEXT("classes"), ClassesArray);

	// Convert functions array
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (const FString& FunctionName : Info.Functions)
	{
		FunctionsArray.Add(MakeShared<FJsonValueString>(FunctionName));
	}
	JsonObj->SetArrayField(TEXT("functions"), FunctionsArray);

	// Convert constants array
	TArray<TSharedPtr<FJsonValue>> ConstantsArray;
	for (const FString& ConstantName : Info.Constants)
	{
		ConstantsArray.Add(MakeShared<FJsonValueString>(ConstantName));
	}
	JsonObj->SetArrayField(TEXT("constants"), ConstantsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertClassInfoToJson(const VibeUE::FPythonClassInfo& Info)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), true);
	JsonObj->SetStringField(TEXT("class_name"), Info.Name);
	JsonObj->SetStringField(TEXT("full_path"), Info.FullPath);
	JsonObj->SetStringField(TEXT("doc_string"), Info.Docstring);

	// Convert base classes
	TArray<TSharedPtr<FJsonValue>> BasesArray;
	for (const FString& BaseName : Info.BaseClasses)
	{
		BasesArray.Add(MakeShared<FJsonValueString>(BaseName));
	}
	JsonObj->SetArrayField(TEXT("base_classes"), BasesArray);

	// Convert methods - need to serialize FPythonFunctionInfo structs
	TArray<TSharedPtr<FJsonValue>> MethodsArray;
	for (const VibeUE::FPythonFunctionInfo& Method : Info.Methods)
	{
		TSharedPtr<FJsonObject> MethodObj = MakeShared<FJsonObject>();
		MethodObj->SetStringField(TEXT("name"), Method.Name);
		MethodObj->SetStringField(TEXT("signature"), Method.Signature);
		MethodObj->SetStringField(TEXT("docstring"), Method.Docstring);
		MethodsArray.Add(MakeShared<FJsonValueObject>(MethodObj));
	}
	JsonObj->SetArrayField(TEXT("methods"), MethodsArray);

	// Convert properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FString& PropName : Info.Properties)
	{
		PropertiesArray.Add(MakeShared<FJsonValueString>(PropName));
	}
	JsonObj->SetArrayField(TEXT("properties"), PropertiesArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertFunctionInfoToJson(const VibeUE::FPythonFunctionInfo& Info)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), true);
	JsonObj->SetStringField(TEXT("function_name"), Info.Name);
	JsonObj->SetStringField(TEXT("signature"), Info.Signature);
	JsonObj->SetStringField(TEXT("doc_string"), Info.Docstring);

	// Convert parameters
	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	for (const FString& ParamName : Info.Parameters)
	{
		ParamsArray.Add(MakeShared<FJsonValueString>(ParamName));
	}
	JsonObj->SetArrayField(TEXT("parameters"), ParamsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}
