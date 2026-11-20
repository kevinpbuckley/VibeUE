# A host editor that releases maps loaded as assets

A map loaded as an asset (`load_asset`, `find_object`, ...) stays in memory, and opening it later fails the engine's
"old level package cleaned up?" check, which is fatal. A host project whose editor releases such maps before every
map load says so by setting the console variable `VibeUE.HostReleasesResidentMaps` to true; `safe_load_level` and
the `RESIDENT_MAPS` log line then defer to that release. It is off by default.

Run this with execute_python_code:

```python
import unreal
print("host releases:", unreal.PythonTools.host_releases_resident_maps())
```

Expected: success, and the output contains `host releases: False` (no host has set the variable).

---

Run this with execute_python_code:

```python
import unreal
before = unreal.PythonTools.host_releases_resident_maps()
unreal.SystemLibrary.execute_console_command(None, "VibeUE.HostReleasesResidentMaps 1")
print("host releases:", unreal.PythonTools.host_releases_resident_maps())
unreal.SystemLibrary.execute_console_command(None, "VibeUE.HostReleasesResidentMaps " + ("1" if before else "0"))
print("restored:", unreal.PythonTools.host_releases_resident_maps() == before)
```

Expected: success, and the output contains `host releases: True` and then `restored: True` (the variable is back
to what it was, so a host that sets it keeps it).
