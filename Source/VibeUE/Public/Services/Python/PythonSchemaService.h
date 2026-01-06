// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Python/Types/PythonTypes.h"
#include "Core/Result.h"

namespace VibeUE
{

/**
 * Service for generating JSON schemas and curated examples for LLMs
 *
 * Converts Python introspection data to MCP-compatible schemas.
 * Provides curated example scripts for common UE Python tasks.
 */
class VIBEUE_API FPythonSchemaService : public FServiceBase
{
public:
	/**
	 * Constructor
	 */
	explicit FPythonSchemaService(TSharedPtr<FServiceContext> Context);

	/**
	 * Get service name
	 */
	virtual FString GetServiceName() const override { return TEXT("PythonSchemaService"); }

	/**
	 * Initialize service - loads example scripts
	 */
	virtual void Initialize() override;

	/**
	 * Generate JSON schema for a Python class
	 *
	 * Useful for LLMs to understand class structure.
	 *
	 * @param ClassInfo Class information from discovery
	 * @return JSON schema string
	 */
	TResult<FString> GenerateClassSchema(const FPythonClassInfo& ClassInfo);

	/**
	 * Generate function signature in TypeScript-like format
	 *
	 * Example: "load_asset(path: str) -> Object"
	 *
	 * @param FuncInfo Function information from discovery
	 * @return Formatted signature string
	 */
	TResult<FString> GenerateFunctionSignature(const FPythonFunctionInfo& FuncInfo);

	/**
	 * Generate comprehensive API documentation as JSON
	 *
	 * Includes all classes, functions, with examples.
	 *
	 * @param ModuleInfo Module information from discovery
	 * @param bIncludeExamples Whether to include example scripts
	 * @return JSON documentation string
	 */
	TResult<FString> GenerateAPIDocumentation(
		const FPythonModuleInfo& ModuleInfo,
		bool bIncludeExamples = true
	);

	/**
	 * Get curated example scripts
	 *
	 * Returns pre-written examples for common tasks.
	 *
	 * @param Category Optional filter by category (e.g., "Asset Management")
	 * @return List of example scripts
	 */
	TResult<TArray<FPythonExampleScript>> GetExampleScripts(
		const FString& Category = FString()
	);

private:
	/**
	 * Convert Python type hint to JSON schema type
	 *
	 * e.g., "str" -> "string", "int" -> "integer"
	 *
	 * @param PythonType Python type hint string
	 * @return JSON schema type
	 */
	FString ConvertPythonTypeToJsonType(const FString& PythonType);

	/**
	 * Load example scripts from embedded data
	 */
	void InitializeExamples();

	/** Cache of example scripts */
	TArray<FPythonExampleScript> ExampleScripts;

	/** Flag to track if examples have been initialized */
	bool bExamplesInitialized = false;
};

} // namespace VibeUE
