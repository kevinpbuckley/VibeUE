# Design Document: InEditorChat Task List Feature

**Issue:** [kevinpbuckley/VibeUE#253](https://github.com/kevinpbuckley/VibeUE/issues/253)  
**Author:** Kevin Buckley  
**Status:** Draft  
**Priority:** 3rd (after Markdown #255, Visual Styling #256; before Sub Agents #254)  
**Effort:** Medium

---

## 1. Overview

When the LLM works on multi-step requests in the VibeUE InEditorChat, the user currently has no structured visibility into what steps the AI plans to take, which one is active, or which are done. VS Code Copilot Chat solves this with a `manage_todo_list` tool that renders a live checklist in the chat UI. VibeUE needs an equivalent system — both the **tool** the LLM calls and a **first-class Slate widget** that renders it inline in `SAIChatWindow`.

### Goals

1. Give the LLM a structured way to plan, track, and report progress on multi-step work.
2. Render an interactive, visually polished task list widget in the chat window (not just raw text).
3. Re-inject the current task state into the system prompt so the LLM maintains awareness across tool-call rounds.
4. Follow the same behavioral contract as VS Code's `manage_todo_list` (single `in-progress`, immediate completion marking, skip for trivial tasks).

### Non-Goals

- User-editable task items (the LLM owns the list — the user observes it).
- Persistent storage of task lists across sessions (they are session-scoped).
- Sub-agent delegation per task item (tracked separately in #254).

---

## 2. How VS Code Copilot Chat Does It

### 2.1 Architecture

VS Code's implementation has three layers:

| Layer | File | Responsibility |
|-------|------|----------------|
| **Tool definition** | `src/extension/tools/node/manageTodoListTool.tsx` | Registers `manage_todo_list` as a core tool. The actual implementation lives in VS Code core — the extension side just provides model-specific description overrides. |
| **Context provider** | `src/extension/prompt/node/todoListContextProvider.ts` | Reads the current todo state by invoking the tool with `operation: 'read'` and returns a string representation. |
| **Prompt injection** | `src/extension/tools/node/todoListContextPrompt.tsx` | A `PromptElement` that wraps the context provider output in a `<todoList>` XML tag and injects it into the `<context>` section of the user message. |
| **System instructions** | `src/extension/prompts/node/agent/vscModelPrompts.tsx`, `anthropicPrompts.tsx` | Detailed behavioral instructions telling the LLM when to use the tool, what constitutes trivial vs non-trivial work, and the rules for status transitions. |

### 2.2 Tool Schema

```json
{
  "name": "manage_todo_list",
  "parameters": {
    "todoList": {
      "type": "array",
      "items": {
        "id": "number",
        "title": "string (3-7 words)",
        "status": "enum: not-started | in-progress | completed"
      }
    }
  }
}
```

Key contract:
- The LLM sends the **entire** array on every call (not deltas).
- At most **one** item may be `in-progress` at a time.
- Items must be marked `completed` **immediately** when done — no batching.
- The tool is **skipped** for trivial single-step tasks.

### 2.3 UI Rendering (VS Code Core)

VS Code's core chat widget renders the todo list as a visual checklist directly in the chat conversation area. The rendering:
- Shows checkboxes with three visual states (empty, spinner, checkmark)
- Updates in real-time as the LLM calls the tool
- Appears inline in the conversation flow
- Is collapsible/expandable

### 2.4 Prompt Injection Flow

On every LLM turn:
1. `TodoListContextProvider.getCurrentTodoContext()` calls the tool with `operation: 'read'`
2. If a task list exists, `TodoListContextPrompt` wraps it in `<todoList>...</todoList>`
3. This block is injected into the `<context>` tag alongside `<currentDate>`, terminal state, etc.
4. The model sees its own plan and can update it

---

## 3. VibeUE Design

### 3.1 Data Model

#### `EVibeUETaskStatus` (new enum)

```cpp
// In ChatTypes.h
UENUM()
enum class EVibeUETaskStatus : uint8
{
    NotStarted    UMETA(DisplayName = "Not Started"),
    InProgress    UMETA(DisplayName = "In Progress"),
    Completed     UMETA(DisplayName = "Completed")
};
```

#### `FVibeUETaskItem` (new struct)

```cpp
// In ChatTypes.h
USTRUCT()
struct FVibeUETaskItem
{
    GENERATED_BODY()

    UPROPERTY()
    int32 Id = 0;

    UPROPERTY()
    FString Title;

    UPROPERTY()
    EVibeUETaskStatus Status = EVibeUETaskStatus::NotStarted;

    // Serialization helpers
    TSharedPtr<FJsonObject> ToJson() const;
    static FVibeUETaskItem FromJson(const TSharedPtr<FJsonObject>& JsonObject);
    
    // Status display helpers
    FString GetStatusString() const;
    static EVibeUETaskStatus ParseStatus(const FString& StatusStr);
};
```

#### Session State (on `FChatSession`)

```cpp
// In ChatSession.h — new private members
TArray<FVibeUETaskItem> TaskList;

// New delegate
DECLARE_DELEGATE_OneParam(FOnTaskListUpdated, const TArray<FVibeUETaskItem>&);
FOnTaskListUpdated OnTaskListUpdated;

// New public methods
void UpdateTaskList(const TArray<FVibeUETaskItem>& NewTaskList);
const TArray<FVibeUETaskItem>& GetTaskList() const;
FString SerializeTaskListForPrompt() const;
void ClearTaskList();
```

### 3.2 Internal Tool: `manage_tasks`

Registered using the existing `REGISTER_VIBEUE_INTERNAL_TOOL` macro (internal-only — no reason to expose task management via MCP to VS Code, since VS Code has its own `manage_todo_list`):

```cpp
// New file: Source/VibeUE/Private/Tools/TaskListToolRegistration.cpp

REGISTER_VIBEUE_INTERNAL_TOOL(manage_tasks,
    "Manage a structured task list to track progress and plan tasks throughout your session. "
    "Use this tool for complex, multi-step work requiring planning and tracking. "
    "Provide the COMPLETE array of all task items on every call. "
    "Each item needs an id (number), title (string, 3-7 words), and status "
    "(not-started, in-progress, or completed). "
    "At most ONE item may be in-progress at a time. "
    "Mark items completed IMMEDIATELY when done — do not batch completions. "
    "Skip this tool for simple, single-step tasks.",
    "Planning",
    TOOL_PARAMS(
        TOOL_PARAM("taskList", 
            "Complete JSON array of all task items. Must include ALL items - both existing and new. "
            "Each item: {\"id\": number, \"title\": \"string\", \"status\": \"not-started|in-progress|completed\"}",
            "array", true)
    ),
    {
        FString TaskListJson = ExtractParamFromJson(Params, TEXT("taskList"));
        
        // Parse JSON array
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TaskListJson);
        if (!FJsonSerializer::Deserialize(Reader, JsonArray))
        {
            return TEXT("{\"success\": false, \"error\": \"Invalid JSON array for taskList\"}");
        }
        
        // Build task list
        TArray<FVibeUETaskItem> NewTaskList;
        int32 InProgressCount = 0;
        for (const auto& JsonValue : JsonArray)
        {
            FVibeUETaskItem Item = FVibeUETaskItem::FromJson(JsonValue->AsObject());
            if (Item.Status == EVibeUETaskStatus::InProgress)
            {
                InProgressCount++;
            }
            NewTaskList.Add(Item);
        }
        
        // Validate: at most one in-progress
        if (InProgressCount > 1)
        {
            return TEXT("{\"success\": false, \"error\": \"At most one task may be in-progress at a time\"}");
        }
        
        // Update session — requires access to current FChatSession
        // Route through FToolRegistry's session context
        if (auto* Session = FToolRegistry::Get().GetCurrentSession())
        {
            Session->UpdateTaskList(NewTaskList);
        }
        
        // Build summary
        int32 Total = NewTaskList.Num();
        int32 Done = 0;
        for (const auto& Item : NewTaskList)
        {
            if (Item.Status == EVibeUETaskStatus::Completed) Done++;
        }
        
        return FString::Printf(
            TEXT("{\"success\": true, \"message\": \"Task list updated: %d/%d completed\"}"),
            Done, Total);
    }
);
```

#### Session Context for Tools

The tool needs access to the current `FChatSession` to call `UpdateTaskList()`. The existing tool execution path in `FChatSession::ExecuteNextToolInQueue()` already has `this` available. Two options:

**Option A (recommended):** Add a `SetCurrentSession` / `GetCurrentSession` pair to `FToolRegistry` that `FChatSession` sets before executing each tool call and clears after. Simple, thread-safe (tool execution is sequential on the game thread).

**Option B:** Pass `FChatSession*` through the `Params` map as a special key. Hacky — not recommended.

```cpp
// In ToolRegistry.h
class FToolRegistry
{
public:
    void SetCurrentSession(FChatSession* Session);
    FChatSession* GetCurrentSession() const;

private:
    FChatSession* CurrentSession = nullptr;
};
```

```cpp
// In ChatSession.cpp — ExecuteNextToolInQueue()
FToolRegistry::Get().SetCurrentSession(this);
FString Result = ToolFunc(ParamsMap);
FToolRegistry::Get().SetCurrentSession(nullptr);
```

### 3.3 UI Widget: `SVibeUETaskList`

A new Slate compound widget that renders inline in the chat message area.

#### Visual Design

```
┌─────────────────────────────────────────────┐
│  📋 Tasks (2/5 completed)            [▼]   │
│─────────────────────────────────────────────│
│  ✅  Set up project structure               │
│  ✅  Create data models                     │
│  ⏳  Implement tool registration     ← orange│
│  ○   Build UI widget                        │
│  ○   Update system prompt                   │
└─────────────────────────────────────────────┘
```

**Status icons:**
| Status | Icon | Color | Description |
|--------|------|-------|-------------|
| `NotStarted` | `○` (empty circle) | `VibeUEColors::TextMuted` (gray) | Not yet begun |
| `InProgress` | `⏳` (animated spinner) | `VibeUEColors::Orange` | Currently working — animated dots cycle |
| `Completed` | `✅` (checkmark) | `VibeUEColors::Green` | Finished |

**Layout structure:**
- Outer container: `SBorder` with `BackgroundCard` color + rounded corners (matches existing tool call widget style)
- Header row: Title text ("Tasks") + progress counter + collapse chevron
- Item rows: Status icon + title text, vertically stacked in `SVerticalBox`
- Collapsible: Click header to toggle item visibility (default: expanded)

#### Widget Class

```cpp
// New file: Source/VibeUE/Public/UI/SVibeUETaskList.h

class SVibeUETaskList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVibeUETaskList) {}
        SLATE_ARGUMENT(TArray<FVibeUETaskItem>, TaskList)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    
    /** Update the displayed task list (called when OnTaskListUpdated fires) */
    void UpdateTaskList(const TArray<FVibeUETaskItem>& NewTaskList);

private:
    /** Rebuild all task item widgets */
    void RebuildItems();
    
    /** Toggle collapsed/expanded state */
    FReply OnHeaderClicked();
    
    /** Get the appropriate icon/color for a task status */
    FText GetStatusIcon(EVibeUETaskStatus Status) const;
    FSlateColor GetStatusColor(EVibeUETaskStatus Status) const;
    FSlateColor GetTitleColor(EVibeUETaskStatus Status) const;
    
    /** Animate the in-progress spinner */
    void TickSpinnerAnimation(float DeltaTime);
    
    TArray<FVibeUETaskItem> CurrentTaskList;
    TSharedPtr<SVerticalBox> ItemsContainer;
    TSharedPtr<STextBlock> HeaderText;
    TSharedPtr<STextBlock> ChevronText;
    bool bIsCollapsed = false;
    
    // Spinner animation
    int32 SpinnerFrame = 0;
    float SpinnerTimer = 0.0f;
    static constexpr float SpinnerInterval = 0.3f;
    static const TArray<FString> SpinnerFrames; // {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"}
};
```

#### Widget Construction (pseudo-code)

```cpp
void SVibeUETaskList::Construct(const FArguments& InArgs)
{
    CurrentTaskList = InArgs._TaskList;
    
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(VibeUEColors::BackgroundCard)
        .Padding(FMargin(12, 8))
        [
            SNew(SVerticalBox)
            
            // Header row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 6)
            [
                SNew(SHorizontalBox)
                
                // "📋 Tasks (2/5 completed)"
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(HeaderText, STextBlock)
                    .ColorAndOpacity(VibeUEColors::TextPrimary)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                ]
                
                // Collapse chevron
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SAssignNew(ChevronText, STextBlock)
                    .Text(FText::FromString(TEXT("▼")))
                    .ColorAndOpacity(VibeUEColors::TextSecondary)
                ]
            ]
            
            // Separator line
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 4)
            [
                SNew(SBorder)
                .BorderBackgroundColor(VibeUEColors::TextMuted * 0.3f)
                .Padding(0)
                [
                    SNew(SSpacer).Size(FVector2D(1, 1))
                ]
            ]
            
            // Task items (rebuilt dynamically)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(ItemsContainer, SVerticalBox)
            ]
        ]
    ];
    
    RebuildItems();
}
```

#### Individual Task Item Row

```cpp
// Inside RebuildItems(), for each FVibeUETaskItem:
ItemsContainer->AddSlot()
.AutoHeight()
.Padding(0, 2)
[
    SNew(SHorizontalBox)
    
    // Status icon
    + SHorizontalBox::Slot()
    .AutoWidth()
    .Padding(0, 0, 8, 0)
    .VAlign(VAlign_Center)
    [
        SNew(STextBlock)
        .Text(GetStatusIcon(Item.Status))
        .ColorAndOpacity(GetStatusColor(Item.Status))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
    ]
    
    // Title text
    + SHorizontalBox::Slot()
    .FillWidth(1.0f)
    .VAlign(VAlign_Center)
    [
        SNew(STextBlock)
        .Text(FText::FromString(Item.Title))
        .ColorAndOpacity(GetTitleColor(Item.Status))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
    ]
];
```

#### Color Mapping

```cpp
FSlateColor SVibeUETaskList::GetStatusColor(EVibeUETaskStatus Status) const
{
    switch (Status)
    {
        case EVibeUETaskStatus::NotStarted:  return VibeUEColors::TextMuted;
        case EVibeUETaskStatus::InProgress:  return VibeUEColors::Orange;
        case EVibeUETaskStatus::Completed:   return VibeUEColors::Green;
        default:                             return VibeUEColors::TextSecondary;
    }
}

FSlateColor SVibeUETaskList::GetTitleColor(EVibeUETaskStatus Status) const
{
    switch (Status)
    {
        case EVibeUETaskStatus::Completed:   return VibeUEColors::TextSecondary; // dimmed
        case EVibeUETaskStatus::InProgress:  return VibeUEColors::TextPrimary;   // bright
        default:                             return VibeUEColors::TextPrimary;
    }
}
```

### 3.4 Integration with SAIChatWindow

The task list widget is rendered in two possible locations:

#### Option A: Sticky Header (recommended)

A single `SVibeUETaskList` instance lives at the **top of the chat window**, below the model selector and above the message scroll box. It is created when the first `manage_tasks` call arrives and persists for the session.

**Rationale:** VS Code renders the todo list as a persistent, always-visible element. Having it at the top means the user can always see progress without scrolling.

```cpp
// In SAIChatWindow::Construct(), add above MessageScrollBox:
+ SVerticalBox::Slot()
.AutoHeight()
.Padding(8, 4)
[
    SAssignNew(TaskListWidget, SVibeUETaskList)
    .Visibility(EVisibility::Collapsed)  // hidden until first task list
]
```

#### Option B: Inline with Messages

The task list is rendered as a special message widget inline with the conversation, updated in-place when the LLM calls the tool again.

**Rationale:** Matches the flow of conversation. But can scroll out of view.

#### Recommendation: **Option A (Sticky Header)**

This matches VS Code's approach where the todo list is always visible. VibeUE's chat window is relatively compact, so a collapsible sticky header is the best UX.

#### Delegate Binding

```cpp
// In SAIChatWindow::Construct()
ChatSession->OnTaskListUpdated.BindSP(this, &SAIChatWindow::HandleTaskListUpdated);

// Handler
void SAIChatWindow::HandleTaskListUpdated(const TArray<FVibeUETaskItem>& NewTaskList)
{
    if (!TaskListWidget.IsValid()) return;
    
    if (NewTaskList.Num() > 0)
    {
        TaskListWidget->SetVisibility(EVisibility::Visible);
        TaskListWidget->UpdateTaskList(NewTaskList);
    }
    else
    {
        TaskListWidget->SetVisibility(EVisibility::Collapsed);
    }
}
```

#### Clear on Chat Reset

```cpp
// In SAIChatWindow::HandleChatReset()
if (TaskListWidget.IsValid())
{
    TaskListWidget->SetVisibility(EVisibility::Collapsed);
}
```

### 3.5 Prompt Injection

Before building API messages, the current task list is serialized into the system prompt so the LLM maintains plan awareness across conversation turns.

#### Serialization Format

```xml
<taskList>
Current task progress:
- [completed] (1) Set up project structure
- [completed] (2) Create data models
- [in-progress] (3) Implement tool registration
- [not-started] (4) Build UI widget
- [not-started] (5) Update system prompt

Progress: 2/5 tasks completed
</taskList>
```

#### Implementation

```cpp
// FChatSession::SerializeTaskListForPrompt()
FString FChatSession::SerializeTaskListForPrompt() const
{
    if (TaskList.Num() == 0) return TEXT("");
    
    FString Result = TEXT("<taskList>\nCurrent task progress:\n");
    int32 CompletedCount = 0;
    
    for (const auto& Item : TaskList)
    {
        Result += FString::Printf(TEXT("- [%s] (%d) %s\n"),
            *Item.GetStatusString(), Item.Id, *Item.Title);
        if (Item.Status == EVibeUETaskStatus::Completed)
            CompletedCount++;
    }
    
    Result += FString::Printf(TEXT("\nProgress: %d/%d tasks completed\n</taskList>"),
        CompletedCount, TaskList.Num());
    
    return Result;
}
```

#### Injection Point

In `FChatSession::BuildApiMessages()`, append the task list context to the system message:

```cpp
TArray<FChatMessage> FChatSession::BuildApiMessages() const
{
    FString FullSystemPrompt = SystemPrompt;
    
    // Inject task list context
    FString TaskContext = SerializeTaskListForPrompt();
    if (!TaskContext.IsEmpty())
    {
        FullSystemPrompt += TEXT("\n\n") + TaskContext;
    }
    
    ApiMessages.Add(FChatMessage(TEXT("system"), FullSystemPrompt));
    // ... rest of message building
}
```

### 3.6 System Prompt Update (`vibeue.instructions.md`)

The VibeUE system prompt lives at:

```
Plugins/VibeUE/Content/instructions/vibeue.instructions.md
```

This file is loaded at runtime by `FLLMClientBase::LoadSystemPromptFromFile()` and concatenated with any other `.md` files in the `instructions/` directory. It defines the LLM's persona, available tools, skills system, critical rules, and communication style.

Two changes are needed:

#### Change 1: Add `manage_tasks` to the tool list

The current tool list section reads:

```markdown
## ⚠️ CRITICAL: Available MCP Tools

**You have ONLY 7 tools:**
1. `execute_python_code` - Execute Python code in Unreal (use `import unreal`)
2. `discover_python_module` - Discover module contents
3. `discover_python_class` - Get class methods and properties
4. `discover_python_function` - Get function signatures
5. `list_python_subsystems` - List UE editor subsystems
6. `manage_skills` - Load domain-specific knowledge on demand
7. `attach_image` - **INTERNAL ONLY** - Attach an image for AI analysis
```

Update to:

```markdown
## ⚠️ CRITICAL: Available MCP Tools

**You have ONLY 8 tools:**
1. `execute_python_code` - Execute Python code in Unreal (use `import unreal`)
2. `discover_python_module` - Discover module contents
3. `discover_python_class` - Get class methods and properties
4. `discover_python_function` - Get function signatures
5. `list_python_subsystems` - List UE editor subsystems
6. `manage_skills` - Load domain-specific knowledge on demand
7. `manage_tasks` - **INTERNAL ONLY** - Track progress on multi-step work (renders checklist in chat UI)
8. `attach_image` - **INTERNAL ONLY** - Attach an image for AI analysis
```

#### Change 2: Add Task Planning section

Insert the following new section **after** the `## 💬 Communication Style` section and **before** `## 🚀 Getting Started Workflow`:

```markdown
---

## 📋 Task Planning

You have access to a `manage_tasks` tool which tracks tasks and progress and renders them to the user as a visual checklist in the chat window. Using this tool helps demonstrate that you've understood the request and convey how you're approaching it.

### When to use task planning:
- The task is non-trivial and requires multiple distinct actions
- There are logical phases or dependencies where sequencing matters
- The work has ambiguity that benefits from outlining high-level goals
- The user asked you to do more than one thing in a single prompt
- You discover additional steps while working

### When to skip task planning:
- The task is simple and direct (e.g., create one asset, answer a question)
- Breaking it down would only produce trivial steps

### Task planning rules:
- Before beginning work on any task: mark exactly ONE task as `in-progress`
- Keep only ONE task `in-progress` at a time
- Mark tasks `completed` IMMEDIATELY when finished — do not batch completions
- Before ending your turn: ensure ALL tasks are explicitly marked
- Provide the COMPLETE task array on every `manage_tasks` call — not deltas
- Each task needs: `id` (sequential number), `title` (3-7 word label), `status` (not-started, in-progress, or completed)

### Examples of non-trivial work (USE planning):
- "Create a character with animations and set up the blueprint" → Create skeletal mesh, Create AnimBP, Set up Pose Search, Wire up Blueprint
- "Add a new weapon system" → Create data assets, Build weapon actor, Add input actions, Create UI widget
- "Debug the movement issue" → Analyze logs, Identify root cause, Implement fix, Validate

### Examples of trivial work (SKIP planning):
- "Create a material"
- "What skeleton does this mesh use?"
- "Rename this blueprint"
- "Execute this Python code"

### manage_tasks tool usage:
```json
{
  "taskList": [
    {"id": 1, "title": "Create data model", "status": "completed"},
    {"id": 2, "title": "Register tool", "status": "in-progress"},
    {"id": 3, "title": "Build UI widget", "status": "not-started"}
  ]
}
```
```

---

## 4. Tool Registration Details

### Help System

Create a markdown help file for the tool following VibeUE's help system pattern:

**File:** `Content/Help/manage_tasks/manage_tasks.md`

```markdown
# manage_tasks

Manage a structured task list to track progress and plan multi-step work.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `taskList` | array | Yes | Complete array of all task items |

### Task Item Schema

Each item in the `taskList` array:

| Field | Type | Description |
|-------|------|-------------|
| `id` | number | Sequential identifier starting from 1 |
| `title` | string | Concise action label (3-7 words) |
| `status` | string | `not-started`, `in-progress`, or `completed` |

## Rules

- Provide the FULL array on every call (not deltas)
- At most ONE item may be `in-progress` at a time
- Mark items `completed` immediately when finished

## Example

```json
{
  "taskList": [
    {"id": 1, "title": "Create data model", "status": "completed"},
    {"id": 2, "title": "Register tool", "status": "in-progress"},
    {"id": 3, "title": "Build UI widget", "status": "not-started"}
  ]
}
```
```

### Docstring Alignment

The tool description in the `REGISTER_VIBEUE_INTERNAL_TOOL` macro, the help markdown, and the system prompt instructions must all reference the same:
- Tool name: `manage_tasks`
- Parameter name: `taskList`
- Status values: `not-started`, `in-progress`, `completed`
- Rules: complete array, single in-progress, immediate completion

---

## 5. File Inventory

### New Files

| File | Type | Purpose |
|------|------|---------|
| `Private/Tools/TaskListToolRegistration.cpp` | C++ | Tool registration via `REGISTER_VIBEUE_INTERNAL_TOOL` |
| `Public/UI/SVibeUETaskList.h` | C++ Header | Task list Slate widget declaration |
| `Private/UI/SVibeUETaskList.cpp` | C++ | Task list Slate widget implementation |
| `Content/Help/manage_tasks/manage_tasks.md` | Markdown | Tool help documentation |

### Modified Files

| File | Change |
|------|--------|
| `Public/Chat/ChatTypes.h` | Add `EVibeUETaskStatus` enum, `FVibeUETaskItem` struct |
| `Private/Chat/ChatTypes.cpp` | Add `FVibeUETaskItem` serialization methods |
| `Public/Chat/ChatSession.h` | Add `TaskList` member, `OnTaskListUpdated` delegate, public methods |
| `Private/Chat/ChatSession.cpp` | Implement `UpdateTaskList()`, `SerializeTaskListForPrompt()`, inject into `BuildApiMessages()` |
| `Public/Core/ToolRegistry.h` | Add `SetCurrentSession()` / `GetCurrentSession()` |
| `Private/Core/ToolRegistry.cpp` | Implement session context accessor |
| `Public/UI/SAIChatWindow.h` | Add `TaskListWidget` member, `HandleTaskListUpdated()` |
| `Private/UI/SAIChatWindow.cpp` | Create widget in `Construct()`, bind delegate, handle reset |
| `Content/instructions/vibeue.instructions.md` | Add `manage_tasks` to tool list, add Task Planning section |

---

## 6. Behavioral Contract

The following rules define how the LLM should use `manage_tasks`. These are enforced both by the tool's validation logic and by the system prompt instructions.

| Rule | Enforcement |
|------|-------------|
| At most 1 `in-progress` item | Tool returns error if violated |
| Every call sends the **complete** array | By design — no delta API |
| Mark completed **immediately** | System prompt instruction (soft) |
| Skip for trivial tasks | System prompt instruction (soft) |
| Consecutive IDs starting from 1 | Convention (not enforced) |
| Title length 3-7 words | Convention (not enforced) |
| All items have explicit status before turn ends | System prompt instruction (soft) |

---

## 7. Sequence Diagram

```
User                    SAIChatWindow           FChatSession           LLM              manage_tasks
  |                          |                       |                  |                     |
  |-- "Add auth system" ---->|                       |                  |                     |
  |                          |-- SendMessage() ----->|                  |                     |
  |                          |                       |-- BuildApiMsgs ->|                     |
  |                          |                       |   (no taskList)  |                     |
  |                          |                       |                  |                     |
  |                          |                       |<-- tool_call: manage_tasks ----------->|
  |                          |                       |                  |                     |
  |                          |                       |-- ExecuteTool ---|-------------------->|
  |                          |                       |   {taskList:[...]}                     |
  |                          |                       |                  |                     |
  |                          |                       |<- UpdateTaskList-|   {success: true}   |
  |                          |                       |                  |                     |
  |                          |<-- OnTaskListUpdated--|                  |                     |
  |                          |                       |                  |                     |
  |   [Widget shows tasks]   |                       |                  |                     |
  |                          |                       |                  |                     |
  |                          |                       |-- BuildApiMsgs ->|                     |
  |                          |                       | (+<taskList>ctx) |                     |
  |                          |                       |                  |                     |
  |                          |                       |<-- tool_call: execute_python_code      |
  |                          |                       |                  |                     |
  |                          |                       |   ... work ...   |                     |
  |                          |                       |                  |                     |
  |                          |                       |<-- tool_call: manage_tasks (update) -->|
  |                          |                       |                  |                     |
  |                          |<-- OnTaskListUpdated--|   (task 1 done)  |                     |
  |   [Widget updates]       |                       |                  |                     |
```

---

## 8. Testing Plan

### Unit Tests

1. **`FVibeUETaskItem` serialization** — Round-trip JSON serialization/deserialization
2. **Status parsing** — `"not-started"` → `NotStarted`, `"in-progress"` → `InProgress`, etc.
3. **Validation** — Multiple `in-progress` items rejected with error
4. **Prompt serialization** — `SerializeTaskListForPrompt()` produces correct XML block
5. **Empty list** — No `<taskList>` block when list is empty

### Integration Tests

1. **Tool invocation** — Call `manage_tasks` through the tool registry, verify `OnTaskListUpdated` fires
2. **Session lifecycle** — Task list cleared on chat reset
3. **Prompt injection** — Verify `<taskList>` block appears in `BuildApiMessages()` output after task creation

### Manual QA

1. Ask the LLM a multi-step question (e.g., "Create a material, apply it to a mesh, and take a screenshot")
2. Verify the task list widget appears in the header area
3. Verify items transition through `not-started` → `in-progress` → `completed` with correct icons/colors
4. Verify the widget collapses/expands on header click
5. Verify the widget disappears on chat reset
6. Verify trivial requests (e.g., "What version of UE is this?") do NOT produce a task list

---

## 9. Future Considerations

- **Sub-agent integration (#254):** Each task item could optionally spawn a sub-agent. The task list widget would show which sub-agent owns which task.
- **User interaction:** Allow clicking a task to scroll to the relevant message/tool call in the chat history.
- **Persistence:** Save task lists to chat session persistence for review later.
- **Progress bar:** Add a slim progress bar at the top of the chat window showing overall completion percentage.
- **MCP exposure:** If external tools (VS Code extension, web UI) want to read VibeUE's task state, expose `manage_tasks` via MCP in read-only mode.

---

## 10. Dependencies

| Dependency | Status | Notes |
|------------|--------|-------|
| Markdown rendering (#255) | Prerequisite | Task titles may contain inline formatting |
| Visual styling (#256) | Prerequisite | Color system and card styling patterns |
| `FToolRegistry` session context | New | Requires adding `CurrentSession` pointer |
| Existing `FToolCallWidgetData` pattern | Reference | Task list widget follows same collapsible card pattern |
