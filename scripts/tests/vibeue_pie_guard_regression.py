"""Regression checks for the PIE guard on VibeUE's asset-writing services and vibeue.require_no_pie() (#705).
Needs a live UE 5.9 editor with VibeUE and a renderer (PIE runs; -RenderOffScreen is enough).

Run from an execute_python script (top-level await) in a DISPOSABLE editor session: it creates
/Game/VibeUETests/SC_D1_PIEGuard (a SoundCue), starts and stops PIE, and leaves the cue saved.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import vibeue_pie_guard_regression as t
    await t.run(call_tool)     # {"passed": [...], "failed": [...], "details": {...}}

During PIE: writers of five services refuse with the same text (from Python and through call_tool, the MCP route),
the cue on disk is unchanged, a read of the same service works, and require_no_pie raises the same text. After PIE
the same writer works again.
"""

import unreal
import vibeue

CUE = "/Game/VibeUETests/SC_D1_PIEGuard"
TAIL = "refused: a Play-In-Editor session is running"


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _refusal(fn):
    """The exception text fn() raises, or "" when it does not raise."""
    try:
        fn()
    except Exception as exc:
        return str(exc)
    return ""


def _generic(text):
    """The refusal with the operation name cut out, to compare texts across services."""
    return text[text.find(TAIL):] if TAIL in text else text


async def run(call_tool):
    results = {"passed": [], "failed": [], "details": {}}
    svc = unreal.SoundCueService
    if not unreal.EditorAssetLibrary.does_asset_exist(CUE):
        svc.create_sound_cue(CUE, "")
        unreal.EditorAssetLibrary.save_asset(CUE, only_if_is_dirty=False)
    nodes_before = len(svc.list_nodes(CUE))
    _check(results, "no PIE: require_no_pie passes", "", _refusal(lambda: vibeue.require_no_pie("test")))

    async with vibeue.pie_session(640, 360, warmup_s=1.0):
        _check(results, "PIE is running", True, bool(unreal.PythonTools.is_play_session_running()))
        texts = {
            "SoundCueService.add_random_node": _refusal(lambda: svc.add_random_node(CUE, 0.0, 0.0)),
            "SoundCueService.create_sound_cue": _refusal(lambda: svc.create_sound_cue(CUE + "_New", "")),
            "InputService.create_action": _refusal(
                lambda: unreal.InputService.create_action("IA_D1_PIEGuard", "/Game/VibeUETests", "Boolean")),
            "StateTreeService.create_state_tree": _refusal(
                lambda: unreal.StateTreeService.create_state_tree("/Game/VibeUETests/ST_D1_PIEGuard")),
            "UVMappingService.add_uv_channel": _refusal(
                lambda: unreal.UVMappingService.add_uv_channel("/Engine/BasicShapes/Cube", 0)),
        }
        for op, text in texts.items():
            # Python prefixes a script error with the class name ("SoundCueService: PIE_ACTIVE: ..."); MCP does not.
            _check(results, "PIE: {} refused".format(op), True, ("PIE_ACTIVE: " + op + " refused") in text)
            _check(results, "PIE: {} says stop PIE first".format(op), True, "Stop PIE first" in text)
        generic = {_generic(t) for t in texts.values()}
        _check(results, "PIE: one refusal text across services", 1, len(generic))

        mcp_error = ""
        try:
            await call_tool("VibeUE.SoundCueService.AddRandomNode", assetPath=CUE, posX=0.0, posY=0.0)
        except Exception as exc:
            mcp_error = str(exc)
        _check(results, "PIE: the MCP route refuses too", True, "PIE_ACTIVE" in mcp_error and "Stop PIE first" in mcp_error)

        _check(results, "PIE: nothing was added", nodes_before, len(svc.list_nodes(CUE)))
        info = svc.get_sound_cue_info(CUE)
        _check(results, "PIE: a read of the same service works", True, info is not None and bool(str(info)))
        guard = _refusal(lambda: vibeue.require_no_pie("texture import"))
        _check(results, "PIE: require_no_pie raises the same text", True,
               guard.startswith("PIE_ACTIVE: texture import") and _generic(guard) in generic)
        _check(results, "PIE: require_no_pie raises PIEActiveError", True,
               isinstance(_exc(lambda: vibeue.require_no_pie()), vibeue.PIEActiveError))

    _check(results, "after PIE: require_no_pie passes", "", _refusal(lambda: vibeue.require_no_pie("test")))
    after = svc.add_random_node(CUE, 0.0, 0.0)
    _check(results, "after PIE: the writer works", nodes_before + 1, len(svc.list_nodes(CUE)))
    unreal.EditorLoadingAndSavingUtils.reload_packages([unreal.find_package(CUE)])   # drop the test node
    return results


def _exc(fn):
    try:
        fn()
    except Exception as exc:
        return exc
    return None
