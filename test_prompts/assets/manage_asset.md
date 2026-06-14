# Asset Management Tests

These tests verify searching, duplicating, opening, saving, and deleting assets. Run sequentially. If you try to create an asset and one already exists, delete the asset and try again.

---

## Setup Assets

Create a basic actor blueprint for testing asset operations. Put it in the Blueprints folder and call it AssetSearchTest.

---

Also make a widget blueprint in the same location called AssetWidgetTest.

---

## Searching for Assets

I'm looking for any widgets that have "radar" in the name.

---

Are there any textures with "background" in the name?

---

Show me all the blueprints in the project.

---

What widgets exist in the UI folder?

---

Let me see some engine content too - search with engine assets included.

---

Just show me the first 10 results for textures.

---

## Duplicating Assets

I want to make a copy of the AssetSearchTest blueprint. Name it AssetSearchTestBackup.

---

Copy the widget test too. Call the duplicate AssetWidgetTestBackup.

---

Search for "Backup" to make sure those copies exist.

---

## Opening and Saving

Open the AssetSearchTest blueprint in the editor.

---

Open the widget test too.

---

Save any unsaved assets without asking me.

---

## Asset References

What references the movement input action? Show me where IA_Move is used.

---

Does anything reference the AssetSearchTest blueprint?

---

## Importing an Image From Disk

Find an existing image file on disk to import — e.g. one of the .png files in the project's
Saved/Screenshots folder. (If none exist, take an editor screenshot to disk first.)

---

Import that image file into /Game/ImportTest as a texture called T_ImportSmoke. Use the asset
manager's import — do NOT use Python import_asset_tasks (that crashes the editor).

---

Confirm the texture asset now exists in the Content Browser at /Game/ImportTest/T_ImportSmoke.

---

Try to import a file that does not exist on disk. It should fail gracefully with a clear error,
not crash.

---

Try to import a .txt (non-image) file. It should fail gracefully and report that the file type is
unsupported.

---

## Cleanup

Delete the test assets created above (AssetSearchTest, AssetWidgetTest, their Backups, and
T_ImportSmoke).

---


