# VibeUE Context Summarization Design Document

## Overview

This document outlines how to implement context summarization in the VibeUE chat system, based on VS Code Copilot Chat's approach. When conversation history exceeds the model's context window, the system will automatically summarize older messages to allow continued conversation without losing important context.

## Key Insights from VS Code Copilot Chat

### 1. Summarization Trigger
- Triggered when **token budget is exceeded** during prompt rendering
- Not triggered at a fixed percentage - happens dynamically when rendering fails
- Uses a `BudgetExceededError` exception to trigger summarization

### 2. Summary Architecture
- **Two Modes**:
  - `SummaryMode.Full` - Comprehensive summarization using the full conversation
  - `SummaryMode.Simple` - Fallback when full mode fails (truncates tool results)
- Uses the **same LLM** to generate summaries
- Summary is stored as a `<conversation-summary>` tag in history

### 3. Summary Prompt Structure
The summary prompt requests a highly structured output:

```
<analysis>
1. Chronological Review: Walk through conversation phases
2. Intent Mapping: Extract user requests and goals
3. Technical Inventory: Catalog technologies and decisions
4. Code Archaeology: Document files and code changes
5. Progress Assessment: Completed vs pending tasks
6. Context Validation: Ensure continuation context
7. Recent Commands Analysis: Last agent commands and results
</analysis>

<summary>
1. Conversation Overview
2. Technical Foundation
3. Codebase Status
4. Problem Resolution
5. Progress Tracking
6. Active Work State
7. Recent Operations
8. Continuation Plan
</summary>
```

### 4. Summary Placement
- Summary replaces older messages in the history
- Recent tool call rounds are preserved verbatim
- Summary is injected as a `UserMessage` with `<conversation-summary>` tag

### 5. Key Design Decisions
- **Non-streaming** summarization request (`stream: false`)
- **Temperature 0** for deterministic summaries
- Summary size is validated against token budget
- Falls back to Simple mode if Full mode fails

---

## VibeUE Implementation Plan

### Phase 1: Token Budget Tracking

#### 1.1 Add Token Estimation to ChatSession

```cpp
// ChatSession.h
class VIBEUE_API UChatSession : public UObject
{
    // ... existing code ...
    
    // Token management
    int32 EstimateTokenCount(const FString& Text) const;
    int32 EstimateConversationTokens() const;
    int32 GetTokenBudget() const;
    float GetTokenUsagePercent() const;
    bool IsNearContextLimit(float ThresholdPercent = 0.8f) const;
    
private:
    // Rough token estimation (4 chars ≈ 1 token for English)
    static constexpr float CharsPerToken = 4.0f;
};
```

#### 1.2 Implementation

```cpp
int32 UChatSession::EstimateTokenCount(const FString& Text) const
{
    // Simple estimation: ~4 characters per token for English
    // This is approximate but good enough for budget tracking
    return FMath::CeilToInt(Text.Len() / CharsPerToken);
}

int32 UChatSession::EstimateConversationTokens() const
{
    int32 TotalTokens = 0;
    
    // Count system prompt tokens
    TotalTokens += EstimateTokenCount(SystemPrompt);
    
    // Count all messages
    for (const FChatMessage& Msg : Messages)
    {
        TotalTokens += EstimateTokenCount(Msg.Role);
        TotalTokens += EstimateTokenCount(Msg.Content);
        
        // Count tool calls
        for (const FToolCall& Tool : Msg.ToolCalls)
        {
            TotalTokens += EstimateTokenCount(Tool.Name);
            TotalTokens += EstimateTokenCount(Tool.Arguments);
        }
    }
    
    // Count tool results
    for (const FToolResult& Result : ToolResults)
    {
        TotalTokens += EstimateTokenCount(Result.Content);
    }
    
    return TotalTokens;
}

int32 UChatSession::GetTokenBudget() const
{
    // Use 90% of context length to leave room for response
    return FMath::FloorToInt(GetCurrentModelContextLength() * 0.9f);
}

bool UChatSession::IsNearContextLimit(float ThresholdPercent) const
{
    int32 CurrentTokens = EstimateConversationTokens();
    int32 Budget = GetTokenBudget();
    return (float)CurrentTokens / Budget >= ThresholdPercent;
}
```

---

### Phase 2: Summarization Request

#### 2.1 Add Summarization to ChatSession

```cpp
// ChatSession.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSummarizationStarted, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSummarizationComplete, bool, bSuccess, const FString&, Summary);

class VIBEUE_API UChatSession : public UObject
{
    // ... existing code ...
    
    // Summarization
    UPROPERTY(BlueprintAssignable)
    FOnSummarizationStarted OnSummarizationStarted;
    
    UPROPERTY(BlueprintAssignable)
    FOnSummarizationComplete OnSummarizationComplete;
    
    UFUNCTION(BlueprintCallable)
    void TriggerSummarizationIfNeeded();
    
    UFUNCTION(BlueprintCallable)
    void ForceSummarize();
    
private:
    void RequestSummarization();
    void HandleSummarizationResponse(const FString& Summary);
    FString BuildSummarizationPrompt() const;
    
    bool bIsSummarizing = false;
    FString CurrentSummary;
    int32 SummarizedUpToIndex = -1; // Message index where summary ends
};
```

#### 2.2 Summarization Prompt

```cpp
FString UChatSession::BuildSummarizationPrompt() const
{
    return TEXT(R"(
Your task is to create a comprehensive summary of the conversation that captures all essential information needed to continue the work without loss of context.

## Summary Structure

Provide your summary in this format:

<conversation-summary>
1. **Conversation Overview**:
   - Primary Objectives: [Main user goals and requests]
   - Session Context: [High-level narrative of conversation flow]

2. **Technical Foundation**:
   - Technologies/frameworks discussed
   - Key architectural decisions made

3. **Codebase Status**:
   - Files modified or discussed
   - Key code changes and their purpose

4. **Problem Resolution**:
   - Issues encountered and how they were resolved
   - Ongoing debugging context

5. **Progress Tracking**:
   - ✅ Completed tasks
   - ⏳ In-progress work
   - ❌ Pending tasks

6. **Active Work State**:
   - Current focus (what was being worked on)
   - Recent tool calls and their results (summarized)

7. **Continuation Plan**:
   - Immediate next steps
   - Priority information
</conversation-summary>

## Guidelines
- Be precise with filenames, function names, and technical terms
- Preserve exact quotes for task specifications
- Include enough detail to continue without re-reading full history
- Truncate very long tool outputs but preserve essential information

Now summarize the conversation:
)");
}
```

#### 2.3 Request Handling

```cpp
void UChatSession::TriggerSummarizationIfNeeded()
{
    if (bIsSummarizing) return;
    
    // Check if we're at 80% of context limit
    if (IsNearContextLimit(0.8f))
    {
        UE_LOG(LogVibeUE, Log, TEXT("[SUMMARIZE] Context at %.1f%%, triggering summarization"),
            GetTokenUsagePercent() * 100.f);
        RequestSummarization();
    }
}

void UChatSession::RequestSummarization()
{
    bIsSummarizing = true;
    OnSummarizationStarted.Broadcast(TEXT("Context limit approaching"));
    
    // Build a separate request for summarization
    TArray<FChatMessage> SummarizationMessages;
    
    // System message with summarization instructions
    FChatMessage SystemMsg;
    SystemMsg.Role = TEXT("system");
    SystemMsg.Content = BuildSummarizationPrompt();
    SummarizationMessages.Add(SystemMsg);
    
    // Add conversation history for summarization
    FChatMessage UserMsg;
    UserMsg.Role = TEXT("user");
    UserMsg.Content = TEXT("Please summarize the following conversation:\n\n");
    
    // Append all messages
    for (const FChatMessage& Msg : Messages)
    {
        UserMsg.Content += FString::Printf(TEXT("[%s]: %s\n\n"), 
            *Msg.Role, *Msg.Content);
    }
    SummarizationMessages.Add(UserMsg);
    
    // Make non-streaming request to LLM
    // ... (implementation depends on LLM client interface)
}
```

---

### Phase 3: Summary Integration

#### 3.1 Apply Summary to History

```cpp
void UChatSession::HandleSummarizationResponse(const FString& Summary)
{
    bIsSummarizing = false;
    
    if (Summary.IsEmpty())
    {
        UE_LOG(LogVibeUE, Error, TEXT("[SUMMARIZE] Received empty summary"));
        OnSummarizationComplete.Broadcast(false, TEXT(""));
        return;
    }
    
    // Store the summary
    CurrentSummary = Summary;
    SummarizedUpToIndex = Messages.Num() - 1; // Keep most recent messages
    
    // Create new message array with summary
    TArray<FChatMessage> NewMessages;
    
    // Add summary as a user message
    FChatMessage SummaryMsg;
    SummaryMsg.Role = TEXT("user");
    SummaryMsg.Content = Summary;
    NewMessages.Add(SummaryMsg);
    
    // Keep recent messages (last N tool call rounds)
    // This preserves immediate context
    int32 RecentCount = FMath::Min(Messages.Num(), 10); // Keep last 10 messages
    for (int32 i = Messages.Num() - RecentCount; i < Messages.Num(); i++)
    {
        if (i >= 0)
        {
            NewMessages.Add(Messages[i]);
        }
    }
    
    // Replace messages
    Messages = NewMessages;
    
    // Clear old tool results, keep recent ones
    // ...
    
    UE_LOG(LogVibeUE, Log, TEXT("[SUMMARIZE] Applied summary, reduced messages from %d to %d"),
        SummarizedUpToIndex + 1, Messages.Num());
    
    OnSummarizationComplete.Broadcast(true, Summary);
}
```

---

### Phase 4: UI Integration

#### 4.1 Show Summarization Status

```cpp
// SAIChatWindow.cpp - Add summarization indicator

void SAIChatWindow::Construct(const FArguments& InArgs)
{
    // ... existing code ...
    
    // Bind to summarization events
    if (ChatSession)
    {
        ChatSession->OnSummarizationStarted.AddDynamic(this, &SAIChatWindow::OnSummarizationStarted);
        ChatSession->OnSummarizationComplete.AddDynamic(this, &SAIChatWindow::OnSummarizationComplete);
    }
}

void SAIChatWindow::OnSummarizationStarted(const FString& Reason)
{
    // Show "Summarizing conversation..." indicator
    AddSystemMessage(TEXT("📋 Summarizing conversation history..."));
}

void SAIChatWindow::OnSummarizationComplete(bool bSuccess, const FString& Summary)
{
    if (bSuccess)
    {
        AddSystemMessage(TEXT("✅ Conversation summarized to save context space."));
    }
    else
    {
        AddSystemMessage(TEXT("⚠️ Failed to summarize conversation."));
    }
}
```

#### 4.2 Token Budget Display

```cpp
// Add to header bar
void SAIChatWindow::UpdateTokenDisplay()
{
    if (!ChatSession) return;
    
    int32 CurrentTokens = ChatSession->EstimateConversationTokens();
    int32 Budget = ChatSession->GetTokenBudget();
    float Percent = ChatSession->GetTokenUsagePercent();
    
    FString TokenText = FString::Printf(TEXT("Tokens: %d / %d (%.0f%%)"),
        CurrentTokens, Budget, Percent * 100.f);
    
    // Update UI element
    if (TokenCountText.IsValid())
    {
        TokenCountText->SetText(FText::FromString(TokenText));
        
        // Color based on usage
        FSlateColor Color = Percent < 0.7f ? FLinearColor::Green :
                           Percent < 0.9f ? FLinearColor::Yellow :
                                            FLinearColor::Red;
        TokenCountText->SetColorAndOpacity(Color);
    }
}
```

---

### Phase 5: Automatic Summarization Flow

#### 5.1 Integration Points

```cpp
// In VibeUEAPIClient.cpp or ChatSession.cpp

void UChatSession::SendMessage(const FString& UserMessage)
{
    // Check if summarization needed BEFORE sending
    TriggerSummarizationIfNeeded();
    
    // ... existing send logic ...
}

void UChatSession::HandleAssistantResponse(const FString& Response)
{
    // ... existing response handling ...
    
    // Check after response if we're getting close to limit
    TriggerSummarizationIfNeeded();
}

void UChatSession::HandleToolResult(const FToolResult& Result)
{
    // ... existing tool result handling ...
    
    // Tool results can be large, check after adding
    TriggerSummarizationIfNeeded();
}
```

---

## Configuration Options

```cpp
// VibeUESettings.h
UCLASS(config=EditorPerProjectUserSettings)
class UVibeUESettings : public UObject
{
    // ... existing settings ...
    
    // Summarization Settings
    UPROPERTY(config, EditAnywhere, Category="Context Management")
    float SummarizationThreshold = 0.8f; // Trigger at 80% of context
    
    UPROPERTY(config, EditAnywhere, Category="Context Management")
    int32 RecentMessagesToKeep = 10; // Messages to preserve after summarization
    
    UPROPERTY(config, EditAnywhere, Category="Context Management")
    bool bAutoSummarize = true; // Enable automatic summarization
    
    UPROPERTY(config, EditAnywhere, Category="Context Management")
    bool bShowTokenCount = true; // Show token count in UI
};
```

---

## Testing Strategy

### Unit Tests
1. `EstimateTokenCount` returns reasonable values
2. `IsNearContextLimit` triggers at correct threshold
3. `HandleSummarizationResponse` correctly replaces messages
4. Summary message format is valid

### Integration Tests
1. Long conversation triggers automatic summarization
2. Summary is applied and conversation continues
3. Tool results are properly preserved/truncated
4. UI updates correctly during summarization

### Manual Tests
1. Have a long conversation (many tool calls)
2. Verify summarization triggers automatically
3. Verify conversation can continue after summarization
4. Verify important context is preserved

---

## Implementation Priority

### P0 - Must Have
- [ ] Token estimation (`EstimateConversationTokens`)
- [ ] Context limit detection (`IsNearContextLimit`)
- [ ] Basic summarization request
- [ ] Summary integration into history

### P1 - Should Have
- [ ] UI token display
- [ ] Summarization status indicator
- [ ] Configuration options
- [ ] Recent messages preservation

### P2 - Nice to Have
- [ ] Simple mode fallback
- [ ] Summary quality validation
- [ ] Multiple summarization rounds
- [ ] Summary caching

---

## Notes

1. **Token Estimation**: Using ~4 chars per token is a rough estimate. For more accuracy, could use tiktoken or similar, but adds complexity.

2. **Summary Size**: The summary itself takes tokens. Copilot validates that summary fits in budget.

3. **Tool Results**: Large tool results are the main context consumers. Truncating old tool results is often sufficient.

4. **Non-Streaming**: Use non-streaming for summarization to get complete response before processing.

5. **Error Handling**: If summarization fails, should fall back to simple truncation rather than blocking conversation.
