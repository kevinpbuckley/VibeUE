# UMG Handler Refactor Plan

The following handlers in `Source/VibeUE/Private/Commands/UMGCommands.cpp` still need service-based refactors so command handlers stay thin and maintainable.

## Component Management
- `HandleAddChildToPanel`
- `HandleRemoveUMGComponent`
- `HandleSetWidgetSlotProperties`

## Discovery & Inspection
- `HandleGetWidgetBlueprintInfo`
- `HandleListWidgetComponents`
- `HandleGetWidgetComponentProperties`

## Validation
- `HandleValidateWidgetHierarchy`

## Input & Event Binding
- `HandleBindInputEvents`
- `HandleGetAvailableEvents`

We will tackle one section at a time, extracting the underlying logic into dedicated services (e.g., `FWidgetComponentService`, forthcoming inspection/validation services) and leaving each handler responsible only for parameter marshalling and JSON translation.
