# VibeUE AI Chat - Speech-to-Text Design Document

**Version:** 1.0  
**Date:** January 1, 2026  
**Author:** Design Team  
**Status:** Draft

---

## Executive Summary

This document outlines the design for implementing Speech-to-Text (STT) functionality in the VibeUE AI Chat for Unreal Engine. The implementation will follow architecture patterns established by VS Code and GitHub Copilot Chat, adapted for Unreal Engine's C++ environment, using ElevenLabs' Scribe v2 Realtime API for speech recognition.

---

## 1. Research Summary

### 1.1 VS Code Speech Architecture

VS Code implements speech functionality through a **provider-based, event-driven architecture**:

#### Core Components

| Component | Purpose |
|-----------|---------|
| `ISpeechService` | Core service managing speech operations |
| `ISpeechProvider` | Interface for pluggable speech providers |
| `ISpeechToTextSession` | Session object representing an active STT session |
| `IVoiceChatService` | Higher-level service for chat-specific voice features |

#### State Machine (SpeechToTextStatus)

```
Started → Recognizing → Recognized → Stopped
                ↓
              Error
```

| State | Description |
|-------|-------------|
| `Started` | Session initialized, listening for audio |
| `Recognizing` | Audio detected, transcription in progress (partial text) |
| `Recognized` | Final transcription for current utterance |
| `Stopped` | Session ended normally |
| `Error` | Session ended with error |

#### Key Design Patterns from VS Code

1. **Provider Registration**: Extensions register speech providers dynamically
2. **Event-Driven**: All state changes communicated via events
3. **Context Keys**: UI state managed through context keys (e.g., `speechToTextInProgress`)
4. **Session Management**: Sessions are cancellable, disposable objects
5. **Telemetry**: Comprehensive session tracking (duration, recognized content, errors)
6. **Voice Chat Service**: Specialized layer that transforms spoken commands ("at workspace slash fix") into chat syntax (`@workspace /fix`)

#### Configuration Options

- `accessibility.voice.speechTimeout` - Auto-submit timeout after speech stops
- `accessibility.voice.speechLanguage` - Target language for recognition
- Multi-language support with 25+ language codes

---

### 1.2 ElevenLabs Scribe v2 Realtime API

#### API Overview

| Feature | Value |
|---------|-------|
| **Endpoint** | `wss://api.elevenlabs.io/v1/speech-to-text/realtime` |
| **Protocol** | WebSocket |
| **Model** | `scribe_v2_realtime` |
| **Latency** | ~150ms |
| **Languages** | 90+ languages with auto-detection |
| **Authentication** | API Key (`xi-api-key` header) or Token (query param) |

#### Audio Formats Supported

| Format | Sample Rate | Description |
|--------|-------------|-------------|
| `pcm_16000` | 16kHz | Default, recommended for speech |
| `pcm_22050` | 22.05kHz | Higher quality |
| `pcm_44100` | 44.1kHz | CD quality |
| `pcm_48000` | 48kHz | Professional quality |
| `ulaw_8000` | 8kHz | Telephony format |

#### WebSocket Message Types

**Outbound (Client → Server):**

```json
{
  "message_type": "input_audio_chunk",
  "audio_base_64": "<base64_encoded_audio>",
  "commit": false,
  "sample_rate": 16000,
  "previous_text": "Optional context for model"
}
```

**Inbound (Server → Client):**

| Message Type | Description |
|--------------|-------------|
| `session_started` | Connection established, includes config |
| `partial_transcript` | Interim transcription (updating) |
| `committed_transcript` | Final transcription for utterance |
| `committed_transcript_with_timestamps` | Final with word-level timing |
| `scribe_error` | Transcription error |
| `scribe_auth_error` | Authentication failure |
| `scribe_rate_limited_error` | Rate limit exceeded |

#### Commit Strategies

| Strategy | Description |
|----------|-------------|
| `manual` | Client explicitly sends `commit: true` |
| `vad` | Voice Activity Detection auto-commits on silence |

#### VAD Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `vad_silence_threshold_secs` | 1.5 | Silence duration to trigger commit |
| `vad_threshold` | 0.4 | Voice detection sensitivity (0-1) |
| `min_speech_duration_ms` | 250 | Minimum speech before recognition |
| `min_silence_duration_ms` | 2500 | Minimum silence to confirm end |

---

## 2. Proposed Architecture

### 2.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     VibeUE AI Chat UI                           │
│  ┌──────────────────┐  ┌──────────────────┐                    │
│  │ Chat Input Widget │  │ Microphone Button │                    │
│  └────────┬─────────┘  └────────┬─────────┘                    │
│           │                      │                              │
│           │                      ▼                              │
│           │         ┌────────────────────────┐                  │
│           │         │  FSpeechToTextService  │◄─────────────────┤
│           │         │  (Session Management)  │                  │
│           │         └────────────┬───────────┘                  │
│           │                      │                              │
│           │                      ▼                              │
│           │         ┌────────────────────────┐                  │
│           │         │  ISpeechProvider       │                  │
│           │         │  (Provider Interface)  │                  │
│           │         └────────────┬───────────┘                  │
│           │                      │                              │
│           │                      ▼                              │
│           │         ┌────────────────────────┐                  │
│           │         │FElevenLabsSpeechProvider│                 │
│           │         │(WebSocket Connection)  │                  │
│           │         └────────────┬───────────┘                  │
│           │                      │                              │
│           ▼                      ▼                              │
│  ┌─────────────────────────────────────────┐                   │
│  │            FChatSession                  │                   │
│  │   (Existing - receives transcribed text) │                   │
│  └─────────────────────────────────────────┘                   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ WebSocket
                              ▼
            ┌─────────────────────────────────┐
            │    ElevenLabs Scribe v2 API     │
            │ wss://api.elevenlabs.io/v1/...  │
            └─────────────────────────────────┘
```

### 2.2 Class Design

#### 2.2.1 Speech Status Enum

```cpp
UENUM()
enum class ESpeechToTextStatus : uint8
{
    Idle,           // Not active
    Connecting,     // WebSocket connecting
    Started,        // Session started, listening
    Recognizing,    // Partial transcription available
    Recognized,     // Final transcription ready
    Stopped,        // Session ended normally
    Error           // Session ended with error
};
```

#### 2.2.2 Speech Events

```cpp
// Delegate for speech status changes
DECLARE_DELEGATE_TwoParams(FOnSpeechStatusChanged, 
    ESpeechToTextStatus /* Status */, 
    const FString& /* Text */);

// Delegate for partial transcription updates
DECLARE_DELEGATE_OneParam(FOnPartialTranscript, 
    const FString& /* PartialText */);

// Delegate for final transcription
DECLARE_DELEGATE_OneParam(FOnFinalTranscript, 
    const FString& /* FinalText */);

// Delegate for speech errors
DECLARE_DELEGATE_OneParam(FOnSpeechError, 
    const FString& /* ErrorMessage */);
```

#### 2.2.3 Speech Provider Interface

```cpp
class ISpeechProvider
{
public:
    virtual ~ISpeechProvider() = default;
    
    /** Get provider display name */
    virtual FString GetDisplayName() const = 0;
    
    /** Check if provider is available/configured */
    virtual bool IsAvailable() const = 0;
    
    /** Start speech-to-text session */
    virtual void StartSession(const FSpeechSessionOptions& Options) = 0;
    
    /** Stop current session */
    virtual void StopSession() = 0;
    
    /** Check if session is active */
    virtual bool IsSessionActive() const = 0;
    
    /** Get current status */
    virtual ESpeechToTextStatus GetStatus() const = 0;
    
    // Event delegates
    FOnSpeechStatusChanged OnStatusChanged;
    FOnPartialTranscript OnPartialTranscript;
    FOnFinalTranscript OnFinalTranscript;
    FOnSpeechError OnError;
};
```

#### 2.2.4 Session Options

```cpp
struct FSpeechSessionOptions
{
    /** Target language (ISO 639-1/639-3 code, empty for auto-detect) */
    FString LanguageCode;
    
    /** Audio sample rate */
    int32 SampleRate = 16000;
    
    /** Commit strategy: "manual" or "vad" */
    FString CommitStrategy = TEXT("vad");
    
    /** VAD silence threshold (seconds) */
    float VADSilenceThreshold = 1.5f;
    
    /** VAD voice detection threshold (0-1) */
    float VADThreshold = 0.4f;
    
    /** Include word-level timestamps */
    bool bIncludeTimestamps = false;
    
    /** Auto-detect language */
    bool bAutoDetectLanguage = true;
    
    /** Previous conversation context for model */
    FString PreviousContext;
};
```

#### 2.2.5 ElevenLabs Provider Class

```cpp
class FElevenLabsSpeechProvider : public ISpeechProvider
{
public:
    FElevenLabsSpeechProvider();
    virtual ~FElevenLabsSpeechProvider();
    
    // ISpeechProvider interface
    virtual FString GetDisplayName() const override;
    virtual bool IsAvailable() const override;
    virtual void StartSession(const FSpeechSessionOptions& Options) override;
    virtual void StopSession() override;
    virtual bool IsSessionActive() const override;
    virtual ESpeechToTextStatus GetStatus() const override;
    
    /** Set API key */
    void SetApiKey(const FString& ApiKey);
    
    /** Get/Save API key from config */
    static FString GetApiKeyFromConfig();
    static void SaveApiKeyToConfig(const FString& ApiKey);
    
private:
    /** WebSocket connection */
    TSharedPtr<IWebSocket> WebSocket;
    
    /** Current status */
    ESpeechToTextStatus CurrentStatus = ESpeechToTextStatus::Idle;
    
    /** API key */
    FString ApiKey;
    
    /** Audio capture component */
    TSharedPtr<FAudioCapture> AudioCapture;
    
    /** WebSocket event handlers */
    void OnWebSocketConnected();
    void OnWebSocketConnectionError(const FString& Error);
    void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void OnWebSocketMessage(const FString& Message);
    
    /** Audio capture callback */
    void OnAudioCaptured(const float* AudioData, int32 NumFrames);
    
    /** Parse incoming WebSocket messages */
    void HandleSessionStarted(const TSharedPtr<FJsonObject>& Json);
    void HandlePartialTranscript(const TSharedPtr<FJsonObject>& Json);
    void HandleCommittedTranscript(const TSharedPtr<FJsonObject>& Json);
    void HandleError(const TSharedPtr<FJsonObject>& Json);
    
    /** Send audio chunk to API */
    void SendAudioChunk(const TArray<uint8>& AudioData, bool bCommit = false);
    
    /** Convert float audio to PCM16 and base64 */
    FString EncodeAudioToBase64(const float* AudioData, int32 NumFrames);
};
```

#### 2.2.6 Speech Service (Session Manager)

```cpp
class FSpeechToTextService : public TSharedFromThis<FSpeechToTextService>
{
public:
    FSpeechToTextService();
    ~FSpeechToTextService();
    
    /** Initialize service */
    void Initialize();
    
    /** Shutdown service */
    void Shutdown();
    
    /** Register a speech provider */
    void RegisterProvider(const FString& Id, TSharedPtr<ISpeechProvider> Provider);
    
    /** Unregister a speech provider */
    void UnregisterProvider(const FString& Id);
    
    /** Get available providers */
    TArray<FString> GetAvailableProviders() const;
    
    /** Set active provider */
    void SetActiveProvider(const FString& Id);
    
    /** Get active provider */
    TSharedPtr<ISpeechProvider> GetActiveProvider() const;
    
    /** Start speech session */
    void StartSession(const FSpeechSessionOptions& Options = FSpeechSessionOptions());
    
    /** Stop current session */
    void StopSession();
    
    /** Check if session is active */
    bool IsSessionActive() const;
    
    /** Get current status */
    ESpeechToTextStatus GetStatus() const;
    
    /** Check if speech is available */
    bool HasSpeechProvider() const;
    
    // Event delegates (forwarded from active provider)
    FOnSpeechStatusChanged OnStatusChanged;
    FOnPartialTranscript OnPartialTranscript;
    FOnFinalTranscript OnFinalTranscript;
    FOnSpeechError OnError;
    
private:
    /** Registered providers */
    TMap<FString, TSharedPtr<ISpeechProvider>> Providers;
    
    /** Active provider ID */
    FString ActiveProviderId;
    
    /** Session timeout timer */
    FTimerHandle TimeoutTimerHandle;
    
    /** Auto-submit timeout (from config) */
    float AutoSubmitTimeout = 3.0f;
    
    /** Timeout callback */
    void OnSessionTimeout();
    
    /** Load settings from config */
    void LoadSettings();
};
```

---

## 3. Integration Points

### 3.1 FChatSession Integration

Extend `FChatSession` with speech support:

```cpp
// New delegates in ChatSession.h
DECLARE_DELEGATE_OneParam(FOnVoiceInputStarted, bool /* bSuccess */);
DECLARE_DELEGATE_TwoParams(FOnVoiceInputText, const FString& /* Text */, bool /* bIsFinal */);
DECLARE_DELEGATE(FOnVoiceInputStopped);

// New methods in FChatSession
/** Start voice input */
void StartVoiceInput();

/** Stop voice input */
void StopVoiceInput();

/** Check if voice input is active */
bool IsVoiceInputActive() const;

/** Check if voice input is available */
bool IsVoiceInputAvailable() const;

// New delegates
FOnVoiceInputStarted OnVoiceInputStarted;
FOnVoiceInputText OnVoiceInputText;
FOnVoiceInputStopped OnVoiceInputStopped;
```

### 3.2 UI Integration

The AI Chat UI requires two main UI additions:

1. **Settings Panel** (Section 3.4): Configuration UI for API keys, provider selection, and options
2. **Microphone Button** (Section 3.5): Push-to-talk button in chat input area

#### Visual Feedback Requirements

- Pulsing/recording animation when listening
- Real-time partial transcription in input field
- Clear status indicators (connecting, listening, processing, error)
- Disabled state when voice input is not configured

### 3.3 Configuration

New config options in `VibeUESettings.ini`:

```ini
[VoiceInput]
; ElevenLabs API key
ElevenLabsApiKey=

; Auto-submit timeout (seconds, 0 = disabled)
AutoSubmitTimeout=3.0

; Default language (empty = auto-detect)
DefaultLanguage=

; Commit strategy: "vad" or "manual"
CommitStrategy=vad

; VAD silence threshold (seconds)
VADSilenceThreshold=1.5

; Enable voice input feature
bEnableVoiceInput=true
```

### 3.4 Settings UI Design

The VibeUE Settings panel should include a **Voice Input** section for configuring STT:

#### Settings Panel Layout

```
┌─────────────────────────────────────────────────────────────┐
│  VibeUE Settings                                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ▼ Voice Input                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ □ Enable Voice Input                                    ││
│  │                                                         ││
│  │ Provider:  [ElevenLabs        ▼]                        ││
│  │                                                         ││
│  │ API Key:   [••••••••••••••••••••] [Show] [Test]         ││
│  │            ℹ️ Get your API key at elevenlabs.io          ││
│  │                                                         ││
│  │ Language:  [Auto-detect       ▼]                        ││
│  │                                                         ││
│  │ ▶ Advanced Settings                                     ││
│  │   Auto-submit timeout: [3.0  ] seconds                  ││
│  │   VAD silence threshold: [1.5] seconds                  ││
│  │   VAD sensitivity: [0.4] (0-1)                          ││
│  └─────────────────────────────────────────────────────────┘│
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### Settings UI Components

| Component | Type | Description |
|-----------|------|-------------|
| Enable Voice Input | Checkbox | Master toggle for STT feature |
| Provider | Dropdown | Select speech provider (ElevenLabs, future: Azure, etc.) |
| API Key | Password Field | Secure input for provider API key |
| Show Button | Toggle | Reveal/hide API key text |
| Test Button | Button | Validate API key with provider |
| Language | Dropdown | Target language or "Auto-detect" |
| Advanced Settings | Collapsible | Hidden by default, shows VAD options |

#### API Key Validation Flow

```
User enters API key → Click "Test" → Show spinner
                                         │
                    ┌────────────────────┴────────────────────┐
                    ▼                                          ▼
              Success                                       Failure
         ✅ "API key valid"                          ❌ "Invalid API key"
         Enable Save button                          Show error details
```

#### Provider-Specific Settings

When provider changes, show relevant API key help:

| Provider | Help Text | Link |
|----------|-----------|------|
| ElevenLabs | "Get your API key at elevenlabs.io" | https://elevenlabs.io/app/settings/api-keys |
| Azure (future) | "Create a Speech resource in Azure Portal" | Azure docs link |
| Google (future) | "Enable Speech-to-Text API in Google Cloud" | GCP docs link |

### 3.5 Chat Window Microphone Button

Voice input is activated via a **push-to-talk button** in the chat input area:

#### Button States

```
┌─────────────────────────────────────────────────────────────┐
│  Chat Input Area                                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────┐  ┌─────┐  │
│  │ Type your message...                        │  │ 🎤  │  │
│  └─────────────────────────────────────────────┘  └─────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

| State | Icon | Appearance | Tooltip |
|-------|------|------------|---------|
| Idle | 🎤 | Default | "Click to start voice input" |
| Unavailable | 🎤 | Grayed out | "Voice input not configured" |
| Connecting | 🎤 | Pulsing | "Connecting..." |
| Listening | 🎤 | Red/Recording | "Listening... Click to stop" |
| Processing | 🎤 | Spinning | "Processing speech..." |
| Error | 🎤 | Red X | "Error: [message]" |

#### Interaction Flow

1. **Click to Start**: User clicks mic button to begin recording
2. **Visual Feedback**: Button changes to "listening" state, input field shows "Listening..."
3. **Partial Text**: As user speaks, partial transcription appears in input field
4. **Auto-Stop**: VAD detects silence and auto-commits transcription
5. **Manual Stop**: User can click again to stop early
6. **Text Ready**: Final transcription in input field, user can edit or send

#### Input Field During Voice Input

```
┌─────────────────────────────────────────────────────────────┐
│  ┌─────────────────────────────────────────────┐  ┌─────┐  │
│  │ 🔴 Create a blueprint for the player...     │  │ ⏹️  │  │
│  └─────────────────────────────────────────────┘  └─────┘  │
│  ↑ Partial transcription updates in real-time              │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Audio Capture in Unreal Engine

### 4.1 Platform Considerations

| Platform | Audio Capture Method |
|----------|---------------------|
| Windows | Windows Audio Session API (WASAPI) |
| macOS | Core Audio |
| Linux | PulseAudio / ALSA |
| Editor | Unreal's Audio Capture component |

### 4.2 Implementation Options

**Option A: UAudioCapture Component (Recommended)**
- Use Unreal's built-in `UAudioCapture` from the AudioCapture plugin
- Cross-platform support
- Integrates with Unreal's audio system
- May have latency considerations

**Option B: Platform-Specific APIs**
- Direct OS-level microphone access
- Lower latency possible
- More implementation effort
- Platform-specific code paths

**Recommendation:** Start with `UAudioCapture` for simplicity, optimize later if needed.

### 4.3 Audio Pipeline

```
Microphone → UAudioCapture → Float Buffer → PCM16 Conversion → Base64 Encode → WebSocket
```

Required processing:
1. Capture at 16kHz (or resample)
2. Convert float32 [-1, 1] to int16 [-32768, 32767]
3. Encode to base64
4. Send in chunks (e.g., every 100ms)

---

## 5. Error Handling

### 5.1 Error Categories

| Category | Examples | User Action |
|----------|----------|-------------|
| Configuration | Missing API key | Prompt to enter key in settings |
| Connection | Network timeout, WebSocket fail | Retry with exponential backoff |
| Authentication | Invalid/expired key | Prompt to update key |
| Rate Limiting | Too many requests | Queue and retry, show warning |
| Audio | No microphone, permission denied | Show clear error message |
| Transcription | Model error, timeout | Retry or notify user |

### 5.2 Retry Strategy

- Maximum 3 retry attempts
- Exponential backoff: 1s, 2s, 4s
- User notification after final failure
- Cancel retries if user initiates new action

---

## 6. Security Considerations

### 6.1 API Key Storage

- Store encrypted in platform-specific credential storage
- Never log API keys
- Support token-based auth for additional security

### 6.2 Audio Data

- Audio sent over secure WebSocket (WSS)
- Option to enable zero-retention mode (`enable_logging=false`)
- Clear audio buffers after transmission

### 6.3 Privacy

- Clear microphone status indicator when active
- Respect system microphone permissions
- Option to disable voice input entirely

---

## 7. Future Considerations

### 7.1 Text-to-Speech (TTS)

ElevenLabs also offers TTS APIs. Future enhancements could include:
- Read AI responses aloud
- Use ElevenLabs Flash v2.5 for low latency (~75ms)
- Voice selection and customization

### 7.2 Multi-Language Support

- Language selector in UI
- Per-conversation language setting
- Auto-language detection with confidence display

### 7.3 Alternative Providers

Design allows for pluggable providers:
- Azure Speech Services
- Google Cloud Speech-to-Text
- AWS Transcribe
- Local/offline models (Whisper.cpp)

---

## 8. Implementation Phases

### Phase 1: Core Infrastructure (Week 1-2)
- [ ] Create speech service interface and classes
- [ ] Implement ElevenLabs WebSocket connection
- [ ] Basic audio capture with UAudioCapture
- [ ] Config file integration

### Phase 2: Provider Implementation (Week 2-3)
- [ ] ElevenLabs provider full implementation
- [ ] Audio encoding and transmission
- [ ] Message parsing and event handling
- [ ] Error handling and retry logic

### Phase 3: ChatSession Integration (Week 3-4)
- [ ] Add speech methods to FChatSession
- [ ] Connect speech events to chat input
- [ ] Auto-submit on timeout
- [ ] Cancel handling

### Phase 4: UI Integration (Week 4-5)
- [ ] Settings panel Voice Input section
- [ ] API key input with show/hide toggle
- [ ] API key validation (Test button)
- [ ] Provider dropdown and language selector
- [ ] Microphone button in chat widget
- [ ] Visual feedback (listening animation, partial text)
- [ ] Button state management (idle, listening, processing, error)

### Phase 5: Testing & Polish (Week 5-6)
- [ ] Cross-platform testing (Windows, Mac)
- [ ] Performance optimization
- [ ] Error message refinement
- [ ] Documentation

---

## 9. Success Metrics

| Metric | Target |
|--------|--------|
| Recognition latency | < 500ms from speech end to text |
| Recognition accuracy | > 95% for English |
| Session start time | < 1 second |
| Error recovery rate | > 90% auto-recovery |
| User satisfaction | Qualitative feedback |

---

## 10. Dependencies

| Dependency | Purpose | Status |
|------------|---------|--------|
| AudioCapture Plugin | Microphone access | Built-in |
| WebSockets Plugin | ElevenLabs API | Built-in |
| JsonUtilities | Message parsing | Built-in |
| ElevenLabs API Key | Speech service | User-provided |

---

## 11. References

- [VS Code Speech Service](https://github.com/microsoft/vscode/tree/main/src/vs/workbench/contrib/speech)
- [ElevenLabs STT Realtime API](https://elevenlabs.io/docs/api-reference/speech-to-text/v-1-speech-to-text-realtime)
- [ElevenLabs Models - Scribe v2](https://elevenlabs.io/docs/overview/models#scribe-v2-realtime)
- [Unreal Engine Audio Capture](https://docs.unrealengine.com/5.0/en-US/audio-capture-in-unreal-engine/)

---

## Appendix A: ElevenLabs WebSocket Message Examples

### Session Start

```json
{
  "message_type": "session_started",
  "session_id": "abc123",
  "config": {
    "sample_rate": 16000,
    "audio_format": "pcm_16000",
    "language_code": "en",
    "model_id": "scribe_v2_realtime"
  }
}
```

### Send Audio Chunk

```json
{
  "message_type": "input_audio_chunk",
  "audio_base_64": "UklGRiQAAABXQVZF...",
  "commit": false,
  "sample_rate": 16000
}
```

### Partial Transcript

```json
{
  "message_type": "partial_transcript",
  "text": "Create a blueprint for"
}
```

### Committed Transcript

```json
{
  "message_type": "committed_transcript",
  "text": "Create a blueprint for the player character"
}
```

---

## Appendix B: Supported Languages (Scribe v2)

ElevenLabs Scribe v2 Realtime supports 90+ languages including:

English, Spanish, French, German, Italian, Portuguese, Russian, Japanese, Chinese (Mandarin), Korean, Arabic, Hindi, Dutch, Polish, Swedish, Danish, Finnish, Norwegian, Turkish, Greek, Czech, Romanian, Hungarian, Vietnamese, Thai, Indonesian, Malay, Filipino, Ukrainian, Hebrew, and many more.

Full list available at: https://elevenlabs.io/docs/overview/models#scribe-v2-realtime
