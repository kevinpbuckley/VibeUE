// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VibeUE
{
	struct FPythonExecutionResult;
}

/**
 * Persists the outcome of every execute_python_code run to per-process JSON files under
 * Project/Saved/VibeUE/Signals (B2).
 *
 * Why this exists: Epic's MCP endpoint runs on the game thread with no request timeout, so a client
 * that gives up after its own timeout (~30s for most, 300s for some) leaves the script still running
 * in the editor. Its stdout, exceptions and result are then gone — the agent can only re-run it
 * (double-executing a mutation) or grep the log. Every run therefore gets a sequential runId, and on
 * completion its full result is written:
 *  - python-<pid>-last.json  — always the latest run (write-then-move, watchers never see a partial).
 *  - python-<pid>-runs.jsonl — one JSON object per line, appended, capped to the last N runs / ~2 MB.
 *
 * How an agent recovers a lost result: the timed-out call kept running in THIS editor process, so the
 * agent's next call — `execute_python_code("import vibeue; print(vibeue.last_python_result())")` —
 * reads python-<pid>-last.json (the read happens before its own result is persisted, so it sees the
 * lost run). vibeue.python_run(run_id) fetches a specific run from the JSONL history.
 *
 * A write failure here must NEVER fail the Python run: every path logs Warning and returns.
 */
class VIBEUE_API FVibeUEPythonResultLog
{
public:
	/** Monotonic per-process run id (1, 2, 3, ...). Call exactly once per execute_python_code run. */
	static int64 NextRunId();

	/** Latest-result file path for a process id. Does not touch the filesystem. */
	static FString GetLastResultPathForPid(uint32 ProcessId);

	/** Run-history (JSON Lines) file path for a process id. Does not touch the filesystem. */
	static FString GetRunsPathForPid(uint32 ProcessId);

	/**
	 * Write the run's outcome to python-<pid>-last.json (atomic replace) and append it to
	 * python-<pid>-runs.jsonl (trimmed to the last N lines / ~2 MB). The run id, success, output,
	 * error, result, execution time and start/finish timestamps come from Result. The label contains
	 * only the run id and operation name; submitted source is deliberately never persisted because it
	 * may contain credentials. Never throws; logs Warning on any failure.
	 */
	static void Record(
		const VibeUE::FPythonExecutionResult& Result,
		const FDateTime& StartedUtc,
		const FDateTime& FinishedUtc);
};
