# deep_research

Web research and GPS geocoding — no API key required.

## Actions

### search
Look up any topic using DuckDuckGo. Returns an abstract, a direct answer if available, and a list of related topic URLs.

**Workflow for deep research:**
1. Call `search` with your topic → get abstract + relevant URLs
2. Call `fetch_page` on the most relevant URL → get full page content as markdown
3. Repeat `fetch_page` on additional URLs as needed

```
action: search
query: Unreal Engine Landscape layer blend material
```

---

### fetch_page
Fetch the full content of any public URL as clean markdown. Uses Jina AI Reader (free). Ideal for reading Unreal Engine documentation, Dev Community posts, API references, blog posts, etc.

```
action: fetch_page
url: https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine
```

---

### geocode
Convert any place name or address to GPS coordinates (lat, lng). Uses OpenStreetMap Nominatim. The returned coordinates can be passed directly to the `terrain_data` tool.

```
action: geocode
query: Mount Fuji
```

Returns:
```json
{
  "success": true,
  "lat": 35.3606,
  "lng": 138.7274,
  "display_name": "富士山, ..., Japan",
  "results": [...]
}
```

---

### reverse_geocode
Convert GPS coordinates to a human-readable place name.

```
action: reverse_geocode
lat: 35.3606
lng: 138.7274
```

---

## Common Workflows

### Terrain generation from a place name
```
1. deep_research(action=geocode, query="Grand Canyon South Rim")
   → lat=36.0544, lng=-112.1401

2. terrain_data(action=preview_elevation, lat=36.0544, lng=-112.1401)
   → suggested base_level and height_scale

3. terrain_data(action=generate_heightmap, lat=36.0544, lng=-112.1401, ...)
```

### Research Unreal Engine topic
```
1. deep_research(action=search, query="Unreal Engine 5 PCG framework documentation")
   → abstract + URLs

2. deep_research(action=fetch_page, url=<best URL from step 1>)
   → full documentation content

3. AI synthesizes answer from content
```

## Notes
- All services are free and require no API keys
- Nominatim has a 1 req/sec rate limit — avoid rapid repeated calls
- `fetch_page` uses Jina AI Reader (r.jina.ai) and works on most public websites
- DuckDuckGo Instant Answer API works best for well-known topics; for niche queries use `fetch_page` with a direct docs URL
