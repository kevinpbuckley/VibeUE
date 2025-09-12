#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Enhanced UMG Commands with better error handling, batching, and reliability improvements
 * Addresses timeout issues and adds powerful new features for bulk operations and theming
 */
class VIBEUE_API FVibeUEUMGCommandsEnhanced
{
public:
    FVibeUEUMGCommandsEnhanced();

    /**
     * Handle enhanced UMG-related commands with improved error handling
     * @param CommandType - The type of command to handle
     * @param Params - JSON parameters for the command
     * @return JSON response with results or error
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // ===================================================================
    // CORE RELIABILITY IMPROVEMENTS
    // ===================================================================

    /**
     * Execute property operations with retry logic and batch processing
     * Addresses timeout issues identified in the issues report
     */
    TSharedPtr<FJsonObject> HandleBatchPropertyOperation(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Robust property setting with automatic fallback and error recovery
     * Handles complex widgets with multiple components more reliably
     */
    TSharedPtr<FJsonObject> HandleRobustPropertySet(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Safe property getter with timeout handling and validation
     * Provides better debugging capabilities for complex widgets
     */
    TSharedPtr<FJsonObject> HandleSafePropertyGet(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Widget validation and health check before operations
     * Prevents operations on corrupted or invalid widget states
     */
    TSharedPtr<FJsonObject> HandleValidateWidgetHealth(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // STYLE TEMPLATE SYSTEM
    // ===================================================================

    /**
     * Create reusable style templates for consistent theming
     * Template structure: { "DarkTheme": { "Background": [R,G,B,A], "Text": [R,G,B,A], ... } }
     */
    TSharedPtr<FJsonObject> HandleCreateStyleTemplate(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Apply style template to single widget or batch of widgets
     * Supports pattern matching and component filtering
     */
    TSharedPtr<FJsonObject> HandleApplyStyleTemplate(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * List all available style templates with their properties
     * Useful for discovery and debugging
     */
    TSharedPtr<FJsonObject> HandleListStyleTemplates(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Export widget styling to template format
     * Enables reverse-engineering existing widget styles
     */
    TSharedPtr<FJsonObject> HandleExportWidgetStyle(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Create comprehensive UI theme with multiple style sets
     * Includes typography, colors, spacing, and component-specific styles
     */
    TSharedPtr<FJsonObject> HandleCreateUITheme(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // BULK OPERATIONS
    // ===================================================================

    /**
     * Apply styles to multiple widgets matching criteria
     * Supports: widget type filtering, name patterns, parent hierarchy
     */
    TSharedPtr<FJsonObject> HandleBulkStyleApplication(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Batch property updates across multiple components
     * Includes transaction support and rollback on failure
     */
    TSharedPtr<FJsonObject> HandleBulkPropertyUpdate(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Mass widget creation with consistent styling
     * Useful for creating lists, grids, and repeated UI elements
     */
    TSharedPtr<FJsonObject> HandleBulkWidgetCreation(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Bulk widget transformation (position, size, visibility)
     * Supports relative and absolute positioning systems
     */
    TSharedPtr<FJsonObject> HandleBulkTransformUpdate(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // ENHANCED LAYOUT MANAGEMENT
    // ===================================================================

    /**
     * Intelligent auto-layout with predefined patterns
     * Patterns: Grid, List, Tabs, Sidebar, Dashboard, Modal
     */
    TSharedPtr<FJsonObject> HandleAutoLayout(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Responsive layout configuration with breakpoints
     * Automatically adjusts widget sizes and positions based on screen size
     */
    TSharedPtr<FJsonObject> HandleCreateResponsiveLayout(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Advanced anchor and margin management
     * Provides fine-grained control over widget positioning
     */
    TSharedPtr<FJsonObject> HandleSetAdvancedAnchoring(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Layout hierarchy analyzer and optimizer
     * Identifies and fixes common layout issues
     */
    TSharedPtr<FJsonObject> HandleAnalyzeLayout(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // ANIMATION SYSTEM
    // ===================================================================

    /**
     * Create simple widget animations (fade, slide, scale, rotate)
     * Predefined easing curves and duration control
     */
    TSharedPtr<FJsonObject> HandleCreateSimpleAnimation(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Animation sequence builder for complex multi-step animations
     * Supports parallel and sequential animation tracks
     */
    TSharedPtr<FJsonObject> HandleCreateAnimationSequence(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Bind animations to widget events (hover, click, focus)
     * Automatic animation state management
     */
    TSharedPtr<FJsonObject> HandleBindAnimationToEvent(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Animation preset library (modern UI transitions)
     * Includes: Material Design, iOS, Windows 11 style animations
     */
    TSharedPtr<FJsonObject> HandleApplyAnimationPreset(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // ADVANCED DEBUGGING AND INSPECTION
    // ===================================================================

    /**
     * Comprehensive widget property inspector
     * Lists all properties with current values, types, and descriptions
     */
    TSharedPtr<FJsonObject> HandleInspectWidgetProperties(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Widget hierarchy visualizer with tree structure
     * Shows parent-child relationships and slot properties
     */
    TSharedPtr<FJsonObject> HandleVisualizeWidgetHierarchy(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Style diff analyzer between widgets
     * Compares styling properties and highlights differences
     */
    TSharedPtr<FJsonObject> HandleCompareWidgetStyles(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Performance analyzer for complex widgets
     * Identifies heavy operations and optimization opportunities
     */
    TSharedPtr<FJsonObject> HandleAnalyzeWidgetPerformance(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // DATA BINDING ENHANCEMENTS
    // ===================================================================

    /**
     * Smart data binding with automatic type conversion
     * Supports complex data structures and nested objects
     */
    TSharedPtr<FJsonObject> HandleSmartDataBinding(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Live data preview system for testing bindings
     * Mock data injection for development and testing
     */
    TSharedPtr<FJsonObject> HandlePreviewDataBinding(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Binding validation and error detection
     * Checks for circular references and invalid binding paths
     */
    TSharedPtr<FJsonObject> HandleValidateDataBindings(const TSharedPtr<FJsonObject>& Params);

    // ===================================================================
    // HELPER STRUCTURES AND UTILITIES
    // ===================================================================

private:
    // Enhanced error handling with retry logic
    struct FOperationResult
    {
        bool bSuccess;
        FString ErrorMessage;
        int32 RetryCount;
        double ExecutionTime;
        TSharedPtr<FJsonObject> Data;
        
        FOperationResult() : bSuccess(false), RetryCount(0), ExecutionTime(0.0) {}
    };
    
    // Style template storage and management
    struct FStyleTemplate
    {
        FString Name;
        FString Description;
        TSharedPtr<FJsonObject> StyleData;
        TArray<FString> SupportedWidgetTypes;
        FDateTime CreatedTime;
        FString Author;
    };
    
    // Animation track definition
    struct FAnimationTrack
    {
        FString PropertyName;
        FString EasingType;
        float Duration;
        TSharedPtr<FJsonValue> StartValue;
        TSharedPtr<FJsonValue> EndValue;
        float Delay;
    };
    
    // Layout pattern definition
    struct FLayoutPattern
    {
        FString PatternName;
        TSharedPtr<FJsonObject> Configuration;
        TArray<FString> RequiredComponents;
        bool bSupportsResponsive;
    };

    // Static registries for enhanced functionality
    static TMap<FString, FStyleTemplate> StyleTemplateRegistry;
    static TMap<FString, TSharedPtr<FJsonObject>> UIThemeRegistry;
    static TMap<FString, FLayoutPattern> LayoutPatternRegistry;
    static TMap<FString, TSharedPtr<FJsonObject>> AnimationPresetRegistry;
    
    // Configuration and settings
    struct FEnhancedConfig
    {
        int32 MaxRetryAttempts;
        float TimeoutDuration;
        bool bEnableBatchOptimization;
        bool bEnablePerformanceTracking;
        bool bEnableDetailedLogging;
        
        FEnhancedConfig() 
            : MaxRetryAttempts(3)
            , TimeoutDuration(10.0f)
            , bEnableBatchOptimization(true)
            , bEnablePerformanceTracking(true)
            , bEnableDetailedLogging(false)
        {}
    };
    
    FEnhancedConfig Config;

    // ===================================================================
    // HELPER METHODS
    // ===================================================================
    
    /**
     * Execute operation with retry logic and timeout handling
     */
    FOperationResult ExecuteWithRetry(TFunction<FOperationResult()> Operation, int32 MaxRetries = -1);
    
    /**
     * Validate widget blueprint and component before operations
     */
    bool ValidateWidgetAndComponent(const FString& WidgetName, const FString& ComponentName, UWidget*& OutWidget, FString& OutError);
    
    /**
     * Parse complex property values with enhanced type support
     */
    bool ParseEnhancedPropertyValue(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, UWidget* Widget, FString& OutError);
    
    /**
     * Create standardized success response with timing and metadata
     */
    TSharedPtr<FJsonObject> CreateEnhancedSuccessResponse(const FString& Operation, const TSharedPtr<FJsonObject>& Data = nullptr, double ExecutionTime = 0.0);
    
    /**
     * Create standardized error response with debugging information
     */
    TSharedPtr<FJsonObject> CreateEnhancedErrorResponse(const FString& Operation, const FString& Error, int32 RetryCount = 0);
    
    /**
     * Initialize default style templates and presets
     */
    void InitializeDefaultTemplates();
    
    /**
     * Load and initialize animation presets
     */
    void InitializeAnimationPresets();
    
    /**
     * Initialize layout patterns
     */
    void InitializeLayoutPatterns();
    
    /**
     * Performance tracking and logging
     */
    void LogPerformanceMetrics(const FString& Operation, double ExecutionTime, bool bSuccess);
};
