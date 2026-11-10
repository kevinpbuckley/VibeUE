# VibeUE test prompts

**These prompt suites are VibeUE's test surface.** The C++ automation tests were removed on the
5-9 branch: VibeUE is an agent-facing plugin, and what matters is whether an agent driving the MCP
endpoint can actually complete a task — which a unit test asserting on a C++ return value never
measured. Each file here is a scripted session: prompts to paste, and what the result must look like.

## How to run a suite

1. Launch the editor on a project with VibeUE enabled and the MCP server listening
   (`Edit → Editor Preferences → Model Context Protocol → Auto Start Server`, or
   `ModelContextProtocol.StartServer`).
2. Point an agent at the endpoint and give it the project's agent guide
   (`VibeUE.GenerateAgentConfig ClaudeCode` writes it).
3. Work through the suite's prompts in order, in one session, and compare against the expectations
   written beside each prompt.
4. A prompt that needs PIE says so. `Smoke_Test.md` is the fastest end-to-end check.

## What a failure means

These suites exercise the **whole path** — skill discovery, tool schema, the editor write, and the
reply an agent sees. A failure can therefore be any of: a broken tool, a skill that names a tool
that no longer exists, or a guide that teaches the wrong call. Check which before assuming the
service is at fault; a stale skill is the most common cause after a trim.

## Scope note for the 5-9 branch

Several suites deliberately test work that is now done with **Epic's** toolsets rather than VibeUE's
(Behavior Trees, Blackboards, EQS, actors, assets, capture, Python execution). That is intentional:
the prompts describe the *goal*, and an agent carrying VibeUE's skills should still reach it using
whatever tool now owns the job. If a suite fails because the agent could not find the right engine
tool, the bug is usually in the skill's mapping table, not in the engine.

Suites for capabilities VibeUE no longer provides at all (MetaSound authoring, user enums and
structs) were removed — those are wholly Epic's `MetaSoundToolset` and
`UserDefinedEnum`/`UserDefinedStruct` toolsets now, and testing them is not VibeUE's job.
