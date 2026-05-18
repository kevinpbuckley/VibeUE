---
name: function-classes
description: Common class names for add_function_call_node - KismetMathLibrary, KismetSystemLibrary, KismetArrayLibrary, GameplayStatics, and other UE library classes
---

This sub-doc continues from skill.md → "Common Function Call Classes".

## Common Function Call Classes

For `add_function_call_node(path, graph, class, func, x, y)`:

- **KismetMathLibrary** — Math (Add_DoubleDouble, Multiply_DoubleDouble, Sin, Sqrt)
- **KismetSystemLibrary** — System (PrintString, Delay, K2_SetTimerDelegate)
- **KismetStringLibrary** — String (Concat_StrStr, MakeLiteralString, Contains)
- **KismetArrayLibrary** — Array operations (Array_Length, Array_Random, Array_Add, Array_Remove, Array_Clear)
- **GameplayStatics** — Game (GetPlayerController, SpawnActor)
- **Actor** — Actor functions, but discover the exact callable/spawner first for graph nodes like `Get Actor Location` and `Set Actor Location`
- **PrimitiveComponent** — Physics (SetSimulatePhysics)
- **SceneComponent** — Transform (AddRelativeRotation, SetRelativeLocation)

---
