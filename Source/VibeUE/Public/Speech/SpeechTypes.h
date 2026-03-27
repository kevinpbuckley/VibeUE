// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpeechTypes.generated.h"

/**
 * Status enum for speech recognition lifecycle
 * Tracks the current state of a speech-to-text session
 */
UENUM()
enum class ESpeechToTextStatus : uint8
{
	Idle,           // Not active
	Connecting,     // WebSocket connecting to provider
	Started,        // Session started, listening for audio
	Recognizing,    // Partial transcription available
	Recognized,     // Final transcription ready
	Stopped,        // Session ended normally
	Error           // Session ended with error
};

/**
 * Session configuration options for speech-to-text
 * Controls language, audio quality, and voice activity detection
 */
USTRUCT()
struct VIBEUE_API FSpeechSessionOptions
{
	GENERATED_BODY()

	/** Target language (ISO 639-1/639-3 code, empty for auto-detect) */
	UPROPERTY()
	FString LanguageCode;

	/** Audio sample rate (default: 16000) */
	UPROPERTY()
	int32 SampleRate = 16000;

	/** Commit strategy: "manual" or "vad" */
	UPROPERTY()
	FString CommitStrategy = TEXT("vad");

	/** VAD silence threshold (seconds) */
	UPROPERTY()
	float VADSilenceThreshold = 1.5f;

	/** VAD voice detection threshold (0-1) */
	UPROPERTY()
	float VADThreshold = 0.4f;

	/** Include word-level timestamps */
	UPROPERTY()
	bool bIncludeTimestamps = false;

	/** Auto-detect language */
	UPROPERTY()
	bool bAutoDetectLanguage = true;

	/** Previous conversation context for model */
	UPROPERTY()
	FString PreviousContext;
};

/**
 * Delegate called when speech status changes
 * @param Status - New status of the speech session
 * @param Text - Associated text (if any)
 */
DECLARE_DELEGATE_TwoParams(FOnSpeechStatusChanged, ESpeechToTextStatus, const FString&);

/**
 * Delegate called when partial transcription is available
 * Fired frequently during speech for real-time updates
 * @param PartialText - Incomplete transcription that may change
 */
DECLARE_DELEGATE_OneParam(FOnPartialTranscript, const FString&);

/**
 * Delegate called when final transcription is ready
 * Fired when speech segment is complete (silence detected or manual commit)
 * @param FinalText - Complete transcription text
 */
DECLARE_DELEGATE_OneParam(FOnFinalTranscript, const FString&);

/**
 * Delegate called when a speech error occurs
 * @param ErrorMessage - Human-readable error description
 */
DECLARE_DELEGATE_OneParam(FOnSpeechError, const FString&);
