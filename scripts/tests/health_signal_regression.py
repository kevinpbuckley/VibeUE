"""Regression check for the slow-task fields of the health heartbeat (issue #681).

Run from an execute_python script (top-level await) in a disposable editor session:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import health_signal_regression as h
    await h.run()

It holds the game thread inside an unreal.ScopedSlowTask for about 6 s (long enough for the 5 s
heartbeat writer to write once), reads Saved/VibeUE/Signals/editor-<pid>-health.json from inside
the task, then lets the editor tick for about 6 s and reads it again. Returns
{"passed": [...], "failed": [...], "details": {...}}. The modal-dialog fields are not exercised:
a Slate modal blocks the script that would open it.
"""

import asyncio
import json
import os
import time

import unreal

TASK_TEXT = "VibeUE health regression slow task"


def _health_path():
    saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    return os.path.join(saved, "VibeUE", "Signals", "editor-{}-health.json".format(os.getpid()))


def _read():
    with open(_health_path(), "r", encoding="utf-8") as handle:
        return json.load(handle)


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}


async def run(hold_s=6.5):
    results = {"passed": [], "failed": [], "details": {}}
    before = _read()
    _check(results, "fields present", True,
           all(k in before for k in ("slowTask", "slowTaskText", "slowTaskSeconds",
                                      "modal", "modalTitle", "modalSeconds")))
    _check(results, "idle editor: slowTask false", False, before.get("slowTask"))

    steps = int(hold_s / 0.25) + 1
    with unreal.ScopedSlowTask(steps, TASK_TEXT) as task:
        for _ in range(steps):
            task.enter_progress_frame(1)
            time.sleep(0.25)      # deliberately blocks the game thread: this IS the slow task
        during = _read()
    _check(results, "inside a slow task: slowTask true", True, during.get("slowTask"))
    _check(results, "slowTaskText names the task", TASK_TEXT, during.get("slowTaskText"))
    _check(results, "slowTaskSeconds counts up", True, float(during.get("slowTaskSeconds", 0)) > 0.0)

    await asyncio.sleep(6.5)      # the editor ticks: the core ticker clears the flag, the writer rewrites
    after = _read()
    _check(results, "after the task: slowTask false", False, after.get("slowTask"))
    _check(results, "after the task: slowTaskText empty", "", after.get("slowTaskText"))
    results["details"]["during"] = during
    results["details"]["after"] = after
    return results
