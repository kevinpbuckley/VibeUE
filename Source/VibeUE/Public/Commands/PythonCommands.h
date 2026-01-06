// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Core/Result.h"

namespace VibeUE
{

// Forward declarations
class FPythonExecutionService;
class FPythonDiscoveryService;
class FPythonSchemaService;

/**
 * Handler class for Python-related MCP commands
 *
 * Routes actions to appropriate Python services and converts between JSON and C++ types.
 */
class VIBEUE_API FPythonCommands
{
public:
	FPythonCommands();

	/**
	 * Handle manage_python_execution command
	 */
	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params
	);

	/**
	 * Help handler
	 */
	TSharedPtr<FJsonObject> HandleHelp(const TSharedPtr<FJsonObject>& Params);

private:
	// Action handlers
	TSharedPtr<FJsonObject> HandleDiscoverModule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDiscoverClass(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDiscoverFunction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListSubsystems(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleExecuteCode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleEvaluateExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetExamples(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleReadSourceFile(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSearchSourceFiles(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListSourceFiles(const TSharedPtr<FJsonObject>& Params);

	// Helper methods
	TSharedPtr<FJsonObject> CreateSuccessResponse() const;
	TSharedPtr<FJsonObject> CreateErrorResponse(
		const FString& ErrorCode,
		const FString& ErrorMessage
	) const;

	// Convert service results to JSON
	TSharedPtr<FJsonObject> ConvertModuleInfoToJson(const struct FPythonModuleInfo& Info);
	TSharedPtr<FJsonObject> ConvertClassInfoToJson(const struct FPythonClassInfo& Info);
	TSharedPtr<FJsonObject> ConvertFunctionInfoToJson(const struct FPythonFunctionInfo& Info);
	TSharedPtr<FJsonObject> ConvertExecutionResultToJson(const struct FPythonExecutionResult& Result);
	TSharedPtr<FJsonObject> ConvertExampleToJson(const struct FPythonExampleScript& Example);
	TSharedPtr<FJsonObject> ConvertSearchResultToJson(const struct FSourceSearchResult& Result);

	// Service instances
	TSharedPtr<FPythonDiscoveryService> DiscoveryService;
	TSharedPtr<FPythonExecutionService> ExecutionService;
	TSharedPtr<FPythonSchemaService> SchemaService;
};

} // namespace VibeUE
