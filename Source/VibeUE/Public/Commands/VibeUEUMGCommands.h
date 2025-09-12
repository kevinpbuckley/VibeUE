#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;
class UUserWidget;

/**
 * Handles UMG (Widget Blueprint) related MCP commands
 * Responsible for creating and modifying UMG Widget Blueprints,
 * adding widget components, and managing widget instances in the viewport.
 */
class VIBEUE_API FVibeUEUMGCommands
{
public:
    FVibeUEUMGCommands();

    /**
     * Handle UMG-related commands
     * @param CommandType - The type of command to handle
     * @param Params - JSON parameters for the command
     * @return JSON response with results or error
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Create a new UMG Widget Blueprint
     * @param Params - Must include "name" for the blueprint name
     * @return JSON response with the created blueprint details
     */
    TSharedPtr<FJsonObject> HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a Text Block widget to a UMG Widget Blueprint
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name for the new Text Block
     *                "text" - Initial text content (optional)
     *                "position" - [X, Y] position in the canvas (optional)
     * @return JSON response with the added widget details
     */
    TSharedPtr<FJsonObject> HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a widget instance to the game viewport
     * @param Params - Must include:
     *                "blueprint_name" - Name of the Widget Blueprint to instantiate
     *                "z_order" - Z-order for widget display (optional)
     * @return JSON response with the widget instance details
     */
    TSharedPtr<FJsonObject> HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a Button widget to a UMG Widget Blueprint
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name for the new Button
     *                "text" - Button text
     *                "position" - [X, Y] position in the canvas
     * @return JSON response with the added widget details
     */
    TSharedPtr<FJsonObject> HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * Bind an event to a widget (e.g. button click)
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name of the widget to bind
     *                "event_name" - Name of the event to bind
     * @return JSON response with the binding details
     */
    TSharedPtr<FJsonObject> HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set up text block binding for dynamic updates
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name of the widget to bind
     *                "binding_name" - Name of the binding to set up
     * @return JSON response with the binding details
     */
    TSharedPtr<FJsonObject> HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params);

    // UMG Discovery Methods
    TSharedPtr<FJsonObject> HandleSearchItems(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetBlueprintInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListWidgetComponents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params);

    // UMG Component Methods
    TSharedPtr<FJsonObject> HandleAddEditableText(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddEditableTextBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddRichTextBlock(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddCheckBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddSlider(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddProgressBar(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddImage(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddSpacer(const TSharedPtr<FJsonObject>& Params);
    
    /**
     * Remove a widget component from a UMG Widget Blueprint
     * @param Params - Must include:
     *                "widget_name" - Name of the target Widget Blueprint
     *                "component_name" - Name of the component to remove
     *                "confirm_removal" - Safety confirmation (optional, default: true)
     * @return JSON response with the removal status
     */
    TSharedPtr<FJsonObject> HandleRemoveWidgetComponent(const TSharedPtr<FJsonObject>& Params);

    // UMG Layout Methods
    TSharedPtr<FJsonObject> HandleAddCanvasPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddOverlay(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBorder(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddHorizontalBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddVerticalBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddScrollBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddGridPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveChildFromPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params);

    // Property Management Methods
    TSharedPtr<FJsonObject> HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetVisibility(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetZOrder(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetFont(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetAlignment(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetSizeToContent(const TSharedPtr<FJsonObject>& Params);

    // AI Guidance Methods
    TSharedPtr<FJsonObject> HandleGetBackgroundColorGuide(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetHierarchyGuide(const TSharedPtr<FJsonObject>& Params);

    // PROPERTY VALIDATION - Added based on Issues Report  
    TSharedPtr<FJsonObject> HandleValidateWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAllAvailableProperties(const TSharedPtr<FJsonObject>& Params);
    
    // IMPROVED ERROR HANDLING - Added based on Issues Report
    TSharedPtr<FJsonObject> HandleDiagnoseWidgetIssues(const TSharedPtr<FJsonObject>& Params);

    // Event and Data Binding Methods
    TSharedPtr<FJsonObject> HandleBindWidgetEventToCpp(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBlueprintFunctionForEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindPropertyToCppVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateCustomEventDelegate(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleUnbindWidgetEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindWidgetToDataSource(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateDataBindingContext(const TSharedPtr<FJsonObject>& Params);

    // List and View Methods
    TSharedPtr<FJsonObject> HandleSetupListItemTemplate(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddListView(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddTileView(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddTreeView(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePopulateListWithData(const TSharedPtr<FJsonObject>& Params);

    // Advanced Widget Methods
    TSharedPtr<FJsonObject> HandleAddWidgetSwitcher(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateWidgetWithParent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateNestedLayout(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRefreshWidget(const TSharedPtr<FJsonObject>& Params);

private:
    
    // Helper method to validate property before setting - Added based on Issues Report
    static bool ValidatePropertyValue(UWidget* Widget, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& ErrorMessage);
    
    // Helper method to batch property operations - Added based on Issues Report
    static bool SetPropertiesWithRetry(UWidget* Widget, const TArray<TPair<FString, TSharedPtr<FJsonValue>>>& Properties, TArray<FString>& Errors);
}; 
