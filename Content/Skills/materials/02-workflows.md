# Material Workflows

---

## Create Basic Material

```python
import unreal

# Create material
path = unreal.MaterialService.create_material("Character", "/Game/Materials/")

# Add color parameter
node_id = unreal.MaterialNodeService.create_parameter(
    path, "Vector", "BaseColor", "Surface", "", -500, 0
)

# Connect to output
unreal.MaterialNodeService.connect_to_material(path, node_id, "BaseColor")

# Add roughness parameter
rough_id = unreal.MaterialNodeService.create_parameter(
    path, "Scalar", "Roughness", "Surface", "0.5", -500, 100
)
unreal.MaterialNodeService.connect_to_material(path, rough_id, "Roughness")

# Compile and save
unreal.MaterialService.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)
```

---

## Create Material Instance

```python
import unreal

# Create instance from parent
instance = unreal.MaterialService.create_instance(
    "/Game/Materials/M_Character",  # Parent
    "PlayerRed",                    # Name
    "/Game/Materials/"              # Folder
)

# Override parameters
unreal.MaterialService.set_vector_parameter(instance, "BaseColor", "(R=1,G=0,B=0,A=1)")
unreal.MaterialService.set_scalar_parameter(instance, "Roughness", 0.3)

# Save
unreal.EditorAssetLibrary.save_asset(instance)
```

---

## Add Texture Parameter

```python
import unreal

path = "/Game/Materials/M_Character"

# Create texture parameter
tex_id = unreal.MaterialNodeService.create_parameter(
    path, "Texture", "DiffuseMap", "Textures", "", -500, 0
)

# Connect to BaseColor
unreal.MaterialNodeService.connect_to_material(path, tex_id, "BaseColor")

# Compile
unreal.MaterialService.compile_material(path)
```

---

## Create Math Expression

```python
import unreal

path = "/Game/Materials/M_Tint"

# Color parameter
color_id = unreal.MaterialNodeService.create_parameter(
    path, "Vector", "TintColor", "Surface", "", -600, 0
)

# Multiply expression
mult_id = unreal.MaterialNodeService.create_expression(path, "Multiply", -300, 0)

# Connect: Color → Multiply.A
unreal.MaterialNodeService.connect_nodes(path, color_id, "", mult_id, "A")

# Scalar intensity
intensity_id = unreal.MaterialNodeService.create_parameter(
    path, "Scalar", "Intensity", "Surface", "1.0", -600, 100
)

# Connect: Intensity → Multiply.B
unreal.MaterialNodeService.connect_nodes(path, intensity_id, "", mult_id, "B")

# Connect: Multiply → BaseColor
unreal.MaterialNodeService.connect_to_material(path, mult_id, "BaseColor")

# Compile
unreal.MaterialService.compile_material(path)
```

---

## Set Material Properties

```python
import unreal

path = "/Game/Materials/M_Glass"

# Set blend mode for transparency
unreal.MaterialService.set_blend_mode(path, "Translucent")

# Set shading model
unreal.MaterialService.set_shading_model(path, "DefaultLit")

# Set two-sided
unreal.MaterialService.set_two_sided(path, True)

# Compile
unreal.MaterialService.compile_material(path)
```

---

## Get Material Info

```python
import unreal

# Get material info
info = unreal.MaterialService.get_info("/Game/Materials/M_Character")
if info:
    print(f"Material: {info.name}")
    print(f"Blend Mode: {info.blend_mode}")
    print(f"Shading Model: {info.shading_model}")
    print(f"Parameters: {info.parameter_count}")
```
