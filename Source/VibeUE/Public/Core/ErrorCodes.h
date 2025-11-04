#pragma once

#include "CoreMinimal.h"

/**
 * @file ErrorCodes.h
 * @brief Centralized error codes for consistent error handling across VibeUE
 *
 * This file defines standardized error code constants organized by category.
 * All error codes are string constants that should be used throughout the codebase
 * instead of hardcoded string literals to ensure consistency and maintainability.
 *
 * Error code ranges:
 * - 1000-1099: Parameter Validation
 * - 2000-2099: Blueprint Operations
 * - 2100-2199: Variable Operations
 * - 2200-2299: Component Operations
 * - 2300-2399: Property Operations
 * - 2400-2499: Node Operations
 * - 3000-3099: UMG/Widget Operations
 * - 4000-4099: Asset Operations
 * - 5000-5099: Event Operations
 * - 6000-6099: Texture/Image Operations
 * - 9000-9099: System/Connection Errors
 *
 * @note All error codes use TEXT() macro for Unreal Engine string compatibility
 */
namespace VibeUE
{
namespace ErrorCodes
{
	// ============================================================================
	// Parameter Validation Errors (1000-1099)
	// ============================================================================

	/** @brief Required parameter is missing from the request */
	constexpr const TCHAR* PARAM_MISSING = TEXT("PARAM_MISSING");

	/** @brief Parameter value is invalid or malformed */
	constexpr const TCHAR* PARAM_INVALID = TEXT("PARAM_INVALID");

	/** @brief Parameter value is empty when a value is required */
	constexpr const TCHAR* PARAM_EMPTY = TEXT("PARAM_EMPTY");

	/** @brief Parameter type does not match expected type */
	constexpr const TCHAR* PARAM_TYPE_MISMATCH = TEXT("PARAM_TYPE_MISMATCH");

	/** @brief Parameter value is out of acceptable range */
	constexpr const TCHAR* PARAM_OUT_OF_RANGE = TEXT("PARAM_OUT_OF_RANGE");

	/** @brief Required field in parameter object is missing */
	constexpr const TCHAR* PARAM_FIELD_REQUIRED = TEXT("PARAM_FIELD_REQUIRED");

	// ============================================================================
	// Blueprint Errors (2000-2099)
	// ============================================================================

	/** @brief Blueprint could not be found at specified path */
	constexpr const TCHAR* BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");

	/** @brief Blueprint failed to load from disk */
	constexpr const TCHAR* BLUEPRINT_LOAD_FAILED = TEXT("BLUEPRINT_LOAD_FAILED");

	/** @brief Blueprint compilation encountered errors */
	constexpr const TCHAR* BLUEPRINT_COMPILATION_FAILED = TEXT("BLUEPRINT_COMPILATION_FAILED");

	/** @brief Blueprint already exists at target path */
	constexpr const TCHAR* BLUEPRINT_ALREADY_EXISTS = TEXT("BLUEPRINT_ALREADY_EXISTS");

	/** @brief Specified parent class is invalid for blueprint */
	constexpr const TCHAR* BLUEPRINT_INVALID_PARENT = TEXT("BLUEPRINT_INVALID_PARENT");

	/** @brief Failed to create new blueprint */
	constexpr const TCHAR* BLUEPRINT_CREATE_FAILED = TEXT("BLUEPRINT_CREATE_FAILED");

	/** @brief Blueprint does not have required construction script */
	constexpr const TCHAR* BLUEPRINT_NO_CONSTRUCTION_SCRIPT = TEXT("BLUEPRINT_NO_CONSTRUCTION_SCRIPT");

	/** @brief EventGraph could not be found or created */
	constexpr const TCHAR* BLUEPRINT_NO_EVENT_GRAPH = TEXT("BLUEPRINT_NO_EVENT_GRAPH");

	// ============================================================================
	// Variable Errors (2100-2199)
	// ============================================================================

	/** @brief Variable with specified name not found */
	constexpr const TCHAR* VARIABLE_NOT_FOUND = TEXT("VARIABLE_NOT_FOUND");

	/** @brief Variable with same name already exists */
	constexpr const TCHAR* VARIABLE_ALREADY_EXISTS = TEXT("VARIABLE_ALREADY_EXISTS");

	/** @brief Variable type is invalid or unsupported */
	constexpr const TCHAR* VARIABLE_TYPE_INVALID = TEXT("VARIABLE_TYPE_INVALID");

	/** @brief Failed to create new variable */
	constexpr const TCHAR* VARIABLE_CREATE_FAILED = TEXT("VARIABLE_CREATE_FAILED");

	/** @brief Failed to delete variable */
	constexpr const TCHAR* VARIABLE_DELETE_FAILED = TEXT("VARIABLE_DELETE_FAILED");

	/** @brief Variable type path must be canonical */
	constexpr const TCHAR* VARIABLE_TYPE_PATH_INVALID = TEXT("VARIABLE_TYPE_PATH_INVALID");

	// ============================================================================
	// Component Errors (2200-2299)
	// ============================================================================

	/** @brief Component with specified name not found */
	constexpr const TCHAR* COMPONENT_NOT_FOUND = TEXT("COMPONENT_NOT_FOUND");

	/** @brief Component type is invalid or unsupported */
	constexpr const TCHAR* COMPONENT_TYPE_INVALID = TEXT("COMPONENT_TYPE_INVALID");

	/** @brief Failed to add component to blueprint */
	constexpr const TCHAR* COMPONENT_ADD_FAILED = TEXT("COMPONENT_ADD_FAILED");

	/** @brief Component name already exists in blueprint */
	constexpr const TCHAR* COMPONENT_NAME_EXISTS = TEXT("COMPONENT_NAME_EXISTS");

	/** @brief Component type incompatible with parent */
	constexpr const TCHAR* COMPONENT_TYPE_INCOMPATIBLE = TEXT("COMPONENT_TYPE_INCOMPATIBLE");

	/** @brief Invalid component template */
	constexpr const TCHAR* COMPONENT_TEMPLATE_INVALID = TEXT("COMPONENT_TEMPLATE_INVALID");

	// ============================================================================
	// Property Errors (2300-2399)
	// ============================================================================

	/** @brief Property with specified name not found */
	constexpr const TCHAR* PROPERTY_NOT_FOUND = TEXT("PROPERTY_NOT_FOUND");

	/** @brief Property is read-only and cannot be modified */
	constexpr const TCHAR* PROPERTY_READ_ONLY = TEXT("PROPERTY_READ_ONLY");

	/** @brief Property value type does not match property type */
	constexpr const TCHAR* PROPERTY_TYPE_MISMATCH = TEXT("PROPERTY_TYPE_MISMATCH");

	/** @brief Failed to set property value */
	constexpr const TCHAR* PROPERTY_SET_FAILED = TEXT("PROPERTY_SET_FAILED");

	/** @brief Failed to get property value */
	constexpr const TCHAR* PROPERTY_GET_FAILED = TEXT("PROPERTY_GET_FAILED");

	// ============================================================================
	// Node Errors (2400-2499)
	// ============================================================================

	/** @brief Node with specified identifier not found */
	constexpr const TCHAR* NODE_NOT_FOUND = TEXT("NODE_NOT_FOUND");

	/** @brief Failed to create new node */
	constexpr const TCHAR* NODE_CREATE_FAILED = TEXT("NODE_CREATE_FAILED");

	/** @brief Node type is invalid or unsupported */
	constexpr const TCHAR* NODE_TYPE_INVALID = TEXT("NODE_TYPE_INVALID");

	/** @brief Node is invalid or not of expected type */
	constexpr const TCHAR* NODE_INVALID = TEXT("NODE_INVALID");

	/** @brief Pin with specified name not found */
	constexpr const TCHAR* PIN_NOT_FOUND = TEXT("PIN_NOT_FOUND");

	/** @brief Failed to connect pins */
	constexpr const TCHAR* PIN_CONNECTION_FAILED = TEXT("PIN_CONNECTION_FAILED");

	/** @brief Pin types are incompatible for connection */
	constexpr const TCHAR* PIN_TYPE_INCOMPATIBLE = TEXT("PIN_TYPE_INCOMPATIBLE");

	/** @brief Node cannot be deleted (protected) */
	constexpr const TCHAR* NODE_DELETE_PROTECTED = TEXT("NODE_DELETE_PROTECTED");

	/** @brief Failed to delete node */
	constexpr const TCHAR* NODE_DELETE_FAILED = TEXT("NODE_DELETE_FAILED");

	// ============================================================================
	// UMG/Widget Errors (3000-3099)
	// ============================================================================

	/** @brief Widget with specified name not found */
	constexpr const TCHAR* WIDGET_NOT_FOUND = TEXT("WIDGET_NOT_FOUND");

	/** @brief Failed to create new widget */
	constexpr const TCHAR* WIDGET_CREATE_FAILED = TEXT("WIDGET_CREATE_FAILED");

	/** @brief Widget type is invalid or unsupported */
	constexpr const TCHAR* WIDGET_TYPE_INVALID = TEXT("WIDGET_TYPE_INVALID");

	/** @brief Widget component not found in hierarchy */
	constexpr const TCHAR* WIDGET_COMPONENT_NOT_FOUND = TEXT("WIDGET_COMPONENT_NOT_FOUND");

	/** @brief Parent widget incompatible with child widget */
	constexpr const TCHAR* WIDGET_PARENT_INCOMPATIBLE = TEXT("WIDGET_PARENT_INCOMPATIBLE");

	/** @brief Failed to add widget to parent */
	constexpr const TCHAR* WIDGET_ADD_FAILED = TEXT("WIDGET_ADD_FAILED");

	/** @brief Widget already has maximum children */
	constexpr const TCHAR* WIDGET_CHILD_LIMIT_REACHED = TEXT("WIDGET_CHILD_LIMIT_REACHED");

	/** @brief Child widget not found in parent */
	constexpr const TCHAR* WIDGET_CHILD_NOT_FOUND = TEXT("WIDGET_CHILD_NOT_FOUND");

	/** @brief Widget blueprint not found */
	constexpr const TCHAR* WIDGET_BLUEPRINT_NOT_FOUND = TEXT("WIDGET_BLUEPRINT_NOT_FOUND");

	// ============================================================================
	// Asset Errors (4000-4099)
	// ============================================================================

	/** @brief Asset could not be found at specified path */
	constexpr const TCHAR* ASSET_NOT_FOUND = TEXT("ASSET_NOT_FOUND");

	/** @brief Asset import operation failed */
	constexpr const TCHAR* ASSET_IMPORT_FAILED = TEXT("ASSET_IMPORT_FAILED");

	/** @brief Asset export operation failed */
	constexpr const TCHAR* ASSET_EXPORT_FAILED = TEXT("ASSET_EXPORT_FAILED");

	/** @brief Failed to load asset from disk */
	constexpr const TCHAR* ASSET_LOAD_FAILED = TEXT("ASSET_LOAD_FAILED");

	/** @brief Asset path is invalid or malformed */
	constexpr const TCHAR* ASSET_PATH_INVALID = TEXT("ASSET_PATH_INVALID");

	/** @brief Asset type is incorrect for operation */
	constexpr const TCHAR* ASSET_TYPE_INCORRECT = TEXT("ASSET_TYPE_INCORRECT");

	/** @brief Asset already exists at target path */
	constexpr const TCHAR* ASSET_ALREADY_EXISTS = TEXT("ASSET_ALREADY_EXISTS");

	/** @brief Asset is corrupted or has dependency issues */
	constexpr const TCHAR* ASSET_CORRUPTED = TEXT("ASSET_CORRUPTED");

	// ============================================================================
	// Event Errors (5000-5099)
	// ============================================================================

	/** @brief Event with specified name not found */
	constexpr const TCHAR* EVENT_NOT_FOUND = TEXT("EVENT_NOT_FOUND");

	/** @brief Failed to create event node */
	constexpr const TCHAR* EVENT_CREATE_FAILED = TEXT("EVENT_CREATE_FAILED");

	/** @brief Event binding failed */
	constexpr const TCHAR* EVENT_BIND_FAILED = TEXT("EVENT_BIND_FAILED");

	/** @brief Event type is invalid */
	constexpr const TCHAR* EVENT_TYPE_INVALID = TEXT("EVENT_TYPE_INVALID");

	// ============================================================================
	// Function Errors (2500-2599)
	// ============================================================================

	/** @brief Function with specified name not found */
	constexpr const TCHAR* FUNCTION_NOT_FOUND = TEXT("FUNCTION_NOT_FOUND");

	/** @brief Function already exists with this name */
	constexpr const TCHAR* FUNCTION_ALREADY_EXISTS = TEXT("FUNCTION_ALREADY_EXISTS");

	/** @brief Failed to create function */
	constexpr const TCHAR* FUNCTION_CREATE_FAILED = TEXT("FUNCTION_CREATE_FAILED");

	/** @brief Function entry node not found */
	constexpr const TCHAR* FUNCTION_ENTRY_NOT_FOUND = TEXT("FUNCTION_ENTRY_NOT_FOUND");

	/** @brief Failed to create function result */
	constexpr const TCHAR* FUNCTION_RESULT_CREATE_FAILED = TEXT("FUNCTION_RESULT_CREATE_FAILED");

	// ============================================================================
	// Parameter Errors (2600-2699)
	// ============================================================================

	/** @brief Parameter with specified name not found */
	constexpr const TCHAR* PARAMETER_NOT_FOUND = TEXT("PARAMETER_NOT_FOUND");

	/** @brief Parameter already exists with this name */
	constexpr const TCHAR* PARAMETER_ALREADY_EXISTS = TEXT("PARAMETER_ALREADY_EXISTS");

	/** @brief Failed to create parameter */
	constexpr const TCHAR* PARAMETER_CREATE_FAILED = TEXT("PARAMETER_CREATE_FAILED");

	/** @brief Parameter type is invalid */
	constexpr const TCHAR* PARAMETER_TYPE_INVALID = TEXT("PARAMETER_TYPE_INVALID");

	/** @brief Parameter direction is invalid */
	constexpr const TCHAR* PARAMETER_INVALID_DIRECTION = TEXT("PARAMETER_INVALID_DIRECTION");

	// ============================================================================
	// Graph Errors (2700-2799)
	// ============================================================================

	/** @brief Graph with specified name not found */
	constexpr const TCHAR* GRAPH_NOT_FOUND = TEXT("GRAPH_NOT_FOUND");

	/** @brief Failed to create graph */
	constexpr const TCHAR* GRAPH_CREATE_FAILED = TEXT("GRAPH_CREATE_FAILED");

	// ============================================================================
	// SCS (Simple Construction Script) Errors (2800-2899)
	// ============================================================================

	/** @brief SCS (Simple Construction Script) not available */
	constexpr const TCHAR* SCS_NOT_AVAILABLE = TEXT("SCS_NOT_AVAILABLE");

	/** @brief Parent component not found in SCS */
	constexpr const TCHAR* PARENT_COMPONENT_NOT_FOUND = TEXT("PARENT_COMPONENT_NOT_FOUND");

	/** @brief Parent is not a scene component */
	constexpr const TCHAR* PARENT_NOT_SCENE_COMPONENT = TEXT("PARENT_NOT_SCENE_COMPONENT");

	/** @brief Failed to create component */
	constexpr const TCHAR* COMPONENT_CREATE_FAILED = TEXT("COMPONENT_CREATE_FAILED");

	/** @brief General operation failed */
	constexpr const TCHAR* OPERATION_FAILED = TEXT("OPERATION_FAILED");

	// ============================================================================
	// Texture/Image Errors (6000-6099)
	// ============================================================================

	/** @brief Texture import operation failed */
	constexpr const TCHAR* TEXTURE_IMPORT_FAILED = TEXT("TEXTURE_IMPORT_FAILED");

	/** @brief Texture data is invalid or corrupted */
	constexpr const TCHAR* TEXTURE_DATA_INVALID = TEXT("TEXTURE_DATA_INVALID");

	/** @brief Texture format is unsupported */
	constexpr const TCHAR* TEXTURE_FORMAT_UNSUPPORTED = TEXT("TEXTURE_FORMAT_UNSUPPORTED");

	/** @brief Another texture import is in progress */
	constexpr const TCHAR* TEXTURE_IMPORT_IN_PROGRESS = TEXT("TEXTURE_IMPORT_IN_PROGRESS");

	/** @brief Decoded image size does not match expected size */
	constexpr const TCHAR* TEXTURE_SIZE_MISMATCH = TEXT("TEXTURE_SIZE_MISMATCH");

	/** @brief Failed to add image to widget */
	constexpr const TCHAR* IMAGE_ADD_FAILED = TEXT("IMAGE_ADD_FAILED");

	/** @brief Cannot add image during serialization */
	constexpr const TCHAR* IMAGE_SERIALIZATION_ERROR = TEXT("IMAGE_SERIALIZATION_ERROR");

	// ============================================================================
	// System/Connection Errors (9000-9099)
	// ============================================================================

	/** @brief Operation is not supported in current context */
	constexpr const TCHAR* OPERATION_NOT_SUPPORTED = TEXT("OPERATION_NOT_SUPPORTED");

	/** @brief Internal error occurred */
	constexpr const TCHAR* INTERNAL_ERROR = TEXT("INTERNAL_ERROR");

	/** @brief Operation timed out */
	constexpr const TCHAR* TIMEOUT = TEXT("TIMEOUT");

	/** @brief Action is not supported */
	constexpr const TCHAR* ACTION_UNSUPPORTED = TEXT("ACTION_UNSUPPORTED");

	/** @brief C++ exception occurred during operation */
	constexpr const TCHAR* CPP_EXCEPTION = TEXT("CPP_EXCEPTION");

	/** @brief Unknown command type */
	constexpr const TCHAR* UNKNOWN_COMMAND = TEXT("UNKNOWN_COMMAND");

} // namespace ErrorCodes
} // namespace VibeUE
