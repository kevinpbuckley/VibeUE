// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Speech/ISpeechProvider.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpeech, Log, All);

/**
 * Manages speech-to-text sessions and provider registry
 * Central service for voice input functionality
 *
 * This is a standalone manager (not FServiceBase) as it doesn't need
 * the full service infrastructure. It's a simpler provider registry
 * and session coordinator.
 */
class VIBEUE_API FSpeechToTextService : public TSharedFromThis<FSpeechToTextService>
{
public:
	FSpeechToTextService();
	~FSpeechToTextService();

	/** Initialize service and load settings */
	void Initialize();

	/** Shutdown service and cleanup resources */
	void Shutdown();

	/**
	 * Register a speech provider
	 * @param Id - Unique identifier (e.g., "elevenlabs", "azure")
	 * @param Provider - Provider implementation
	 */
	void RegisterProvider(const FString& Id, TSharedPtr<ISpeechProvider> Provider);

	/**
	 * Unregister a speech provider
	 * @param Id - Provider identifier to remove
	 */
	void UnregisterProvider(const FString& Id);

	/**
	 * Get list of available provider IDs
	 * @return Array of registered provider IDs
	 */
	TArray<FString> GetAvailableProviders() const;

	/**
	 * Set the active provider
	 * @param Id - Provider identifier to activate
	 */
	void SetActiveProvider(const FString& Id);

	/**
	 * Get the active provider instance
	 * @return Current provider, or nullptr if none active
	 */
	TSharedPtr<ISpeechProvider> GetActiveProvider() const;

	/**
	 * Start a speech-to-text session with default or custom options
	 * @param Options - Session configuration
	 */
	void StartSession(const FSpeechSessionOptions& Options = FSpeechSessionOptions());

	/**
	 * Stop the current session
	 */
	void StopSession();

	/**
	 * Check if a session is currently active
	 * @return true if session is running
	 */
	bool IsSessionActive() const;

	/**
	 * Get current session status
	 * @return Current status enum value
	 */
	ESpeechToTextStatus GetStatus() const;

	/**
	 * Check if speech is available (any provider configured and ready)
	 * @return true if at least one provider is available
	 */
	bool HasSpeechProvider() const;

	// Event delegates (forwarded from active provider)

	/** Fired when speech status changes */
	FOnSpeechStatusChanged OnStatusChanged;

	/** Fired when partial transcript is available */
	FOnPartialTranscript OnPartialTranscript;

	/** Fired when final transcript is ready */
	FOnFinalTranscript OnFinalTranscript;

	/** Fired when speech error occurs */
	FOnSpeechError OnError;

	// Configuration static methods

	/** Get auto-submit timeout from config (seconds) */
	static float GetAutoSubmitTimeoutFromConfig();

	/** Save auto-submit timeout to config */
	static void SaveAutoSubmitTimeoutToConfig(float Timeout);

	/** Get default language from config */
	static FString GetDefaultLanguageFromConfig();

	/** Save default language to config */
	static void SaveDefaultLanguageToConfig(const FString& Language);

	/** Get voice input enabled state from config */
	static bool GetVoiceInputEnabledFromConfig();

	/** Save voice input enabled state to config */
	static void SaveVoiceInputEnabledToConfig(bool bEnabled);

private:
	/** Registered providers (e.g., "elevenlabs", "azure") */
	TMap<FString, TSharedPtr<ISpeechProvider>> Providers;

	/** Active provider ID */
	FString ActiveProviderId;

	/** Timer handle for auto-submit timeout */
	FTimerHandle AutoSubmitTimerHandle;

	/** Auto-submit timeout (seconds, 0 = disabled) */
	float AutoSubmitTimeout = 3.0f;

	/** Forward provider events to service delegates */
	void OnProviderStatusChanged(ESpeechToTextStatus Status, const FString& Text);
	void OnProviderPartialTranscript(const FString& Text);
	void OnProviderFinalTranscript(const FString& Text);
	void OnProviderError(const FString& Error);

	/** Auto-submit timer callback */
	void OnAutoSubmitTimeout();

	/** Load settings from config */
	void LoadSettings();
};
