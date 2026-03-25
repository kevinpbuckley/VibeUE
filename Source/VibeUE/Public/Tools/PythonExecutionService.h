// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ServiceBase.h"
#include "Tools/PythonTypes.h"
#include "Core/Result.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"

namespace VibeUE
{

/**
 * Service for executing Python code in Unreal Engine
 *
 * Wraps IPythonScriptPlugin for safe execution with output capture.
 * Provides in-memory code execution without disk-based scripts.
 */
class VIBEUE_API FPythonExecutionService : public FServiceBase
{
public:
	/**
	 * Constructor
	 */
	explicit FPythonExecutionService(TSharedPtr<FServiceContext> Context);

	/**
	 * Get service name
	 */
	virtual FString GetServiceName() const override { return TEXT("PythonExecutionService"); }

	/**
	 * Execute Python code as a multi-line script
	 *
	 * @param Code Python code to execute
	 * @param ExecutionScope Private (isolated) or Public (shared console state)
	 * @param TimeoutMs Maximum execution time in milliseconds (0 = no timeout)
	 * @return Execution result with output, errors, and timing
	 */
	TResult<FPythonExecutionResult> ExecuteCode(
		const FString& Code,
		EPythonFileExecutionScope ExecutionScope = EPythonFileExecutionScope::Private,
		int32 TimeoutMs = 30000
	);

	/**
	 * Evaluate a Python expression and return the result
	 *
	 * @param Expression Single Python expression (e.g., "2 + 2", "unreal.load_asset('/Game/Test')")
	 * @return Execution result with expression value
	 */
	TResult<FPythonExecutionResult> EvaluateExpression(const FString& Expression);

	/**
	 * Execute code with validation checks
	 *
	 * Optionally validates for dangerous operations before execution.
	 *
	 * @param Code Python code to execute
	 * @param bValidateBeforeExecution If true, validates code for dangerous patterns
	 * @return Execution result
	 */
	TResult<FPythonExecutionResult> ExecuteCodeSafe(
		const FString& Code,
		bool bValidateBeforeExecution = false
	);

	/**
	 * Check if Python is available and initialized
	 *
	 * @return True if Python is ready to use
	 */
	TResult<bool> IsPythonAvailable();

	/**
	 * Get Python interpreter version and path
	 *
	 * @return Python version information
	 */
	TResult<FString> GetPythonInfo();

private:
	/**
	 * Convert FPythonCommandEx result to our result structure
	 *
	 * @param CommandEx UE Python command result
	 * @param ExecutionTimeMs Execution time in milliseconds
	 * @return Converted execution result
	 */
	FPythonExecutionResult ConvertExecutionResult(
		const FPythonCommandEx& CommandEx,
		float ExecutionTimeMs
	);

	/**
	 * Validate code for potentially dangerous operations
	 *
	 * Checks for subprocess, os.system, file writes, etc.
	 *
	 * @param Code Python code to validate
	 * @return Error if dangerous patterns detected, success otherwise
	 */
	TResult<void> ValidateCode(const FString& Code);

	/**
	 * Parse Python exception traceback into structured format
	 *
	 * @param Traceback Raw Python traceback string
	 * @return Formatted error message
	 */
	FString ParsePythonException(const FString& Traceback);

	/** Track if we've validated Python availability */
	bool bPythonValidated = false;
};

} // namespace VibeUE
