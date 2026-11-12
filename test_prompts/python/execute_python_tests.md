# execute_python (engine-native, editor_toolset.toolsets.python_scripting.PythonScriptingTools)

## Code that mentions a .py file

The Python plugin's ExecuteFile mode takes a command whose first ".py" is followed by whitespace or the end as a
file path, so these used to fail "Python execution failed" without running.

Run this with execute_python, exactly as written:

```python
import unreal
x = 21  # written by build_level.py
print("doubled:", x * 2)
```

Expected: success, and the output contains `doubled: 42`.

---

Run this with execute_python:

```python
"""Helpers for make_level.py"""
import unreal
print("docstring ok")
```

Expected: success, and the output contains `docstring ok`.

---

Run this with execute_python:

```python
import unreal
# this comment names check.py
raise RuntimeError("line three")
```

Expected: failure, and the error names `line 3` and `line three`.

---

## Running a script file

`code` is Python source, never a script path. Write a file `Saved/VibeUE/run_me.py` in the project containing
`print("ran from file")`, then:

1. Call execute_python with `script` set to just that file's absolute path.
   Expected: failure (a SyntaxError or NameError); the file is not run.
2. Call execute_python with `import unreal, runpy` followed by `runpy.run_path(r"<that path>")`.
   Expected: success, and the output contains `ran from file`.

Delete the file afterwards.

---

## Editor-safety helpers (resident maps, dirty packages, safe_load_level)

These replace the crash guards `execute_python_code` used to apply on its own. Every step below is
read-only or cleans up after itself.

### Resident maps

Run with execute_python:

```python
import unreal
r = unreal.PythonTools.get_resident_maps()
{"type": type(r).__name__, "resident": [str(m) for m in r]}
```

Expected: success; `type` is a list type (`Array` or `list`). In a freshly launched editor with one
level open, `resident` is `[]`. A non-empty list names maps loaded besides the open level. That is
the state in which the next level load fatals the editor, so stop and report it rather than
continuing.

### Dirty packages and the snapshot delta

Run with execute_python:

```python
import unreal, vibeue
P = "/Game/__VibeUETest_Dirty"
assert not unreal.EditorAssetLibrary.does_directory_exist(P), "leftover test folder; delete it first"
before = vibeue.dirty_snapshot()
f = unreal.DataAssetFactory()
f.set_editor_property("data_asset_class", unreal.PrimaryDataAsset)
a = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_DirtyProbe", P, unreal.PrimaryDataAsset, f)
delta = vibeue.dirty_delta(before)
listed = [str(p) for p in unreal.PythonTools.get_dirty_packages()]
unreal.EditorAssetLibrary.delete_directory(P)
{"delta": delta, "probe_listed": any(p.startswith(P) for p in listed),
 "cleaned": not unreal.EditorAssetLibrary.does_directory_exist(P)}
```

Expected: success. `delta` is `{"dirtied": ["/Game/__VibeUETest_Dirty/DA_DirtyProbe"], "cleaned": []}`,
`probe_listed` is `true`, and `cleaned` is `true`.

### safe_load_level refuses instead of crashing

Run with execute_python:

```python
import unreal, vibeue
out = {}
for label, path in [("missing", "/Game/__NoSuchMap_VibeUETest"), ("not_long_path", "Maps/Foo"), ("empty", "")]:
    try:
        vibeue.safe_load_level(path)
        out[label] = "LOADED (wrong)"
    except vibeue.LevelLoadRefused as ex:
        out[label] = "refused: " + str(ex)[:120]
out["level_unchanged"] = str(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name())
out
```

Expected: success. All three entries start with `refused:`. `missing` says there is no map at that
path, `not_long_path` says it is not a long package path, and `empty` asks for a non-empty package
path. `level_unchanged` is the same level that was open before the call, because nothing was loaded.
