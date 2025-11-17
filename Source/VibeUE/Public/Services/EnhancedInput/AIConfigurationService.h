// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"

class UInputAction;
class UInputMappingContext;
class UInputModifier;
class UInputTrigger;

/**
 * @struct FParsedConfigurationResult
 * @brief Result from parsing natural language descriptions
 */
struct FParsedConfigurationResult
{
	/** Resolved class or template name */
	FString ClassName;
	
	/** Configuration properties as key-value pairs */
	TMap<FString, FString> Properties;
};

/**
 * AI-Assisted Configuration Service
 * 
 * Provides natural language configuration and AI-powered setup assistance
 * for input actions, modifiers, and triggers using template-based generation.
 * 
 * CRITICAL DESIGN PRINCIPLE: ZERO HARDCODING
 * - All configurations use reflection-based discovery
 * - All templates reference reflection metadata
 * - No hardcoded configuration patterns
 */
class VIBEUE_API FAIConfigurationService : public FServiceBase
{
public:
	explicit FAIConfigurationService(TSharedPtr<class FServiceContext> Context);
	virtual ~FAIConfigurationService() = default;

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("AIConfigurationService"); }
	virtual void Initialize() override;
	virtual void Shutdown() override;

	// ═══════════════════════════════════════════════════════════════════
	// Natural Language Configuration
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Parse natural language description and generate input action configuration
	 * @param Description - Natural language description (e.g., "quick double-click fire button")
	 * @return Generated configuration as key-value pairs
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<TMap<FString, FString>> ParseActionDescription(const FString& Description);

	/**
	 * Parse natural language description and generate modifier configuration
	 * @param Description - Natural language description (e.g., "smooth joystick with deadzone")
	 * @return Modifier class and property configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<FParsedConfigurationResult> ParseModifierDescription(const FString& Description);

	/**
	 * Parse natural language description and generate trigger configuration
	 * @param Description - Natural language description (e.g., "hold for 0.5 seconds")
	 * @return Trigger class and property configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<FParsedConfigurationResult> ParseTriggerDescription(const FString& Description);

	/**
	 * Get available configuration templates for common input scenarios
	 * @param Category - Template category (e.g., "movement", "combat", "ui")
	 * @return Array of template descriptions and their configurations
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<TArray<FString>> GetConfigurationTemplates(const FString& Category);

	/**
	 * Apply a preset template configuration
	 * @param TemplateName - Name of the template
	 * @param Action - Target input action
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<bool> ApplyConfigurationTemplate(const FString& TemplateName, UInputAction* Action);

	/**
	 * Generate optimization suggestions for current configuration
	 * @param Action - Input action to analyze
	 * @param Modifiers - Current modifiers
	 * @param Triggers - Current triggers
	 * @return Array of optimization suggestions
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<TArray<FString>> GenerateOptimizationSuggestions(UInputAction* Action, 
		const TArray<UInputModifier*>& Modifiers, 
		const TArray<UInputTrigger*>& Triggers);

	/**
	 * Create a preset from current configuration
	 * @param PresetName - Name for the preset
	 * @param Action - Input action
	 * @param Modifiers - Modifiers in the configuration
	 * @param Triggers - Triggers in the configuration
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<bool> CreateConfigurationPreset(const FString& PresetName, UInputAction* Action,
		const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);

	/**
	 * Get available presets
	 * @return Array of preset names
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<TArray<FString>> GetAvailablePresets();

	/**
	 * Load a preset configuration
	 * @param PresetName - Name of preset to load
	 * @return Preset configuration data
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|AI Configuration")
	TResult<TMap<FString, FString>> LoadPreset(const FString& PresetName);
};

