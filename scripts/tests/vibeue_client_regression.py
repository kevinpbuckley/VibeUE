"""Regression checks for scripts/vibeue_client.py against a scripted stand-in MCP server (#700). No editor needed.

    python scripts/tests/vibeue_client_regression.py

Each case starts a tiny HTTP server on 127.0.0.1 that answers the way UE 5.9's MCP server does (the refusal texts
and JSON-RPC error reasons are copied from ModelContextProtocolServer.cpp), runs the client against it and checks
its exit code, its output and how many calls it made. Prints one line per check and exits 1 on any failure.

Cases: a call refused twice as "running a slow task" then served (retried, exit 0); a modal dialog (reported at once,
exit 3, no retry); a session the server forgot (HTTP 404, re-initialized once); a server that never answers (exit 5);
no server at all (exit 4); --busy-wait running out (exit 2); a health signal that reports a modal (exit 3 before any
tools/call is sent).
"""

import contextlib
import io
import json
import os
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import vibeue_client  # noqa: E402

SLOW = ("Not run: the Unreal Editor is running a slow task, which blocks editor tools. This call made no changes. "
        "The task must finish before this call can run, so wait for it to finish, then retry. Retrying while it runs "
        "returns this message again.")
MODAL = ("Not run: the Unreal Editor is showing a modal dialog, which blocks editor tools. This call made no changes. "
         "The dialog must close before this call can run, so ask the user to close it, then retry. Retrying while it "
         "is open returns this message again.")


class Script(object):
    """What the stand-in server answers to the n-th tools/call."""

    def __init__(self, answers, forget_session_once=False, hang=False):
        self.answers = list(answers)
        self.calls = 0
        self.initializes = 0
        self.forget_session_once = forget_session_once
        self.hang = hang


def make_handler(script):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def _reply(self, code, payload, session="s-1"):
            body = json.dumps(payload).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Mcp-Session-Id", session)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            length = int(self.headers.get("Content-Length", 0))
            msg = json.loads(self.rfile.read(length) or b"{}")
            method = msg.get("method")
            if method == "initialize":
                script.initializes += 1
                return self._reply(200, {"jsonrpc": "2.0", "id": msg["id"], "result": {"protocolVersion": "2025-06-18"}},
                                   session="s-{}".format(script.initializes))
            if method == "notifications/initialized":
                self.send_response(202)
                self.end_headers()
                return None
            if method == "tools/call":
                if script.hang:
                    threading.Event().wait(5)
                    return None
                if script.forget_session_once and script.initializes == 1:
                    script.forget_session_once = False
                    self.send_response(404)
                    self.end_headers()
                    return None
                script.calls += 1
                answer = script.answers.pop(0) if script.answers else ("ok", '{"returnValue": "done"}')
                kind, text = answer
                if kind == "rpc-error":
                    return self._reply(400, {"jsonrpc": "2.0", "id": msg["id"], "error": {
                        "code": -32603, "message": "busy", "data": {"reason": text}}})
                return self._reply(200, {"jsonrpc": "2.0", "id": msg["id"], "result": {
                    "content": [{"type": "text", "text": text}], "isError": kind == "error"}})
            return self._reply(200, {"jsonrpc": "2.0", "id": msg.get("id"), "result": {}})

    return Handler


@contextlib.contextmanager
def server(script):
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(script))
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    try:
        yield httpd.server_address[1]
    finally:
        httpd.shutdown()
        httpd.server_close()


def run_client(argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = vibeue_client.main(argv)
    return code, out.getvalue(), err.getvalue()


RESULTS = []


def check(name, expected, actual):
    ok = expected == actual
    RESULTS.append(ok)
    print("{} {}: expected {!r}, got {!r}".format("PASS" if ok else "FAIL", name, expected, actual))


def main():
    # Shorten the busy pause so the suite runs in seconds.
    real_sleep = vibeue_client.time.sleep
    vibeue_client.time.sleep = lambda s: real_sleep(min(s, 0.05))

    # 1. Slow task twice, then served.
    script = Script([("error", SLOW), ("error", SLOW)])
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "call", "EditorToolset.EditorAppToolset", "IsPIERunning"])
    check("slow task: exit code", 0, code)
    check("slow task: calls made (2 refused + 1 served)", 3, script.calls)
    check("slow task: result printed", "done", out.strip())
    check("slow task: says it waited", True, "editor busy" in err)

    # 2. Modal: reported at once, never retried.
    script = Script([("error", MODAL)])
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "exec", "1+1"])
    check("modal: exit code", vibeue_client.EXIT_MODAL, code)
    check("modal: calls made", 1, script.calls)
    check("modal: message", True, "modal dialog" in err)

    # 3. JSON-RPC level refusals (other methods): slowTaskInProgress retried, modalDialogOpen reported.
    script = Script([("rpc-error", "slowTaskInProgress"), ("rpc-error", "modalDialogOpen")])
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "exec", "1+1"])
    check("rpc refusals: exit code", vibeue_client.EXIT_MODAL, code)
    check("rpc refusals: calls made", 2, script.calls)

    # 4. A forgotten session (editor restarted): re-initialized once, then served.
    script = Script([], forget_session_once=True)
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "exec", "1+1"])
    check("forgotten session: exit code", 0, code)
    check("forgotten session: initialize count", 2, script.initializes)

    # 5. Busy longer than --busy-wait: exit 2, nothing run.
    script = Script([("error", SLOW)] * 50)
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "--busy-wait", "0.2", "exec", "1+1"])
    check("busy-wait runs out: exit code", vibeue_client.EXIT_BUSY, code)

    # 6. No reply within --timeout: exit 5 and a do-not-resend note.
    script = Script([], hang=True)
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "--timeout", "1", "exec", "1+1"])
    check("timeout: exit code", vibeue_client.EXIT_TIMEOUT, code)
    check("timeout: says do not resend", True, "do not send it again" in err)

    # 7. Nothing listening: exit 4.
    with server(Script([])) as port:
        pass
    code, out, err = run_client(["--port", str(port), "exec", "1+1"])
    check("no server: exit code", vibeue_client.EXIT_CONNECT, code)

    # 8. A health signal that says a modal is open: refused before any tools/call goes out.
    project = tempfile.mkdtemp(prefix="vibeue_client_test_")
    signals = os.path.join(project, "Saved", "VibeUE", "Signals")
    os.makedirs(signals)
    with open(os.path.join(signals, "editor-1234-health.json"), "w") as handle:
        json.dump({"signal": "health", "pid": 1234, "slowTask": False, "modal": True,
                   "modalTitle": "Save Content"}, handle)
    script = Script([])
    with server(script) as port:
        code, out, err = run_client(["--port", str(port), "--project", project, "exec", "1+1"])
    check("health modal: exit code", vibeue_client.EXIT_MODAL, code)
    check("health modal: no tools/call sent", 0, script.calls)
    check("health modal: names the dialog", True, "Save Content" in err)

    vibeue_client.time.sleep = real_sleep
    failed = RESULTS.count(False)
    print("{} passed, {} failed".format(RESULTS.count(True), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
