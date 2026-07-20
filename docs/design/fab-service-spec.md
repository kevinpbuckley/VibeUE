# FabService — Design Spec

**Issue:** [kevinpbuckley/VibeUE#517](https://github.com/kevinpbuckley/VibeUE/issues/517) — "Add ability to import already owned FAB assets. No purchase functionality."

**Status:** Phase 1 (discovery) + Phase 2 (import of BuildPatch pack/plugin assets) implemented & validated
live. glTF/FBX/Quixel import is future work.
**Target engine:** UE 5.8 (Fab plugin `0.0.13`, ships with source in `Engine/Plugins/Fab/`)
**Owning module:** `VibeUE` (Editor)

> **Implementation notes (validated live against a real 405-asset account):**
> - **Auth (Route A2):** own EOS platform with the Fab plugin's baked creds (read reflectively from
>   `/Fab/Data/FabEos.FabEos`), `PersistentAuth` login reusing the launcher session, token via
>   `EOS_Auth_CopyUserAuthToken`. Silent — no second sign-in.
> - **Library host:** `https://www.fab.com` (not `fab.com` — that host redirects and strips the auth
>   header, giving a headless 404). `GET /e/accounts/{id}/ue/library?count=N`, paged via `cursors.next`.
> - **Download negotiation:** `POST https://www.fab.com/e/artifacts/{artifactId}/manifest` with body
>   `{"namespace":<assetNamespace>,"itemId":<assetId>,"platform":"Windows"}` → `downloadInfo[]` with
>   signed `distributionPoints[].manifestUrl` (3 CDNs) + `distributionPointBaseUrls[]`.
> - **Import (Route R):** reuse the engine Fab plugin's `FAB_API` `FFabDownloadRequest` (BuildPatch,
>   non-destructive into the project) for the download; reimplement the post-install step in VibeUE
>   (BuildPatch does **not** populate `DownloadedFiles`, so we diff the set of `.uasset`/`.umap` files
>   under the project Content dir around the download, then `ScanFilesSynchronous` the new ones).

---

## 1. Goal & scope

Give an AI agent (and, incidentally, any Python caller) the ability to **discover the user's owned Fab
library and import chosen assets into the current project — headlessly, in one round-trip per step**, with
no store UI, no purchasing, and no new sign-in when the editor is already logged into an Epic account.

**In scope**
- Enumerate the signed-in Epic account's owned Fab library (free + previously purchased), with paging,
  engine-version compatibility, and distribution method per item.
- Filter/search that library locally (by name, type, engine version, source — Quixel/Sketchfab/UE).
- Import a chosen owned asset into the project (download → import via the engine's own Fab pipelines).
- Report import results with verifiable evidence (asset paths created).

**Out of scope (explicit)**
- **No purchasing, no "add free listing to library" claim flows, no price/checkout.** #517 is import-only.
- No browsing the *whole* Fab catalog (only what the account owns). Catalog search is a possible later phase.
- No re-distribution of raw asset files outside the user's own project (License red line — see §8).

**Success criteria**
- `unreal.FabService.list_library()` returns the same set of assets the Fab window's "My Library" shows.
- `unreal.FabService.import_asset(asset_id)` lands the asset under `/Game/Fab/...` (or unpacks a pack into
  `/Game/...`) and the agent can confirm it via `EditorAssetLibrary`.
- No second login prompt when the editor/launcher session is already authenticated.

---

## 2. Why this is buildable (key findings)

The official Fab plugin already implements auth, library listing, download, and import. It is **present in
the 5.8 source tree with a partially-exported C++ surface**, so FabService is mostly *orchestration*, not
reimplementation. Findings from reading `Engine/Plugins/Fab/`:

| Capability | Where it lives today | Reusable by us? |
|---|---|---|
| **Auth** (Epic account) | `Private/FabAuthentication.cpp` — **EOS SDK** (`EOS_Auth_*`), creds baked in cooked asset `/Fab/Data/FabEos.FabEos`. Reuses the launcher/editor session via `PersistentAuth` (silent) or `ExchangeCode` (launcher passes `-AUTH_TYPE=exchangecode -AUTH_PASSWORD=…`). Token via `EOS_Auth_CopyUserAuthToken`. | **Token accessor is Private (not exported)** — this is the one real gap. See §4/§6. |
| **List owned library** | `Private/Teds/FabMyFolderIntegration.cpp:43` — a single authenticated REST GET | Endpoint is known; **we replicate the one HTTP call** (needs the token). |
| **Download** | `Public/FabDownloader.h` — `FFabDownloadRequest(AssetID, DownloadURL, Location, EFabDownloadType)` + `FFabDownloadQueue::AddDownloadToQueue` are **`FAB_API`-exported**. HTTP (glTF/FBX/Quixel zip) or BuildPatchServices (UE packs/plugins, chunked, non-destructive into ProjectDir). | **Yes — drivable directly from our module.** |
| **Import** | `Public/Workflows/FabWorkflow.h` (`FFabAssetMetadata`, `IFabWorkflow`) + `FabWorkflowFactoryRegistry` are `FAB_API`, **but every concrete workflow** (`FPackImportWorkflow`, `FGenericImportWorkflow`, Quixel, MetaHuman, plugin) is **Private, and no factory is ever registered** — the AssetType→workflow mapping is hardcoded inside the browser-driven `UFabBrowserApi`. | **No — see §4.2. Import must be reimplemented OR a small engine patch must register the built-in factories.** |
| **The download-URL/manifest call** | Made by the **fab.com JS frontend** in the CEF webview, *not* in C++. Given `artifactId` from the library feed, it mints the signed URL / BuildPatch `manifestURL,baseURL`. | **Gap — we replicate one more REST call** (`POST /e/artifacts/{id}/manifest`, see §5). |

**Net:** two REST calls (library list + artifact manifest) sit outside the exported surface and must be
reproduced by us; everything downstream (download engine, BuildPatch, Interchange import, plugin-dependency
scan) is reusable engine code. The auth **token** is the linchpin (§6).

### Endpoints (reverse-engineered, unofficial — treat as volatile)
Base URL is environment-dependent (`FabSettings.cpp`): Prod = `https://fab.com`.
- **Library:** `GET https://fab.com/e/accounts/{EpicAccountId}/ue/library?count={N}&cursor="{next}"`
  → `Authorization: Bearer {token}`, `accept: application/json`.
  Response: `results[]` (`title`, `assetId`, `assetNamespace`, `listingType`, `distributionMethod`,
  `source`, `url`, `images[]`, `projectVersions[]` → `{artifactId, engineVersions[], targetPlatforms[],
  buildVersions[]}`), plus `cursors.next`.
- **Manifest / download info:** `POST https://fab.com/e/artifacts/{artifactId}/manifest` (Bearer)
  → BuildPatch manifest URL + signed distribution point base URLs.

> These strings are **load-bearing and unofficial.** Isolate them as constants (§7) and verify against the
> community `egs-api-rs` `src/api/fab.rs` at maintenance time — Epic has already changed this API once.

---

## 3. Public API (Python / AICallable surface)

FabService is a static `UToolsetDefinition` class in `Source/VibeUE/Public/PythonAPI/UFabService.h`, matching
the house pattern (auto-registered by `Module.cpp` reflection; auto-exposed to Python as `unreal.FabService`;
methods return a JSON string; no registration code needed). Every method returns the standard envelope
(`{"success": true, ...}` / `{"success": false, "error_code": "...", "error": "..."}`).

```cpp
UCLASS(BlueprintType)
class VIBEUE_API UFabService : public UToolsetDefinition
{
    GENERATED_BODY()
public:
    // --- auth / status ---
    // Report whether an Epic session is available and the library is reachable. RUN FIRST.
    UFUNCTION(BlueprintCallable, meta=(AICallable), Category="VibeUE|Fab")
    static FString AuthStatus();

    // --- discovery ---
    // List owned Fab assets. Cached in-process; pass refresh=true to re-fetch. Optional local filters.
    UFUNCTION(BlueprintCallable, meta=(AICallable), Category="VibeUE|Fab")
    static FString ListLibrary(const FString& NameFilter = TEXT(""),
                               const FString& TypeFilter = TEXT(""),      // "pack"|"plugin"|"model"|"material"|"quixel"...
                               const FString& EngineVersion = TEXT(""),   // "" = current engine
                               int32 Limit = 200, int32 Offset = 0, bool bRefresh = false);

    // Full record for one owned asset (all projectVersions, images, engineVersions, distributionMethod).
    UFUNCTION(BlueprintCallable, meta=(AICallable), Category="VibeUE|Fab")
    static FString GetAsset(const FString& AssetId);

    // --- import ---
    // Download + import an owned asset into the project. Idempotent: skips if already imported (UFabLocalAssets).
    // EngineVersion/Quality/Format default to best match for the current engine.
    UFUNCTION(BlueprintCallable, meta=(AICallable), Category="VibeUE|Fab")
    static FString ImportAsset(const FString& AssetId,
                               const FString& EngineVersion = TEXT(""),
                               const FString& Quality = TEXT(""),   // "" | "Low" | "Medium" | "High" | "Raw"
                               const FString& Format  = TEXT(""));  // "" | "gltf" | "fbx"

    // Poll a running import (downloads are async; returns progress% + phase, or final asset paths).
    UFUNCTION(BlueprintCallable, meta=(AICallable), Category="VibeUE|Fab")
    static FString ImportStatus(const FString& AssetId);
};
```

**Design notes**
- `ListLibrary` fetches the full paginated feed once, caches it in a file-static (mirrors PerformanceService's
  session-state convention), and applies filters locally — cheap re-queries, no repeated network.
- Return a **compact projection** (id, title, type, source, compatible-engine flag, distributionMethod, thumbnail
  url), never the raw feed — the raw JSON is large (TerrainDataTools already codifies "summarize, don't dump").
  `GetAsset` returns the full record on demand.
- Import is **async** (downloads run on `FFabDownloadQueue`, 2 concurrent). `ImportAsset` kicks it off and returns
  `{"success": true, "status": "downloading", "asset_id": ...}`; the agent polls `ImportStatus`. **No blocking
  waits** (house rule). Completion resolves via the download/workflow delegates.
- Idempotency: check `UFabLocalAssets` (Fab's own install registry, `ImportLocation → AssetId`) and
  `EditorAssetLibrary.does_asset_exist` before downloading.

---

## 4. Architecture

```
Agent / Python
   │  unreal.FabService.*  (AICallable, JSON envelope)
   ▼
UFabService (Public/PythonAPI/UFabService.{h,cpp})     ← thin, static, house pattern
   ├── FFabAuthBridge      obtain Epic bearer token           ─┐
   ├── FFabLibraryClient   GET …/ue/library  (paged, cached)   │ new, small
   ├── FFabManifestClient  POST …/artifacts/{id}/manifest      ─┘  (sync HTTP like TerrainDataTools)
   └── FFabImportDriver    build FFabDownloadRequest + workflow ── drives ↓
                                                              Engine Fab plugin (FAB_API)
                                                              FFabDownloadRequest / EFabDownloadType
                                                              IFabWorkflow / FFabAssetMetadata
                                                              BuildPatchServices · Interchange · UFabLocalAssets
```

- **HTTP**: reuse the synchronous-with-timeout pattern from `Private/Tools/TerrainDataTools.cpp`
  (`FHttpModule::Get().GetHttpManager().Tick(0)` loop, 15–45 s) for the two REST calls. Library listing and
  manifest fetch are quick; the large chunked download is handled by the engine's own async `FabDownloader`.
- **Build.cs**: add `Fab` (and, if we self-auth, `EOSSDK` + `EOSShared`) to `PrivateDependencyModuleNames`, and
  `Fab` to the `.uplugin` plugin dependencies. `HTTP`/`Json` are already deps.
- **No new module, no MCP registration, no toolset registration** — reflection in `Module.cpp` picks up any
  `UToolsetDefinition` with an AICallable method automatically.

### 4.1 The auth-token gap — decision point
`FabAuthentication`'s token accessors are Private to the Fab module (not `FAB_API`). Three ways to get a bearer
token, in order of preference:

**Option A — Reuse the Fab plugin's live EOS session (recommended).**
The editor, when launched from the Epic Games Launcher, is already logged in and the Fab plugin already holds a
valid EOS token. We need to read it. Sub-options:
- **A1 (cleanest, needs a tiny engine touch):** submit a 1-line PR to expose `FabAuthentication::GetAuthToken()`
  as a `FAB_API` free function (or a `BlueprintCallable` on a minimal facade). Low risk, upstreamable, removes all
  fragility. **Preferred if we're willing to carry/submit an engine patch.**
- **A2 (no engine change):** stand up our **own** EOS platform from our module using `EOSSDK`/`EOSShared`,
  loading the same creds by reading `/Fab/Data/FabEos.FabEos` reflectively (the `UEosConstants` UClass is Private,
  but its properties are readable via `FProperty` reflection without the header), then `EOS_Auth_Login` with
  `PersistentAuth`. Fully self-contained, no engine edit, but duplicates the EOS platform and depends on the
  cooked creds asset staying put.

**Option B — Independent launcher OAuth (`egs-api` protocol).**
Do the launcher authorization-code exchange ourselves (well-known `launcherAppClient2` client id) → `eg1` bearer,
which the same `/e/…` endpoints accept. Works even if the Fab plugin is disabled, and is the community-proven path
(`egs-api-rs`/`egs-api-py`). Downside: a **separate** one-time sign-in (paste an auth code), and it's the more
overtly "gray" route (§8). Good **fallback** when no EOS session exists (e.g. Git/source editor never launched
from the launcher).

**Recommendation:** ship **A2** (self-contained EOS `PersistentAuth`, zero engine edits) as the default, keep the
door open to **A1** (upstream the accessor) if we prefer riding engine-supported code, and offer **B** as an
opt-in fallback for non-launcher editors. `AuthStatus()` reports which path is active and what to do if none is.
**Built (Phase 1): A2** — `FabAuthBridge` stands up our own EOS platform with Fab's creds (read reflectively from
`/Fab/Data/FabEos.FabEos`), logs in via `PersistentAuth` with an exchange-code fallback, and copies the token.

### 4.2 The import-reuse gap — decision point (Phase 2)
Reading the shipped Fab plugin closely (post-spec) showed the **import** half is *not* cleanly reusable, unlike
the download half. Two viable routes, to be chosen before building Phase 2 import:

- **Route P (engine patch, ~20 lines, recommended for full fidelity):** in `FFabModule::StartupModule`, register
  the built-in workflows as `IFabWorkflowFactory`s (or move their headers to `Public/` + `FAB_API`). Then our
  module does `FFabWorkflowFactoryRegistry::GetFactory(assetType)->Create(metadata, downloadUrl)->Execute()` and
  gets Epic's *exact* `/Game` placement, plugin-dependency prompts, and Interchange import for free. Cost: we carry
  (and ideally upstream) an engine patch.
- **Route R (reimplement in VibeUE, no engine touch):** reuse only the `FAB_API` **download** path
  (`FFabDownloadRequest` + `AddDownloadToQueue`) and hand-roll the post-download import per type. Packs are easy
  (BuildPatch already lands `.uasset`s under ProjectDir → asset-registry scan + Content Browser sync via public
  APIs); glTF/FBX need our own Interchange import; Quixel/Megascans is the most work. More code, more surface to
  keep working, but self-contained.

Both routes still need the **signed-manifest negotiation** (§2's second gap) reimplemented — the highest-risk
unverified piece, and the reason Phase 1 (which doesn't need it) validates the premise first.

---

## 5. Import flow (per asset) — as built (Route R)

1. `ImportAsset(AssetId)` resolves the `projectVersions[]` entry for the target engine → its `artifactId`
   (`FFabLibraryAsset::ArtifactIdForEngine`). Compatibility guard first: no matching version →
   `ENGINE_VERSION_MISMATCH` listing the available versions.
2. `FVibeFabManifest::Fetch` → `POST /e/artifacts/{artifactId}/manifest` with
   `{namespace, itemId=assetId, platform:"Windows"}` → `FFabDownloadInfo` (signed manifest URL +
   `distributionPointBaseUrls`). Non-BuildPatch (`type != "manifest"`) formats → `DOWNLOAD_INFO_FAILED`
   (glTF/FBX/Quixel not handled yet).
3. `FVibeFabImport::Start` snapshots the set of `.uasset`/`.umap` files under the project Content dir,
   then constructs `FFabDownloadRequest(assetId, "manifestUrl,baseUrl1,baseUrl2,…", location, type)` —
   `BuildPatchRequest` + ProjectDir for packs, `BuildPatchInstallRequest` + ProjectPluginsDir for plugins —
   binds the progress/complete delegates, and `ExecuteRequest()` (async). `ImportAsset` returns
   `status:"downloading"` immediately.
4. BuildPatch downloads + installs non-destructively on the editor's background ticks. `ImportStatus`
   reports `percent`/bytes from the progress delegate.
5. On completion, `FinishImport` re-walks the Content dir, diffs against the pre-snapshot to find the
   installed files (BuildPatch does **not** report them), converts them to `/Game/…` packages, and
   `ScanFilesSynchronous` makes them visible. `ImportStatus` then returns `status:"imported"` with
   `asset_paths[]` + `install_root`.
6. Re-importing an already-imported asset is idempotent (`already_imported` / no new files).

---

## 6. Risks & mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| Unofficial endpoints change (Epic already changed the Fab API once) | High | Constants isolated in one header (§7); `AuthStatus`/`ListLibrary` fail loud with `error_code`; document the `egs-api-rs/fab.rs` cross-check; pin behind a feature flag so a break can't regress the rest of VibeUE. |
| Cloudflare / bot-challenge on fab.com to headless clients (403) | Med | Send realistic headers/UA; the authenticated `/e/…` calls with a valid EOS token are the ones community tools reach successfully (captcha mostly hits browser-session `/i/…` variants). Surface a clear error if challenged. |
| Auth-token access (Private in engine) | Med | §4.1 — A2 self-EOS default; A1 upstream accessor; B launcher-OAuth fallback. |
| Fab plugin disabled / editor not launched from launcher (no EOS session) | Med | Detect in `AuthStatus`; fall back to Option B (explicit one-time login) with a clear instruction, or report the requirement. |
| Download flakiness (Fab has FAB001/IS-0009 errors even in the official UI) | Low-Med | Reuse the engine downloader's retry/cache (`FFabAssetsCache`, `%TEMP%/FabLibrary`); expose failures via `ImportStatus`, don't hang. |
| Headless commandlet mode | Low | The stock Fab plugin disables itself under `IsRunningCommandlet()`; FabService is an interactive-editor feature. If headless import is ever needed, Option A2 + our own downloader driving is required (document as non-goal for now). |

---

## 7. File plan

```
Source/VibeUE/Public/PythonAPI/UFabService.h          # UToolsetDefinition, 5 AICallable methods
Source/VibeUE/Private/PythonAPI/UFabService.cpp        # orchestration + OkJson/ErrJson helpers
Source/VibeUE/Private/PythonAPI/Fab/FabEndpoints.h      # ALL unofficial URL/const strings, one place
Source/VibeUE/Private/PythonAPI/Fab/FabAuthBridge.{h,cpp}   # token acquisition (A2 default; B fallback)
Source/VibeUE/Private/PythonAPI/Fab/FabLibraryClient.{h,cpp}# paged GET …/ue/library, in-process cache
Source/VibeUE/Private/PythonAPI/Fab/FabManifestClient.{h,cpp}# POST …/artifacts/{id}/manifest
Source/VibeUE/Private/PythonAPI/Fab/FabImportDriver.{h,cpp} # FFabDownloadRequest + workflow glue
Source/VibeUE/Private/PythonAPI/FabServiceTests.cpp    # WITH_AUTOMATION_TESTS: JSON contract + filter logic
Content/Skills/fab/SKILL.md                            # agent-facing usage skill (+ sub-docs as needed)
test_prompts/fab/fab_tests.md                          # NL prompt pack (discover → import → verify)
docs/design/fab-service-spec.md                        # this doc
```
`VibeUE.Build.cs`: `+Fab` (private) and, for A2, `+EOSSDK +EOSShared`. `VibeUE.uplugin`: add `Fab` to `Plugins`.

Pure logic (library filtering, engine-version matching, download-type selection) factors into a header so it's
testable headless — mirroring `PerformanceVerdict.h` behind `PerformanceServiceTests.cpp`.

---

## 8. Legal / ToS posture (must be explicit in UX)

- **License to *use* owned assets in your own project is clear.** Both Fab tiers (Personal / Professional) grant
  the right to use, modify, and distribute assets **embedded in your own projects**. Downloading what you own,
  for your own project, is within the grant.
- **Red line:** re-distributing the raw standalone asset files outside your project. FabService imports into the
  project only — it must never expose a "save the raw pack elsewhere" path.
- **Access method is a gray area.** There is **no official Fab API**; these endpoints are reverse-engineered.
  Epic's ToS anti-automation language sits in the anti-cheat/gameplay-integrity context, not a general
  anti-scraping clause; community tools (legendary since 2020, Epic-Asset-Manager) have operated for years with
  no reported bans, and Epic responded to mass library automation (Dec 2024 Megascans) by shipping an official
  tool, not sanctions. **Tolerated, unsupported, not guaranteed.**
- **Requirement:** FabService authenticates only with the **user's own** Epic account and acts only on the
  **user's own** library. `AuthStatus`/the skill doc must disclose that this uses Epic's private, unofficial API
  at the user's discretion — mirroring how legendary/EAM present themselves. No credentials are ever stored by
  VibeUE beyond what the EOS/launcher session already holds.

---

## 9. Phasing

- **Phase 1 — Discover (low risk):** `AuthStatus` + `ListLibrary` + `GetAsset`. Depends only on the token (§4.1)
  and the one library GET. Ships value immediately (agent can answer "what do I own that fits this engine?") and
  de-risks the auth decision before we build download.
- **Phase 2 — Import:** `ImportAsset` + `ImportStatus` via the manifest call + engine downloader/workflows.
- **Phase 3 — Polish:** thumbnail passthrough, richer filters (target platform, source), optional catalog search
  of *owned-adjacent* free items (still no purchase), Insights-style progress events.

---

## 10. Open decisions (for review)

1. **Auth path (§4.1):** ship A2 (self-contained EOS `PersistentAuth`) as default? Or invest in A1 (upstream a
   `FAB_API` token accessor to the engine Fab plugin) for a cleaner, engine-blessed dependency?
2. **Fallback login (Option B):** include the launcher-OAuth fallback in v1, or defer until a non-launcher user
   actually needs it?
3. **Service vs dynamic tool:** confirm the `UToolsetDefinition` service style (Python-visible, per-method tools,
   the editor-domain convention) over the `REGISTER_VIBEUE_TOOL` dynamic-tool style used by `terrain_data`.
   Recommendation: **service style** — this is editor-domain and benefits from Python visibility + discovery.
4. **Scope of "discover":** owned library only (as #517 says), or also surface *free* catalog listings the user
   doesn't yet own for one-tap import? The latter needs the "add-to-library" claim call — **out of scope per
   #517's "no purchase functionality,"** flagged here only to confirm we're deliberately excluding it.
