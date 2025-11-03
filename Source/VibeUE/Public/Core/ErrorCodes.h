// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Centralized error codes for consistent error handling across VibeUE
 * Organized by domain and operation type with numerical ranges
 * 
 * Usage:
 * @code
 * return TResult<UBlueprint*>::Error(
 *     VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
 *     FString::Printf(TEXT("Blueprint '%s' not found"), *Name)
 * );
 * @endcode
 */
namespace VibeUE
{
	namespace ErrorCodes
	{
		// ============================================================================
		// Parameter Validation Errors (1000-1099)
		// ============================================================================
		
		/** Required parameter is missing from request */
		constexpr const TCHAR* PARAM_MISSING = TEXT("PARAM_MISSING");
		
		/** Parameter value is invalid or out of range */
		constexpr const TCHAR* PARAM_INVALID = TEXT("PARAM_INVALID");
		
		/** Parameter type doesn't match expected type */
		constexpr const TCHAR* PARAM_TYPE_MISMATCH = TEXT("PARAM_TYPE_MISMATCH");

		// ============================================================================
		// Blueprint Errors (2000-2099)
		// ============================================================================
		
		/** Blueprint asset not found in asset registry */
		constexpr const TCHAR* BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");
		
		/** Failed to load Blueprint asset */
		constexpr const TCHAR* BLUEPRINT_LOAD_FAILED = TEXT("BLUEPRINT_LOAD_FAILED");
		
		/** Blueprint compilation failed */
		constexpr const TCHAR* BLUEPRINT_COMPILATION_FAILED = TEXT("BLUEPRINT_COMPILATION_FAILED");
		
		/** Blueprint with this name already exists */
		constexpr const TCHAR* BLUEPRINT_ALREADY_EXISTS = TEXT("BLUEPRINT_ALREADY_EXISTS");
		
		/** Invalid parent class for Blueprint */
		constexpr const TCHAR* BLUEPRINT_INVALID_PARENT = TEXT("BLUEPRINT_INVALID_PARENT");
		
		/** Failed to create new Blueprint */
		constexpr const TCHAR* BLUEPRINT_CREATE_FAILED = TEXT("BLUEPRINT_CREATE_FAILED");

		// ============================================================================
		// Variable Errors (2100-2199)
		// ============================================================================
		
		/** Variable not found in Blueprint */
		constexpr const TCHAR* VARIABLE_NOT_FOUND = TEXT("VARIABLE_NOT_FOUND");
		
		/** Variable with this name already exists */
		constexpr const TCHAR* VARIABLE_ALREADY_EXISTS = TEXT("VARIABLE_ALREADY_EXISTS");
		
		/** Invalid variable type specified */
		constexpr const TCHAR* VARIABLE_TYPE_INVALID = TEXT("VARIABLE_TYPE_INVALID");
		
		/** Failed to create variable */
		constexpr const TCHAR* VARIABLE_CREATE_FAILED = TEXT("VARIABLE_CREATE_FAILED");
		
		/** Failed to delete variable */
		constexpr const TCHAR* VARIABLE_DELETE_FAILED = TEXT("VARIABLE_DELETE_FAILED");

		// ============================================================================
		// Component Errors (2200-2299)
		// ============================================================================
		
		/** Component not found in Blueprint */
		constexpr const TCHAR* COMPONENT_NOT_FOUND = TEXT("COMPONENT_NOT_FOUND");
		
		/** Invalid component type specified */
		constexpr const TCHAR* COMPONENT_TYPE_INVALID = TEXT("COMPONENT_TYPE_INVALID");
		
		/** Failed to add component to Blueprint */
		constexpr const TCHAR* COMPONENT_ADD_FAILED = TEXT("COMPONENT_ADD_FAILED");
		
		/** Failed to remove component from Blueprint */
		constexpr const TCHAR* COMPONENT_REMOVE_FAILED = TEXT("COMPONENT_REMOVE_FAILED");
		
		/** Component hierarchy is invalid */
		constexpr const TCHAR* COMPONENT_HIERARCHY_INVALID = TEXT("COMPONENT_HIERARCHY_INVALID");

		// ============================================================================
		// Property Errors (2300-2399)
		// ============================================================================
		
		/** Property not found on object */
		constexpr const TCHAR* PROPERTY_NOT_FOUND = TEXT("PROPERTY_NOT_FOUND");
		
		/** Property is read-only and cannot be modified */
		constexpr const TCHAR* PROPERTY_READ_ONLY = TEXT("PROPERTY_READ_ONLY");
		
		/** Property type doesn't match provided value */
		constexpr const TCHAR* PROPERTY_TYPE_MISMATCH = TEXT("PROPERTY_TYPE_MISMATCH");
		
		/** Failed to set property value */
		constexpr const TCHAR* PROPERTY_SET_FAILED = TEXT("PROPERTY_SET_FAILED");
		
		/** Failed to get property value */
		constexpr const TCHAR* PROPERTY_GET_FAILED = TEXT("PROPERTY_GET_FAILED");

		// ============================================================================
		// Node Errors (2400-2499)
		// ============================================================================
		
		/** Node not found in Blueprint graph */
		constexpr const TCHAR* NODE_NOT_FOUND = TEXT("NODE_NOT_FOUND");
		
		/** Failed to create node */
		constexpr const TCHAR* NODE_CREATE_FAILED = TEXT("NODE_CREATE_FAILED");
		
		/** Invalid node type specified */
		constexpr const TCHAR* NODE_TYPE_INVALID = TEXT("NODE_TYPE_INVALID");
		
		/** Pin not found on node */
		constexpr const TCHAR* PIN_NOT_FOUND = TEXT("PIN_NOT_FOUND");
		
		/** Failed to connect pins */
		constexpr const TCHAR* PIN_CONNECTION_FAILED = TEXT("PIN_CONNECTION_FAILED");
		
		/** Pin types are incompatible for connection */
		constexpr const TCHAR* PIN_TYPE_INCOMPATIBLE = TEXT("PIN_TYPE_INCOMPATIBLE");
		
		/** Failed to break pin connection */
		constexpr const TCHAR* PIN_BREAK_FAILED = TEXT("PIN_BREAK_FAILED");

		// ============================================================================
		// Function Errors (2500-2599)
		// ============================================================================
		
		/** Function not found in Blueprint */
		constexpr const TCHAR* FUNCTION_NOT_FOUND = TEXT("FUNCTION_NOT_FOUND");
		
		/** Failed to create function */
		constexpr const TCHAR* FUNCTION_CREATE_FAILED = TEXT("FUNCTION_CREATE_FAILED");
		
		/** Function with this name already exists */
		constexpr const TCHAR* FUNCTION_ALREADY_EXISTS = TEXT("FUNCTION_ALREADY_EXISTS");
		
		/** Invalid function signature */
		constexpr const TCHAR* FUNCTION_SIGNATURE_INVALID = TEXT("FUNCTION_SIGNATURE_INVALID");

		// ============================================================================
		// Graph Errors (2600-2699)
		// ============================================================================
		
		/** Graph not found in Blueprint */
		constexpr const TCHAR* GRAPH_NOT_FOUND = TEXT("GRAPH_NOT_FOUND");
		
		/** Failed to create graph */
		constexpr const TCHAR* GRAPH_CREATE_FAILED = TEXT("GRAPH_CREATE_FAILED");
		
		/** Graph is invalid or corrupted */
		constexpr const TCHAR* GRAPH_INVALID = TEXT("GRAPH_INVALID");

		// ============================================================================
		// UMG/Widget Errors (3000-3099)
		// ============================================================================
		
		/** Widget Blueprint not found */
		constexpr const TCHAR* WIDGET_NOT_FOUND = TEXT("WIDGET_NOT_FOUND");
		
		/** Failed to create Widget Blueprint */
		constexpr const TCHAR* WIDGET_CREATE_FAILED = TEXT("WIDGET_CREATE_FAILED");
		
		/** Invalid widget type specified */
		constexpr const TCHAR* WIDGET_TYPE_INVALID = TEXT("WIDGET_TYPE_INVALID");
		
		/** Widget component not found */
		constexpr const TCHAR* WIDGET_COMPONENT_NOT_FOUND = TEXT("WIDGET_COMPONENT_NOT_FOUND");
		
		/** Parent widget cannot contain this child type */
		constexpr const TCHAR* WIDGET_PARENT_INCOMPATIBLE = TEXT("WIDGET_PARENT_INCOMPATIBLE");
		
		/** Failed to add widget component */
		constexpr const TCHAR* WIDGET_COMPONENT_ADD_FAILED = TEXT("WIDGET_COMPONENT_ADD_FAILED");
		
		/** Widget hierarchy is invalid */
		constexpr const TCHAR* WIDGET_HIERARCHY_INVALID = TEXT("WIDGET_HIERARCHY_INVALID");

		// ============================================================================
		// Asset Errors (4000-4099)
		// ============================================================================
		
		/** Asset not found in asset registry */
		constexpr const TCHAR* ASSET_NOT_FOUND = TEXT("ASSET_NOT_FOUND");
		
		/** Failed to import asset */
		constexpr const TCHAR* ASSET_IMPORT_FAILED = TEXT("ASSET_IMPORT_FAILED");
		
		/** Failed to export asset */
		constexpr const TCHAR* ASSET_EXPORT_FAILED = TEXT("ASSET_EXPORT_FAILED");
		
		/** Failed to save asset */
		constexpr const TCHAR* ASSET_SAVE_FAILED = TEXT("ASSET_SAVE_FAILED");
		
		/** Failed to delete asset */
		constexpr const TCHAR* ASSET_DELETE_FAILED = TEXT("ASSET_DELETE_FAILED");
		
		/** Asset path is invalid */
		constexpr const TCHAR* ASSET_PATH_INVALID = TEXT("ASSET_PATH_INVALID");

		// ============================================================================
		// System Errors (9000-9099)
		// ============================================================================
		
		/** Operation is not supported */
		constexpr const TCHAR* OPERATION_NOT_SUPPORTED = TEXT("OPERATION_NOT_SUPPORTED");
		
		/** Internal error occurred */
		constexpr const TCHAR* INTERNAL_ERROR = TEXT("INTERNAL_ERROR");
		
		/** Operation timed out */
		constexpr const TCHAR* TIMEOUT = TEXT("TIMEOUT");
		
		/** Service is not initialized */
		constexpr const TCHAR* SERVICE_NOT_INITIALIZED = TEXT("SERVICE_NOT_INITIALIZED");
		
		/** Invalid state for this operation */
		constexpr const TCHAR* INVALID_STATE = TEXT("INVALID_STATE");
	}
}
