# add_component.pyx — Add components (with a child) and set their properties.
#
# Sample script for the blueprints skill. Run via execute_python_code.
# Compile after adding components, before setting properties that depend on them.
import unreal
bs = unreal.BlueprintService

BP_PATH = "/Game/Blueprints/BP_Player"

bs.add_component(BP_PATH, "StaticMeshComponent", "BodyMesh")          # parent defaults to root
bs.add_component(BP_PATH, "PointLightComponent", "Glow", "BodyMesh")  # child of BodyMesh
bs.compile_blueprint(BP_PATH)

# property values are strings; UE strips the lowercase b prefix (bVisible -> set as "bVisible" key here is fine)
bs.set_component_property(BP_PATH, "BodyMesh", "bVisible", "true")
bs.set_component_property(BP_PATH, "Glow", "Intensity", "5000.0")
# LightColor is FColor: integer bytes 0-255 (LinearColor-style 0-1 floats give a near-black light)
bs.set_component_property(BP_PATH, "Glow", "LightColor", "(R=255,G=127,B=0,A=255)")

c = bs.compile_blueprint(BP_PATH)
print("compile:", c.success, "errors:", c.num_errors)
unreal.EditorAssetLibrary.save_asset(BP_PATH)
