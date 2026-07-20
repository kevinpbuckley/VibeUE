# Fab Library Import Tests

Tests for discovering the signed-in Epic account's OWNED Fab library and importing assets into the
project (FabService). Run sequentially. Every FabService method returns a JSON string — parse it and
assert on fields; print the evidence behind each pass/fail.

These require an editor signed into an Epic account (the Fab window logged in, or the editor launched
from the Epic Games Launcher). If auth is unavailable, the auth test should fail *cleanly* with a clear
error_code — that is itself a valid, expected result to report, not a crash.

**No purchasing is tested or supported — import-only.**

---

## Auth — run first

Check Fab authentication status. Report whether an Epic session is available and the library endpoint is
reachable. (Expected: JSON with success and an auth block — e.g. authenticated=true plus an epic_account
id / display hint when signed in; or success=true with authenticated=false and a clear next-step message,
or error_code=NOT_AUTHENTICATED — any of these is a valid outcome to report. Must NOT hang or crash.)

---

If auth reported not-authenticated, STOP the import tests and report that the editor needs a Fab/Epic
sign-in. The discovery tests below assume auth succeeded.

---

## Discover — list library

List the owned Fab library. Report the number of assets and, for the first few, their id, title, type,
source, and whether they're compatible with the current engine. (Expected: JSON with success=true and a
compact results array — each item has an id, title, type, distribution_method, and a compatible flag;
NOT the raw fab.com feed. An empty library is a valid result if the account owns nothing.)

---

Re-list the library filtered to a name substring you saw in the previous result. (Expected: results are a
subset filtered locally by name_filter; the same cached data is reused — no error, fast return.)

---

List the library filtered to a single type (e.g. type_filter for packs, or quixel/megascans). (Expected:
every returned item matches the requested type; if none match, an empty results array with success=true.)

---

## Discover — get one asset

Take the first asset id from the list and fetch its full record. Report its projectVersions, the
engineVersions each supports, its distributionMethod, and its image thumbnails. (Expected: JSON with
success=true and the full record — projectVersions[] with artifactId + engineVersions[], distributionMethod,
images[]. Richer than the list projection.)

---

## Compatibility guard

Pick an owned asset (from get_asset) that has NO projectVersions entry for the current engine, if one
exists, and attempt import_asset on it. (Expected: success=false, error_code=ENGINE_VERSION_MISMATCH, and
the error lists the engine versions that ARE available. If every owned asset is compatible, state that and
skip — do not force a mismatched import.)

---

## Import — download + verify (async)

Import a small owned asset that IS compatible with the current engine. (Expected: import_asset returns
quickly with success=true and status=downloading/queued plus the asset_id — it must NOT block.)

---

Poll import status for that asset until it resolves. (Expected: import_status returns status transitioning
to imported with one or more created /Game/... asset paths, or a clean failed with error_code — never a
hang. Poll a bounded number of times, not indefinitely.)

---

Verify the import landed: confirm one of the reported /Game/... paths exists via
unreal.EditorAssetLibrary.does_asset_exist. (Expected: the path exists in the project. Report the concrete
path as evidence.)

---

## Idempotency

Import the SAME asset again. (Expected: success=true but it is recognised as already imported — reports the
existing path and does NOT re-download or duplicate the asset.)
