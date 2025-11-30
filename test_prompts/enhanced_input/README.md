# Enhanced Input MCP Tool - Test Prompts Guide

This folder contains comprehensive test prompts for the VibeUE Enhanced Input MCP tool. These prompts are designed to test all 40+ actions across the three phases of the Enhanced Input system implementation.

## File Organization

### 01_phase1_reflection_discovery.md
Tests for Phase 1 services focusing on reflection infrastructure, type discovery, and validation:
- Reflection Service: Type and metadata discovery
- Discovery Service: Comprehensive enumeration of modifiers and triggers
- Validation Service: Configuration integrity checking
- **Topics:** 24 test prompts covering type discovery, metadata retrieval, and validation workflows

### 02_phase2_action_mapping.md
Tests for Phase 2 services covering input action and mapping context management:
- Input Action Service: Complete lifecycle (create, list, configure, delete)
- Input Mapping Service: Context and key binding management
- Key Mapping Operations: Setting up specific key-to-action bindings
- **Topics:** 45+ test prompts covering basic input setup through complex workflow scenarios

### 03_phase3_advanced_ai.md
Tests for Phase 3 services providing advanced configuration and natural language AI integration:
- Advanced Modifier Service: Modifier discovery, creation, optimization, and comparison
- Advanced Trigger Service: Trigger configuration, performance analysis, and conflict detection
- AI Configuration Service: Natural language parsing, template application, and recommendations
- **Topics:** 60+ test prompts covering modifier/trigger configuration, AI-assisted workflows, performance optimization, and template-based system generation

## How to Use These Prompts

### For Manual Testing
1. Read a prompt from the appropriate phase file
2. Use the prompt text as-is or adapt it to your needs
3. Pass it to the Enhanced Input MCP tool via the manage_enhanced_input action
4. Verify the response matches expected behavior

### For AI Assistant Testing
1. Copy the prompt text verbatim
2. Ask your AI assistant (Claude, GPT, etc.) to interpret the human language
3. Have it call the appropriate manage_enhanced_input actions
4. Verify the results make sense for the requested task

### For Regression Testing
1. Run all prompts periodically to ensure no functionality breaks
2. Compare results against expected behaviors
3. Document any deviations or edge cases found

## Test Coverage by Phase

### Phase 1: Reflection & Discovery
**Status:** Discovery, Analysis, and Validation
**Actions Tested:**
- reflection_discover_types
- reflection_get_metadata
- discovery_enumerate_modifiers
- discovery_enumerate_triggers
- discovery_get_modifier_info
- discovery_get_trigger_info
- validation_check_action
- validation_check_mapping

**Prompt Count:** 24 prompts
**Scenarios:** Type discovery, metadata retrieval, validation workflows

### Phase 2: Action & Mapping Management
**Status:** Lifecycle Management and Configuration
**Actions Tested:**
- action_create
- action_list
- action_configure
- action_get_properties
- mapping_create_context
- mapping_list_contexts
- mapping_add_key_mapping
- mapping_get_mappings
- mapping_get_properties
- mapping_update_context

**Note:** For delete/duplicate operations, use `manage_asset(action="delete"/"duplicate")` instead.

**Prompt Count:** 45+ prompts
**Scenarios:** Input action setup, context management, key binding, complex workflows, edge cases

### Phase 3: Advanced Configuration & AI
**Status:** Advanced Configuration, Performance Analysis, and AI Integration
**Actions Tested:**
- modifier_discover_types
- modifier_get_metadata
- modifier_create_instance
- modifier_configure_advanced
- modifier_optimize_stack
- modifier_clone
- modifier_compare
- trigger_discover_types
- trigger_get_metadata
- trigger_create_instance
- trigger_analyze_performance
- trigger_detect_conflicts
- trigger_clone
- trigger_compare
- ai_parse_action_description
- ai_parse_modifier_description
- ai_parse_trigger_description
- ai_get_templates
- ai_apply_template

**Prompt Count:** 60+ prompts
**Scenarios:** Advanced configuration, performance optimization, AI-assisted workflows, template generation, best practices

## Prompt Characteristics

All prompts in this folder follow these guidelines:

### Generic Language
- Written in natural, conversational human language
- No technical jargon unless necessary for clarity
- Prompts describe the intent/goal rather than the implementation
- Suitable for both direct testing and AI interpretation

### Comprehensive Coverage
- Each file covers its phase completely
- Multiple variations of similar operations for edge case testing
- Real-world use cases and game genre examples
- Performance and optimization scenarios

### Workflow-Based
- Some prompts test individual actions
- Others test complete workflows combining multiple actions
- Combined prompts show how services work together
- Edge cases and error scenarios included

### AI-Friendly
- Prompts can be used directly with AI assistants
- Clear intent stated in each prompt
- AI can decide optimal action sequence
- Multiple valid interpretations encouraged

## Example Usage Scenarios

### Scenario 1: Manual Testing
```
Tester reads: "Create a basic input action for player movement called IA_Move 
that handles 2D input for forward/back and left/right movement."

Tester calls: manage_enhanced_input(
    action="action_create",
    service="action",
    action_name="IA_Move",
    action_value_type="Value2D"
)

Result: Action created successfully
```

### Scenario 2: AI-Assisted Testing
```
Developer: "We need a complete FPS input system. Can you set it up?"

AI interprets the human request and makes multiple calls:
1. action_create for IA_Move, IA_Look, IA_Jump, etc.
2. mapping_create_context for IMC_Default
3. mapping_add_key_mapping for each action binding
4. modifier_create_instance for smoothing, deadzone
5. trigger_create_instance for various button interactions

Result: Complete, optimized FPS input system
```

### Scenario 3: Regression Testing
```
QA Team runs all 130+ prompts
Compares results against baseline
Documents any differences
Ensures no functionality degradation
```

## Testing Best Practices

1. **Start with Phase 1:** Understand available types before creating actions
2. **Progress to Phase 2:** Set up basic input system before advanced config
3. **Use Phase 3 for Optimization:** Fine-tune after basic system works
4. **Mix Individual and Workflow Tests:** Test both specific actions and complete flows
5. **Include Edge Cases:** Test boundary conditions and error scenarios
6. **Document Results:** Keep records of successful and failed test cases

## Integration with Development

These prompts should be used:
- During feature development for verification
- In CI/CD pipelines for automated testing
- For documentation and example generation
- For AI training and optimization
- In user documentation and tutorials
- For performance benchmarking and analysis

## Statistics

- **Total Test Prompts:** 130+
- **Phase 1 Prompts:** 24 (reflection, discovery, validation)
- **Phase 2 Prompts:** 45+ (action and mapping management)
- **Phase 3 Prompts:** 60+ (advanced config and AI)
- **Actions Covered:** 40+
- **Workflow Scenarios:** 15+
- **Game Genres Represented:** FPS, RPG, Action, Platformer, Racing, Tactical, Mobile, VR
