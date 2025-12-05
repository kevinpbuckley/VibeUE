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
    
    @mcp.tool(description="Enhanced Input System management: Input Actions, Mapping Contexts, Modifiers, Triggers. Services: reflection, action, mapping, modifier, trigger, ai. Actions: action_create, action_list, mapping_create_context, mapping_add_key_mapping, etc. Use get_help(topic='enhanced-input') for examples.")
    def manage_enhanced_input(
        ctx: Context,
        action: str,
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
        
        action_lower = action.lower()
        
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
        
        return result
    
    logger.info("Enhanced Input management tool registered")
