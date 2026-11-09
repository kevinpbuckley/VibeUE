---
name: function-classes
description: Common class names for a build_graph function_call node / engine create_node - KismetMathLibrary, KismetSystemLibrary, KismetArrayLibrary, GameplayStatics, and other UE library classes
---

This sub-doc continues from skill.md → "Common Function Call Classes".

## Common Function Call Classes

For a `build_graph` `function_call` node (`params: {"class": ..., "function": ...}`) — or the engine `BlueprintTools.create_node` equivalent:

- **KismetMathLibrary** — Math (Add_DoubleDouble, Multiply_DoubleDouble, Sin, Sqrt)
- **KismetSystemLibrary** — System (PrintString, Delay, K2_SetTimerDelegate)
- **KismetStringLibrary** — String (Concat_StrStr, MakeLiteralString, Contains)
- **KismetArrayLibrary** — Array operations (Array_Length, Array_Random, Array_Add, Array_Remove, Array_Clear)
- **GameplayStatics** — Game (GetPlayerController, SpawnActor)
- **Actor** — Actor functions, but discover the exact callable/spawner first for graph nodes like `Get Actor Location` and `Set Actor Location`
- **PrimitiveComponent** — Physics (SetSimulatePhysics)
- **SceneComponent** — Transform (AddRelativeRotation, SetRelativeLocation)

---
