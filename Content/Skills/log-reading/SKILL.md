---
name: log-reading
display_name: Log Reading & Diagnostics
description: Read, tail, filter, and watch Unreal Engine log files with the read_logs tool — find errors/warnings, debug Blueprint or Niagara compiles, inspect VibeUE chat and raw LLM API logs, and monitor for new output. Use when the user asks to check the logs, find errors or warnings, see what happened during startup, investigate a crash, review recent AI chat activity, or watch for new log output.
vibeue_classes:
  - LogReaderService
keywords:
  - log
  - logs
  - read logs
  - tail
  - errors
  - warnings
  - filter
  - grep
  - crash
  - debug
  - diagnostics
  - output log
  - chat log
  - llm log
  - startup
  - monitor
  - watch
---

# Log Reading & Diagnostics

The `read_logs` MCP tool reads Unreal Engine log files directly — even while the editor has them open for writing. Use it instead of Python file I/O or shell commands.

## Actions

| Action | What it does | Key params |
|--------|-------------|-----------|
| `list` | Browse available log files | `category` (System, Blueprint, Niagara, VibeUE) |
| `info` | File details: size, line count, modified time | `file` |
| `read` | Paginated reading | `file`, `offset` (0-based), `limit` (default 2000, 0 = whole file) |
| `tail` | Last N lines | `file`, `lines` (default 50) |
| `head` | First N lines | `file`, `lines` (default 50) |
| `filter` | Regex search | `file`, `pattern`, `case_sensitive`, `context_lines`, `max_matches` (default 100) |
| `errors` | Lines matching `Error`/`Fatal` | `file`, `max_matches` |
| `warnings` | Lines matching `Warning` | `file`, `max_matches` |
| `since` | New content after a line number | `file`, `last_line` |
| `help` | Full documentation | — |

## File aliases

| Alias | File | Notes |
|-------|------|-------|
| `main` / `system` / `project` | `<ProjectName>.log` | The big one — engine output, typically 10k+ lines |
| `chat` / `vibeue` | `VibeUE_Chat.log` | Chat transcript: user messages, tool calls, tool results |
| `llm` / `rawllm` | `VibeUE_RawLLM.log` | Raw API requests/responses — **only exists when raw LLM logging is enabled**; expect FILE_NOT_FOUND otherwise |

Plain filenames (`FPS57.log`) and paths relative to `Saved/Logs/` also resolve. Archived chat logs live in `Saved/Logs/VibeUE_ChatArchive/` and show up in `list`.

## Standard workflows

**Triage a problem session:**
1. `errors` on `main` (`max_matches` ~20) — UE "Handled ensure" blocks span many lines but are one incident; group by timestamp.
2. `warnings` on `main` — startup warning noise (missing slate .png files, scalability CVar overrides) is normal; flag security or asset-related warnings.
3. `filter` with `context_lines: 3` around anything suspicious for the surrounding story.

**Watch for new output (e.g. across a PIE run or compile):**
1. `info` on the file → note `line_count`.
2. Do the activity.
3. `since` with `last_line` = the noted count → only the new lines. Repeat with the updated `total_lines`.

**Debug a Blueprint/Niagara compile:** `filter` with a targeted pattern like `Blueprint.*Error` or `LogNiagara.*(Error|Warning)` — not a bare keyword (see gotchas).

## Gotchas

- **Bare keywords are noisy on `main`.** Python introspection dumps (`LogPython`) contain entire class docstrings, so `pattern: "Blueprint"` matches thousands of irrelevant JSON lines. Anchor patterns to log categories or levels: `LogBlueprint.*Error`, `: Warning:.*Widget`.
- **Lines longer than 2000 chars are truncated** with a `...[line truncated; N chars total]` marker (all actions). This bounds LogPython docstring dumps; if you genuinely need a full long line, run the Python that produced it again rather than re-reading the log.
- **Timezones differ between logs.** Timestamps inside `main` log lines (`[2026.06.12-22.21.26:415]`) are **UTC**; `VibeUE_Chat.log` line timestamps are **local time**. `list`/`info` report `modified` in local time (labeled) plus `modified_utc` — don't mix them up when correlating chat activity with engine events.
- **`limit: 0` reads the ENTIRE file.** On a 15k-line main log that's ~1.5 MB into context. Prefer `tail`, paginated `read`, or `filter`; only use `limit: 0` on files you've confirmed small via `info`.
- **Filter results are not paginated by line offset.** Each content line is prefixed `<1-based line number>: `, gaps are marked `---`, and `has_more: true` means the `max_matches` cap was hit — raise `max_matches` or narrow the pattern; there is no `next_offset` for filters.
- **`errors` also matches the word "Error" inside messages** (e.g. callstack lines of a single ensure), and `warnings` can surface `Error:`-level lines that contain the word "Warning". Read the matched lines, don't just count them.
- **Line numbers in filter output are 1-based; `read` offsets are 0-based.** To read around filter match "line 489", use `offset: 484, limit: 10`.
- **The main log grows while you work** — your own tool calls append MCP/chat lines to it. Re-run `info` rather than caching counts.
- **A handled "ensure" is not a crash.** `=== Handled ensure: ===` blocks (e.g. deprecated CVar warnings) are non-fatal; only `Fatal error` / `LogWindows: Error: ... Fatal` indicate a real crash.
