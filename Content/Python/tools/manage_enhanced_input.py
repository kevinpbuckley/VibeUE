"""
Enhanced Input System Management Tool

Unified multi-action tool for complete Enhanced Input system control covering:
- Phase 1: Reflection infrastructure
- Phase 2: Input action and mapping management  
- Phase 3: Advanced modifier, trigger, and AI configuration

This tool provides comprehensive access to all Enhanced Input services including
action creation, mapping context management, modifier/trigger configuration, and
natural language-based action description parsing.

Architecture:
- Consolidates 7 C++ services into 1 cohesive MCP tool
- 35+ actions across reflection and management
- Full reflection-based operation (zero hardcoding of Enhanced Input types)
- Natural language parsing for action configuration via AI
"""

import logging
from typing import Dict, Any, List, Optional, Union
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP.EnhancedInput")


def _dispatch(command: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    """Dispatch command to Unreal Engine via socket connection."""
    from vibe_ue_server import get_unreal_connection
    
    unreal = get_unreal_connection()
    if not unreal:
        logger.error("Failed to connect to Unreal Engine")
        return {"success": False, "error": "Failed to connect to Unreal Engine"}
    
    logger.info(f"Dispatching {command} with payload keys: {list(payload.keys())}")
    response = unreal.send_command(command, payload)
    if not response:
        logger.error("No response from Unreal Engine")
        return {"success": False, "error": "No response from Unreal Engine"}
    
    logger.debug(f"{command} response: {response}")
    return response


def register_enhanced_input_tools(mcp: FastMCP) -> None:
    """Register unified Enhanced Input management tool with MCP server."""
    
    @mcp.tool(description="Enhanced Input System management: Input Actions, Mapping Contexts, Modifiers, Triggers. Services: reflection, action, mapping, modifier, trigger, ai. Actions: action_create, action_list, mapping_create_context, mapping_add_key_mapping, etc. Use action='help' for all actions and detailed parameter info.")
    def manage_enhanced_input(
        ctx: Context,
        action: str,
        help_action: str = "",
        service: str = "reflection",
        # Reflection & Discovery parameters
        input_type: str = "",
        include_inherited: bool = False,
        # Action management parameters
        action_name: str = "",
        action_path: str = "",
        asset_path: str = "",
        action_value_type: str = "",
        value_type: str = "",
        display_name: str = "",
        description: str = "",
        # Mapping context parameters
        context_name: str = "",
        context_path: str = "",
        priority: int = 0,
        key: str = "",
        new_name: str = "",
        mapping_index: int = 0,
        # Modifier parameters
        modifier_type: str = "",
        modifier_index: int = 0,
        modifier_config: Optional[Dict[str, Any]] = None,
        # Trigger parameters
        trigger_type: str = "",
        trigger_index: int = 0,
        trigger_config: Optional[Dict[str, Any]] = None,
        # AI Configuration parameters
        description_text: str = "",
        use_template: bool = False,
        template_name: str = "",
        # Property/inspection parameters
        property_name: str = "",
        property_value: Optional[Union[str, int, float, bool]] = None,
        # Performance parameters
        max_results: int = 100,
        include_details: bool = True,
        force_delete: bool = False,
    ) -> Dict[str, Any]:
        """Route to Enhanced Input action handlers.
        
        | mapping_get_properties | context_path | Get context properties |
        | mapping_add_modifier | context_path, mapping_index, modifier_type | Add modifier |
        | mapping_add_trigger | context_path, mapping_index, trigger_type | Add trigger |
        
        MODIFIER/TRIGGER SERVICE:
        | Action | Required Params | Description |
        |--------|-----------------|-------------|
        | modifier_discover_types | (none) | List available modifiers |
        | modifier_create_instance | modifier_type, modifier_config | Create modifier |
        | trigger_discover_types | (none) | List available triggers |
        | trigger_create_instance | trigger_type, trigger_config | Create trigger |
        
        AI SERVICE:
        | Action | Required Params | Description |
        |--------|-----------------|-------------|
        | ai_get_templates | (none) | Get FPS/RPG/TopDown templates |
        | ai_apply_template | template_name | Apply input template |
        
        PATH PARAMETERS:
        - asset_path: For CREATION (action_create, mapping_create_context)
        - action_path: Reference existing Input Action
        - context_path: Reference existing Input Mapping Context
        
        VALUE TYPES: Digital, Axis1D, Axis2D, Axis3D
        """
        
        # Handle help action
        if action and action.lower() == "help":
            from help_system import generate_help_response
            return generate_help_response("manage_enhanced_input", help_action if help_action else None)
        
        # Import error response helper
        from help_system import generate_error_response
        
        # Validate action is provided
        if not action:
            return generate_error_response(
                "manage_enhanced_input", "",
                "action is required. Use action='help' to see all available actions."
            )
        
        action_lower = action.lower()
        
        # Valid actions by service
        valid_actions = [
            # Reflection service
            "reflection_get_types", "reflection_get_properties", "reflection_get_enums",
            # Action service
            "action_create", "action_delete", "action_list", "action_get_properties", "action_set_property", "action_rename",
            # Mapping service
            "mapping_create_context", "mapping_delete_context", "mapping_list_contexts",
            "mapping_add_key_mapping", "mapping_remove_key_mapping", "mapping_get_mappings",
            "mapping_get_properties", "mapping_add_modifier", "mapping_remove_modifier",
            "mapping_add_trigger", "mapping_remove_trigger",
            # Modifier service
            "modifier_discover_types", "modifier_create_instance", "modifier_get_properties",
            # Trigger service
            "trigger_discover_types", "trigger_create_instance", "trigger_get_properties",
            # AI service
            "ai_parse_description", "ai_get_templates", "ai_apply_template"
        ]
        
        if action_lower not in valid_actions:
            return generate_error_response(
                "manage_enhanced_input", action,
                f"Invalid action '{action}'. Use action='help' to see valid actions."
            )
        
        # Action-specific validation
        missing = []
        
        # Action creation requires asset_path and value_type
        if action_lower == "action_create":
            if not asset_path:
                missing.append("asset_path")
            if not value_type and not action_value_type:
                missing.append("value_type")
            if missing:
                return generate_error_response(
                    "manage_enhanced_input", action,
                    f"action_create requires: {', '.join(missing)}. Value types: Digital, Axis1D, Axis2D, Axis3D",
                    missing_params=missing
                )
        
        # Action operations require action_path
        if action_lower in ["action_delete", "action_get_properties", "action_set_property", "action_rename"]:
            if not action_path:
                return generate_error_response(
                    "manage_enhanced_input", action,
                    f"{action} requires 'action_path' (full path to existing Input Action)",
                    missing_params=["action_path"]
                )
        
        # Mapping context creation requires asset_path
        if action_lower == "mapping_create_context":
            if not asset_path:
                return generate_error_response(
                    "manage_enhanced_input", action,
                    "mapping_create_context requires 'asset_path'",
                    missing_params=["asset_path"]
                )
        
        # Mapping operations require context_path
        mapping_actions = ["mapping_add_key_mapping", "mapping_remove_key_mapping", "mapping_get_mappings",
                          "mapping_get_properties", "mapping_add_modifier", "mapping_remove_modifier",
                          "mapping_add_trigger", "mapping_remove_trigger", "mapping_delete_context"]
        if action_lower in mapping_actions and not context_path:
            return generate_error_response(
                "manage_enhanced_input", action,
                f"{action} requires 'context_path' (full path to existing Input Mapping Context)",
                missing_params=["context_path"]
            )
        
        # Key mapping requires action_path and key
        if action_lower == "mapping_add_key_mapping":
            if not action_path:
                missing.append("action_path")
            if not key:
                missing.append("key")
            if missing:
                return generate_error_response(
                    "manage_enhanced_input", action,
                    f"mapping_add_key_mapping requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        # Template application
        if action_lower == "ai_apply_template":
            if not template_name:
                return generate_error_response(
                    "manage_enhanced_input", action,
                    "ai_apply_template requires 'template_name'. Use ai_get_templates to see available templates.",
                    missing_params=["template_name"]
                )
        
        # Debug logging
        logger.info(f"manage_enhanced_input called with action={action_lower}, service={service}")
        logger.info(f"action_name={action_name}, asset_path={asset_path}, value_type={value_type}")
        
        # Build command payload
        payload = {
            "action": action_lower,
            "service": service,
        }
        
        # Add optional parameters that are set
        if action_name:
            payload["action_name"] = action_name
        if action_path:
            payload["action_path"] = action_path
        if asset_path:
            payload["asset_path"] = asset_path
        if action_value_type:
            payload["action_value_type"] = action_value_type
        if value_type:
            payload["value_type"] = value_type
        if context_name:
            payload["context_name"] = context_name
        if context_path:
            payload["context_path"] = context_path
        if key:
            payload["key"] = key
        if new_name:
            payload["new_name"] = new_name
        if mapping_index >= 0:
            payload["mapping_index"] = mapping_index
        if modifier_type:
            payload["modifier_type"] = modifier_type
        if trigger_type:
            payload["trigger_type"] = trigger_type
        if description_text:
            payload["description_text"] = description_text
        if template_name:
            payload["template_name"] = template_name
        if property_name:
            payload["property_name"] = property_name
        if property_value is not None:
            payload["property_value"] = property_value
        if modifier_config:
            payload["modifier_config"] = modifier_config
        if trigger_config:
            payload["trigger_config"] = trigger_config
        if display_name:
            payload["display_name"] = display_name
        if description:
            payload["description"] = description
        if priority > 0:
            payload["priority"] = priority
        if modifier_index > 0:
            payload["modifier_index"] = modifier_index
        if trigger_index > 0:
            payload["trigger_index"] = trigger_index
        if max_results != 100:
            payload["max_results"] = max_results
        if not include_inherited:
            payload["include_inherited"] = include_inherited
        if not include_details:
            payload["include_details"] = include_details
        if use_template:
            payload["use_template"] = use_template
        if force_delete:
            payload["force_delete"] = force_delete
        
        # Dispatch to Unreal
        result = _dispatch("manage_enhanced_input", payload)
        
        # Log result
        if result.get("success"):
            logger.info(f"Enhanced Input operation successful: {action}")
        else:
            logger.warning(f"Enhanced Input operation failed: {result.get('error', 'Unknown error')}")
            # Add help tips to failed responses
            if not result.get("help_tip"):
                result["help_tip"] = f"Use manage_enhanced_input(action='help', help_action='{action}') to see correct parameters for this action."
                result["general_help"] = "Use manage_enhanced_input(action='help') to see all available actions."
        
        return result
    
    logger.info("Enhanced Input management tool registered")
