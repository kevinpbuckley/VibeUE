# VibeUE AI Chat - Design Document

## Overview

Add an in-editor AI chat window to the VibeUE plugin that allows users to interact with AI models via OpenRouter.ai directly within Unreal Engine.

## Goals

1. **Accessibility**: Chat window accessible from Tools menu and keyboard shortcut
2. **Model Selection**: Users can choose from available OpenRouter models
3. **Context Management**: Users can reset chat context to start fresh conversations
4. **Future MCP Integration**: Architecture should support future integration with existing MCP tools

---

## User Interface

### Chat Window (Dockable Tab)

```
┌─────────────────────────────────────────────────────────────┐
│ VibeUE AI Chat                                    [_][□][X] │
├─────────────────────────────────────────────────────────────┤
│ Model: [anthropic/claude-3.5-sonnet     ▼] [⟳ Reset Chat]   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                                                     │   │
│  │  [Assistant]                                        │   │
│  │  Hello! I'm ready to help you with your Unreal     │   │
│  │  Engine project. What would you like to do?        │   │
│  │                                                     │   │
│  │  [You]                                              │   │
│  │  Create a new material with a red base color       │   │
│  │                                                     │   │
│  │  [Assistant]                                        │   │
│  │  I'll create a material with a red base color...   │   │
│  │                                                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ [Type your message here...                        ] [Send]  │
└─────────────────────────────────────────────────────────────┘
```

### UI Components

| Component | Type | Description |
|-----------|------|-------------|
| Model Dropdown | SComboBox | Select AI model from OpenRouter |
| Reset Button | SButton | Clear conversation history |
| Message List | SScrollBox + SVerticalBox | Display chat messages |
| Input Field | SMultiLineEditableTextBox | User message input |
| Send Button | SButton | Submit message (also Enter key) |

---

## Access Methods

### 1. Tools Menu
- **Location**: Tools → VibeUE → AI Chat
- **Implementation**: FExtender to add menu entry

### 2. Keyboard Shortcut
- **Default**: `Ctrl+Shift+V` (VibeUE)
- **Configurable**: Via Editor Preferences
- **Implementation**: FUICommandInfo + FInputBindingManager

---

## Architecture

### Module Structure

```
Source/VibeUE/
├── Private/
│   ├── Chat/
│   │   ├── OpenRouterClient.cpp      # HTTP client for OpenRouter API
│   │   ├── ChatSession.cpp           # Manages conversation state
│   │   └── AIChatCommands.cpp        # UI commands registration
│   └── UI/
│       └── SAIChatWindow.cpp         # Slate chat widget
├── Public/
│   ├── Chat/
│   │   ├── OpenRouterClient.h
│   │   ├── ChatSession.h
│   │   ├── ChatTypes.h               # Message structs, model info
│   │   └── AIChatCommands.h
│   └── UI/
│       └── SAIChatWindow.h
```

### Class Diagram

```
┌─────────────────────┐
│   SAIChatWindow     │ ◄── Slate Widget (UI)
│   (SCompoundWidget) │
├─────────────────────┤
│ - ModelSelector     │
│ - MessageList       │
│ - InputField        │
├─────────────────────┤
│ + OnSendClicked()   │
│ + OnResetClicked()  │
│ + OnModelChanged()  │
└────────┬────────────┘
         │ uses
         ▼
┌─────────────────────┐
│    FChatSession     │ ◄── Conversation State
├─────────────────────┤
│ - Messages[]        │
│ - CurrentModel      │
│ - SystemPrompt      │
├─────────────────────┤
│ + SendMessage()     │
│ + Reset()           │
│ + GetHistory()      │
└────────┬────────────┘
         │ uses
         ▼
┌─────────────────────┐
│  FOpenRouterClient  │ ◄── API Client
├─────────────────────┤
│ - ApiKey            │
│ - BaseUrl           │
│ - CachedModels[]    │
├─────────────────────┤
│ + SendChatRequest() │
│ + FetchModels()     │
│ + StreamResponse()  │
└─────────────────────┘
```

---

## OpenRouter Integration

### API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `https://openrouter.ai/api/v1/models` | GET | Fetch available models |
| `https://openrouter.ai/api/v1/chat/completions` | POST | Send chat messages |

### Request Format (Chat Completion)

```json
{
  "model": "anthropic/claude-3.5-sonnet",
  "messages": [
    {"role": "system", "content": "You are a helpful Unreal Engine assistant..."},
    {"role": "user", "content": "Create a red material"},
    {"role": "assistant", "content": "I'll help you create..."},
    {"role": "user", "content": "Now make it metallic"}
  ],
  "stream": true
}
```

### Response Handling

- **Streaming**: Use Server-Sent Events (SSE) for real-time response display
- **Error Handling**: Display API errors in chat as system messages
- **Rate Limiting**: Respect OpenRouter rate limits, show user-friendly messages

---

## Configuration

### Settings (Editor Preferences → Plugins → VibeUE)

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| OpenRouter API Key | FString | "" | User's API key (stored securely) |
| Default Model | FString | "anthropic/claude-3.5-sonnet" | Preferred model |
| Chat Shortcut | FInputChord | Ctrl+Shift+V | Keyboard shortcut |
| System Prompt | FString | (see below) | Custom system prompt |
| Max Context Messages | int32 | 50 | Limit conversation history |

### Default System Prompt

```
You are an AI assistant integrated into Unreal Engine via the VibeUE plugin. 
You help users with:
- Blueprint development
- Material creation and editing
- Asset management
- UMG widget design
- Enhanced Input setup
- General Unreal Engine questions

Be concise and provide actionable guidance. When suggesting code or Blueprint 
logic, be specific about node names and connections.
```

---

## API Key Storage

### Options Considered

1. **Editor Preferences (Selected)**: Stored in `Saved/Config/` - per-user, not in source control
2. Environment Variable: `OPENROUTER_API_KEY`
3. Project Settings: Would be committed to source control (security risk)

### Implementation

```cpp
// Store in GConfig (Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini)
GConfig->SetString(TEXT("VibeUE"), TEXT("OpenRouterApiKey"), ApiKey, GEditorPerProjectIni);

// Retrieve
FString ApiKey;
GConfig->GetString(TEXT("VibeUE"), TEXT("OpenRouterApiKey"), ApiKey, GEditorPerProjectIni);
```

---

## Data Types

### FChatMessage

```cpp
USTRUCT()
struct FChatMessage
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Role;           // "user", "assistant", "system"
    
    UPROPERTY()
    FString Content;        // Message text
    
    UPROPERTY()
    FDateTime Timestamp;    // When sent/received
    
    UPROPERTY()
    bool bIsStreaming;      // True while receiving streamed response
};
```

### FOpenRouterModel

```cpp
USTRUCT()
struct FOpenRouterModel
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Id;             // "anthropic/claude-3.5-sonnet"
    
    UPROPERTY()
    FString Name;           // "Claude 3.5 Sonnet"
    
    UPROPERTY()
    int32 ContextLength;    // 200000
    
    UPROPERTY()
    float PricingPrompt;    // Per 1M tokens
    
    UPROPERTY()
    float PricingCompletion;
};
```

---

## Implementation Phases

### Phase 1: Full-Featured Chat (Streaming + Persistence)
- [ ] Create SAIChatWindow Slate widget
- [ ] Implement FOpenRouterClient with SSE streaming
- [ ] Add Tools menu entry
- [ ] Keyboard shortcut (Ctrl+Shift+V)
- [ ] Streaming responses with real-time display
- [ ] API key configuration in Editor Preferences
- [ ] Model selection dropdown (dynamic from API)
- [ ] Dynamic model list from API
- [ ] Chat history persistence between editor sessions (JSON file)
- [ ] Copy message to clipboard
- [ ] Markdown rendering for code blocks
- [ ] Reset chat button

### Phase 2: MCP Integration (Future)
- [ ] Connect to existing VibeUE MCP tools
- [ ] Allow AI to execute Blueprint, Material, Asset commands
- [ ] Tool call UI feedback
- [ ] Confirmation dialogs for destructive operations

---

## Chat Persistence

### Storage Location
```
{ProjectDir}/Saved/VibeUE/ChatHistory.json
```

### Persistence Format
```json
{
  "version": 1,
  "lastModel": "anthropic/claude-3.5-sonnet",
  "messages": [
    {
      "role": "user",
      "content": "Create a red material",
      "timestamp": "2024-01-15T10:30:00Z"
    },
    {
      "role": "assistant", 
      "content": "I'll help you create a red material...",
      "timestamp": "2024-01-15T10:30:05Z"
    }
  ]
}
```

### Persistence Behavior
- Auto-save after each message exchange
- Load on chat window open
- Clear on "Reset Chat" action
- Limit to MaxContextMessages (configurable, default 50)

---

## Dependencies

### Unreal Modules Required

```cpp
// VibeUE.Build.cs additions
PrivateDependencyModuleNames.AddRange(new string[] {
    "HTTP",           // HTTP requests
    "Json",           // JSON parsing
    "JsonUtilities",  // JSON serialization
    "Slate",          // UI (already included)
    "SlateCore",      // UI (already included)
    "EditorStyle",    // Editor theming
    "InputCore",      // Keyboard shortcuts
    "ToolMenus",      // Tools menu integration
});
```

---

## Error States

| Error | User Message | Recovery |
|-------|--------------|----------|
| No API Key | "Please set your OpenRouter API key in Editor Preferences → Plugins → VibeUE" | Link to settings |
| Invalid API Key | "Invalid API key. Please check your OpenRouter API key." | Link to settings |
| Network Error | "Unable to connect to OpenRouter. Check your internet connection." | Retry button |
| Rate Limited | "Rate limit exceeded. Please wait a moment." | Auto-retry with backoff |
| Model Unavailable | "Selected model is unavailable. Switching to default." | Auto-switch |

---

## Security Considerations

1. **API Key**: Never log or display full API key; store in user-specific config
2. **HTTPS**: All API calls over HTTPS only
3. **No Telemetry**: Do not send usage data without explicit consent
4. **Local Storage**: Chat history stays local, not uploaded

---

## Testing Plan

### Unit Tests
- JSON parsing for model list
- JSON parsing for chat responses
- Message formatting

### Integration Tests
- API key validation
- Model fetching
- Chat completion (mock server)

### Manual Tests
- [ ] Open chat from Tools menu
- [ ] Open chat via keyboard shortcut (Ctrl+Shift+V)
- [ ] Send message and receive streaming response
- [ ] Verify tokens appear incrementally during streaming
- [ ] Change model mid-conversation
- [ ] Reset chat clears history and file
- [ ] Handle network disconnect gracefully
- [ ] API key error shows helpful message
- [ ] Close and reopen editor - chat history persists
- [ ] Copy message to clipboard works
- [ ] Code blocks render with syntax highlighting

---

## Design Decisions

1. **Streaming**: ✅ Implement streaming from the start (SSE-based)
2. **Chat History Persistence**: ✅ Save chat history between editor sessions (JSON file)
3. **Multiple Chat Windows**: Single window for now (future consideration)
4. **Model Filtering**: Show all models, user can choose any
5. **MCP Integration**: Deferred to future phase

---

## Timeline Estimate

| Phase | Effort | Dependencies |
|-------|--------|--------------|
| Phase 1 (Streaming + Persistence) | 3-4 days | None |
| Phase 2 (MCP Integration) | 3-5 days | Phase 1, MCP architecture review |

---

## Appendix: OpenRouter Models API Response

```json
{
  "data": [
    {
      "id": "anthropic/claude-3.5-sonnet",
      "name": "Claude 3.5 Sonnet",
      "context_length": 200000,
      "pricing": {
        "prompt": "0.000003",
        "completion": "0.000015"
      }
    }
  ]
}
```
