// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Speech/SpeechTypes.h"

/**
 * Abstract interface for speech-to-text providers
 * Enables pluggable providers (ElevenLabs, Azure, Google, etc.)
 *
 * Implementations handle:
 * - Network connection to speech API
 * - Audio streaming
 * - Transcription result parsing
 * - Error handling and recovery
 */
class VIBEUE_API ISpeechProvider
{
public:
	virtual ~ISpeechProvider() = default;

	/**
	 * Get provider display name (e.g., "ElevenLabs", "Azure Speech")
	 * @return User-friendly provider name
	 */
	virtual FString GetDisplayName() const = 0;

	/**
	 * Check if provider is available and configured
	 * Typically checks for API key and network connectivity
	 * @return true if provider is ready to use
	 */
	virtual bool IsAvailable() const = 0;

	/**
	 * Start a speech-to-text session
	 * Initializes connection, starts audio capture, begins transcription
	 * @param Options - Session configuration (language, sample rate, VAD settings)
	 */
	virtual void StartSession(const FSpeechSessionOptions& Options) = 0;

	/**
	 * Stop the current session
	 * Closes connection, stops audio capture, cleans up resources
	 */
	virtual void StopSession() = 0;

	/**
	 * Check if a session is currently active
	 * @return true if session is running (Started, Recognizing, or Recognized state)
	 */
	virtual bool IsSessionActive() const = 0;

	/**
	 * Get current session status
	 * @return Current status enum value
	 */
	virtual ESpeechToTextStatus GetStatus() const = 0;

	// Event delegates
	// Implementations broadcast these events when state changes occur

	/** Fired when session status changes (e.g., Idle → Connecting → Started) */
	FOnSpeechStatusChanged OnStatusChanged;

	/** Fired when partial transcript is available (real-time updates during speech) */
	FOnPartialTranscript OnPartialTranscript;

	/** Fired when final transcript is ready (after silence or manual commit) */
	FOnFinalTranscript OnFinalTranscript;

	/** Fired when an error occurs (connection failure, auth error, etc.) */
	FOnSpeechError OnError;
};
