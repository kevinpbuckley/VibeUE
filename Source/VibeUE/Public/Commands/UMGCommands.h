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
 * adding widget components
 */
class VIBEUE_API FUMGCommands
{
public:
    FUMGCommands();

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
     * Add a Button widget to a UMG Widget Blueprint
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name for the new Button
     *                "text" - Button text
     *                "position" - [X, Y] position in the canvas
     * @return JSON response with the added widget details
     */
    TSharedPtr<FJsonObject> HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params);

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
    
    // UMG Layout Methods
    TSharedPtr<FJsonObject> HandleAddCanvasPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddSizeBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddOverlay(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddHorizontalBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddVerticalBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddScrollBox(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddGridPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params);  // Universal component removal
    TSharedPtr<FJsonObject> HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params);

    // Property Management Methods
    TSharedPtr<FJsonObject> HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params);
    // set_widget_transform/visibility/z_order removed

    // Event and Data Binding Methods (Active)
    TSharedPtr<FJsonObject> HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params);

    // List and View Methods (Active)
    // add_list_view/add_tile_view/add_tree_view removed

    // Advanced Widget Methods (Active)
    TSharedPtr<FJsonObject> HandleAddWidgetSwitcher(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params);

    // NEW: Deletion Methods (Active)
    TSharedPtr<FJsonObject> HandleDeleteWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

private:
    
    // Helper method to validate property before setting - Added based on Issues Report
    static bool ValidatePropertyValue(UWidget* Widget, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& ErrorMessage);
    
    // Helper method to batch property operations - Added based on Issues Report
    static bool SetPropertiesWithRetry(UWidget* Widget, const TArray<TPair<FString, TSharedPtr<FJsonValue>>>& Properties, TArray<FString>& Errors);
}; 
