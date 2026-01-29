# Log Reader Tool Tests - Comprehensive Test Suite

These tests validate the `read_logs` tool functionality. Run sequentially through the VibeUE chat interface or via MCP. Tests cover listing, reading, filtering, tailing, and change detection for Unreal Engine log files.

---

## Part 1: Help and Discovery

Show me the help documentation for the log reading tool.

---

What actions are available for reading logs?

---

What file aliases can I use instead of full paths?

---

## Part 2: List Log Files

List all available log files in the project.

---

List only System category logs.

---

List only VibeUE category logs.

---

Show me all log files sorted by most recently modified.

---

## Part 3: File Information

Get detailed information about the main project log file.

---

Get info on the VibeUE chat log using the "chat" alias.

---

Get info on the raw LLM log using the "llm" alias.

---

How many lines are in the main log file?

---

What's the size of the main log file?

---

When was the main log file last modified?

---

## Part 4: Basic Reading

Read the first 50 lines from the main log.

---

Read the first 100 lines from the chat log.

---

Read the main log with default settings.

---

## Part 5: Paginated Reading

Read lines 0-100 from the main log (first page).

---

Read lines 100-200 from the main log (second page).

---

Read lines 200-300 from the main log (third page).

---

Continue reading until you reach the end of the file, noting how many total lines there are.

---

Read 500 lines starting from line 1000 in the main log.

---

## Part 6: Tail Operations

Show me the last 50 lines of the main log.

---

Show me the last 100 lines of the main log.

---

Tail the chat log - show last 25 lines.

---

Show me the most recent 10 lines from the main log.

---

## Part 7: Head Operations

Show me the first 50 lines of the main log.

---

Show me the first 20 lines of the chat log.

---

Read the header/beginning of the LLM log.

---

## Part 8: Error Filtering

Find all errors in the main log.

---

Find errors in the main log, limit to 50 matches.

---

Show me all errors from today's session.

---

Are there any Fatal errors in the log?

---

## Part 9: Warning Filtering

Find all warnings in the main log.

---

Show me warnings, limited to the first 25.

---

Find warnings in the chat log.

---

## Part 10: Pattern Filtering - Basic

Search the main log for "Blueprint".

---

Search for "Compile" in the main log.

---

Search for "VibeUE" in the main log.

---

Search for "Niagara" in the main log.

---

Search for "Widget" in the main log.

---

## Part 11: Pattern Filtering - Regex

Search for lines containing "Error" OR "Warning" using regex.

---

Find all lines that start with a timestamp pattern.

---

Search for any Blueprint-related errors using pattern: Blueprint.*Error

---

Find all log entries from LogTemp category.

---

Search for function calls that contain "Create" in them.

---

## Part 12: Pattern Filtering with Context

Search for "Error" with 2 lines of context before and after each match.

---

Search for "Warning" with 5 lines of context.

---

Find "Blueprint" mentions with 3 lines of surrounding context.

---

## Part 13: Case Sensitivity

Search for "error" (lowercase) with case-insensitive matching.

---

Search for "ERROR" (uppercase) with case-sensitive matching.

---

Compare results of case-sensitive vs case-insensitive search for "Warning".

---

## Part 14: Combined Filtering

Find errors in the last 1000 lines of the log.

---

Search for "Blueprint" only in the VibeUE chat log.

---

Find compilation-related messages (Compile, Compiling, Compiled).

---

## Part 15: Change Detection

How many lines are currently in the main log?

---

Get new content since line 100 (simulate watching for changes).

---

Record the current line count, then check again after some activity.

---

Use the "since" action to get content added after line 500.

---

## Part 16: Log File Aliases

Read from "main" alias.

---

Read from "system" alias (should be same as main).

---

Read from "project" alias (should be same as main).

---

Read from "chat" alias.

---

Read from "vibeue" alias (should be same as chat).

---

Read from "llm" alias.

---

Read from "rawllm" alias (should be same as llm).

---

## Part 17: Relative Paths

Try reading a log by its filename only (e.g., "FPS57.log").

---

Try reading using a relative path from the Saved/Logs directory.

---

## Part 18: Error Handling

Try to read a log file that doesn't exist.

---

Try to use an invalid action name.

---

Try to filter without providing a pattern.

---

Try to use "since" action without providing last_line.

---

Try to read beyond the end of a file (offset greater than line count).

---

## Part 19: Edge Cases

Read a file with limit=0 (should read all lines).

---

Read with offset=0, limit=1 (just the first line).

---

Tail with lines=1 (just the last line).

---

Filter with max_matches=1 (just the first match).

---

What happens when filtering a pattern with no matches?

---

## Part 20: Large File Handling

Read 2000 lines from the main log (default max).

---

Can we read more than 2000 lines at once?

---

What's the total line count of the largest log file?

---

Read the middle section of a large log file.

---

## Part 21: Specific Log Categories

Find all Blueprint compilation messages.

---

Find all Niagara-related log entries.

---

Find all asset loading messages.

---

Find all Python execution results.

---

Find all MCP server messages.

---

## Part 22: Timestamp Analysis

Find log entries from the last hour (if timestamps are parseable).

---

What's the timestamp of the first log entry?

---

What's the timestamp of the most recent log entry?

---

## Part 23: Performance Testing

Time how long it takes to count lines in the main log.

---

Time how long it takes to filter for a common pattern.

---

Time how long it takes to read 1000 lines.

---

## Part 24: Real-World Scenarios

### Debugging a Blueprint Error

I'm getting a Blueprint compilation error. Find all Blueprint errors and show me context around them.

---

### Checking Plugin Status

Show me the last 100 lines of the VibeUE chat log to see recent AI interactions.

---

### Finding Startup Issues

Show me the first 200 lines of the main log to see what happened during editor startup.

---

### Monitoring for Specific Issues

Set up a watch for any new errors by recording the current line count, then checking for new content with error filtering.

---

### Investigating Crashes

Look for any Fatal errors or crash-related messages in the logs.

---

### API Debugging

Check the raw LLM log for recent API requests and responses.

---

## Part 25: Integration with Other Tools

After finding an error in the logs, can you help me understand what Blueprint or asset it relates to?

---

Find Niagara compilation errors and then help me fix them.

---

Find any Python execution errors and show the code that caused them.

---

## Part 26: Summary and Verification

List all log files one more time with their sizes and line counts.

---

What's the total size of all log files combined?

---

What's the most common error type in the logs?

---

Give me a summary of:
1. Total log files discovered
2. Combined line count across all logs
3. Number of errors found in main log
4. Number of warnings found in main log
5. Most frequently occurring log categories

---

## Part 27: Cleanup Verification

Verify the log reader tool is working correctly by:
1. Listing files
2. Reading a few lines
3. Filtering for a known pattern
4. Tailing the most recent entries

---
