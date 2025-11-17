# Phase 3: Advanced Configuration & AI Integration Test Prompts

These prompts test advanced modifier/trigger configuration, performance optimization, and natural language AI integration.

## Advanced Modifier Service Tests

### Test: Discover Available Modifiers
Show me all the different modifier types that can be applied to input values. What categories of modifiers exist?

### Test: Get Modifier Metadata
I want to understand the "Normalize" modifier in detail. What does it do, what properties can I adjust, and what are the defaults?

### Test: Create a Deadzone Modifier
Create a modifier that adds a deadzone of 0.2 to filter out small stick movements on a gamepad.

### Test: Create a Swizzle Modifier
I need a modifier to swap the X and Y axes for an input. Create a Swizzle modifier with the appropriate configuration.

### Test: Create Multiple Modifiers
Build a modifier stack for gamepad input: First normalize, then apply a deadzone of 0.15, then scale the output by 1.5.

### Test: Configure Modifier Details
Take an existing modifier and adjust its properties. For example, change a deadzone value from 0.1 to 0.25.

### Test: Optimize Modifier Stack
I have several modifiers applied in sequence. Analyze them for performance and suggest optimizations.

### Test: Clone Modifier Configuration
I have a good modifier setup that works well. Clone it so I can use the same configuration in another place with slightly different settings.

### Test: Compare Modifier Configurations
I have two different modifier setups for gamepad input. Compare them to see which one is better for sensitivity.

### Test: Remove a Modifier
I don't need the scaling modifier anymore. Remove it from the stack while keeping the others intact.

### Test: Reorder Modifiers
The order of modifiers matters for the final result. Change the order so deadzone is applied before normalization instead of after.

## Advanced Trigger Service Tests

### Test: Discover Available Triggers
What different types of triggers are available? Show me all the trigger classes I can use.

### Test: Get Trigger Metadata
Explain the "Held" trigger to me in detail. What properties does it have? How long does someone need to hold for it to trigger?

### Test: Create Press Trigger
Set up a trigger that fires when a button is first pressed (not held, just the initial press).

### Test: Create Held Trigger
Create a trigger that activates when a button is held down for more than 0.5 seconds.

### Test: Create Combo Trigger
I want a trigger that only activates when two buttons are pressed in sequence (like a fighting game combo). Set this up.

### Test: Create Range Trigger
Create a trigger that activates when an analog input (like a trigger or stick) exceeds a certain threshold value.

### Test: Analyze Trigger Performance
I have several triggers set up. Analyze them and tell me which ones might cause performance issues.

### Test: Detect Trigger Conflicts
Show me if any of my triggers might conflict with each other or cause unexpected behavior.

### Test: Clone Trigger Configuration
I have a trigger setup that works well. Clone it so I can apply the same settings elsewhere.

### Test: Compare Triggers
I'm deciding between two different trigger configurations. Compare them - which one is better for quick reaction time?

### Test: Adjust Trigger Timing
Change a held trigger's wait time from 0.5 seconds to 0.3 seconds for faster response.

## AI Configuration Service Tests

### Test: Parse Natural Language Action Description
I want to create an action "for moving the player in all directions with smooth analog stick input". Parse this description and suggest the best configuration.

### Test: Parse Modifier from Description
I describe what I want: "reduce noise from the gamepad stick and make it dead in the center". What modifier configuration does this suggest?

### Test: Parse Trigger from Description
I say "I want something that fires when I tap a button really fast, like a double-click". What trigger would work?

### Test: Parse Complex Input Description
"I need a 2D movement input for WASD controls that feels responsive and has a bit of smoothing to prevent jitter." What's the best setup?

### Test: Parse Gamepad Setup Description
"Set up my right stick for camera look - it should feel responsive but not twitchy, with a small deadzone in the center." Suggest configuration.

### Test: Get Available Templates
Show me what pre-built input templates are available. Do you have anything for FPS games, RPGs, or third-person action games?

### Test: Apply FPS Template
I'm building an FPS game. Apply the FPS input template to set up all the standard actions and mappings.

### Test: Apply Third-Person Template
I need a third-person action game input setup. Apply the appropriate template.

### Test: Apply Mobile Template
I'm developing for mobile/touch. Apply the mobile input template.

### Test: Apply RPG Template
Set up a classic RPG control scheme using your template.

### Test: Customize Template
Apply the FPS template but then ask the AI to modify it for a faster, more arcade-style feel.

## Complex AI-Assisted Workflows

### Test: Build Input System from Description
"I'm making a sci-fi action game where the player controls a drone with a gamepad. I need smooth analog input for movement, quick triggers for weapons, and some cool visual feedback modifiers." Design the complete input system.

### Test: Migrate Control Scheme
"I currently have keyboard controls. Help me add gamepad support alongside them, with the same actions but optimized for a controller."

### Test: Optimize Existing Setup
"I have an input system but it feels sluggish and imprecise. Analyze it and suggest optimizations for better responsiveness."

### Test: Create Accessibility Setup
"Create an input setup for a player with limited hand mobility - everything should be reachable without complex combinations."

### Test: Design Competitive Setup
"I'm building a competitive fighting game. Design an input system optimized for fast, precise inputs and competitive play."

### Test: Build Skill-Based Setup
"Create two input profiles: one for beginners (forgiving, simplified) and one for experts (sensitive, responsive). Help me set both up."

### Test: Parse Multi-Part Request
"I need an action for player movement (smooth 2D input), a trigger for attacking (quick response), and a modifier to reduce stick drift. Set me up."

### Test: Suggest Best Practices
"What's the recommended way to set up input actions for a mobile action game? Any best practices I should follow?"

## Performance and Validation Tests

### Test: Validate Modifier Stack
I have a complex modifier stack. Is it optimal? Are there any performance concerns?

### Test: Validate Trigger Combinations
Show me if my trigger setup is efficient and won't cause frame rate issues.

### Test: Check for Redundancy
Do I have any redundant modifiers that are doing the same thing? Optimize them.

### Test: Analyze Input Latency
My input feels delayed. Analyze my modifier and trigger setup - what might be causing latency?

### Test: Get Performance Metrics
Measure the performance impact of my current modifier and trigger configuration. What's the CPU cost?

### Test: Suggest Simplifications
My input setup is complex. What can I simplify without losing functionality?

## AI Template Generation Tests

### Test: Generate Retro Arcade Template
Create an input template for classic arcade game feel - responsive, snappy, immediate feedback.

### Test: Generate Realistic Simulation Template
Design input for a realistic vehicle simulation - smooth, gradual, physical response.

### Test: Generate Tactical Game Template
Create input for a tactical/turn-based strategy game - deliberate, precise, emphasis on accuracy.

### Test: Generate Immersive VR Template
Build input configuration optimized for VR games - smooth, responsive, motion sickness minimization.

### Test: Generate Platformer Template
Create classic platformer input - tight controls, instant response, pixel-perfect jumping.

### Test: Generate Top-Down Action Template
Design input for top-down action games like Diablo or Zelda - smooth, responsive, good for kiting.

### Test: Generate Racing Game Template
Set up input for racing games - sensitive steering, acceleration/brake triggers, handbrake control.

## AI Suggestions and Recommendations

### Test: Get AI Recommendations
Based on my action and mapping setup, what improvements would you recommend?

### Test: Get Template Suggestions
What template would be most appropriate for a "stealth-action game with melee combat"?

### Test: Get Configuration Presets
Show me some preset modifier configurations that work well together for common scenarios.

### Test: Get Best Practices Guide
What are the best practices for setting up input for modern action games?

### Test: Get Troubleshooting Advice
Players say the input feels unresponsive. What might be wrong with my configuration?

### Test: Get Optimization Suggestions
How can I make my input system feel snappier and more responsive without adding input lag?
