"""
niagara_scratchpad_trackpainter.py
==================================

Reference end-to-end build of a persistent vehicle track-painter Niagara system using the
new NiagaraScratchPadService. Designed to be runnable verbatim from VibeUE's
`execute_python_code` tool, or executed in-editor via Edit > Editor Python.

What this script does (in order):
  1. Create/find a Niagara System asset at SYSTEM_PATH.
  2. Add a minimal "CompletelyEmpty" emitter if not present.
  3. Add the User.* parameters the BP_TrackManager will write to every frame.
  4. Add an EmitterUpdate SpawnBurst_Instantaneous module (linked to User.BatchSize).
  5. Add a Particle Update scratch module "SplatLine" and populate its graph:
        Map Get (Module.*)  -->  Custom HLSL (splat math)  -->  Map Set (Particles.Splatted)
  6. Apply + recompile + save.

After this script runs, the system is ready to be driven from BP_TrackManager via
Set Niagara Variable calls on the matching User.* parameters.

REQUIREMENTS
  - VibeUE plugin built with NiagaraScratchPadService (this script will fail with a clear
    "module 'unreal' has no attribute 'NiagaraScratchPadService'" message if not).
  - UE 5.7.
  - The Niagara editor for the target system should be CLOSED while this runs (graph edits
    apply to the stored asset; an open editor holds an "edit" copy that may be overwritten
    on apply).

TUNING
  - Adjust SYSTEM_PATH / EMITTER_NAME / GRID_RES / TIRE_RADIUS at the top.
  - The HLSL body in build_splat_hlsl() is intentionally compact and conservative; review
    the Niagara Grid2D write API names for your engine version (`SetVector4Value` /
    `SetVectorValue` vary by Grid2D attribute layout).
"""

import unreal

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SYSTEM_PATH       = "/Game/_GameplaySystems/SYS_TracksManager/Niagara/NS_TrackPainter"
EMITTER_NAME      = "CompletelyEmpty"
SCRATCH_MODULE    = "SplatLine"
SCRATCH_STAGE     = "ParticleUpdate"

GRID_RES_DEFAULT  = 512
TIRE_RADIUS       = 45.0
DECAY_RATE        = 0.997
BATCH_SIZE        = 200
VOLUME_HALF_EXT   = 5000.0  # half-extent on XY; Z is small for terrain projection


# ---------------------------------------------------------------------------
# Splat HLSL (NIAGARA-flavored HLSL, not stock HLSL)
#
# Convention reminders:
#   - Vector types are PascalCase (`Float3`, `Float2`, `Float4`) in Niagara HLSL.
#   - Pin variables appear in scope by their pin name.
#   - Array DI calls follow `Pin.Get(Index)`, returning the array's element type.
#   - Grid 2D writes go through the generated `SetVector4Value(AttributeIndex, X, Y, V)`
#     (or `SetVectorValue` if the grid uses a separate attribute per channel). Check your
#     Grid 2D Collection's "Attributes" array in the Niagara Editor for exact names.
# ---------------------------------------------------------------------------
def build_splat_hlsl() -> str:
    return r"""
// SplatLine - per-frame line-segment splat into TracksGrid
// Inputs:  StartPositions, EndPositions, Directions (Vector arrays, indexed by ExecutionIndex)
//          VolumeMin, VolumeMax (Vector) - world-space tracking volume bounds
//          GridWidth, GridHeight (int)   - resolution
//          TireRadius (float)            - splat radius in world units
//          DecayRate  (float)            - persistence multiplier (unused below; applied by a
//                                          companion full-grid decay pass or the export step)
//          TracksGrid (Grid2D)           - target collection
// Output:  Splatted (bool) - true when at least one cell was written for this segment

const int Idx = ExecutionIndex;

Float3 A = StartPositions.Get(Idx);
Float3 B = EndPositions.Get(Idx);
Float3 D = Directions.Get(Idx);

// World -> normalized [0..1] in the volume's XY plane
Float2 Extents = Float2(VolumeMax.x - VolumeMin.x, VolumeMax.y - VolumeMin.y);
Float2 Auv = Float2((A.x - VolumeMin.x) / Extents.x, (A.y - VolumeMin.y) / Extents.y);
Float2 Buv = Float2((B.x - VolumeMin.x) / Extents.x, (B.y - VolumeMin.y) / Extents.y);

// Tire radius in UV space (use min axis so it stays circular)
float Ruv = TireRadius / min(Extents.x, Extents.y);

// AABB of (Auv, Buv) expanded by Ruv, clipped to [0..1]
Float2 lo = max(Float2(0.0, 0.0), min(Auv, Buv) - Float2(Ruv, Ruv));
Float2 hi = min(Float2(1.0, 1.0), max(Auv, Buv) + Float2(Ruv, Ruv));

int x0 = (int) floor(lo.x * (float)GridWidth);
int y0 = (int) floor(lo.y * (float)GridHeight);
int x1 = (int)  ceil(hi.x * (float)GridWidth);
int y1 = (int)  ceil(hi.y * (float)GridHeight);

// Encode normalized direction into RG (pack -1..1 -> 0..1)
Float2 DirRG = Float2(D.x * 0.5 + 0.5, D.y * 0.5 + 0.5);

bool bAny = false;

for (int y = y0; y < y1; ++y)
{
    for (int x = x0; x < x1; ++x)
    {
        // Cell center in UV
        Float2 P = Float2(((float)x + 0.5) / (float)GridWidth, ((float)y + 0.5) / (float)GridHeight);

        // Distance from P to segment Auv-Buv (in UV space)
        Float2 AB = Buv - Auv;
        float t  = saturate(dot(P - Auv, AB) / max(1e-6, dot(AB, AB)));
        Float2 Q = Auv + AB * t;
        float d  = length(P - Q);

        if (d < Ruv)
        {
            // RG = direction, B = ditch strength accumulating, A = mound strength accumulating
            // (Reads pre-decay; the companion decay pass slowly fades unwritten cells.)
            float Strength = 1.0 - (d / Ruv);
            TracksGrid.SetVector4Value(0, x, y, Float4(DirRG.x, DirRG.y, Strength, Strength * 0.5));
            bAny = true;
        }
    }
}

Splatted = bAny;
""".strip()


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def _ensure_system(path: str) -> bool:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return True
    folder = path.rsplit("/", 1)[0]
    name   = path.rsplit("/", 1)[1]
    res = unreal.NiagaraService.create_system(name, folder, "")
    return res is not None and getattr(res, "b_success", True)


def _ensure_emitter(system: str, emitter: str) -> bool:
    for e in unreal.NiagaraService.list_emitters(system):
        if str(e.emitter_name) == emitter:
            return True
    name = unreal.NiagaraService.add_emitter(system, "minimal", emitter)
    return bool(name)


def _add_user_param(system: str, name: str, type_name: str, default: str = "") -> None:
    """Idempotent add_user_parameter wrapper."""
    try:
        unreal.NiagaraService.add_user_parameter(system, name, type_name, default)
    except Exception as ex:
        unreal.log_warning(f"add_user_parameter({name}, {type_name}) raised: {ex}")


def _check_scratchpad_service_available() -> None:
    if not hasattr(unreal, "NiagaraScratchPadService"):
        raise RuntimeError(
            "unreal.NiagaraScratchPadService is not loaded. Rebuild the VibeUE plugin in "
            "Unreal so the new C++ class compiles, then restart the editor before re-running."
        )


# ---------------------------------------------------------------------------
# Main build
# ---------------------------------------------------------------------------
def build():
    _check_scratchpad_service_available()

    print(f"[trackpainter] system = {SYSTEM_PATH}, emitter = {EMITTER_NAME}")

    assert _ensure_system(SYSTEM_PATH),  f"could not create/find {SYSTEM_PATH}"
    assert _ensure_emitter(SYSTEM_PATH, EMITTER_NAME), f"could not add emitter {EMITTER_NAME}"

    # --- User parameters (BP_TrackManager writes to these every frame) ----------
    user_params = [
        ("User.BatchSize",       "Int",            str(BATCH_SIZE)),
        ("User.DecayRate",       "Float",          f"{DECAY_RATE:.6f}"),
        ("User.GridWidth",       "Int",            str(GRID_RES_DEFAULT)),
        ("User.GridHeight",      "Int",            str(GRID_RES_DEFAULT)),
        ("User.TireRadius",      "Float",          f"{TIRE_RADIUS:.3f}"),
        ("User.VolumeMin",       "Vector",         f"(X=-{VOLUME_HALF_EXT},Y=-{VOLUME_HALF_EXT},Z=-500)"),
        ("User.VolumeMax",       "Vector",         f"(X={VOLUME_HALF_EXT},Y={VOLUME_HALF_EXT},Z=500)"),
        ("User.StartPositions",  "NiagaraDataInterfaceArrayFloat3", ""),
        ("User.EndPositions",    "NiagaraDataInterfaceArrayFloat3", ""),
        ("User.Directions",      "NiagaraDataInterfaceArrayFloat3", ""),
        ("User.DynamicRT",       "TextureRenderTarget", ""),
    ]
    for name, type_name, default in user_params:
        _add_user_param(SYSTEM_PATH, name, type_name, default)

    # --- Scratch module ---------------------------------------------------------
    existing = unreal.NiagaraScratchPadService.list_scratch_modules(SYSTEM_PATH, EMITTER_NAME)
    if SCRATCH_MODULE in [str(m) for m in existing]:
        print(f"[trackpainter] scratch module '{SCRATCH_MODULE}' already exists - skipping creation")
        mod = SCRATCH_MODULE
    else:
        r = unreal.NiagaraScratchPadService.create_scratch_module(
            SYSTEM_PATH, EMITTER_NAME, SCRATCH_STAGE, SCRATCH_MODULE
        )
        if not r.b_success:
            raise RuntimeError(f"create_scratch_module failed: {r.message}")
        mod = str(r.module_name)
        print(f"[trackpainter] created scratch module {mod} at {r.script_path}")

    # --- Module inputs (Map Get reads) -----------------------------------------
    module_inputs = [
        ("StartPositions", "ArrayVector"),
        ("EndPositions",   "ArrayVector"),
        ("Directions",     "ArrayVector"),
        ("VolumeMin",      "Vector"),
        ("VolumeMax",      "Vector"),
        ("GridWidth",      "int"),
        ("GridHeight",     "int"),
        ("TireRadius",     "float"),
        ("DecayRate",      "float"),
        ("TracksGrid",     "Grid2D"),
    ]
    for name, type_name in module_inputs:
        r = unreal.NiagaraScratchPadService.add_module_input(
            SYSTEM_PATH, EMITTER_NAME, mod, name, type_name
        )
        if not r.b_success:
            unreal.log_warning(f"add_module_input({name}) -> {r.message}")

    # --- Custom HLSL node with the splat code ----------------------------------
    hlsl = unreal.NiagaraScratchPadService.add_custom_hlsl_node(
        SYSTEM_PATH, EMITTER_NAME, mod, build_splat_hlsl(), 350, 0
    )
    if not hlsl.b_success:
        raise RuntimeError(f"add_custom_hlsl_node failed: {hlsl.message}")
    hlsl_id = str(hlsl.node_id)

    # Typed pins matching the HLSL pin variables
    for name, type_name in module_inputs:
        unreal.NiagaraScratchPadService.add_pin(
            SYSTEM_PATH, EMITTER_NAME, mod, hlsl_id, "Input", type_name, name
        )
    unreal.NiagaraScratchPadService.add_pin(
        SYSTEM_PATH, EMITTER_NAME, mod, hlsl_id, "Output", "bool", "Splatted"
    )

    # --- Wire Map Get -> Custom HLSL -> Map Set --------------------------------
    nodes = unreal.NiagaraScratchPadService.list_nodes(SYSTEM_PATH, EMITTER_NAME, mod)
    mapget = next((n for n in nodes if str(n.node_type) == "MapGet"), None)
    mapset = next((n for n in nodes if str(n.node_type) == "MapSet"), None)
    if not mapget or not mapset:
        raise RuntimeError("scratch module is missing default Map Get/Map Set nodes")

    for name, _ in module_inputs:
        unreal.NiagaraScratchPadService.connect_pins(
            SYSTEM_PATH, EMITTER_NAME, mod,
            str(mapget.node_id), f"Module.{name}",
            hlsl_id,              name,
        )

    # Module output -> particles attribute write
    unreal.NiagaraScratchPadService.add_module_output(
        SYSTEM_PATH, EMITTER_NAME, mod, "Particles.Splatted", "bool"
    )
    unreal.NiagaraScratchPadService.connect_pins(
        SYSTEM_PATH, EMITTER_NAME, mod,
        hlsl_id,              "Splatted",
        str(mapset.node_id),  "Particles.Splatted",
    )

    # --- Apply + recompile + save ----------------------------------------------
    ok = unreal.NiagaraScratchPadService.apply_changes(SYSTEM_PATH)
    print(f"[trackpainter] apply_changes -> {ok}")

    result = unreal.NiagaraService.compile_with_results(SYSTEM_PATH)
    print(f"[trackpainter] compile_with_results -> success={result.success}, "
          f"errors={result.error_count}, warnings={result.warning_count}")
    if not result.success:
        for e in result.errors:
            print(f"  ERR: {e}")
    return ok and result.success


if __name__ == "__main__":
    print("OK" if build() else "FAILED")
