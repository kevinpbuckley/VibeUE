# VibeUE Search and Read Tools - Gap Analysis

## Executive Summary

After analyzing vscode-copilot-chat's search and file tools, VibeUE needs to implement comprehensive file discovery and search capabilities. Currently, VibeUE focuses on asset-level operations but lacks fundamental filesystem and code search tools that AI agents rely on for context gathering.

## VSCode-Copilot-Chat Tools (Reference Implementation)

### 1. **read_file** - File Content Reading
**Purpose**: Read file contents with line range support
**Key Features**:
- Line-based reading (startLine, endLine, 1-indexed)
- Can read any file in workspace
- Optimized for large files (read only what's needed)
- Used extensively before making code edits

**Current VibeUE Status**: ❌ **MISSING**
- Only has Python file reading (read_source_file) for plugin files
- No general-purpose file reading capability
- Cannot read C++, Blueprint text assets, config files, etc.

### 2. **file_search** - Glob Pattern File Search
**Purpose**: Find files by name/pattern using glob syntax
**Key Features**:
- Glob patterns (e.g., `**/*.cpp`, `src/**/*.h`)
- Fast filename matching
- Returns file paths
- Works from workspace root

**Current VibeUE Status**: ❌ **MISSING**
- manage_asset has search but only for UE assets (Blueprints, Materials, etc.)
- No filesystem-level file pattern matching
- Cannot find source files, config files, or non-asset files

### 3. **grep_search** - Text Content Search (Ripgrep-style)
**Purpose**: Fast text/regex search across workspace files
**Key Features**:
- Regex support (isRegexp parameter)
- Include/exclude patterns (glob-based)
- includeIgnoredFiles option (search in node_modules, build outputs)
- maxResults limiting
- Returns file paths + matching lines with context
- Case-insensitive by default

**Current VibeUE Status**: ❌ **MISSING**
- No text search capability
- Cannot find code patterns, TODO comments, function definitions
- Critical for understanding unfamiliar codebases

### 4. **semantic_search** - AI-Powered Semantic Search
**Purpose**: Natural language semantic search using embeddings
**Key Features**:
- Query by meaning, not exact text
- Uses vector embeddings + TF-IDF hybrid approach
- Workspace chunking and indexing
- Reranking for relevance
- Keyword extraction
- Returns relevant code chunks with context

**Current VibeUE Status**: ❌ **MISSING**
- No semantic understanding of workspace
- Critical for "find code related to X" queries
- Used heavily by AI agents for context gathering

### 5. **list_dir** - Directory Listing
**Purpose**: List directory contents
**Key Features**:
- Shows files and subdirectories
- Used for workspace exploration
- Helps understand project structure

**Current VibeUE Status**: ⚠️ **PARTIAL**
- Can list UE asset paths via manage_asset search
- No general filesystem directory listing
- Cannot explore Source/, Config/, plugin directories

## Critical Missing Capabilities

### File System Operations
1. **No General File Reading**: Cannot read .cpp, .h, .ini, .json, .md files
2. **No File Pattern Search**: Cannot find "all .cpp files" or "headers in this folder"
3. **No Text Search**: Cannot search for function names, keywords, patterns
4. **Limited Directory Navigation**: Only asset-level, not filesystem-level

### Impact on AI Effectiveness
- **Cannot gather code context** before making changes
- **Cannot understand existing implementations** to maintain consistency
- **Cannot find examples** of patterns to follow
- **Cannot analyze project structure** efficiently
- **Forces blind edits** without understanding surrounding code

## Recommended Implementation Plan

### Phase 1: Essential File Operations (HIGH PRIORITY)
Implement these first as they're foundational for AI agents:

#### 1.1 read_file Tool
```cpp
// Tool: read_file
// Description: Read file contents with line range support
// Parameters:
//   - filePath: string (required) - absolute or relative path
//   - startLine: integer (default: 1) - start line (1-indexed)
//   - endLine: integer (default: -1) - end line (-1 = end of file)
// Returns: File contents as string with metadata
```

**Implementation Notes**:
- Use `FFileHelper::LoadFileToStringArray()` for line-based reading
- Support both absolute paths and workspace-relative paths
- Add line number prefixes for clarity
- Return total line count in metadata
- Handle UTF-8 encoding properly

#### 1.2 list_dir Tool
```cpp
// Tool: list_dir
// Description: List directory contents
// Parameters:
//   - path: string (required) - directory path
//   - recursive: bool (default: false) - recursive listing
// Returns: Array of {name, type (file/dir), size, modified}
```

**Implementation Notes**:
- Use `IFileManager::Get().IterateDirectory()`
- Mark directories with trailing slash
- Return file sizes and timestamps
- Support both absolute and relative paths

#### 1.3 file_search Tool
```cpp
// Tool: file_search
// Description: Find files matching glob patterns
// Parameters:
//   - query: string (required) - glob pattern (e.g., "**/*.cpp")
//   - maxResults: integer (default: 100)
// Returns: Array of matching file paths
```

**Implementation Notes**:
- Use `IFileManager::Get().FindFilesRecursive()`
- Support glob syntax: `*`, `**`, `?`, `[abc]`
- Search from project root or Plugins/VibeUE root
- Exclude common ignore patterns (.git, Intermediate, Binaries)

### Phase 2: Text Search (MEDIUM PRIORITY)

#### 2.1 grep_search Tool
```cpp
// Tool: grep_search
// Description: Search for text/regex patterns in files
// Parameters:
//   - query: string (required) - search pattern
//   - isRegexp: bool (default: false) - treat as regex
//   - includePattern: string (optional) - file glob pattern
//   - includeIgnoredFiles: bool (default: false)
//   - maxResults: integer (default: 50)
// Returns: Array of {file, line, lineNumber, matchRanges}
```

**Implementation Notes**:
- Use `FRegexPattern` for regex support
- Read files line-by-line for memory efficiency
- Include 2-3 lines of context around matches
- Respect .gitignore patterns
- Support case-insensitive search

### Phase 3: Semantic Search (LOWER PRIORITY)

#### 3.1 Basic Semantic Search
- Requires embedding service integration
- Text chunking strategy
- Vector storage and indexing
- Complex to implement, high value for AI

**Recommendation**: Start with Phases 1-2, evaluate need for semantic search based on usage patterns.

## Integration Points

### Tool Registry Updates
All new tools should register with VibeUE's existing `FToolRegistry`:

```cpp
// In ExampleTools.cpp or new FileSystemTools.cpp
void RegisterFileSystemTools()
{
    FToolRegistry& Registry = FToolRegistry::Get();
    
    // read_file
    Registry.RegisterTool({
        TEXT("read_file"),
        TEXT("Read file contents with line range support"),
        TEXT("Filesystem"),
        { /* parameters */ },
        &ExecuteReadFile
    });
    
    // file_search
    Registry.RegisterTool({
        TEXT("file_search"),
        TEXT("Find files matching glob patterns"),
        TEXT("Filesystem"),
        { /* parameters */ },
        &ExecuteFileSearch
    });
    
    // grep_search
    Registry.RegisterTool({
        TEXT("grep_search"),
        TEXT("Search for text patterns in files"),
        TEXT("Filesystem"),
        { /* parameters */ },
        &ExecuteGrepSearch
    });
    
    // list_dir
    Registry.RegisterTool({
        TEXT("list_dir"),
        TEXT("List directory contents"),
        TEXT("Filesystem"),
        { /* parameters */ },
        &ExecuteListDir
    });
}
```

### MCP Server Integration
Tools automatically exposed via MCP once registered in ToolRegistry. No additional MCP changes needed.

### EditorTools.h Updates
Add corresponding BlueprintCallable functions:

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Filesystem")
static FString ReadFile(const FString& FilePath, int32 StartLine = 1, int32 EndLine = -1);

UFUNCTION(BlueprintCallable, Category = "VibeUE|Filesystem")
static FString ListDirectory(const FString& Path, bool bRecursive = false);

UFUNCTION(BlueprintCallable, Category = "VibeUE|Filesystem")  
static FString FileSearch(const FString& Pattern, int32 MaxResults = 100);

UFUNCTION(BlueprintCallable, Category = "VibeUE|Filesystem")
static FString GrepSearch(const FString& Query, bool bIsRegex = false, 
                          const FString& IncludePattern = TEXT(""), 
                          int32 MaxResults = 50);
```

## File Structure

Create new files for filesystem tools:

```
Source/VibeUE/
├── Private/
│   ├── Commands/
│   │   └── FileSystemCommands.h/cpp  (NEW)
│   └── Tools/
│       └── FileSystemTools.h/cpp      (NEW)
└── Public/
    └── ...
```

## Security Considerations

### Path Traversal Protection
- Validate all paths to prevent `../` escapes
- Restrict to project directory and subdirectories
- Block access to system directories

### File Size Limits
- Limit read_file to reasonable sizes (e.g., 10MB max)
- Add warnings for large files
- Support partial reading with line ranges

### Ignore Patterns
- Respect .gitignore by default
- Add .vibeue_ignore support
- Exclude: Binaries/, Intermediate/, DerivedDataCache/

### Performance
- Cache directory listings
- Implement cancellation support for long searches
- Async execution where possible

## Testing Strategy

### Unit Tests
- Test glob pattern matching
- Test regex search
- Test line range reading
- Test path validation

### Integration Tests
- Search across real project files
- Read various file types (.cpp, .h, .ini, .json)
- Test with large files
- Test with Unicode content

### AI Agent Testing
- Test with Claude/GPT agents
- Verify context gathering workflows
- Measure improvement in code understanding

## Success Metrics

### Quantitative
- AI agents use read_file before 90% of code edits
- grep_search finds relevant code in <2 seconds
- file_search returns results in <500ms

### Qualitative
- AI agents make fewer assumptions about code
- Better consistency with existing patterns
- Fewer "blind edits" that break code

## Timeline Estimate

- **Phase 1 (Essential)**: 2-3 days
  - read_file: 4-6 hours
  - list_dir: 3-4 hours
  - file_search: 6-8 hours
  - Testing & integration: 4-6 hours

- **Phase 2 (Text Search)**: 2-3 days
  - grep_search: 8-12 hours
  - Regex support: 4-6 hours
  - Testing: 4-6 hours

- **Phase 3 (Semantic)**: 1-2 weeks
  - Research & design: 2-3 days
  - Implementation: 5-7 days
  - Testing: 2-3 days

## Conclusion

Implementing these tools will significantly enhance VibeUE's AI agent capabilities by providing the context-gathering tools that vscode-copilot-chat relies on. Start with Phase 1 (essential file operations) to get immediate value, then expand to text search. Semantic search can be added later once basic tools prove valuable.

The implementation aligns with VibeUE's existing architecture (ToolRegistry, MCP integration, EditorTools pattern) and requires no major refactoring—just adding new tool implementations.
