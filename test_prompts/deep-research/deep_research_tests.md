# Deep Research Tool Tests

Tests for the `deep_research` tool — web research, page fetching, and GPS geocoding. No API key required. Run from any level.

---

## Discovery

What actions does the deep_research tool support?

---

Show me the help documentation for the deep_research tool.

---

## Search — Basic

Search for "Unreal Engine Landscape".

---

Search for "Unreal Engine PCG procedural content generation".

---

Search for "UE5 Nanite virtualized geometry".

---

## Search — Unreal Engine Documentation Topics

Search for "Unreal Engine material layer blend".

---

Search for "Unreal Engine Blueprint communication event dispatcher".

---

Search for "Unreal Engine Niagara particle system overview".

---

Search for "Unreal Engine Animation Blueprint state machine".

---

## Search — Returns URLs

Search for "Unreal Engine Landscape painting layers". Report the abstract URL and list all related topic URLs returned.

---

Search for "Epic Games Dev Community landscape tutorial". What URLs are in the related topics?

---

## Fetch Page — Unreal Engine Documentation

Fetch the page at https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine and summarize what it says about layer blending.

---

Fetch https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine and report the key technical constraints (component count, quads per section, etc.).

---

Fetch https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview and give me a short summary of what PCG can do.

---

## Search → Fetch Page Workflow

Search for "Unreal Engine Runtime Virtual Textures landscape". Pick the most relevant URL from the results and fetch that page. Summarize what you find.

---

Search for "Unreal Engine foliage painting tool". Fetch the most relevant page from the results and list the key steps for painting foliage.

---

## Geocode — Landmarks

Get the GPS coordinates for Mount Fuji.

---

Get the GPS coordinates for the Grand Canyon South Rim.

---

Get the GPS coordinates for Zermatt, Switzerland.

---

Get the GPS coordinates for Tokyo, Japan.

---

Get the GPS coordinates for San Francisco, California.

---

## Geocode — Multiple Results

Geocode "Springfield". How many results come back and what states/countries are they in?

---

Geocode "Richmond". List all results returned.

---

## Geocode — Specific Addresses

Get the GPS coordinates for "Yosemite Valley, California".

---

Get the GPS coordinates for "Sahara Desert".

---

Get the GPS coordinates for "Dolomites, Italy".

---

## Geocode → Terrain Workflow

Geocode "Aoraki Mount Cook, New Zealand", then use those coordinates to preview the elevation with terrain_data.

---

Geocode "Patagonia, Argentina", then generate a heightmap for that location using the preview's suggested settings.

---

## Reverse Geocode

What place is at latitude 35.3606, longitude 138.7274? (That's where we got a heightmap earlier.)

---

What place is at latitude 36.0544, longitude -112.1401?

---

What place is at latitude 46.0207, longitude 7.7480?

---

What place is at latitude -43.5950, longitude 170.1418?

---

## Error Handling

Call the search action without providing a query.

---

Call the fetch_page action without providing a url.

---

Call the geocode action without providing a query.

---

Call the reverse_geocode action without lat or lng.

---

Call deep_research with an unknown action name like "lookup".

---

## Fetch Page — Edge Cases

Fetch a URL that doesn't exist: https://dev.epicgames.com/documentation/this-page-does-not-exist-xyz123

---

Fetch a Wikipedia page: https://en.wikipedia.org/wiki/Unreal_Engine and give me the key facts from the introduction.

---

## Integration — Full Research + Terrain Build

*Chain the research and terrain tools together for a complete workflow.*

---

### Step 1 — Delete Existing Landscapes

Load the `landscape` skill. List all landscapes in the level and delete every one of them so we start with a clean slate.

---

### Step 2 — Research the Location

I want to build a landscape of the Swiss Alps near Grindelwald. First, search for "Grindelwald Switzerland" to confirm it's a real place.

---

### Step 3 — Geocode

Get the GPS coordinates for Grindelwald, Switzerland.

---

### Step 4 — Preview Elevation

Use the coordinates from step 3 to preview the elevation around Grindelwald.

---

### Step 5 — Research the Terrain Type

Search for "Swiss Alps terrain characteristics elevation range". Fetch the most relevant page and summarize the key landscape features (peaks, valleys, glaciers).

---

### Step 6 — Generate Heightmap

Generate the heightmap using the suggested settings from step 4. Save as PNG.

---

### Step 7 — Research UE Landscape Settings

Fetch https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine and tell me what ComponentCount and QuadsPerSection I should use for a 17km map.

---

### Step 8 — Import and Build

Import the heightmap as a new landscape called "GrindelwaldTerrain" using the settings recommended from step 7.

---
