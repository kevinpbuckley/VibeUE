---
name: fab
display_name: Fab Asset Import
description: Search Fab's public free catalog (including Quixel Megascans) and directly import eligible assets without adding them to the account library, or discover/import the signed-in Epic account's owned Fab library. Use for free Fab/Quixel searches, owned-library questions, compatibility checks, and Fab imports. No purchasing or entitlement claiming.
vibeue_classes:
  - FabService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - fab
  - fab.com
  - marketplace
  - free assets
  - quixel
  - megascans
  - owned assets
  - import asset
  - download asset
  - engine version compatibility
---

# Fab asset import

`FabService` supports two deliberately separate paths:

- **Public free catalog:** search exact zero-price listings and directly import eligible glTF/GLB
  downloads. This does not sign in, purchase, claim an entitlement, or add the listing to My Library.
- **Owned library:** use the editor/launcher Epic session to list and import assets already owned by the
  account, including migrated Unreal Marketplace items.

## Public free flow (Quixel/Megascans and other sellers)

1. Call `search_free_catalog(query="", seller_filter="Quixel Megascans", format_filter="gltf")`.
   Results are compact and include `id`, `title`, `seller`, `price`, formats, thumbnail, and an opaque
   `next_cursor`. Results require an exact-zero **Personal** tier; UEFN-reference-only offers are
   excluded. No authentication is required.
2. Show the chosen result and the Fab EULA to the user. **Never set `accept_eula=true` on the user's
   behalf.** It must represent their explicit acceptance.
3. Call `import_free_asset(listing_id, quality="High", format="gltf", license_slug="personal",
   accept_eula=true)`. The service re-fetches the listing and requires the selected license price to
   still be exactly zero before it requests a signed download.
4. Poll `import_status(listing_id)` until `imported` or `failed`, then verify a returned asset path with
   `unreal.EditorAssetLibrary.does_asset_exist(path)`.

Direct free imports land under `/Game/Fab/Free/<Title>`. The original archive is cached under the
project's `Saved/Fab/Free` directory; no raw-file export API is exposed. ZIP paths are validated before
extraction. The first version supports Fab glTF/GLB ZIPs through automated Unreal Interchange import.
That is suitable for Quixel 3D assets, but it does not reproduce every private Megascans material setup
or support every Fab product format yet.

## Owned-library flow

1. Call `auth_status()` first. If it is not authenticated, tell the user to open the Fab window or
   launch the editor from the Epic Games Launcher.
2. Call `list_library(...)`; use `refresh=true` only when the account library may have changed.
3. Call `get_asset(asset_id)` to inspect versions and compatibility.
4. Call `import_asset(asset_id, ...)`, then poll `import_status(asset_id)`.
5. Verify the returned `/Game/...` paths before reporting success.

Owned BuildPatch packs and plugins are supported. Sparse or non-BuildPatch owned assets may still be
discoverable without being importable through `import_asset`; use the public-free path only when the
listing is currently free and provides a supported source format.

## Rules

- Never purchase, claim, call add-to-library, or accept a license on the user's behalf.
- Never treat a search filter as proof of price. `import_free_asset` performs the authoritative
  zero-price check immediately before download.
- Never expose signed download URLs or offer to save distributable raw asset files elsewhere.
- Imports are asynchronous and idempotent within the editor session; poll with a finite interval.
- These Fab web endpoints are unofficial and may change. Surface their errors rather than retrying the
  same request blindly.

Every method returns a JSON string with `success: true|false`. Errors include `error_code` and `error`.
Discover exact signatures with `discover_python_class('unreal.FabService')`. Current methods:
`auth_status`, `list_library`, `get_asset`, `search_free_catalog`, `import_asset`, `import_free_asset`,
and `import_status`.
