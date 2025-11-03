// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @namespace VibeUE::ErrorCodes
 * @brief Centralized error codes for consistent error handling
 * 
 * Error codes are organized by domain and operation type with numeric ranges.
 */
namespace VibeUE
{
	namespace ErrorCodes
	{
		// Parameter validation errors (1000-1099)
		constexpr const TCHAR* PARAM_MISSING = TEXT("PARAM_MISSING");
		constexpr const TCHAR* PARAM_INVALID = TEXT("PARAM_INVALID");
		constexpr const TCHAR* PARAM_TYPE_MISMATCH = TEXT("PARAM_TYPE_MISMATCH");
		constexpr const TCHAR* PARAM_EMPTY = TEXT("PARAM_EMPTY");
		constexpr const TCHAR* PARAM_OUT_OF_RANGE = TEXT("PARAM_OUT_OF_RANGE");

		// Blueprint errors (2000-2099)
		constexpr const TCHAR* BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");
		constexpr const TCHAR* BLUEPRINT_LOAD_FAILED = TEXT("BLUEPRINT_LOAD_FAILED");
		constexpr const TCHAR* BLUEPRINT_COMPILATION_FAILED = TEXT("BLUEPRINT_COMPILATION_FAILED");
		constexpr const TCHAR* BLUEPRINT_ALREADY_EXISTS = TEXT("BLUEPRINT_ALREADY_EXISTS");
		constexpr const TCHAR* BLUEPRINT_INVALID_PARENT = TEXT("BLUEPRINT_INVALID_PARENT");

		// Variable errors (2100-2199)
		constexpr const TCHAR* VARIABLE_NOT_FOUND = TEXT("VARIABLE_NOT_FOUND");
		constexpr const TCHAR* VARIABLE_ALREADY_EXISTS = TEXT("VARIABLE_ALREADY_EXISTS");
		constexpr const TCHAR* VARIABLE_TYPE_INVALID = TEXT("VARIABLE_TYPE_INVALID");
		constexpr const TCHAR* VARIABLE_CREATE_FAILED = TEXT("VARIABLE_CREATE_FAILED");

		// Component errors (2200-2299)
		constexpr const TCHAR* COMPONENT_NOT_FOUND = TEXT("COMPONENT_NOT_FOUND");
		constexpr const TCHAR* COMPONENT_TYPE_INVALID = TEXT("COMPONENT_TYPE_INVALID");
		constexpr const TCHAR* COMPONENT_ADD_FAILED = TEXT("COMPONENT_ADD_FAILED");

		// Property errors (2300-2399)
		constexpr const TCHAR* PROPERTY_NOT_FOUND = TEXT("PROPERTY_NOT_FOUND");
		constexpr const TCHAR* PROPERTY_READ_ONLY = TEXT("PROPERTY_READ_ONLY");
		constexpr const TCHAR* PROPERTY_TYPE_MISMATCH = TEXT("PROPERTY_TYPE_MISMATCH");
		constexpr const TCHAR* PROPERTY_SET_FAILED = TEXT("PROPERTY_SET_FAILED");

		// Node errors (2400-2499)
		constexpr const TCHAR* NODE_NOT_FOUND = TEXT("NODE_NOT_FOUND");
		constexpr const TCHAR* NODE_CREATE_FAILED = TEXT("NODE_CREATE_FAILED");
		constexpr const TCHAR* NODE_TYPE_INVALID = TEXT("NODE_TYPE_INVALID");
		constexpr const TCHAR* PIN_NOT_FOUND = TEXT("PIN_NOT_FOUND");
		constexpr const TCHAR* PIN_CONNECTION_FAILED = TEXT("PIN_CONNECTION_FAILED");
		constexpr const TCHAR* PIN_TYPE_INCOMPATIBLE = TEXT("PIN_TYPE_INCOMPATIBLE");

		// UMG/Widget errors (3000-3099)
		constexpr const TCHAR* WIDGET_NOT_FOUND = TEXT("WIDGET_NOT_FOUND");
		constexpr const TCHAR* WIDGET_CREATE_FAILED = TEXT("WIDGET_CREATE_FAILED");
		constexpr const TCHAR* WIDGET_TYPE_INVALID = TEXT("WIDGET_TYPE_INVALID");
		constexpr const TCHAR* WIDGET_COMPONENT_NOT_FOUND = TEXT("WIDGET_COMPONENT_NOT_FOUND");
		constexpr const TCHAR* WIDGET_PARENT_INCOMPATIBLE = TEXT("WIDGET_PARENT_INCOMPATIBLE");

		// Asset errors (4000-4099)
		constexpr const TCHAR* ASSET_NOT_FOUND = TEXT("ASSET_NOT_FOUND");
		constexpr const TCHAR* ASSET_IMPORT_FAILED = TEXT("ASSET_IMPORT_FAILED");
		constexpr const TCHAR* ASSET_EXPORT_FAILED = TEXT("ASSET_EXPORT_FAILED");

		// System errors (9000-9099)
		constexpr const TCHAR* OPERATION_NOT_SUPPORTED = TEXT("OPERATION_NOT_SUPPORTED");
		constexpr const TCHAR* INTERNAL_ERROR = TEXT("INTERNAL_ERROR");
		constexpr const TCHAR* TIMEOUT = TEXT("TIMEOUT");
	}
}
