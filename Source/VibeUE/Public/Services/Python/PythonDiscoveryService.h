// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Python/Types/PythonTypes.h"
#include "Core/Result.h"

namespace VibeUE
{

// Forward declaration
class FPythonExecutionService;

/**
 * Service for discovering Python API via runtime introspection
 *
 * Uses Python's inspect module to discover unreal module contents.
 * Also provides access to UE Python plugin source files for understanding API patterns.
 */
class VIBEUE_API FPythonDiscoveryService : public FServiceBase
{
public:
	/**
	 * Constructor
	 */
	explicit FPythonDiscoveryService(
		TSharedPtr<FServiceContext> Context,
		TSharedPtr<FPythonExecutionService> ExecutionService
	);

	/**
	 * Get service name
	 */
	virtual FString GetServiceName() const override { return TEXT("PythonDiscoveryService"); }

	/**
	 * Discover all members of the unreal module
	 *
	 * @param MaxDepth How many levels deep to introspect (default 1)
	 * @param Filter Optional filter string (e.g., "Editor", "Asset")
	 * @return Module information with classes, functions, constants
	 */
	TResult<FPythonModuleInfo> DiscoverUnrealModule(
		int32 MaxDepth = 1,
		const FString& Filter = FString()
	);

	/**
	 * Get detailed information about a specific class
	 *
	 * @param ClassName Fully qualified class name (e.g., "EditorActorSubsystem" or "unreal.EditorActorSubsystem")
	 * @return Class information with methods and properties
	 */
	TResult<FPythonClassInfo> DiscoverClass(const FString& ClassName);

	/**
	 * Get function signature and documentation
	 *
	 * @param FunctionPath Full path (e.g., "load_asset" or "unreal.load_asset")
	 * @return Function information with signature and documentation
	 */
	TResult<FPythonFunctionInfo> DiscoverFunction(const FString& FunctionPath);

	/**
	 * List all available editor subsystems
	 *
	 * Returns class names that can be passed to get_editor_subsystem()
	 *
	 * @return List of subsystem class names
	 */
	TResult<TArray<FString>> ListEditorSubsystems();

	/**
	 * Search for classes/functions matching a pattern
	 *
	 * @param SearchPattern Pattern to match (case-insensitive)
	 * @param SearchType "class", "function", or "all"
	 * @return List of matching items
	 */
	TResult<TArray<FString>> SearchAPI(
		const FString& SearchPattern,
		const FString& SearchType = TEXT("all")
	);

	/**
	 * Read a source file from UE Python plugin
	 *
	 * @param RelativePath Relative path from plugin source root (e.g., "Public/IPythonScriptPlugin.h")
	 * @param StartLine Optional start line (default 0)
	 * @param MaxLines Optional max lines to read (default 1000)
	 * @return File contents
	 */
	TResult<FString> ReadSourceFile(
		const FString& RelativePath,
		int32 StartLine = 0,
		int32 MaxLines = 1000
	);

	/**
	 * Search for pattern in UE Python plugin source files
	 *
	 * @param Pattern Regex pattern to search
	 * @param FilePattern Glob pattern for files (e.g., "*.h", "*.cpp", "*.py")
	 * @param ContextLines Number of context lines before/after match
	 * @return Search results with file paths, line numbers, and context
	 */
	TResult<TArray<FSourceSearchResult>> SearchSourceFiles(
		const FString& Pattern,
		const FString& FilePattern = TEXT("*.h,*.cpp,*.py"),
		int32 ContextLines = 3
	);

	/**
	 * List source files in UE Python plugin directory
	 *
	 * @param SubDirectory Optional subdirectory (e.g., "Public", "Private", "Content/Python")
	 * @param FilePattern Optional file pattern filter (e.g., "*.h", "*.py")
	 * @return List of file paths relative to plugin source root
	 */
	TResult<TArray<FString>> ListSourceFiles(
		const FString& SubDirectory = FString(),
		const FString& FilePattern = TEXT("*")
	);

private:
	/**
	 * Execute Python introspection code and parse results
	 *
	 * Generates Python code using inspect module, executes it, returns JSON.
	 *
	 * @param PythonCode Python introspection script to execute
	 * @return JSON result string
	 */
	TResult<FString> ExecuteIntrospectionScript(const FString& PythonCode);

	/**
	 * Parse JSON result from Python introspection
	 */
	bool ParseModuleInfo(const FString& JsonResult, FPythonModuleInfo& OutInfo);
	bool ParseClassInfo(const FString& JsonResult, FPythonClassInfo& OutInfo);
	bool ParseFunctionInfo(const FString& JsonResult, FPythonFunctionInfo& OutInfo);

	/**
	 * Get the UE Python plugin source root path
	 *
	 * @return Absolute path to Python plugin source (e.g., "E:/Program Files/Epic Games/UE_5.7/Engine/Plugins/Experimental/PythonScriptPlugin")
	 */
	FString GetPluginSourceRoot() const;

	/**
	 * Validate that file path is within plugin source (security)
	 *
	 * Prevents directory traversal attacks.
	 *
	 * @param Path Path to validate
	 * @return True if path is safe
	 */
	bool IsValidSourcePath(const FString& Path) const;

	/**
	 * Get full path from relative path
	 *
	 * @param RelativePath Relative path from plugin source root
	 * @return Full absolute path
	 */
	FString GetFullSourcePath(const FString& RelativePath) const;

	/** Cache for discovered classes */
	TMap<FString, FPythonClassInfo> ClassCache;

	/** Cache for discovered module info */
	TMap<FString, FPythonModuleInfo> ModuleCache;

	/** Flag to track if unreal module has been validated */
	bool bUnrealModuleValidated = false;

	/** Execution service for running Python scripts */
	TSharedPtr<FPythonExecutionService> ExecutionService;
};

} // namespace VibeUE
