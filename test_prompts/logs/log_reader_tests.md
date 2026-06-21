# Log Reading Tests

> **Rewritten (issue #457).** The old `read_logs` MCP tool (with its `main`/`chat`/`llm`
> aliases, pagination/tail/head/filter actions) was **removed**. Log reading now works two ways:
>
> 1. **Epic `LogsToolset`** via `call_tool` — `GetLogEntries` returns structured editor log
>    entries. **You MUST pass `category=""` and `pattern=""`** for "all"; the defaults are a bogus
>    `"LogsToolset"` category that matches nothing. Discover the exact action/params with
>    `describe_toolset("EditorToolset.LogsToolset")` (or whatever `list_toolsets` shows).
> 2. **Raw file I/O** from `execute_python_code` — the editor log lives under
>    `<ProjectDir>/Saved/Logs/`. Read/tail/grep it with plain Python (`open().read().splitlines()`).
>
> These prompts validate both paths. Run sequentially.

---

## Part 0 — Smoke Test

Use `list_toolsets` / `describe_toolset` to find the engine Logs toolset, then call its
`GetLogEntries` action with `category=""` and `pattern=""` and show me the most recent ~20 entries.

---

Separately, use `execute_python_code` to locate the active editor `.log` file under
`unreal.Paths.project_saved_dir() + "Logs/"` and print its full path, size in KB, and total line count.

---

## Part 1 — Discovery

List the log files in the project's `Saved/Logs` directory (name, size, last-modified), sorted by
most-recently-modified, using Python file I/O.

---

What logging actions does the engine `LogsToolset` expose, and what are the parameters for
`GetLogEntries`? Read them back from `describe_toolset`.

---

## Part 2 — Reading

Read the **first 50** lines of the editor log (Python: `open(path).read().splitlines()[:50]`).

---

Read the **last 50** lines (tail) of the editor log (`splitlines()[-50:]`).

---

Read lines 200–300 of the editor log (a middle page).

---

## Part 3 — Error / Warning Filtering

Find the last 10 lines containing `Error` in the editor log (case-insensitive Python filter).
Then do the same via `LogsToolset.GetLogEntries` with `pattern="Error"` and compare the two results.

---

Find the last 10 lines containing `Warning`.

---

Are there any `Fatal` / crash lines in the editor log? Report how many and show them (there should
normally be none).

---

## Part 4 — Pattern Search

Search the editor log for `Blueprint` (Python substring) and report the match count.

---

Search for `VibeUE` and show 2 lines of surrounding context for the first few matches.

---

Search for lines matching the regex `Error|Warning` and report how many of each.

---

## Part 5 — File Info

Report for the editor log: total line count, byte size, last-modified timestamp, and the timestamp
of the first and last log line (if the lines start with a parseable `[...]` timestamp).

---

## Part 6 — Error Handling (should fail gracefully, not crash)

Try to read a log file that does not exist (e.g. `.../Saved/Logs/DoesNotExist.log`) — confirm you
get a clean Python error / empty result, not a crash.

---

Call `LogsToolset.GetLogEntries` with the **default** category (do NOT pass `category=""`) and show
that it returns nothing useful — confirming the documented gotcha that the default category
(`"LogsToolset"`) matches no real log category.

---

## Part 7 — Real-World Scenario

I think a Blueprint failed to compile. Find any `Blueprint.*Error` lines in the editor log and show
me the surrounding context, then tell me which asset they refer to (if the path is in the message).

---

## Part 8 — Summary

Summarize: which log file is active, its line count and size, how many Error vs Warning lines it
contains, and whether both the `LogsToolset` path and the raw-file path agree.
