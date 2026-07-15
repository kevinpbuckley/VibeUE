#!/usr/bin/env python3
"""Consistency check between the functional-area dispatcher table in
Content/Skills/vibeue/SKILL.md and the actual skill packs on disk.

Fails (exit 1) on:
  * dead routes  — a slug in the table's "Load skill(s)" column with no
                   Content/Skills/<slug>/SKILL.md behind it
  * orphan packs — a Content/Skills/<slug>/SKILL.md no dispatcher row routes to
                   (the dispatcher itself, `vibeue`, is exempt)
  * missing descriptions — a SKILL.md without a non-empty frontmatter
                   `description:` (init_unreal.py registers the skill from it,
                   and Epic's AgentSkillToolset.ListSkills HIDES skills whose
                   description is empty — an empty one is an invisible skill)

Stdlib only. Run from anywhere:  python3 scripts/check_skills_consistency.py
"""
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SKILLS_DIR = REPO / "Content" / "Skills"
DISPATCHER = SKILLS_DIR / "vibeue" / "SKILL.md"
DISPATCHER_SLUG = "vibeue"


def dispatcher_routed_slugs(text):
    """Backticked skill slugs from the Load column of dispatcher table rows.

    Rows start with '| **<Area>**'; the Load column is the last cell. Slugs are
    lowercase/digits/hyphens only, which excludes toolset names like
    `list_toolsets` (underscore) or `PhysicsToolsets` (capitals).
    """
    routed = set()
    for line in text.splitlines():
        if not line.startswith("| **"):
            continue
        cells = [c.strip() for c in line.split("|")]
        if len(cells) < 4:
            continue
        load_cell = cells[-2]
        routed.update(re.findall(r"`([a-z0-9]+(?:-[a-z0-9]+)*)`", load_cell))
    return routed


def frontmatter_description(text):
    if not text.startswith("---"):
        return None
    close = text.find("\n---", 3)
    if close == -1:
        return None
    for line in text[3:close].splitlines():
        stripped = line.strip()
        if stripped.startswith("description:"):
            return stripped[len("description:"):].strip().strip('"').strip("'")
    return None


def main():
    errors = []

    if not DISPATCHER.is_file():
        print(f"FATAL: dispatcher not found at {DISPATCHER}", file=sys.stderr)
        return 1

    routed = dispatcher_routed_slugs(DISPATCHER.read_text(encoding="utf-8"))
    packs = {p.name for p in SKILLS_DIR.iterdir() if (p / "SKILL.md").is_file()}

    dead = sorted(routed - packs)
    orphans = sorted(packs - routed - {DISPATCHER_SLUG})
    if dead:
        errors.append(f"dead routes (in dispatcher table, no pack on disk): {dead}")
    if orphans:
        errors.append(f"orphan packs (on disk, no dispatcher row routes to them): {orphans}")

    for slug in sorted(packs):
        desc = frontmatter_description((SKILLS_DIR / slug / "SKILL.md").read_text(encoding="utf-8"))
        if not desc:
            errors.append(
                f"{slug}/SKILL.md has no frontmatter description — the registered "
                "skill would be hidden from ListSkills")

    if errors:
        print("skills-consistency: FAIL")
        for e in errors:
            print(f"  - {e}")
        return 1

    print(f"skills-consistency: OK — {len(routed)} routed slugs match "
          f"{len(packs) - 1} packs (+ dispatcher), all descriptions present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
