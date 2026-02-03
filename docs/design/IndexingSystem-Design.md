# VibeUE Indexing System Design

## Goals
- Provide fast, accurate retrieval of existing Unreal assets and metadata to improve AI generation and modification tasks.
- Enable domain‑specific reasoning (Niagara, Animation, Blueprints, Materials, UMG, etc.) via structured summaries.
- Mirror successful Copilot patterns: incremental indexing, selective context retrieval, and small, relevant payloads to the LLM.
- Keep indexing opt‑in, incremental, and lightweight while supporting deeper “on‑demand” extraction.

## Non‑Goals
- Full binary asset indexing (raw curve data, full anim keyframes, full Niagara VM graphs) as default.
- Replacing editor/runtime evaluation for transforms or simulation output.

---

## 1) High‑Level Architecture

### 1.1 Components
1. **Index Orchestrator**
   - Owns indexing lifecycle and scheduling.
   - Tracks asset changes and triggers partial re‑index.
   - Exposes APIs to AI and MCP tools.

2. **Asset Discovery Layer**
   - Uses Asset Registry for asset enumeration and change events.
   - Filters by asset class and project settings.

3. **Domain Extractors**
   - Per‑asset‑type extractors that generate **lightweight summaries**.
   - Examples: AnimationSummaryExtractor, NiagaraSummaryExtractor, BlueprintSummaryExtractor, SkeletonSummaryExtractor.

4. **Index Stores**
   - **Metadata Store**: JSON/SQLite for structured data (fast filter/search).
   - **Embedding Store**: optional vector index (FAISS / SQLite+HNSW / external service).

5. **Retrieval Layer**
   - Hybrid search: metadata + semantic embeddings.
   - Reranking by relevance to user intent and usage patterns.

6. **AI Context Builder**
   - Selects small, relevant summaries.
   - Produces prompt‑ready snippets and references.

---

## 2) Index Data Model

### 2.1 Base Asset Record
```
AssetRecord
- assetId: string (soft object path)
- assetName: string
- assetClass: string
- packagePath: string
- tags: string[]
- dependencies: string[] (soft references)
- lastModified: timestamp
- sourceHash: string (optional)
```

### 2.2 Domain Summaries (Examples)

#### SkeletonSummary
```
SkeletonSummary
- skeletonId: string
- boneHierarchy: [{ name, parent }]
- sockets: [{ name, bone, relativeTransform? }]
- virtualBones: [{ name, source, target }]
- retargetSources: string[]
- previewMesh: string
```

#### AnimationSummary
```
AnimationSummary
- animationId: string
- skeletonId: string
- lengthSeconds: float
- frameRate: float
- rootMotion: { enabled, rootBone }
- curveNames: string[]
- notifyNames: string[]
- keyPoseTags: string[] (optional, curated)
```

#### NiagaraSummary
```
NiagaraSummary
- systemId: string
- emitters: [{ name, rendererTypes, moduleNames }]
- parameterDefaults: [{ name, type, value }]
- effectsTags: string[] (optional)
```

#### BlueprintSummary
```
BlueprintSummary
- blueprintId: string
- parentClass: string
- components: [{ name, class }]
- functions: string[]
- variables: [{ name, type }]
- interfaces: string[]
```

---

## 3) Indexing Workflow

### 3.1 Initial Full Index
- Enumerate asset registry for supported classes.
- Run extractor for each asset type.
- Store Base Asset Record + Domain Summary.

### 3.2 Incremental Updates
- Use Asset Registry events: OnAssetAdded, OnAssetRemoved, OnAssetUpdated.
- Re‑index only affected assets.
- Maintain an **IndexVersion** and **AssetVersion** to detect stale records.

### 3.3 On‑Demand Deep Extraction
- Triggered when user queries need more detail.
- Example: request bone transforms or animation curve shapes.
- This data is **not** stored in index by default; it is pulled live.

---

## 4) Retrieval Strategy

### 4.1 Metadata Filtering
- Filter by asset class, skeleton, tags, path prefix.
- Example: “Find walk animations for SK_Mannequin” → filter by `skeletonId`.

### 4.2 Semantic Search
- Embed asset summaries and user prompt.
- Search vector store for top K matching assets.

### 4.3 Reranking & Scoring
Score = w1 * metadataMatch + w2 * semanticScore + w3 * usageSignal
- Usage signals: recent edits, frequently used assets, curated “golden” assets.

---

## 5) Using Index in AI Flows

### 5.1 AI Request Pipeline
1. User asks for asset creation/modification.
2. Retrieve relevant summaries (top N).
3. Provide to AI with **short, structured context**.
4. AI chooses one of:
   - Clone existing asset
   - Create new based on template
   - Modify existing asset

### 5.2 Example: Niagara System Creation
Context:
- Provide emitter module stacks and renderer types from top candidates.
- AI can clone a similar system or build from known stacks.

### 5.3 Example: Animation Creation
Context:
- Provide skeleton hierarchy and key curve/notify names.
- AI selects correct skeleton, uses existing animations as references.

---

## 6) Bone Rotation & Socket Understanding

### 6.1 Index Additions for Skeletons
- Bone hierarchy with parent/child links.
- Socket names and attached bones.
- Virtual bones and constraints.

### 6.2 Live Query Support
For accurate transform data:
- Provide API to query current bone/socket transforms from preview mesh or runtime skeleton.
- Use live data rather than static index for transforms.

---

## 7) Storage Options

### 7.1 Local JSON Store
- Simple and portable.
- Good for small/medium projects.

### 7.2 SQLite
- Better filtering and indexing.
- Enables structured queries without heavy memory use.

### 7.3 Vector Store
- Optional; used only when semantic search is enabled.
- Local FAISS or SQLite+HNSW; optional remote service.

---

## 8) API & Tooling

### 8.1 New Service Layer
- `FIndexingService`
  - `StartIndexing()`
  - `StopIndexing()`
  - `ReindexAssetsByClass()`
  - `GetIndexStatus()`
  - `SearchAssets(query, filters)`

### 8.2 MCP Tool Actions (Examples)
- `index_assets` (start/stop/rebuild)
- `search_index` (query + filters)
- `get_asset_summary` (retrieve stored summary)

---

## 9) Performance & Safety
- Default index: minimal summaries only.
- Deep extraction on‑demand only.
- Budgeted indexing (time‑sliced on editor tick).
- Opt‑in for embeddings and heavy metadata.

---

## 10) Implementation Plan (Phased)

### Phase 1: Core Metadata Index
- Base Asset Record store
- SkeletonSummary + AnimationSummary
- Basic search/filter

### Phase 2: Domain Expanders
- NiagaraSummary + BlueprintSummary
- Embeddings (optional)

### Phase 3: AI Integration
- Context builder
- Retrieval + reranking

### Phase 4: Live Queries
- Bone/socket transform query API
- On‑demand data expansion

---

## 11) Open Questions
- Which storage backend (JSON vs SQLite) best fits expected scale?
- Do we store embeddings locally or via external service?
- How to manage index in multi‑project workspaces?

---

## 12) Summary
A structured, incremental index with domain‑specific summaries will materially improve AI accuracy for Niagara, Animation, and Blueprint workflows. It provides the right balance between performance and intelligence: lightweight summaries for retrieval, with on‑demand deep queries for precision tasks like bone transforms and socket usage.
