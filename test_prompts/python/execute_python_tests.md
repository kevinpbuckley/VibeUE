# execute_python (engine-native, EditorToolset.PythonScriptingTools)

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
