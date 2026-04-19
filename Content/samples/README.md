# Sample AI Instructions

This folder contains a project-generic template for AI coding assistant instructions that include VibeUE integration.

## Files

- **AGENTS.md.sample** - Template instructions for any AI coding assistant

## What's Included

- **VibeUE MCP Integration** - MCP tool guidance and discovery workflow
- **Skills System** - All 22 available skills with descriptions and when to load each
- **Skill Response Usage** - How to use `vibeue_apis` for correct method signatures
- **Log Reading** - How to read Unreal and VibeUE logs via MCP

---

## Usage by AI Tool

### Claude Code ✅ Import Supported (Recommended)

Create `CLAUDE.md` at your Unreal project root with a single import line:

```md
# My Unreal Project

<!-- Add project-specific notes here -->

@Plugins/VibeUE/Content/samples/AGENTS.md.sample
```

The `@` directive tells Claude Code to inline the file automatically. No copying needed — updates to the sample are picked up immediately.

### Generic agents (AGENTS.md)

Copy `AGENTS.md.sample` to your project root as `AGENTS.md`:

```
AGENTS.md
```

Read by OpenAI Codex, Cursor, and other agents that follow the AGENTS.md convention.

### GitHub Copilot — Copy Required

Copilot does not support file imports. Copy `AGENTS.md.sample` to:

```
.github/copilot-instructions.md
```

You'll need to re-copy whenever the sample is updated.

### Cursor — Copy Required

Cursor does not support file imports. Copy `AGENTS.md.sample` to:

```
.cursor/rules/vibeue.mdc
```

Or use the legacy format: `.cursorrules`

You'll need to re-copy whenever the sample is updated.

### Google Antigravity — Copy Required

Antigravity does not support file imports. Copy `AGENTS.md.sample` to your workspace rules directory:

```
.agent/rules/vibeue.md
```

You'll need to re-copy whenever the sample is updated.

---

## Customization

Add project-specific sections to your instructions file above or below the VibeUE content:

- Project architecture and structure
- Game-specific patterns (character system, gameplay mechanics, etc.)
- Custom build scripts and commands
- Team conventions and coding standards
- Project-specific asset naming conventions

For Claude Code, add these sections directly in your `CLAUDE.md` around the `@import` line.
