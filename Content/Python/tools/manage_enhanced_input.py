"""
Enhanced Input System Management Tool

Unified multi-action tool for complete Enhanced Input system control covering:
- Phase 1: Reflection and discovery infrastructure
- Phase 2: Input action and mapping management  
- Phase 3: Advanced modifier, trigger, and AI configuration

This tool provides comprehensive access to all Enhanced Input services including
action creation, mapping context management, modifier/trigger configuration, and
natural language-based action description parsing.

Architecture:
- Consolidates 9 C++ services into 1 cohesive MCP tool
- 40+ actions across reflection, discovery, validation, and management
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
    
    @mcp.tool()
    def manage_enhanced_input(
        ctx: Context,
        action: str,
        service: str = "reflection",
        # Reflection & Discovery parameters
        input_type: str = "",
        include_inherited: bool = False,
        # Action management parameters
        action_name: str = "",
        action_value_type: str = "",
        display_name: str = "",
        description: str = "",
        # Mapping context parameters
        context_name: str = "",
        priority: int = 0,
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
    ) -> Dict[str, Any]:
        """
        Enhanced Input System Management Tool
        
        Unified interface for complete Enhanced Input system control with 40+ actions across
        all phases of development. Supports action creation, mapping management, advanced
        modifier/trigger configuration, and natural language-based AI integration.
        
        ============================================================================
        PHASE 1: REFLECTION & DISCOVERY INFRASTRUCTURE
        ============================================================================
        
        **Reflection Service - Type Discovery & Metadata**
        
        **action**: "reflection_discover_types"
        Discover all available Enhanced Input types (Actions, Modifiers, Triggers, etc.)
        ```python
        manage_enhanced_input(
            action="reflection_discover_types",
            service="reflection",
            input_type="modifier",  # or "trigger", "action", "context"
            include_inherited=True
        )
        # Returns: List of available types with inheritance hierarchy
        ```
        
        **action**: "reflection_get_metadata"
        Get detailed metadata for a specific Enhanced Input type
        ```python
        manage_enhanced_input(
            action="reflection_get_metadata",
            service="reflection",
            input_type="modifier",
            property_name="FInputModifierSwizzleAxis"
        )
        # Returns: Type hierarchy, properties, defaults, constraints
        ```
        
        **Discovery Service - Type Enumeration & Analysis**
        
        **action**: "discovery_enumerate_modifiers"
        List all available modifier types with categorization
        ```python
        manage_enhanced_input(
            action="discovery_enumerate_modifiers",
            service="discovery"
        )
        # Returns: Organized list of all modifier classes
        ```
        
        **action**: "discovery_enumerate_triggers"
        List all available trigger types with categorization
        ```python
        manage_enhanced_input(
            action="discovery_enumerate_triggers",
            service="discovery"
        )
        # Returns: Organized list of all trigger classes
        ```
        
        **action**: "discovery_get_modifier_info"
        Get comprehensive information about a specific modifier type
        ```python
        manage_enhanced_input(
            action="discovery_get_modifier_info",
            service="discovery",
            modifier_type="SwizzleAxis"
        )
        # Returns: Properties, default values, valid configurations
        ```
        
        **action**: "discovery_get_trigger_info"
        Get comprehensive information about a specific trigger type
        ```python
        manage_enhanced_input(
            action="discovery_get_trigger_info",
            service="discovery",
            trigger_type="Pressed"
        )
        # Returns: Properties, default values, valid configurations
        ```
        
        **Validation Service - Configuration Integrity**
        
        **action**: "validation_check_action"
        Validate action configuration before creation/modification
        ```python
        manage_enhanced_input(
            action="validation_check_action",
            service="validation",
            action_name="TestAction",
            action_value_type="Axis1D"
        )
        # Returns: Valid/invalid status with specific issues
        ```
        
        **action**: "validation_check_mapping"
        Validate mapping context configuration
        ```python
        manage_enhanced_input(
            action="validation_check_mapping",
            service="validation",
            context_name="TestContext",
            priority=0
        )
        # Returns: Configuration validity status
        ```
        
        ============================================================================
        PHASE 2: INPUT ACTION & MAPPING CONTEXT MANAGEMENT
        ============================================================================
        
        **Action Service - Complete Lifecycle**
        
        **action**: "action_create"
        Create new Input Action asset
        ```python
        manage_enhanced_input(
            action="action_create",
            service="action",
            action_name="IA_Move",
            action_value_type="Value1D",
            display_name="Move",
            description="Movement input action"
        )
        # Returns: Path to created asset, success status
        ```
        
        **action**: "action_list"
        List all Input Actions in project
        ```python
        manage_enhanced_input(
            action="action_list",
            service="action"
        )
        # Returns: All actions with paths and value types
        ```
        
        **action**: "action_delete"
        Remove Input Action asset
        ```python
        manage_enhanced_input(
            action="action_delete",
            service="action",
            action_name="IA_OldAction"
        )
        # Returns: Deletion status and cascading effects
        ```
        
        **action**: "action_configure"
        Modify Input Action properties
        ```python
        manage_enhanced_input(
            action="action_configure",
            service="action",
            action_name="IA_Move",
            display_name="Movement",
            property_name="bConsumeInput",
            property_value=True
        )
        # Returns: Configuration result
        ```
        
        **action**: "action_get_properties"
        Retrieve all properties of an Input Action
        ```python
        manage_enhanced_input(
            action="action_get_properties",
            service="action",
            action_name="IA_Move"
        )
        # Returns: Complete property set with current values
        ```
        
        **Mapping Service - Context & Key Binding**
        
        **action**: "mapping_create_context"
        Create new Input Mapping Context
        ```python
        manage_enhanced_input(
            action="mapping_create_context",
            service="mapping",
            context_name="IMC_Combat",
            priority=1,
            display_name="Combat Controls"
        )
        # Returns: Path to created context asset
        ```
        
        **action**: "mapping_list_contexts"
        List all Input Mapping Contexts
        ```python
        manage_enhanced_input(
            action="mapping_list_contexts",
            service="mapping"
        )
        # Returns: All contexts with paths and priority
        ```
        
        **action**: "mapping_delete_context"
        Remove Input Mapping Context
        ```python
        manage_enhanced_input(
            action="mapping_delete_context",
            service="mapping",
            context_name="IMC_OldContext"
        )
        # Returns: Deletion status
        ```
        
        **action**: "mapping_add_key_mapping"
        Add key binding to mapping context
        ```python
        manage_enhanced_input(
            action="mapping_add_key_mapping",
            service="mapping",
            context_name="IMC_Combat",
            action_name="IA_Attack",
            property_name="Key",
            property_value="LeftMouseButton"
        )
        # Returns: Mapping result
        ```
        
        **action**: "mapping_get_mappings"
        Get all key mappings in a context
        ```python
        manage_enhanced_input(
            action="mapping_get_mappings",
            service="mapping",
            context_name="IMC_Combat"
        )
        # Returns: List of key mappings and bindings
        ```
        
        ============================================================================
        PHASE 3: ADVANCED CONFIGURATION & AI INTEGRATION
        ============================================================================
        
        **Advanced Modifier Service - Deep Configuration**
        
        **action**: "modifier_discover_types"
        Discover all available modifier types (same as discovery service)
        ```python
        manage_enhanced_input(
            action="modifier_discover_types",
            service="modifier"
        )
        # Returns: Complete modifier type inventory
        ```
        
        **action**: "modifier_get_metadata"
        Get detailed metadata for modifier type
        ```python
        manage_enhanced_input(
            action="modifier_get_metadata",
            service="modifier",
            modifier_type="Swizzle"
        )
        # Returns: Properties, constraints, recommended values
        ```
        
        **action**: "modifier_create_instance"
        Instantiate modifier with configuration
        ```python
        manage_enhanced_input(
            action="modifier_create_instance",
            service="modifier",
            modifier_type="Swizzle",
            modifier_config={"SwizzleY": {"Type": "Negate"}, "SwizzleZ": {"Type": "Negate"}}
        )
        # Returns: Modifier instance reference
        ```
        
        **action**: "modifier_configure_advanced"
        Configure modifier with advanced settings
        ```python
        manage_enhanced_input(
            action="modifier_configure_advanced",
            service="modifier",
            modifier_index=0,
            modifier_config={"param": "value"}
        )
        # Returns: Configuration result
        ```
        
        **action**: "modifier_optimize_stack"
        Analyze and optimize modifier stack for performance
        ```python
        manage_enhanced_input(
            action="modifier_optimize_stack",
            service="modifier"
        )
        # Returns: Optimization suggestions and impact analysis
        ```
        
        **action**: "modifier_clone"
        Clone modifier configuration to new instance
        ```python
        manage_enhanced_input(
            action="modifier_clone",
            service="modifier",
            modifier_index=0
        )
        # Returns: New modifier instance reference
        ```
        
        **Advanced Trigger Service - Trigger Analysis**
        
        **action**: "trigger_discover_types"
        Discover all available trigger types
        ```python
        manage_enhanced_input(
            action="trigger_discover_types",
            service="trigger"
        )
        # Returns: Complete trigger type inventory
        ```
        
        **action**: "trigger_get_metadata"
        Get detailed metadata for trigger type
        ```python
        manage_enhanced_input(
            action="trigger_get_metadata",
            service="trigger",
            trigger_type="Pressed"
        )
        # Returns: Properties, constraints, recommended values
        ```
        
        **action**: "trigger_create_instance"
        Instantiate trigger with configuration
        ```python
        manage_enhanced_input(
            action="trigger_create_instance",
            service="trigger",
            trigger_type="Pressed",
            trigger_config={}
        )
        # Returns: Trigger instance reference
        ```
        
        **action**: "trigger_analyze_performance"
        Analyze trigger performance characteristics
        ```python
        manage_enhanced_input(
            action="trigger_analyze_performance",
            service="trigger",
            trigger_index=0
        )
        # Returns: Performance metrics and bottleneck analysis
        ```
        
        **action**: "trigger_detect_conflicts"
        Detect conflicting trigger configurations
        ```python
        manage_enhanced_input(
            action="trigger_detect_conflicts",
            service="trigger"
        )
        # Returns: List of potential conflicts with resolutions
        ```
        
        **action**: "trigger_clone"
        Clone trigger configuration to new instance
        ```python
        manage_enhanced_input(
            action="trigger_clone",
            service="trigger",
            trigger_index=0
        )
        # Returns: New trigger instance reference
        ```
        
        **AI Configuration Service - Natural Language Integration**
        
        **action**: "ai_parse_action_description"
        Parse natural language action description
        ```python
        manage_enhanced_input(
            action="ai_parse_action_description",
            service="ai",
            description_text="Create a 2D movement action for cardinal directions with value clamping"
        )
        # Returns: Parsed configuration: {ClassName, Properties, Recommendations}
        ```
        
        **action**: "ai_parse_modifier_description"
        Parse natural language modifier description
        ```python
        manage_enhanced_input(
            action="ai_parse_modifier_description",
            service="ai",
            description_text="Normalize stick input and add dead zone of 0.2"
        )
        # Returns: Parsed modifier config with properties
        ```
        
        **action**: "ai_parse_trigger_description"
        Parse natural language trigger description
        ```python
        manage_enhanced_input(
            action="ai_parse_trigger_description",
            service="ai",
            description_text="Trigger on left mouse button press"
        )
        # Returns: Parsed trigger config with properties
        ```
        
        **action**: "ai_get_templates"
        Get AI configuration templates for quick setup
        ```python
        manage_enhanced_input(
            action="ai_get_templates",
            service="ai"
        )
        # Returns: Available templates: FPS, RPG, TopDown, etc.
        ```
        
        **action**: "ai_apply_template"
        Apply pre-configured template to setup
        ```python
        manage_enhanced_input(
            action="ai_apply_template",
            service="ai",
            use_template=True,
            template_name="FPS"
        )
        # Returns: Applied configuration
        ```
        
        ============================================================================
        PARAMETERS REFERENCE
        ============================================================================
        
        **Service Selection:**
        - "reflection" - Type discovery and metadata reflection
        - "discovery" - Enhanced type enumeration
        - "validation" - Configuration validation
        - "action" - Input Action lifecycle
        - "mapping" - Mapping Context management
        - "modifier" - Advanced modifier configuration
        - "trigger" - Advanced trigger configuration
        - "ai" - Natural language and template integration
        
        **Common Parameters:**
        - action: Action name (lowercase, underscore-separated)
        - input_type: Enhanced Input class ("modifier", "trigger", "action", "context")
        - include_inherited: Include base class types
        - max_results: Limit result count
        - include_details: Include detailed metadata
        
        **Configuration Parameters:**
        - modifier_config: Dict with modifier properties
        - trigger_config: Dict with trigger properties
        - property_name/property_value: Single property modification
        
        **Status Codes:**
        - success: true/false
        - error: Error message if applicable
        """
        
        action_lower = action.lower()
        
        # Build command payload
        payload = {
            "action": action_lower,
            "service": service,
        }
        
        # Add optional parameters that are set
        if action_name:
            payload["action_name"] = action_name
        if action_value_type:
            payload["action_value_type"] = action_value_type
        if context_name:
            payload["context_name"] = context_name
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
        
        # Dispatch to Unreal
        result = _dispatch("manage_enhanced_input", payload)
        
        # Log result
        if result.get("success"):
            logger.info(f"Enhanced Input operation successful: {action}")
        else:
            logger.warning(f"Enhanced Input operation failed: {result.get('error', 'Unknown error')}")
        
        return result
    
    logger.info("Enhanced Input management tool registered")
