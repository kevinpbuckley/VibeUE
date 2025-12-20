import sys
import subprocess
import json

# Run the server and capture ONLY stdout
result = subprocess.run(
    [
        r"e:\az-dev-ops\FPS57\Plugins\VibeUE\Content\Python\.venv\Scripts\python.exe",
        "-m", "vibe_ue_server"
    ],
    cwd=r"e:\az-dev-ops\FPS57\Plugins\VibeUE\Content\Python",
    env={"PYTHONPATH": r"e:\az-dev-ops\FPS57\Plugins\VibeUE\Content\Python"},
    input='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}\n',
    capture_output=True,
    text=True,
    timeout=5
)

print("=== STDOUT ===")
print(repr(result.stdout))
print("\n=== STDERR ===")
print(repr(result.stderr))
print("\n=== Return Code ===")
print(result.returncode)
