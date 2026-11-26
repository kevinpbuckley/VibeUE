#!/usr/bin/env python3
# Copyright Buckley Builds LLC 2026 All Rights Reserved.
"""A small command-line client for the Unreal Editor's MCP endpoint (UE 5.9), standard library only.

For sessions whose MCP tools never connected, for shell scripts, and for any caller that needs
"wait while the editor is busy" done right. (#700)

    python vibeue_client.py [options] py FILE              run a Python file through execute_python
    python vibeue_client.py [options] exec "CODE"          run inline Python
    python vibeue_client.py [options] call TOOLSET TOOL ['{"json": "arguments"}']
    python vibeue_client.py [options] job KEY              vibeue.job_status(KEY) (a vibeue.start_job job)
    python vibeue_client.py [options] tools                tools/list
    python vibeue_client.py [options] health               the editor's VibeUE health signal (needs --project)

Options:
    --url URL            endpoint (default http://127.0.0.1:<port>/mcp)
    --port N             port when --url is not given (default 8000)
    --project DIR        the project folder: enables the health-signal checks below
    --timeout S          seconds to wait for one HTTP reply (default 120)
    --busy-wait S        how long to keep retrying while the editor runs a slow task (default 900)
    --json               print the raw tool result JSON instead of its text

What it does that a bare HTTP call does not:
- initializes an MCP session, sends Mcp-Session-Id, re-initializes once when the server forgets it
  (HTTP 404 / 400 after an editor restart);
- reads Server-Sent-Events replies as well as plain JSON;
- a call refused with "Not run: the Unreal Editor is running a slow task" made no change, so it is
  retried with a growing pause (2 s up to 30 s) until --busy-wait runs out (exit 2). With --project
  it watches Saved/VibeUE/Signals/editor-<pid>-health.json instead of calling again while slowTask is
  true;
- a modal dialog is NOT waited on: only a person can close it. It prints the dialog's title (from the
  health signal when --project is given) and exits 3, so a script never loops on it;
- a reply that does not come within --timeout exits 5 and says so: the editor may still be running the
  script, so do not send it again; read the Output Log or a job's status first.

Exit codes: 0 ok, 1 the tool or script failed (its error is printed), 2 still busy after --busy-wait,
3 a modal dialog is open, 4 cannot connect, 5 no reply within --timeout, 64 usage error.

    python vibeue_client.py --port 8000 --project C:/Proj exec "import unreal; unreal.SystemLibrary.get_engine_version()"
    python vibeue_client.py py C:/Proj/Tools/build_level.py
    python vibeue_client.py call EditorToolset.EditorAppToolset IsPIERunning
"""

import argparse
import glob
import itertools
import json
import os
import socket
import sys
import time
import urllib.error
import urllib.request

PYTHON_TOOLSET = "editor_toolset.toolsets.python_scripting.PythonScriptingTools"
SLOW_TASK_MARK = "running a slow task"
MODAL_MARK = "showing a modal dialog"

EXIT_OK, EXIT_FAILED, EXIT_BUSY, EXIT_MODAL, EXIT_CONNECT, EXIT_TIMEOUT, EXIT_USAGE = 0, 1, 2, 3, 4, 5, 64


class ClientError(Exception):
    """A failure with the exit code it maps to."""

    def __init__(self, message, code):
        super().__init__(message)
        self.code = code


def _log(message):
    sys.stderr.write("vibeue_client: {}\n".format(message))
    sys.stderr.flush()


class HealthSignal(object):
    """Reads <project>/Saved/VibeUE/Signals/editor-<pid>-health.json (the newest one)."""

    def __init__(self, project):
        self.project = project

    def read(self):
        if not self.project:
            return None
        pattern = os.path.join(self.project, "Saved", "VibeUE", "Signals", "editor-*-health.json")
        files = sorted(glob.glob(pattern), key=os.path.getmtime, reverse=True)
        for path in files:
            try:
                with open(path, "r", encoding="utf-8") as handle:
                    data = json.load(handle)
            except (OSError, ValueError):
                continue
            data["_age_s"] = round(time.time() - os.path.getmtime(path), 1)
            data["_path"] = path
            return data
        return None


class Client(object):
    def __init__(self, url, timeout=120.0, busy_wait=900.0, project=None):
        self.url = url
        self.timeout = float(timeout)
        self.busy_wait = float(busy_wait)
        self.health = HealthSignal(project)
        self.session = None
        self._ids = itertools.count(int.from_bytes(os.urandom(3), "big"))

    # --- transport -------------------------------------------------------------------------

    def _post(self, payload):
        req = urllib.request.Request(self.url, data=json.dumps(payload).encode("utf-8"), method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("Accept", "application/json, text/event-stream")
        req.add_header("MCP-Protocol-Version", "2025-06-18")
        if self.session:
            req.add_header("Mcp-Session-Id", self.session)
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as reply:
                sid = reply.headers.get("Mcp-Session-Id")
                if sid:
                    self.session = sid
                body = reply.read().decode("utf-8", "replace")
                ctype = reply.headers.get("Content-Type", "")
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", "replace") if exc.fp else ""
            parsed = _parse_body(body, exc.headers.get("Content-Type", "") if exc.headers else "", payload)
            if parsed is not None and "error" in parsed:
                return parsed, exc.code
            raise
        except (socket.timeout, TimeoutError):
            raise ClientError("no reply within {:.0f} s. The editor may still be running the call: do not send it "
                              "again; read the Output Log (or vibeue.job_status) first.".format(self.timeout),
                              EXIT_TIMEOUT)
        except urllib.error.URLError as exc:
            if isinstance(exc.reason, (socket.timeout, TimeoutError)):
                raise ClientError("no reply within {:.0f} s. The editor may still be running the call: do not "
                                  "send it again.".format(self.timeout), EXIT_TIMEOUT)
            raise ClientError("cannot connect to {}: {}. Is the editor running with its MCP server on this "
                              "port?".format(self.url, exc.reason), EXIT_CONNECT)
        except ConnectionError as exc:
            raise ClientError("connection to {} failed: {}".format(self.url, exc), EXIT_CONNECT)
        return _parse_body(body, ctype, payload), 200

    def initialize(self):
        self.session = None
        reply, _ = self._post({"jsonrpc": "2.0", "id": next(self._ids), "method": "initialize",
                               "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                                          "clientInfo": {"name": "vibeue_client", "version": "1.0"}}})
        if reply and "error" in reply:
            raise ClientError("initialize failed: {}".format(json.dumps(reply["error"])), EXIT_FAILED)
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        return reply

    def rpc(self, method, params=None):
        """One JSON-RPC call with session handling and busy-editor retries. Returns its result."""
        if self.session is None:
            self.initialize()
        deadline = time.monotonic() + self.busy_wait
        pause = 2.0
        reinitialized = False
        while True:
            self._check_modal_before_call()
            payload = {"jsonrpc": "2.0", "id": next(self._ids), "method": method, "params": params or {}}
            try:
                reply, status = self._post(payload)
            except urllib.error.HTTPError as exc:
                if exc.code in (400, 404) and not reinitialized:
                    reinitialized = True      # the server forgot the session (editor restarted)
                    self.initialize()
                    continue
                raise ClientError("HTTP {} from {}".format(exc.code, self.url), EXIT_FAILED)
            if reply is None:
                return None
            if "error" in reply:
                error = reply["error"] or {}
                reason = ((error.get("data") or {}).get("reason") or "") if isinstance(error, dict) else ""
                text = json.dumps(error)
                if reason == "modalDialogOpen":
                    raise self._modal_error()
                if reason == "slowTaskInProgress":
                    pause = self._wait_busy(deadline, pause, "a slow task")
                    continue
                if "session" in text.lower() and not reinitialized:
                    reinitialized = True
                    self.initialize()
                    continue
                raise ClientError("JSON-RPC error: {}".format(text), EXIT_FAILED)
            result = reply.get("result")
            text = _result_text(result)
            if isinstance(result, dict) and result.get("isError"):
                if text.startswith("Not run:") and MODAL_MARK in text:
                    raise self._modal_error()
                if text.startswith("Not run:") and SLOW_TASK_MARK in text:
                    pause = self._wait_busy(deadline, pause, "a slow task")
                    continue
            return result

    def _check_modal_before_call(self):
        signal = self.health.read()
        if signal and signal.get("_age_s", 99) < 20 and signal.get("modal"):
            raise self._modal_error(signal)

    def _modal_error(self, signal=None):
        signal = signal or self.health.read() or {}
        title = signal.get("modalTitle") or "(title unknown: pass --project to read it)"
        return ClientError("the editor is showing a modal dialog: {}. Only a person can close it; nothing was "
                           "run. Ask the user to close it, then call again.".format(title), EXIT_MODAL)

    def _wait_busy(self, deadline, pause, what):
        """Sleep before retrying a refused call; with a health signal, until slowTask turns false."""
        if time.monotonic() >= deadline:
            raise ClientError("the editor was still running {} after {:.0f} s (--busy-wait). Nothing was run; "
                              "call again later.".format(what, self.busy_wait), EXIT_BUSY)
        signal = self.health.read()
        if signal and signal.get("_age_s", 99) < 20 and signal.get("slowTask"):
            _log("editor busy: {} ({:.0f} s so far); waiting".format(
                signal.get("slowTaskText") or "slow task", signal.get("slowTaskSeconds") or 0))
            while time.monotonic() < deadline:
                time.sleep(min(pause, max(0.5, deadline - time.monotonic())))
                signal = self.health.read()
                if not signal or not signal.get("slowTask"):
                    break
                if signal.get("modal"):
                    raise self._modal_error(signal)
            return min(pause * 1.5, 30.0)
        _log("editor busy ({}); retrying in {:.0f} s".format(what, pause))
        time.sleep(min(pause, max(0.5, deadline - time.monotonic())))
        return min(pause * 1.5, 30.0)

    # --- calls -----------------------------------------------------------------------------

    def call(self, toolset, tool, arguments=None):
        """call_tool(toolset_name, tool_name, arguments). Returns (text, is_error, raw result)."""
        result = self.rpc("tools/call", {"name": "call_tool", "arguments": {
            "toolset_name": toolset, "tool_name": tool, "arguments": arguments or {}}})
        return _result_text(result), bool((result or {}).get("isError")), result

    def run_python(self, source):
        """execute_python with the script text. Returns (text, is_error, raw result)."""
        return self.call(PYTHON_TOOLSET, "execute_python",
                         {"script": {"language": "Python", "content": source}})

    def tools(self):
        return self.rpc("tools/list")


def _parse_body(body, ctype, payload):
    if not body or not body.strip():
        return None
    if "text/event-stream" in (ctype or ""):
        messages = []
        for block in body.replace("\r\n", "\n").split("\n\n"):
            lines = [line[5:].lstrip() for line in block.splitlines() if line.startswith("data:")]
            if lines:
                try:
                    messages.append(json.loads("\n".join(lines)))
                except ValueError:
                    pass
        for message in reversed(messages):
            if message.get("id") == payload.get("id"):
                return message
        return messages[-1] if messages else None
    try:
        return json.loads(body)
    except ValueError:
        return {"error": {"message": body.strip()[:500]}}


def _result_text(result):
    if not isinstance(result, dict):
        return ""
    parts = [c.get("text", "") for c in result.get("content") or [] if c.get("type") == "text"]
    return "\n".join(parts)


def _unwrap_call_tool_text(text):
    """call_tool wraps the tool's value as {"returnValue": ...}; show the value itself."""
    try:
        data = json.loads(text)
    except ValueError:
        return text
    if isinstance(data, dict) and set(data) == {"returnValue"}:
        value = data["returnValue"]
        return value if isinstance(value, str) else json.dumps(value, indent=1)
    return json.dumps(data, indent=1)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--url")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--project")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--busy-wait", type=float, default=900.0)
    parser.add_argument("--json", action="store_true")
    sub = parser.add_subparsers(dest="command")
    p_py = sub.add_parser("py")
    p_py.add_argument("file")
    p_exec = sub.add_parser("exec")
    p_exec.add_argument("code")
    p_call = sub.add_parser("call")
    p_call.add_argument("toolset")
    p_call.add_argument("tool")
    p_call.add_argument("arguments", nargs="?", default="{}")
    p_job = sub.add_parser("job")
    p_job.add_argument("key")
    sub.add_parser("tools")
    sub.add_parser("health")
    args = parser.parse_args(argv)
    if not args.command:
        parser.print_help()
        return EXIT_USAGE

    url = args.url or "http://127.0.0.1:{}/mcp".format(args.port)
    client = Client(url, timeout=args.timeout, busy_wait=args.busy_wait, project=args.project)
    try:
        if args.command == "health":
            signal = client.health.read()
            if signal is None:
                _log("no health signal found (pass --project <project folder>; the editor writes it every ~5 s)")
                return EXIT_FAILED
            print(json.dumps(signal, indent=1))
            return EXIT_OK
        if args.command == "tools":
            print(json.dumps(client.tools(), indent=1))
            return EXIT_OK
        if args.command == "py":
            with open(args.file, "r", encoding="utf-8") as handle:
                text, is_error, raw = client.run_python(handle.read())
        elif args.command == "exec":
            text, is_error, raw = client.run_python(args.code)
        elif args.command == "job":
            text, is_error, raw = client.run_python(
                "import vibeue\nvibeue.job_status({!r})".format(args.key))
        else:
            try:
                arguments = json.loads(args.arguments)
            except ValueError as exc:
                _log("arguments must be a JSON object: {}".format(exc))
                return EXIT_USAGE
            text, is_error, raw = client.call(args.toolset, args.tool, arguments)
    except ClientError as exc:
        _log(str(exc))
        return exc.code
    if args.json:
        print(json.dumps(raw, indent=1))
    else:
        print(text if is_error else _unwrap_call_tool_text(text))
    return EXIT_FAILED if is_error else EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
