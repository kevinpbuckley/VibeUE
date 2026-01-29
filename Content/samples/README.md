# Sample AI Instructions

This folder contains project-generic templates for AI coding assistant instructions that include VibeUE integration.

## Files

- **CLAUDE.sample.md** - Template for Claude Code (claude.ai/code) - place at project root as `CLAUDE.md`
- **copilot-instructions.sample.md** - Template for GitHub Copilot - place at `.github/copilot-instructions.md`

## Usage

1. Copy the appropriate sample file to your project
2. Rename it (remove `.sample` from the filename)
3. Update the `<!-- TODO: ... -->` sections with your project-specific details
4. Add any project-specific architecture, commands, or patterns

## What's Included

Both templates include:

- **VibeUE MCP Integration** - Configuration for connecting AI assistants to Unreal Editor
- **Skills System Documentation** - How to use the lazy-loading skills for efficient context management
- **All 13 Available Skills** - Complete list with descriptions and when to load each
- **vibeue_apis Pattern** - How to use skill responses for correct method signatures
- **Python Scripting Basics** - Safety rules and common patterns
- **Build/Debug Instructions** - Common issues and solutions

## Customization

Add your project-specific sections:

- Project architecture and structure
- Game-specific patterns (character system, gameplay mechanics, etc.)
- Custom build scripts and commands
- Team conventions and coding standards
- Project-specific asset naming conventions
