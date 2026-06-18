# Copyright Buckley Builds LLC 2026 All Rights Reserved.
#
# Registers VibeUE's markdown skill packs as native Unreal Engine 5.8 agent skills, so they are
# discovered through Epic's own ToolsetRegistry.AgentSkillToolset (ListSkills / GetSkills) on the
# MCP endpoint — the same single skill system the engine ships its skills through.
#
# Each Content/Skills/<name>/SKILL.md becomes one skill; each sibling sub-doc *.md becomes its own
# skill so deep-reference material stays individually loadable. Skills are data-driven from the
# markdown (read at startup) — there is no generated asset and no chat dependency.

import inspect
import unreal

from pathlib import Path

from toolset_registry.agent_skill import agent_skill

# Keep references to the dynamically created skill classes alive for the session.
_VIBEUE_SKILL_CLASSES = []

_SKILLS_DIR = Path(__file__).resolve().parent.parent / "Skills"


def _parse_frontmatter(text):
    """Return (description, body) from a markdown file with optional YAML frontmatter."""
    if not text.startswith("---"):
        return "", text.strip()

    close = text.find("\n---", 3)
    if close == -1:
        return "", text.strip()

    frontmatter = text[3:close]
    body = text[close + 4:].lstrip("\r\n")

    description = ""
    for line in frontmatter.splitlines():
        stripped = line.strip()
        if stripped.startswith("description:"):
            description = stripped[len("description:"):].strip().strip('"').strip("'")
            break
    return description, body


def _register_skill(class_suffix, description, instructions):
    """Create and register a UAgentSkill subclass with the given content."""
    class_name = "VibeUE_" + class_suffix
    skill_cls = type(class_name, (unreal.AgentSkill,), {
        "__doc__": description or class_suffix,
        "instructions": instructions,
    })
    skill_cls = agent_skill(skill_cls)
    # agent_skill() sets the CDO from the docstring; set it explicitly too so multi-line
    # descriptions are preserved verbatim rather than cleandoc-normalized.
    cdo = skill_cls.get_default_object()
    cdo.set_editor_property("description", inspect.cleandoc(description or class_suffix))
    cdo.set_editor_property("instructions", instructions)
    _VIBEUE_SKILL_CLASSES.append(skill_cls)


def _identifier(value):
    """Turn a skill/section name into a valid Python identifier fragment."""
    return "".join(ch if ch.isalnum() else "_" for ch in value)


def _register_vibeue_skills():
    if not _SKILLS_DIR.is_dir():
        unreal.log_warning("VibeUE: Skills directory not found at {}".format(_SKILLS_DIR))
        return

    count = 0
    for skill_dir in sorted(p for p in _SKILLS_DIR.iterdir() if p.is_dir()):
        skill_name = skill_dir.name
        skill_md = skill_dir / "SKILL.md"
        if not skill_md.is_file():
            continue

        try:
            description, body = _parse_frontmatter(skill_md.read_text(encoding="utf-8"))
            _register_skill(_identifier(skill_name), description, body)
            count += 1

            # Register sibling sub-docs (deep reference material) as their own skills.
            for sub in sorted(skill_dir.glob("*.md")):
                if sub.name.lower() == "skill.md":
                    continue
                sub_desc, sub_body = _parse_frontmatter(sub.read_text(encoding="utf-8"))
                section = sub.stem
                label = "[{} sub-doc] {}".format(skill_name, sub_desc or section)
                _register_skill(
                    "{}__{}".format(_identifier(skill_name), _identifier(section)),
                    label, sub_body)
                count += 1
        except Exception as exc:  # noqa: BLE001 — one bad pack shouldn't break the rest
            unreal.log_warning("VibeUE: failed to register skill '{}': {}".format(skill_name, exc))

    unreal.log("VibeUE: registered {} skill(s) with Unreal's AgentSkill system".format(count))


_register_vibeue_skills()
