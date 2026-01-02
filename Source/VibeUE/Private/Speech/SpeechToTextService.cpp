// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Speech/SpeechToTextService.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogSpeech);

FSpeechToTextService::FSpeechToTextService()
	: AutoSubmitTimeout(3.0f)
{
}

FSpeechToTextService::~FSpeechToTextService()
{
	Shutdown();
}

void FSpeechToTextService::Initialize()
{
	UE_LOG(LogSpeech, Log, TEXT("Initializing SpeechToTextService"));
	LoadSettings();
}

void FSpeechToTextService::Shutdown()
{
	UE_LOG(LogSpeech, Log, TEXT("Shutting down SpeechToTextService"));

	// Stop any active session
	if (IsSessionActive())
	{
		StopSession();
	}

	// Clear all providers
	Providers.Empty();
	ActiveProviderId.Empty();
}

void FSpeechToTextService::RegisterProvider(const FString& Id, TSharedPtr<ISpeechProvider> Provider)
{
	if (!Provider.IsValid())
	{
		UE_LOG(LogSpeech, Warning, TEXT("Attempted to register null provider with ID: %s"), *Id);
		return;
	}

	Providers.Add(Id, Provider);
	UE_LOG(LogSpeech, Log, TEXT("Registered speech provider: %s (%s)"), *Id, *Provider->GetDisplayName());

	// If this is the first provider, make it active
	if (ActiveProviderId.IsEmpty())
	{
		SetActiveProvider(Id);
	}
}

void FSpeechToTextService::UnregisterProvider(const FString& Id)
{
	if (Providers.Remove(Id) > 0)
	{
		UE_LOG(LogSpeech, Log, TEXT("Unregistered speech provider: %s"), *Id);

		// If this was the active provider, clear it
		if (ActiveProviderId == Id)
		{
			ActiveProviderId.Empty();
		}
	}
}

TArray<FString> FSpeechToTextService::GetAvailableProviders() const
{
	TArray<FString> ProviderIds;
	Providers.GetKeys(ProviderIds);
	return ProviderIds;
}

void FSpeechToTextService::SetActiveProvider(const FString& Id)
{
	if (!Providers.Contains(Id))
	{
		UE_LOG(LogSpeech, Warning, TEXT("Attempted to set non-existent provider as active: %s"), *Id);
		return;
	}

	// Unbind old provider delegates if any
	if (!ActiveProviderId.IsEmpty() && Providers.Contains(ActiveProviderId))
	{
		TSharedPtr<ISpeechProvider> OldProvider = Providers[ActiveProviderId];
		OldProvider->OnStatusChanged.Unbind();
		OldProvider->OnPartialTranscript.Unbind();
		OldProvider->OnFinalTranscript.Unbind();
		OldProvider->OnError.Unbind();
	}

	// Set new active provider
	ActiveProviderId = Id;
	TSharedPtr<ISpeechProvider> NewProvider = Providers[Id];

	// Bind new provider delegates
	NewProvider->OnStatusChanged.BindSP(this, &FSpeechToTextService::OnProviderStatusChanged);
	NewProvider->OnPartialTranscript.BindSP(this, &FSpeechToTextService::OnProviderPartialTranscript);
	NewProvider->OnFinalTranscript.BindSP(this, &FSpeechToTextService::OnProviderFinalTranscript);
	NewProvider->OnError.BindSP(this, &FSpeechToTextService::OnProviderError);

	UE_LOG(LogSpeech, Log, TEXT("Active speech provider set to: %s"), *Id);
}

TSharedPtr<ISpeechProvider> FSpeechToTextService::GetActiveProvider() const
{
	if (ActiveProviderId.IsEmpty() || !Providers.Contains(ActiveProviderId))
	{
		return nullptr;
	}

	return Providers[ActiveProviderId];
}

void FSpeechToTextService::StartSession(const FSpeechSessionOptions& Options)
{
	TSharedPtr<ISpeechProvider> Provider = GetActiveProvider();
	if (!Provider.IsValid())
	{
		UE_LOG(LogSpeech, Warning, TEXT("Cannot start session: No active provider"));
		OnError.ExecuteIfBound(TEXT("No speech provider configured"));
		return;
	}

	if (!Provider->IsAvailable())
	{
		UE_LOG(LogSpeech, Warning, TEXT("Cannot start session: Provider not available"));
		OnError.ExecuteIfBound(TEXT("Speech provider not configured. Please add API key in settings."));
		return;
	}

	UE_LOG(LogSpeech, Log, TEXT("Starting speech session with provider: %s"), *Provider->GetDisplayName());
	Provider->StartSession(Options);
}

void FSpeechToTextService::StopSession()
{
	TSharedPtr<ISpeechProvider> Provider = GetActiveProvider();
	if (Provider.IsValid())
	{
		UE_LOG(LogSpeech, Log, TEXT("Stopping speech session"));
		Provider->StopSession();
	}

	// Clear auto-submit timer
	if (AutoSubmitTimerHandle.IsValid())
	{
		// TODO: Clear timer when UWorld is available
		AutoSubmitTimerHandle.Invalidate();
	}
}

bool FSpeechToTextService::IsSessionActive() const
{
	TSharedPtr<ISpeechProvider> Provider = GetActiveProvider();
	return Provider.IsValid() && Provider->IsSessionActive();
}

ESpeechToTextStatus FSpeechToTextService::GetStatus() const
{
	TSharedPtr<ISpeechProvider> Provider = GetActiveProvider();
	return Provider.IsValid() ? Provider->GetStatus() : ESpeechToTextStatus::Idle;
}

bool FSpeechToTextService::HasSpeechProvider() const
{
	TSharedPtr<ISpeechProvider> Provider = GetActiveProvider();
	return Provider.IsValid() && Provider->IsAvailable();
}

// Provider event handlers - forward to service delegates

void FSpeechToTextService::OnProviderStatusChanged(ESpeechToTextStatus Status, const FString& Text)
{
	UE_LOG(LogSpeech, Verbose, TEXT("Status changed: %d, Text: %s"), (int32)Status, *Text);
	OnStatusChanged.ExecuteIfBound(Status, Text);
}

void FSpeechToTextService::OnProviderPartialTranscript(const FString& Text)
{
	UE_LOG(LogSpeech, Verbose, TEXT("Partial transcript: %s"), *Text);
	OnPartialTranscript.ExecuteIfBound(Text);
}

void FSpeechToTextService::OnProviderFinalTranscript(const FString& Text)
{
	UE_LOG(LogSpeech, Log, TEXT("Final transcript: %s"), *Text);
	OnFinalTranscript.ExecuteIfBound(Text);

	// TODO: Start auto-submit timer if configured
	// Will implement when we have access to UWorld for timers
}

void FSpeechToTextService::OnProviderError(const FString& Error)
{
	UE_LOG(LogSpeech, Error, TEXT("Speech error: %s"), *Error);
	OnError.ExecuteIfBound(Error);
}

void FSpeechToTextService::OnAutoSubmitTimeout()
{
	// TODO: Implement auto-submit logic
	// Will auto-send final transcript to chat after timeout
	UE_LOG(LogSpeech, Log, TEXT("Auto-submit timeout triggered"));
}

void FSpeechToTextService::LoadSettings()
{
	AutoSubmitTimeout = GetAutoSubmitTimeoutFromConfig();
	UE_LOG(LogSpeech, Log, TEXT("Loaded settings: AutoSubmitTimeout=%.1fs"), AutoSubmitTimeout);
}

// Static config methods

float FSpeechToTextService::GetAutoSubmitTimeoutFromConfig()
{
	float Timeout = 3.0f;
	GConfig->GetFloat(TEXT("VibeUE.VoiceInput"), TEXT("AutoSubmitTimeout"), Timeout, GEditorPerProjectIni);
	return FMath::Clamp(Timeout, 0.0f, 30.0f);
}

void FSpeechToTextService::SaveAutoSubmitTimeoutToConfig(float Timeout)
{
	GConfig->SetFloat(TEXT("VibeUE.VoiceInput"), TEXT("AutoSubmitTimeout"), Timeout, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

FString FSpeechToTextService::GetDefaultLanguageFromConfig()
{
	FString Language;
	GConfig->GetString(TEXT("VibeUE.VoiceInput"), TEXT("DefaultLanguage"), Language, GEditorPerProjectIni);
	return Language;
}

void FSpeechToTextService::SaveDefaultLanguageToConfig(const FString& Language)
{
	GConfig->SetString(TEXT("VibeUE.VoiceInput"), TEXT("DefaultLanguage"), *Language, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

bool FSpeechToTextService::GetVoiceInputEnabledFromConfig()
{
	bool bEnabled = true; // Default to enabled
	GConfig->GetBool(TEXT("VibeUE.VoiceInput"), TEXT("bEnableVoiceInput"), bEnabled, GEditorPerProjectIni);
	return bEnabled;
}

void FSpeechToTextService::SaveVoiceInputEnabledToConfig(bool bEnabled)
{
	GConfig->SetBool(TEXT("VibeUE.VoiceInput"), TEXT("bEnableVoiceInput"), bEnabled, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}
