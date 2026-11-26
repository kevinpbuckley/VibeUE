"""Regression checks for vibeue.start_job / job_status / wait_idle (#700). Needs a live UE 5.9 editor with VibeUE.

Two execute_python calls, because the point is that a job outlives the call that starts it:

    # call 1
    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import vibeue_jobs_regression as t
    t.start()                       # starts the jobs, returns at once

    # call 2, a few seconds later (top-level await)
    import vibeue_jobs_regression as t
    await t.finish()                # {"passed": [...], "failed": [...], "details": {...}}

start(long_s=180) also runs the acceptance case: a job of long_s seconds whose status is read in finish()
(call finish() after long_s). Creates nothing in the project except Saved/VibeUE/Jobs/vibeue-test-*.json.
"""

import asyncio
import os
import time

import unreal
import vibeue

_STATE = {}


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


async def _counting(job):
    for i in range(20):
        job.report(i, "step {}".format(i))
        await asyncio.sleep(0.1)
    # An object that exists in any editor state (the editor world can be None mid level load).
    return {"steps": 20, "world": unreal.Actor.get_default_object()}


async def _failing():
    await asyncio.sleep(0.05)
    raise ValueError("expected failure")


async def _forever(job):
    while True:
        await asyncio.sleep(0.1)


async def _long(job, seconds):
    t0 = time.monotonic()
    while time.monotonic() - t0 < seconds:
        job.report(round((time.monotonic() - t0) / seconds, 3))
        await asyncio.sleep(0.25)
    return "finished"


def start(long_s=0):
    """Call 1: start the jobs and return at once (also measures how long the start took)."""
    t0 = time.monotonic()
    vibeue.start_job(_counting, "vibeue-test-count", replace=True)
    vibeue.start_job(_failing, "vibeue-test-fail", replace=True)
    vibeue.start_job(_forever, "vibeue-test-cancel", replace=True)
    if long_s:
        vibeue.start_job(lambda job: _long(job, long_s), "vibeue-test-long", replace=True)
    refused = ""
    try:
        vibeue.start_job(_forever, "vibeue-test-cancel")
    except vibeue.JobRefused as exc:
        refused = str(exc)
    _STATE.update({"start_s": round(time.monotonic() - t0, 3), "refused": refused, "long_s": long_s,
                   "started_at": time.monotonic()})
    return dict(_STATE)


async def finish():
    """Call 2: read the jobs back, cancel the endless one, check wait_idle."""
    results = {"passed": [], "failed": [], "details": {}}
    _check(results, "start_job returned at once (< 1 s for all starts)", True, _STATE.get("start_s", 99) < 1.0)
    _check(results, "a running key is refused", True, "already running" in _STATE.get("refused", ""))

    count = await vibeue.wait_job("vibeue-test-count", 10)
    _check(results, "job advanced between calls and finished", "done", count["status"])
    _check(results, "progress was reported", 19, count["progress"])
    _check(results, "result is a JSON-ready copy (object -> path)", True,
           isinstance(count["result"], dict) and isinstance(count["result"].get("world"), str))
    _check(results, "status file written", True, os.path.isfile(count["file"]))

    fail = vibeue.job_status("vibeue-test-fail")
    _check(results, "a raising job is failed", "failed", fail["status"])
    _check(results, "its traceback is kept", True, "expected failure" in fail["error"])

    _check(results, "cancel_job on a running job", True, vibeue.cancel_job("vibeue-test-cancel"))
    cancelled = await vibeue.wait_job("vibeue-test-cancel", 5)
    _check(results, "cancelled job status", "cancelled", cancelled["status"])

    unknown = vibeue.job_status("vibeue-test-never-started")
    _check(results, "unknown key", "unknown", unknown["status"])

    if _STATE.get("long_s"):
        long_job = vibeue.job_status("vibeue-test-long")
        _check(results, "long job finished", "done", long_job["status"])
        _check(results, "long job ran its whole length", True, long_job["elapsed_s"] >= _STATE["long_s"] - 1)

    idle = await vibeue.wait_idle(30)
    _check(results, "wait_idle reports an idle editor", True, idle["idle"])
    busy = vibeue.editor_busy()
    _check(results, "editor_busy fields", sorted(["idle", "slow_task", "slow_task_text", "modal", "modal_title",
                                                  "shader_jobs", "asset_compile_jobs"]), sorted(busy))
    return results
