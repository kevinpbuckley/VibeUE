# A host editor that releases maps loaded as assets

A map loaded as an asset (`load_asset`, `find_object`, ...) stays in memory, and opening it later fails the engine's
"old level package cleaned up?" check, which is fatal. A host project whose editor releases such maps before every
map load says so by setting the console variable `VibeUE.HostReleasesResidentMaps` to true; `vibeue.safe_load_level`
then defers to that release. It is off by default. (Ported to 5-9 from PR #685, which fixed #613 on 5-8.)

Run this with `execute_python`:

```python
import unreal
"host releases: {}".format(unreal.PythonTools.host_releases_resident_maps())
```

Expected: the result is `host releases: False` (no host has set the variable).

---

Run this with `execute_python`:

```python
import unreal
before = unreal.PythonTools.host_releases_resident_maps()
unreal.SystemLibrary.execute_console_command(None, "VibeUE.HostReleasesResidentMaps 1")
during = unreal.PythonTools.host_releases_resident_maps()
unreal.SystemLibrary.execute_console_command(None, "VibeUE.HostReleasesResidentMaps " + ("1" if before else "0"))
{"during": during, "restored": unreal.PythonTools.host_releases_resident_maps() == before}
```

Expected: `during` is true and `restored` is true (the variable is back to what it was, so a host that sets it
keeps it).
