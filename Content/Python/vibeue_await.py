# Copyright Buckley Builds LLC 2026 All Rights Reserved.
"""Runs execute_python_code scripts that use top-level await.

What such a script awaits usually needs the editor to tick, so it cannot run to completion inside
one Python command. UPythonTools starts it with begin() and then calls step() from the core ticker,
once per editor frame, until the script ends. Everything the script prints during a step lands in
that step's command output, under the same crash guard as any other execute_python_code run.

Each run has its own event loop, so a crash or an error is attributed to one script and a slow
script never delays another. A loop only runs inside step(); between steps the editor has the game
thread to itself.
"""

import ast
import asyncio
import base64
import inspect

# How many loop iterations a finished script's leftover tasks get to unwind before its loop closes.
_CLEANUP_STEPS = 8

_runs = {}


class _Run:
    def __init__(self, loop, task):
        self.loop = loop
        self.task = task


def _compile(encoded_source):
    source = base64.b64decode(encoded_source)
    return compile(source, "<string>", "exec", flags=ast.PyCF_ALLOW_TOP_LEVEL_AWAIT, dont_inherit=True)


def _advance(loop):
    """One iteration of the loop: what is ready runs, and nothing waits for a timer that is not due."""
    loop.call_soon(loop.stop)
    loop.run_forever()


def _close(run_id):
    run = _runs.pop(run_id, None)
    if run is None:
        return
    loop = run.loop
    try:
        leftovers = asyncio.all_tasks(loop)
        for task in leftovers:
            task.cancel()
        for _ in range(_CLEANUP_STEPS):
            if all(task.done() for task in leftovers):
                break
            _advance(loop)
    finally:
        loop.close()


def needs_await(encoded_source):
    """True when the source compiles only as a coroutine. A syntax error is False: the normal path reports it."""
    try:
        code = _compile(encoded_source)
    except (SyntaxError, ValueError):
        return False
    return bool(code.co_flags & inspect.CO_COROUTINE)


def begin(run_id, encoded_source, namespace=None):
    """Start the script without running any of it. namespace defaults to a new one for this run."""
    if namespace is None:
        namespace = {"__name__": "__main__"}
    coroutine = eval(_compile(encoded_source), namespace)
    loop = asyncio.new_event_loop()
    _runs[run_id] = _Run(loop, loop.create_task(coroutine))


def step(run_id):
    """Advance the script one iteration. Returns 'running', 'done' or 'cancelled'; a script's exception is raised."""
    run = _runs[run_id]
    try:
        _advance(run.loop)
        if not run.task.done():
            return "running"
        run.task.result()
        return "done"
    except asyncio.CancelledError:
        return "cancelled"
    except SystemExit as error:
        # Left to propagate, the interpreter would act on it and take the editor down.
        raise RuntimeError("the script called sys.exit({!r})".format(error.code)) from error
    finally:
        if run.task.done():
            _close(run_id)


def cancel(run_id):
    """Ask the script to stop: CancelledError at its current await, on the next step()."""
    run = _runs.get(run_id)
    if run is not None:
        run.task.cancel()


def discard(run_id):
    """Drop a run whose step() failed or crashed, whatever state it is in."""
    run = _runs.get(run_id)
    if run is not None:
        run.task.cancel()
    _close(run_id)
