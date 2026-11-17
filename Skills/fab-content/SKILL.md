---
name: fab-content
display_name: Fab Content (find, see, install)
description: Find Fab content the user owns or that is free, SEE what it looks like before and after installing it, install it, and check what an installed pack holds (FabPreviewService on top of the engine's FabToolset). Use when the user asks for assets from Fab or "my library", wants to pick between Fab listings or packs by how they look, asks what a pack contains, or before building or generating an asset that might already exist on Fab.
keywords:
  - fab
  - marketplace
  - my library
  - owned assets
  - asset pack
  - install from fab
  - thumbnail
  - contact sheet
  - what does it look like
vibeue_classes:
  - FabPreviewService
---

# Fab content: find, see, install

The engine (UE 5.9, plugin `Fab`) owns the Fab account work in `FabToolset.FabToolset`:

| Tool | Does |
|---|---|
| `SearchFabMyLibrary` | What the signed-in account owns: `listingId`, `title`, `seller`, `sellerId`, `installable`, `installOptions`, `thumbnailUrl`, `nextCursor` |
| `SearchFabListings` | The public catalog (`bFreeOnly` for free ones) |
| `GetFabListingDetails` | Description, licence, `fabUrl` of a LIVE listing |
| `InstallFabListing` | Starts the download of one install option (fire-and-forget) |
| `FabSignIn` | Opens the browser sign-in (only when a search says nobody is signed in) |

VibeUE adds what those cannot do, `VibeUE.FabPreviewService`, with results that arrive as **real images**
(an MCP image block; in Python a `ToolsetImage` with base64 `data`, plus `save_image_path` to write a file):

| Tool | Does |
|---|---|
| `ListingImageSheet(imageUrls, labels, cellSize=256, columns=0, saveImagePath="")` | Downloads up to 48 listing pictures (the `thumbnailUrl` of search results) and tiles them into ONE numbered contact sheet. Each cell reports the picture's real size; `bLowDetail` (orange number) = under 256 px, too small to judge by |
| `PackSheet(contentPath, classNames="StaticMesh,SkeletalMesh", maxAssets=30, cellSize=256, columns=0, bRenderMissing=true, saveImagePath="")` | After an install: the assets under a folder as one numbered sheet (saved thumbnails, rendering the ones without a usable one), the folder's asset count by class and its bytes on disk |
| `RefreshThumbnails(contentPath, classNames="StaticMesh,SkeletalMesh", bOnlyBlank=true, bSave=true)` | Re-renders (after finishing the asset's builds) and saves the saved thumbnails under a folder that are missing or blank, so the Content Browser and later sheets show the asset |

For one asset at full size use `EditorToolset.EditorAppToolset.CaptureAssetImage`; for saved thumbnails one
image per asset, `GetAssetThumbnails`.

## Workflow

```python
import json
lib = json.loads(await call_tool("FabToolset.FabToolset.SearchFabMyLibrary", keywords="military vehicle", count=24))
rows = lib["results"]
sheet = await call_tool("VibeUE.FabPreviewService.ListingImageSheet",
                        imageUrls=[r["thumbnailUrl"] for r in rows], labels=[r["title"] for r in rows])
{"summary": sheet["summary"], "cells": [(c["index"], c["label"], c["bLowDetail"]) for c in sheet["cells"]]}
```

Over MCP call it directly to get the picture in front of you:
`call_tool(toolset_name="VibeUE.FabPreviewService", tool_name="ListingImageSheet", arguments={"imageUrls": [...], "labels": [...]})`.
From a script the image is base64 in the result (`sheet["sheet"]["data"]`): pass `saveImagePath` and open the
file with your image viewer / file-reading tool. Then install the pick:

```python
opt = row["installOptions"][0]   # prefer assetFormat "unreal-engine"
await call_tool("FabToolset.FabToolset.InstallFabListing", listingId=row["listingId"], title=row["title"], **opt)
```

and once it has landed (see below) look at what came in:

```python
s = unreal.FabPreviewService.pack_sheet("/Game/<PackRoot>/<Pack>", "StaticMesh,SkeletalMesh", 30, 256, 0, True,
                                        r"<abs path>/pack.jpg")
(s.summary, [(c.index, c.label, c.asset_class, c.image_origin) for c in s.cells])
```

## Facts (measured on UE 5.9.0, 2026-11-17)

- **Delisted listings still install.** A pack whose seller left Fab stays in the owner's library as
  `installable: true` with its install options, and `InstallFabListing` downloads it. `GetFabListingDetails`
  answers "no such listing" for it, and its `thumbnailUrl` is a blurred ~143x78 placeholder of 1.7-2.5 KB:
  `ListingImageSheet` flags those `bLowDetail`. Install and judge it with `PackSheet` instead.
- **Filter by seller with the id.** `SearchFabMyLibrary(keywords="<seller name>")` once, take every
  `sellerId` of that seller, then page with `sellers=<id>`, `sortBy="title"`, `count=24` and `cursor` until
  `nextCursor` is absent. There is no total count.
- **Install is fire-and-forget**: `status` is `handed-to-editor`, never "installed". An asset pack downloads
  straight into the project's `Content/<PackRoot>/` and is done when the log has
  `LogFab: Display: Scanning <N> assets for plugin dependencies...` followed by
  `LogFab: Display: Fab workflow completed for asset <listingId>` (35-100 s per pack of a few hundred MB here).
  Install one pack at a time: packs of one seller share folders (`<PackRoot>/Shared`) and later packs rewrite
  them. Do not load or open a pack's assets while another install of the same seller runs.
- **Demo content is most of the bytes.** A typical vehicle pack was 597 MB on disk, 340 MB of it the demo
  level's lighting build data; the meshes, materials and textures were about 255 MB. Measure with `PackSheet`'s
  `bytesOnDisk` per folder before deciding what to keep.
- **Packs can write outside `Content/`.** Eleven 4.27/5.0 vehicle packs each dropped a 192x192 `<PackFolder>.png`
  into the PROJECT ROOT. After an install run `git status` on the whole project, not only `Content/`, and remove
  what the pack left there (a file of the same name may already be tracked: check before deleting).
- **Delete an unwanted pack root from disk, not asset by asset.** `AssetDiscoveryService.delete_asset_unattended`
  took about 2.5 s per asset (403 assets would be 17 min) and refuses inside an editor transaction: a script with no
  top-level `await` runs inside one, so `await asyncio.sleep(0)` first. Faster and safe: check with the registry that
  nothing outside the root refers to it, relaunch the editor (nothing from it is loaded then), delete the folder on
  disk, relaunch again so the asset registry forgets it.
- **A re-saved mesh can keep a blank thumbnail.** The editor renders the thumbnail at save time without waiting
  for the asset to finish building, so meshes saved right after a move plus a collision or Nanite change got a
  dark empty-scene checker as their saved thumbnail (13 of 25 in one kit). `PackSheet` does not trust a saved
  thumbnail that holds no picture (no opaque pixel, or near-uniform): it renders that asset, notes it in the cell's
  `error` and the summary, and with `bRenderMissing=false` leaves the cell empty. Fix the packages with
  `RefreshThumbnails(folder)` after any batch edit, before committing. A cell's `imageOrigin` alone does not prove a
  picture: look at the sheet.
- Never press a "Missing plugins: Enable and restart" notification for a pack from an unattended agent; record
  it instead.

## Rules

- Before generating or building an asset, look for one: the project, then the owner's library, then free
  listings. Install only from those two searches; never buy.
- Look at the picture before choosing. A keyword match is not a fit.
- After an install, apply the project's content layout (move what is used, delete the rest) through
  `AssetTools.rename_assets` - never by moving files on disk.
