// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PythonTools.generated.h"

// Forward declarations - in VibeUE namespace
namespace VibeUE
{
	class FPythonExecutionService;
	class FPythonDiscoveryService;
	class FPythonSchemaService;
	struct FPythonModuleInfo;
	struct FPythonClassInfo;
	struct FPythonFunctionInfo;
	struct FPythonExecutionResult;
}

/**
 * MCP tools for Python code execution and discovery in Unreal Engine
 *
 * These tools are automatically exposed to MCP clients via the ToolRegistry.
 * They allow AI assistants to execute Python code and discover Python APIs.
 */
UCLASS(meta=(ToolCategory="Python"))
class VIBEUE_API UPythonTools : public UObject
{
	GENERATED_BODY()

public:
	UPythonTools();

	/**
	 * Execute Python code in the Unreal Engine Python environment
	 *
	 * @param Code - Python code to execute
	 * @return Execution result including output, errors, and success status
	 */
	UFUNCTION(BlueprintCallable, meta=(
		ToolName="execute_python_code",
		ToolDescription="Execute Python code in Unreal Engine. Returns stdout, stderr, and execution status.",
		ParamDescription_Code="Python code to execute"
	))
	static FString ExecutePythonCode(const FString& Code);

	/**
	 * Discover a Python module and list its contents
	 *
	 * @param ModuleName - Name of the module to discover (e.g. "unreal")
	 * @return JSON string with module information including classes, functions, and attributes
	 */
	UFUNCTION(BlueprintCallable, meta=(
		ToolName="discover_python_module",
		ToolDescription="Discover contents of a Python module. Returns classes, functions, and attributes.",
		ParamDescription_ModuleName="Name of the Python module (e.g. 'unreal', 'unreal.BlueprintEditorLibrary')"
	))
	static FString DiscoverPythonModule(const FString& ModuleName);

	/**
	 * Discover a Python class and list its methods and attributes
	 *
	 * @param ClassName - Fully qualified class name (e.g. "unreal.BlueprintFactory")
	 * @return JSON string with class information including methods, properties, and docstrings
	 */
	UFUNCTION(BlueprintCallable, meta=(
		ToolName="discover_python_class",
		ToolDescription="Discover methods and attributes of a Python class.",
		ParamDescription_ClassName="Fully qualified class name (e.g. 'unreal.BlueprintFactory')"
	))
	static FString DiscoverPythonClass(const FString& ClassName);

	/**
	 * Discover a Python function signature and documentation
	 *
	 * @param FunctionName - Fully qualified function name (e.g. "unreal.load_asset")
	 * @return JSON string with function signature, parameters, and docstring
	 */
	UFUNCTION(BlueprintCallable, meta=(
		ToolName="discover_python_function",
		ToolDescription="Get signature and documentation for a Python function.",
		ParamDescription_FunctionName="Fully qualified function name (e.g. 'unreal.load_asset')"
	))
	static FString DiscoverPythonFunction(const FString& FunctionName);

	/**
	 * List all Unreal Engine subsystems accessible from Python
	 *
	 * @return JSON array of subsystem names and descriptions
	 */
	UFUNCTION(BlueprintCallable, meta=(
		ToolName="list_python_subsystems",
		ToolDescription="List all Unreal Engine subsystems accessible from Python."
	))
	static FString ListPythonSubsystems();

	// Helper functions for converting service results to JSON - made public for use in registration
	static FString ConvertModuleInfoToJson(const VibeUE::FPythonModuleInfo& Info);
	static FString ConvertClassInfoToJson(const VibeUE::FPythonClassInfo& Info);
	static FString ConvertFunctionInfoToJson(const VibeUE::FPythonFunctionInfo& Info);
	static FString ConvertExecutionResultToJson(const VibeUE::FPythonExecutionResult& Result);

	// Helper to get or create service instances
	static TSharedPtr<VibeUE::FPythonDiscoveryService> GetDiscoveryService();
	
	/**
	 * Clean up all static service instances.
	 * Must be called before Python shuts down to avoid access violations.
	 */
	static void Shutdown();

private:
	static TSharedPtr<VibeUE::FPythonExecutionService> GetExecutionService();
	static TSharedPtr<VibeUE::FPythonSchemaService> GetSchemaService();
};
