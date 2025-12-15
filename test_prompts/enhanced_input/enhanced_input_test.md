# Enhanced Input System Tests

These tests should be run sequentially. Each test builds on the previous ones. create any required assets if they don't exist not found.

---

## Discovery & Information

I'm starting a new project and need to understand what input options are available. Can you show me all the different input types the engine supports? I want to see modifiers, triggers, and everything else.

---

How does the Negate modifier work? What settings can I configure on it?

---

I want to use a hold mechanic for charging attacks. Tell me everything about how hold triggers work and what I can customize.

---

What keys can I bind for input? Give me a list of keyboard, mouse, and controller options.

---

Show me what modifiers exist. I want to know what ways I can transform input values.

---

What trigger behaviors are available? I need to understand when inputs fire.

---

## Input Action Management

What input actions do we have in the project right now?

---

I need a basic button press action for interacting with objects. Set it up in the TestAssets folder under Input. Call it something like InteractAction.

---

For character movement I need a 2D axis input. Put it in /Game/Input/TestAssets and name it MoveAction.

---

Camera look needs its own 2D input as well. Create that alongside the movement one.

---

My shooter needs a fire button. Make a digital action for shooting.

---

Vehicles need a throttle control. That should be a 1D axis ranging from 0 to 1.

---

Now that we have a few actions, show me the properties on the movement action.

---

The interact action should consume input so other things don't trigger at the same time. Update it.

---

Add a description to the interact action saying it handles player object interaction.

---

## Input Mapping Context Setup

Show me all the mapping contexts in the project.

---

I need a place to put my default player controls. Create a mapping context for general gameplay.

---

Combat needs its own context so I can swap input schemes. Make one for that.

---

Vehicles will have different controls entirely. Set up a context for driving.

---

What bindings are in the general gameplay context right now?

---

## Key Bindings

Let's set up the basic controls. The player should press E to interact with things.

---

Sprinting should be on Left Shift. Hook that up to a sprint action. If the action doesn't exist, create it.

---

Left mouse button fires the weapon. Bind that in the combat context.

---

Controller players should use the right trigger to shoot. Add that binding too.

---

Mouse movement controls where the player looks. Connect Mouse2D to the look action in the gameplay context.

---

Walking around uses the left stick on controller. Bind the left analog stick to movement.

---

The gas pedal on vehicles is the right trigger. Set that up in the vehicle context.

---

Let me see all the bindings in the gameplay context now to make sure everything looks right.

---

Actually, remove the first binding from the combat context. I want to redo it.

---

## Input Modifiers

The look controls feel backwards. Add something to invert the Y axis on the first binding in the gameplay context.

---

The mouse look input needs a dead zone. Add one to that binding.

---

Vehicle throttle should be scaled down a bit. Add a scalar modifier to make it less sensitive.

---

Show me what modifiers are on the first gameplay binding now.

---

Take off that invert modifier we added. I changed my mind.

---

## Input Triggers

Interaction should trigger on button press, not while held. Set that up on the first binding in the gameplay context.

---

For the combat context, I want the player to hold the button briefly before firing. Add a hold trigger to the first binding.

---

Actually let's also support a quick tap for the combat controls. Add that trigger type.

---

What triggers are configured on that first combat binding?

---

Remove the first trigger from the gameplay context. Let's try a different approach.

---

## Complete Input Setups

I need a reload mechanic for the shooter. Create the action, bind R to it in combat, and make it trigger on press.

---

Add strafing to the movement system. It should be a 2D input bound to the right stick with a dead zone and axis swizzling.

---

I'm building a menu system. Create a new context for menus, make actions for pause, confirm, and back buttons, then bind Escape to pause, Enter to confirm, and Backspace to go back.

---