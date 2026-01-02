// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Speech/ISpeechProvider.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// Forward declare
class FAudioCaptureRunnable;

/**
 * ElevenLabs Speech-to-Text API provider
 * Uses simple POST API for audio transcription
 *
 * Architecture:
 * - Microphone → AudioCaptureRunnable (background thread)
 * - Accumulate audio in memory while recording
 * - On stop: Convert to WAV → POST to API → Parse response
 *
 * Thread Safety:
 * - Audio capture on background thread with lock
 * - HTTP POST on game thread
 */
class VIBEUE_API FElevenLabsSpeechProvider : public ISpeechProvider, public TSharedFromThis<FElevenLabsSpeechProvider>
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

	/**
	 * Set ElevenLabs API key
	 * @param ApiKey - ElevenLabs API key (get from https://elevenlabs.io/app/settings/api-keys)
	 */
	void SetApiKey(const FString& ApiKey);

	/**
	 * Get ElevenLabs API key from config
	 * @return Stored API key, or empty string if not set
	 */
	static FString GetApiKeyFromConfig();

	/**
	 * Save ElevenLabs API key to config
	 * @param ApiKey - API key to save
	 */
	static void SaveApiKeyToConfig(const FString& ApiKey);

private:
	/** Current status */
	ESpeechToTextStatus CurrentStatus;

	/** API key */
	FString ApiKey;

	/** Current session options */
	FSpeechSessionOptions SessionOptions;

	/** Audio capture thread */
	TSharedPtr<FAudioCaptureRunnable> AudioCaptureThread;

	/** Accumulated audio data for send on stop */
	TArray<float> AccumulatedAudio;

	/** Critical section for thread safety (status and audio access) */
	mutable FCriticalSection StatusLock;
	FCriticalSection AudioLock;

	// Audio processing

	/**
	 * Called when audio data is available from capture thread
	 * Runs on background thread - accumulates audio for sending on stop
	 */
	void OnAudioDataAvailable(const TArray<float>& AudioData);

	// HTTP handling

	/**
	 * Send accumulated audio to ElevenLabs API via POST
	 * Converts to WAV format and posts to /v1/speech-to-text
	 * @param Json - Parsed JSON object
	 */
	void HandleCommittedTranscript(const TSharedPtr<FJsonObject>& Json);

	/**
	 * Parse and handle error messages
	 * @param Json - Parsed JSON object
	 */
	void HandleError(const TSharedPtr<FJsonObject>& Json);

	// Audio encoding

	/**
	 * Send audio chunk to ElevenLabs API
	 * @param AudioData - Float audio buffer
	 * @param bCommit - true to manually commit transcription
	 */
	void SendAudioChunk(const TArray<float>& AudioData, bool bCommit = false);

	/**
	 * Convert float audio samples to WAV format (PCM16)
	 * @param FloatData - Float samples (range -1.0 to 1.0)
	 * @param SampleRate - Audio sample rate (default 16000)
	 * @return WAV file bytes with proper RIFF header
	 */
	TArray<uint8> ConvertToWAV(const TArray<float>& FloatData, int32 SampleRate = 16000);

	// Status management

	/**
	 * Set status with thread safety
	 * @param NewStatus - New status value
	 */
	void SetStatus(ESpeechToTextStatus NewStatus);

	// HTTP response handling

	/**
	 * Handle transcription response from POST request
	 * @param Request - HTTP request object
	 * @param Response - HTTP response object
	 * @param bSuccess - Whether request succeeded
	 */
	void OnTranscriptionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
};
