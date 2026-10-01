# execute_python_code

## Code that mentions a .py file

The Python plugin's ExecuteFile mode takes a command whose first ".py" is followed by whitespace or the end as a
file path, so these used to fail "Python execution failed" without running.

Run this with execute_python_code, exactly as written:

```python
import unreal
x = 21  # written by build_level.py
print("doubled:", x * 2)
```

Expected: success, and the output contains `doubled: 42`.

---

Run this with execute_python_code:

```python
"""Helpers for make_level.py"""
import unreal
print("docstring ok")
```

Expected: success, and the output contains `docstring ok`.

---

Run this with execute_python_code:

```python
import unreal
# this comment names check.py
raise RuntimeError("line three")
```

Expected: failure, and the error names `line 3` and `line three`.
