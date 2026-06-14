# build_emitter.pyx — Create a Niagara system, add an emitter, add a renderer + module, compile.
#
# Sample script for the niagara-emitters skill. Run via execute_python_code.
# add_emitter needs an emitter template asset path — discover with list_emitter_templates().
import unreal
ns = unreal.NiagaraService
nes = unreal.NiagaraEmitterService

NAME = "NS_SkillTest"
FOLDER = "/Game/VFX"

asset_path = f"{FOLDER}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    unreal.EditorAssetLibrary.delete_asset(asset_path)

r = ns.create_system(NAME, FOLDER)          # NiagaraCreateResult
print("create_system:", r.success, r.asset_path)
system = r.asset_path

# Discover a template emitter to add (e.g. a Fountain/Sprite template)
templates = ns.list_emitter_templates()
print("templates:", [str(t) for t in templates][:5])
if templates:
    tmpl = str(templates[0]).split(" ")[0]
    emitter = ns.add_emitter(system, tmpl, "MyEmitter")
    print("add_emitter:", emitter)

    nes.add_renderer(system, "MyEmitter", "SpriteRenderer")   # or Ribbon/Mesh/Light
    print("renderers:", [str(x) for x in nes.list_renderers(system, "MyEmitter")])

ns.compile_system(system, True)
unreal.EditorAssetLibrary.save_asset(system)
