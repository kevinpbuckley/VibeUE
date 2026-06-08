# recolor_emitter.pyx — Recolor a Niagara emitter (hue shift preferred over tint/scale).
#
# Sample script for the niagara-emitters skill. Run via execute_python_code.
# shift_color_hue rotates the color wheel and PRESERVES gradients (best for recoloring effects).
# set_color_tint MULTIPLIES channels — strong tints lose detail; zero values kill channels.
import unreal
nes = unreal.NiagaraEmitterService
ns = unreal.NiagaraService

SYSTEM = "/Game/VFX/NS_SkillTest"
EMITTER = "MyEmitter"

# Hue shift (degrees): 120=toward green, 180=cyan/opposite, 240=blue/purple, -60=toward red
print("shift_color_hue:", nes.shift_color_hue(SYSTEM, EMITTER, 120.0, "ColorFromCurve"))

# Quick uniform tint alternative (preserves gradients when channels scaled equally):
# nes.set_color_tint(SYSTEM, EMITTER, 1.2, 1.2, 1.2, 1.0)

ns.compile_system(SYSTEM, True)
unreal.EditorAssetLibrary.save_asset(SYSTEM)
