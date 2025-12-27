// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Parameter validation utilities for MCP command handlers.
 * Provides consistent error messages that list valid parameters to help LLMs self-correct.
 */
namespace ParamValidation
{
	/**
	 * Check if any of the specified parameters exist in the JSON object
	 * @param Params The JSON parameters object
	 * @param ParamNames Array of parameter names to check
	 * @return true if at least one parameter exists
	 */
	inline bool HasAnyParam(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& ParamNames)
	{
		if (!Params.IsValid())
		{
			return false;
		}
		
		for (const FString& Param : ParamNames)
		{
			if (Params->HasField(Param))
			{
				return true;
			}
		}
		return false;
	}
	
	/**
	 * Check if a specific parameter exists and has a non-empty string value
	 * @param Params The JSON parameters object
	 * @param ParamName The parameter name to check
	 * @return true if parameter exists and has non-empty string value
	 */
	inline bool HasStringParam(const TSharedPtr<FJsonObject>& Params, const FString& ParamName)
	{
		if (!Params.IsValid())
		{
			return false;
		}
		
		FString Value;
		return Params->TryGetStringField(ParamName, Value) && !Value.IsEmpty();
	}
	
	/**
	 * Check if any of the specified string parameters exist with non-empty values
	 * @param Params The JSON parameters object
	 * @param ParamNames Array of parameter names to check
	 * @return true if at least one parameter has a non-empty string value
	 */
	inline bool HasAnyStringParam(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& ParamNames)
	{
		if (!Params.IsValid())
		{
			return false;
		}
		
		for (const FString& Param : ParamNames)
		{
			FString Value;
			if (Params->TryGetStringField(Param, Value) && !Value.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}
	
	/**
	 * Build an error message that lists valid parameters
	 * @param Description What's missing or wrong
	 * @param ValidParams Array of valid parameter names
	 * @return Formatted error message
	 */
	inline FString BuildError(const FString& Description, const TArray<FString>& ValidParams)
	{
		return FString::Printf(TEXT("%s. Valid parameters: %s"), 
			*Description, *FString::Join(ValidParams, TEXT(", ")));
	}
	
	/**
	 * Create a standard error response JSON object
	 * @param Code Error code (e.g., "MISSING_PARAMS")
	 * @param Message Error message
	 * @return JSON response object with success=false
	 */
	inline TSharedPtr<FJsonObject> ErrorResponse(const FString& Code, const FString& Message)
	{
		TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error_code"), Code);
		Response->SetStringField(TEXT("error"), Message);
		return Response;
	}
	
	/**
	 * Create a missing params error response
	 * @param Description What's missing
	 * @param ValidParams Array of valid parameter names
	 * @return JSON error response
	 */
	inline TSharedPtr<FJsonObject> MissingParamsError(const FString& Description, const TArray<FString>& ValidParams)
	{
		return ErrorResponse(TEXT("MISSING_PARAMS"), BuildError(Description, ValidParams));
	}
	
	// =========================================================================
	// Common parameter sets for reuse
	// =========================================================================
	
	/** Blueprint identifier parameters */
	inline const TArray<FString>& BlueprintIdentifierParams()
	{
		static const TArray<FString> Params = {
			TEXT("blueprint_name"), TEXT("blueprint_path")
		};
		return Params;
	}
	
	/** Check if blueprint identifier is present */
	inline bool HasBlueprintIdentifier(const TSharedPtr<FJsonObject>& Params)
	{
		return HasAnyStringParam(Params, BlueprintIdentifierParams());
	}
	
	/** Asset identifier parameters */
	inline const TArray<FString>& AssetIdentifierParams()
	{
		static const TArray<FString> Params = {
			TEXT("asset_path"), TEXT("asset_name")
		};
		return Params;
	}
	
	/** Check if asset identifier is present */
	inline bool HasAssetIdentifier(const TSharedPtr<FJsonObject>& Params)
	{
		return HasAnyStringParam(Params, AssetIdentifierParams());
	}
	
	/** Material identifier parameters */
	inline const TArray<FString>& MaterialIdentifierParams()
	{
		static const TArray<FString> Params = {
			TEXT("material_path"), TEXT("material_name")
		};
		return Params;
	}
	
	/** Check if material identifier is present */
	inline bool HasMaterialIdentifier(const TSharedPtr<FJsonObject>& Params)
	{
		return HasAnyStringParam(Params, MaterialIdentifierParams());
	}
}
