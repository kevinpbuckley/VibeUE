# VibeUE Vision Support Design

## Overview

Add screenshot/image analysis capability to VibeUE by passing images directly in the message context to the same LLM model. This follows the VS Code Copilot pattern - no model switching, just include the image in the conversation.

## Architecture Decision

**Same model, image in context** - Just like VS Code Copilot, we pass images to the same model the user has selected. If the model supports vision, it works. If not, the request fails or image is skipped.

This keeps things simple:
- No `VISION_MODEL` config
- No routing logic
- User picks a vision-capable model if they want vision features

## Components to Modify

### 1. Worker API (`VibeUE-Worker-API`)

**File: `src/openai-compatible.ts`**

Update `ChatMessage` interface to support content arrays (OpenAI/OpenRouter vision format):

```typescript
// Content can be string OR array of content parts (for vision)
export type ContentPart = 
  | { type: 'text'; text: string }
  | { type: 'image_url'; image_url: { url: string; detail?: 'auto' | 'low' | 'high' } };

export interface ChatMessage {
  role: 'system' | 'user' | 'assistant' | 'tool';
  content?: string | ContentPart[] | null;  // Updated to support arrays
  name?: string | null;
  tool_calls?: any[];
  tool_call_id?: string | null;
}
```

#### Backwards Compatibility

**Critical: Existing clients must continue to work unchanged.**

The TypeScript union type `string | ContentPart[] | null` is inherently backwards compatible:
- Existing clients send `"content": "Hello"` (string) → Works as before
- New clients send `"content": [{"type": "text", "text": "Hello"}, {"type": "image_url", ...}]` → New vision format

**No transformation needed** - the worker passes `content` through to OpenRouter as-is. OpenRouter already accepts both formats per the OpenAI API spec.

**Validation (optional but recommended):**
```typescript
// In request handler, validate content format
function validateContent(content: unknown): content is string | ContentPart[] | null {
  if (content === null || content === undefined) return true;
  if (typeof content === 'string') return true;
  if (Array.isArray(content)) {
    return content.every(part => 
      (part.type === 'text' && typeof part.text === 'string') ||
      (part.type === 'image_url' && typeof part.image_url?.url === 'string')
    );
  }
  return false;
}
```

**No breaking changes:**
- Existing request format: ✅ Works
- Existing response format: ✅ Unchanged
- Rate limiting: ✅ Same (token-based)
- Authentication: ✅ Same

### 2. VibeUE Plugin (`FPS57/Plugins/VibeUE`)

#### A. New Tool: `capture_viewport`

**Location:** `Source/VibeUE/Private/Tools/VisionTools.cpp` (new file)

```cpp
// MCP Tool definition
{
  "name": "capture_viewport",
  "description": "Capture a screenshot of the current editor viewport and add it to the conversation context for visual analysis",
  "inputSchema": {
    "type": "object",
    "properties": {
      "prompt": {
        "type": "string",
        "description": "Question or analysis request about the screenshot"
      },
      "resolution_scale": {
        "type": "number",
        "description": "Screenshot resolution multiplier (1-4, default 1)",
        "default": 1
      }
    },
    "required": ["prompt"]
  }
}
```

**Implementation Flow:**
1. Execute `HighResShot {scale}` console command
2. Wait for screenshot file to appear in `Saved/Screenshots/WindowsEditor/`
3. Read PNG file to memory
4. Encode as base64
5. Return structured result that includes the image data

#### B. Update LLM Clients to Handle Images

**Files to modify:**
- `Source/VibeUE/Private/Chat/OpenRouterClient.cpp`
- `Source/VibeUE/Private/Chat/VibeUEAPIClient.cpp`
- `Source/VibeUE/Public/Chat/ChatTypes.h`

**ChatTypes.h - Add content part types:**

```cpp
// Content part for multimodal messages
USTRUCT()
struct FContentPart
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Type;  // "text" or "image_url"
    
    UPROPERTY()
    FString Text;  // For type="text"
    
    UPROPERTY()
    FString ImageUrl;  // For type="image_url" (base64 data URL or http URL)
    
    UPROPERTY()
    FString ImageDetail;  // "auto", "low", or "high"
};

// Update FChatMessage
USTRUCT()
struct FChatMessage
{
    // ... existing fields ...
    
    // For multimodal: if ContentParts is non-empty, use it instead of Content
    UPROPERTY()
    TArray<FContentPart> ContentParts;
    
    bool IsMultimodal() const { return ContentParts.Num() > 0; }
};
```

**OpenRouterClient.cpp / VibeUEAPIClient.cpp - Update message serialization:**

```cpp
// In BuildHttpRequest, when serializing messages:
if (Message.IsMultimodal())
{
    // Serialize as content array
    TArray<TSharedPtr<FJsonValue>> ContentArray;
    for (const FContentPart& Part : Message.ContentParts)
    {
        TSharedPtr<FJsonObject> PartObj = MakeShared<FJsonObject>();
        PartObj->SetStringField(TEXT("type"), Part.Type);
        
        if (Part.Type == TEXT("text"))
        {
            PartObj->SetStringField(TEXT("text"), Part.Text);
        }
        else if (Part.Type == TEXT("image_url"))
        {
            TSharedPtr<FJsonObject> ImageUrlObj = MakeShared<FJsonObject>();
            ImageUrlObj->SetStringField(TEXT("url"), Part.ImageUrl);
            if (!Part.ImageDetail.IsEmpty())
            {
                ImageUrlObj->SetStringField(TEXT("detail"), Part.ImageDetail);
            }
            PartObj->SetObjectField(TEXT("image_url"), ImageUrlObj);
        }
        ContentArray.Add(MakeShared<FJsonValueObject>(PartObj));
    }
    MsgObj->SetArrayField(TEXT("content"), ContentArray);
}
else
{
    // Existing string content serialization
    MsgObj->SetStringField(TEXT("content"), Message.Content);
}
```

#### C. Integration: Tool Result → Multimodal Message

When `capture_viewport` tool returns, the chat session needs to:

1. Create a user message with the image + prompt as content parts
2. Send to LLM for analysis
3. Return LLM response

**Option A: Tool returns image, AI adds to context automatically**
- Tool returns: `{ "image_base64": "...", "prompt": "is the spotlight too big?" }`
- Chat session intercepts, creates multimodal message, sends to LLM

**Option B: Tool makes the LLM call internally**
- Tool captures screenshot
- Tool calls OpenRouter/VibeUE API directly with multimodal message
- Tool returns text analysis

**Recommendation: Option B** - Simpler, tool is self-contained, no need to modify chat session flow.

## File Changes Summary

| File | Change |
|------|--------|
| `VibeUE-Worker-API/src/openai-compatible.ts` | Add `ContentPart` type, update `ChatMessage.content` to accept arrays |
| `VibeUE/Source/VibeUE/Public/Chat/ChatTypes.h` | Add `FContentPart`, update `FChatMessage` |
| `VibeUE/Source/VibeUE/Private/Chat/OpenRouterClient.cpp` | Update message serialization for content arrays |
| `VibeUE/Source/VibeUE/Private/Chat/VibeUEAPIClient.cpp` | Update message serialization for content arrays |
| `VibeUE/Source/VibeUE/Private/Tools/VisionTools.cpp` | New file - `capture_viewport` tool implementation |
| `VibeUE/Source/VibeUE/Private/Tools/EditorTools.cpp` | Register new vision tool |

## Example Usage

```
User: "Is the purple spotlight too big for the castle?"

AI: I'll capture a screenshot to analyze the lighting.
    [calls capture_viewport(prompt="Analyze the purple spotlight. Is it too big relative to the castle?")]

Tool Result: "The purple spotlight appears oversized. The cone angle (60°) and 
attenuation radius (10,000 units) create a light that covers approximately 3x the 
castle's footprint. Consider reducing the outer cone angle to 25-30° and attenuation 
radius to 3,000-4,000 units for a more focused dramatic effect."

AI: Based on my analysis of the screenshot, the purple spotlight is indeed too large...
```

## Model Requirements

For vision to work, the selected model must support multimodal input:

| Provider | Vision-Capable Models |
|----------|----------------------|
| OpenRouter | `google/gemini-2.5-flash`, `google/gemini-2.0-flash`, `anthropic/claude-3.5-sonnet`, `openai/gpt-4o` |
| VibeUE API | Depends on `OPENAI_MODEL` config - if using OpenRouter backend, same as above |

If user selects a non-vision model, the tool should gracefully fail with a message like:
> "The current model does not support image analysis. Please select a vision-capable model like Gemini 2.5 Flash or GPT-4o."

## Implementation Order

1. **Worker API** - Update ChatMessage interface (5 min)
2. **ChatTypes.h** - Add FContentPart struct (10 min)
3. **LLM Clients** - Update message serialization (30 min)
4. **VisionTools.cpp** - Implement capture_viewport (1-2 hours)
5. **Testing** - End-to-end with Gemini Flash (30 min)

## Open Questions

1. **Screenshot location**: Use `HighResShot` console command or Unreal's viewport capture API directly?
2. **Image size**: Base64 images can be large. Should we compress/resize before sending?
3. **Rate limiting**: Vision requests use more tokens. Any special handling needed?

## References

- [OpenRouter Vision Docs](https://openrouter.ai/docs/requests) - Content array format
- [Gemini Vision Support](https://ai.google.dev/gemini-api/docs/models#gemini-2.0-flash) - Confirms image input support
- VS Code Copilot `image.tsx` - Reference implementation for same-model vision approach
