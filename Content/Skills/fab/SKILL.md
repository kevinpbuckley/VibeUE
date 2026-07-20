---
name: fab
display_name: Fab Library Import
description: Discover the signed-in Epic account's OWNED Fab library (free + already-purchased) and import chosen assets into the project — no purchasing (FabService). Use when the user asks "what Fab assets do I own", to find/list/search their Fab (or old UE Marketplace) library, to check Fab compatibility with this engine, or to import/download an owned Fab asset (pack, plugin, Megascans/Quixel, glTF/FBX model, material) into the project. Not for buying assets or browsing the whole Fab store — this only sees what the account already owns.
vibeue_classes:
  - FabService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - fab
  - fab.com
  - marketplace
  - unreal marketplace
  - my library
  - owned assets
  - quixel
  - megascans
  - bridge
  - epic account
  - import asset
  - download asset
  - asset pack
  - entitlements
  - free assets
  - purchased assets
  - engine version compatibility
---

# Fab library import

Discover and import assets the signed-in **Epic account already owns** on [Fab](https://fab.com)
(free + previously purchased, including migrated UE Marketplace items) into the current project.
**There is no purchase capability — this only lists and imports what the account owns.**

`FabService` is a thin orchestration layer over Unreal 5.8's built-in **Fab plugin** (auth, download,
import). It reuses the Epic login the editor/launcher already holds — normally **no separate sign-in**.

## When to use / not use

- Use for: "what Fab assets do I own?", "list/search my Fab library", "is this compatible with 5.8?",
  "import that Megascans rock / that pack / that plugin I own".
- Do **not** use for: buying assets, browsing the full Fab catalog, or claiming free listings you don't
  yet own — none of that is supported (issue #517 is import-only).

## The flow (always this order)

1. **`auth_status()` FIRST.** Confirms an Epic session is available and the library is reachable. If it
   reports not-authenticated, tell the user to sign into Fab once (open the Fab window / Epic Games
   Launcher) — do not retry blindly.
2. **`list_library(...)`** — enumerate owned assets (cached in-process; pass `refresh=true` to re-fetch).
   Filter locally by `name_filter`, `type_filter`, `engine_version` (empty = current engine).
3. **`get_asset(asset_id)`** — full record for one asset (all `projectVersions`, `engineVersions`,
   `distributionMethod`, images) when you need to choose a version or confirm compatibility.
4. **`import_asset(asset_id, ...)`** — download + import. **Async**: returns immediately with
   `status: "downloading"`. Poll **`import_status(asset_id)`** until it reports `imported` (with the
   created `/Game/...` asset paths + `install_root`) or `failed`. Packs can be large (hundreds of MB to
   several GB), so the download runs on background ticks — poll periodically, don't block on one call.
5. **Verify** with `unreal.EditorAssetLibrary.does_asset_exist(path)` (or `does_directory_exist(install_root)`)
   before claiming success.

**Supported import types:** UE **packs** and **plugins** (BuildPatch — the `unreal-engine` /
`ASSET_PACK` / `COMPLETE_PROJECT` / `ENGINE_PLUGIN` assets), installed non-destructively into the
project. **Not yet supported:** glTF/FBX 3D models and Quixel/Megascans (they use a different download
format) — `import_asset` returns `DOWNLOAD_INFO_FAILED` with a clear message for those. Discovery
(`list_library`/`get_asset`) covers **all** owned assets regardless.

## Return shape

Every method returns a JSON string with `success: true|false`. Errors carry `error_code` + `error`
(e.g. `NOT_AUTHENTICATED`, `ENGINE_VERSION_MISMATCH`, `ASSET_NOT_OWNED`, `DOWNLOAD_FAILED`). `list_library`
returns a **compact** projection (id, title, type, source, `compatible` flag, `distributionMethod`,
thumbnail url); call `get_asset` for the full record — never dump the raw library feed.

## Idempotency & rules

- `import_asset` is idempotent — it skips assets already imported (checks the Fab install registry and
  `EditorAssetLibrary`). Re-importing reports the existing path rather than duplicating.
- Respect engine compatibility: if `get_asset` shows no `projectVersions` entry for this engine,
  `import_asset` returns `ENGINE_VERSION_MISMATCH` listing the versions that *are* available — surface
  those, don't force a mismatched build.
- **Never** expose or offer to save the raw downloaded asset files outside the project. The Fab license
  permits use embedded in your project; redistributing raw asset files is the red line.

## Discover exact signatures

`discover_python_class('unreal.FabService')` for the current method list and defaults. Methods:
`auth_status`, `list_library`, `get_asset`, `import_asset`, `import_status`.

> This uses Epic's private, unofficial Fab API with the user's own account. It's how the built-in Fab
> plugin works too; it is unsupported and can change. Act only on the user's own library.
