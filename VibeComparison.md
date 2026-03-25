# VibeUE vs Ludus AI — Comparison & Recommendations
_March 2026_

---

## Overview

| | VibeUE | Ludus AI |
|---|---|---|
| **Model** | Open source plugin | Commercial SaaS + plugin |
| **MCP** | Full tool execution inside UE | Read-only knowledge search, no project context |
| **Blueprint control** | Direct graph manipulation via Python API | "Blueprints copilot" — unclear depth |
| **C++ assistance** | Not yet | Core headline feature |
| **3D generation** | No | Yes |
| **Project awareness** | Full — reads/writes your actual project | Plugin only; MCP has none |
| **AI model** | Bring your own (Claude, etc.) | Hosted proprietary service |
| **Pricing** | Free / open source | Freemium subscription |
| **In-editor chat** | Yes | No |
| **UE version support** | 5.7 (active dev) | 5.1–5.7 |

---

## Where VibeUE Wins

- **Execution over knowledge** — VibeUE actually does things inside the editor (create assets, manipulate blueprints, run Python, read logs). Ludus's MCP is a knowledge search with no project context — it can tell you *how* to do something but can't do it for you.
- **Bring your own model** — not locked into a hosted service. Works with Claude, can work with any MCP-compatible client.
- **In-editor chat** — Ludus has no equivalent. Unique feature.
- **Open source** — community can extend it; no subscription dependency; no data leaving the machine.
- **Higher ceiling** — local execution with full project context is architecturally superior. As the tool matures, it can do things a cloud knowledge search simply cannot.

## Where Ludus Wins (Currently)

- **C++ assistance** — generation and guidance for C++ is a headline feature. VibeUE has no equivalent yet.
- **3D generation** — dedicated tool. VibeUE has no equivalent.
- **Polish and docs** — Ludus has a proper docs site, structured onboarding, and has been in market longer. VibeUE setup is still friction-heavy (proxy, token, session init quirks).
- **Discoverability** — Ludus is more visible and has broader mindshare currently.

---

## Key Risks for VibeUE

1. **Gatekeeping** — Ludus is building mindshare. If developers adopt it first and it's "good enough," switching cost grows even if VibeUE is objectively stronger.
2. **Setup friction** — proxy setup, token config, and session init quirks create a high barrier to first use. First impressions matter.
3. **Missing C++ story** — for many UE developers, C++ assistance is the primary ask. Without it, VibeUE is seen as a Blueprint tool only.

---

## Recommendations for Kevin

### Short-term (polish + close gaps)
- **Reduce onboarding friction** — the proxy/token setup is the biggest barrier. A one-click setup or installer would help significantly. Issue #12 (first-run nudge) is a small step.
- **Docs site** — even a basic docs.vibeue.com would signal maturity and improve discoverability.
- **Security warning** — issue #13 is a quick win that also signals the project takes security seriously.

### Medium-term (close feature gaps)
- **C++ assistance** — an MVP is achievable without a full C++ service: a skill + Python workflow that scaffolds boilerplate and triggers Live Coding. This is the single biggest feature gap vs Ludus.
- **Stability** — the proxy and blueprint API fixes already underway (PRs #326–330) are the right foundation.

### Strategic
- **Lean into "execution not advice"** as the differentiator. Ludus tells you what to do. VibeUE does it. That's a fundamentally stronger value proposition once the rough edges are gone.
- **In-editor chat** is underrated — no other tool has this. Worth highlighting more prominently.
- **Community / GitHub presence** — more visibility on the open issues, changelog, and roadmap would help build confidence and contributors.

---

_Notes compiled from direct use of VibeUE and review of ludusengine.com / docs.ludusengine.com (March 2026)._
