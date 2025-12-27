// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Helper macros for declaring AI tools
 * These simplify the metadata declaration for tool functions
 */

// Declare a tool class with category
#define DECLARE_TOOL_CLASS(ClassName, Category, Description) \
	UCLASS(meta = ( \
		ToolCategory = Category, \
		ToolDescription = Description \
	)) \
	class VIBEUE_API ClassName : public UObject \
	{ \
		GENERATED_BODY() \
	public:

// Declare a tool function with metadata
#define DECLARE_TOOL_FUNCTION(ToolName, Description, Category, Examples) \
	UFUNCTION(meta = ( \
		ToolName = ToolName, \
		ToolDescription = Description, \
		ToolCategory = Category, \
		ToolExamples = Examples \
	))

// Declare a tool parameter with metadata
#define DECLARE_TOOL_PARAM(ParamName, Description, Required, Type, Default) \
	UPROPERTY(meta = ( \
		ToolParamName = ParamName, \
		ToolParamDescription = Description, \
		ToolParamRequired = Required ? "true" : "false", \
		ToolParamType = Type, \
		ToolParamDefault = Default \
	))

// Simplified parameter declarations
#define TOOL_PARAM_STRING(ParamName, Description, Required) \
	DECLARE_TOOL_PARAM(ParamName, Description, Required, "string", "")

#define TOOL_PARAM_INT(ParamName, Description, Required) \
	DECLARE_TOOL_PARAM(ParamName, Description, Required, "int", "0")

#define TOOL_PARAM_FLOAT(ParamName, Description, Required) \
	DECLARE_TOOL_PARAM(ParamName, Description, Required, "float", "0.0")

#define TOOL_PARAM_BOOL(ParamName, Description, Required) \
	DECLARE_TOOL_PARAM(ParamName, Description, Required, "bool", "false")

#define TOOL_PARAM_STRING_OPTIONAL(ParamName, Description, Default) \
	DECLARE_TOOL_PARAM(ParamName, Description, false, "string", Default)

