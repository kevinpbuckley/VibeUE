// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Speech/ElevenLabsSpeechProvider.h"
#include "Speech/SpeechToTextService.h"
#include "AudioCaptureRunnable.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

FElevenLabsSpeechProvider::FElevenLabsSpeechProvider()
	: CurrentStatus(ESpeechToTextStatus::Idle)
{
}

FElevenLabsSpeechProvider::~FElevenLabsSpeechProvider()
{
	StopSession();
}

FString FElevenLabsSpeechProvider::GetDisplayName() const
{
	return TEXT("ElevenLabs Scribe v2");
}

bool FElevenLabsSpeechProvider::IsAvailable() const
{
	return !ApiKey.IsEmpty();
}

void FElevenLabsSpeechProvider::StartSession(const FSpeechSessionOptions& Options)
{
	if (ApiKey.IsEmpty())
	{
		UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Cannot start session - API key not configured"));
		SetStatus(ESpeechToTextStatus::Error);
		OnError.ExecuteIfBound(TEXT("ElevenLabs API key not configured"));
		return;
	}

	if (IsSessionActive())
	{
		UE_LOG(LogSpeech, Warning, TEXT("ElevenLabs: Session already active, stopping previous session"));
		StopSession();
	}

	UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Starting speech-to-text session"));

	// Clean up old audio files
	FString ProjectDir = FPaths::ProjectDir();
	FString SaveDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("Speech"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (PlatformFile.DirectoryExists(*SaveDir))
	{
		TArray<FString> FoundFiles;
		PlatformFile.FindFiles(FoundFiles, *SaveDir, TEXT(".wav"));
		for (const FString& FilePath : FoundFiles)
		{
			PlatformFile.DeleteFile(*FilePath);
		}
		UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Cleaned up %d old audio files"), FoundFiles.Num());
	}

	SessionOptions = Options;
	SetStatus(ESpeechToTextStatus::Connecting);

	// Clear any previous accumulated audio
	{
		FScopeLock Lock(&AudioLock);
		AccumulatedAudio.Empty();
	}

	// Start audio capture
	AudioCaptureThread = MakeShared<FAudioCaptureRunnable>(SessionOptions.SampleRate);
	AudioCaptureThread->OnAudioDataCaptured.BindSP(this, &FElevenLabsSpeechProvider::OnAudioDataAvailable);

	if (!AudioCaptureThread->StartCapture())
	{
		UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Failed to start audio capture"));
		SetStatus(ESpeechToTextStatus::Error);
		OnError.ExecuteIfBound(TEXT("Failed to start microphone capture"));
		return;
	}

	UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Audio capture started - recording..."));
	SetStatus(ESpeechToTextStatus::Started);
	OnStatusChanged.ExecuteIfBound(ESpeechToTextStatus::Started, TEXT(""));
}

void FElevenLabsSpeechProvider::StopSession()
{
	UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Stopping session"));

	// Stop audio capture first
	if (AudioCaptureThread.IsValid())
	{
		AudioCaptureThread->StopCapture();
		AudioCaptureThread.Reset();
	}

	// Get accumulated audio samples
	TArray<float> AudioToSend;
	{
		FScopeLock Lock(&AudioLock);
		AudioToSend = AccumulatedAudio;
		AccumulatedAudio.Empty();
	}

	UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Accumulated %d audio samples"), AudioToSend.Num());

	// If we have audio, convert to WAV and POST to API
	if (AudioToSend.Num() > 0)
	{
		SetStatus(ESpeechToTextStatus::Recognizing);
		OnStatusChanged.ExecuteIfBound(ESpeechToTextStatus::Recognizing, TEXT(""));

		// Convert to WAV format
		TArray<float> ResampledAudio;
		int32 TargetSampleRate = 16000;  // ElevenLabs expects 16kHz
		
		// Save original audio for debugging
		FString ProjectDir = FPaths::ProjectDir();
		FString SaveDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("Speech"));
		FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*SaveDir))
		{
			PlatformFile.CreateDirectoryTree(*SaveDir);
		}
		
		// Save original 48kHz audio
		TArray<uint8> OriginalWAV = ConvertToWAV(AudioToSend, SessionOptions.SampleRate);
		FString OriginalPath = FPaths::Combine(SaveDir, FString::Printf(TEXT("Original_48kHz_%s.wav"), *Timestamp));
		if (FFileHelper::SaveArrayToFile(OriginalWAV, *OriginalPath))
		{
			UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Saved original 48kHz audio to: %s"), *OriginalPath);
		}
		
		// Resample if needed
		if (SessionOptions.SampleRate != TargetSampleRate)
		{
			UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Resampling from %d Hz to %d Hz"), SessionOptions.SampleRate, TargetSampleRate);
			
			float ResampleRatio = (float)TargetSampleRate / (float)SessionOptions.SampleRate;
			int32 TargetNumSamples = FMath::CeilToInt(AudioToSend.Num() * ResampleRatio);
			ResampledAudio.Reserve(TargetNumSamples);
			
			// Simple linear interpolation resampling
			for (int32 i = 0; i < TargetNumSamples; i++)
			{
				float SourceIndex = i / ResampleRatio;
				int32 Index0 = FMath::FloorToInt(SourceIndex);
				int32 Index1 = FMath::Min(Index0 + 1, AudioToSend.Num() - 1);
				float Fraction = SourceIndex - Index0;
				
				if (Index0 < AudioToSend.Num())
				{
					float Sample = FMath::Lerp(AudioToSend[Index0], AudioToSend[Index1], Fraction);
					ResampledAudio.Add(Sample);
				}
			}
			
			UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Resampled from %d to %d samples"), AudioToSend.Num(), ResampledAudio.Num());
		}
		else
		{
			ResampledAudio = AudioToSend;
		}
		
		TArray<uint8> WAVData = ConvertToWAV(ResampledAudio, TargetSampleRate);
		UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Converted to WAV (%d bytes)"), WAVData.Num());

		// Save resampled 16kHz audio for debugging
		FString ResampledPath = FPaths::Combine(SaveDir, FString::Printf(TEXT("Resampled_16kHz_%s.wav"), *Timestamp));
		if (FFileHelper::SaveArrayToFile(WAVData, *ResampledPath))
		{
			UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Saved resampled 16kHz audio to: %s"), *ResampledPath);
		}

		// Build multipart/form-data boundary
		FString Boundary = FString::Printf(TEXT("----UnrealBoundary%d"), FPlatformTime::Cycles());
		FString ContentType = FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary);

		// Build multipart body
		TArray<uint8> MultipartBody;
		FString MultipartString;

		// Add model_id field
		MultipartString += FString::Printf(TEXT("--%s\r\n"), *Boundary);
		MultipartString += TEXT("Content-Disposition: form-data; name=\"model_id\"\r\n\r\n");
		MultipartString += TEXT("scribe_v2\r\n");

		// Add language field if specified
		if (!SessionOptions.LanguageCode.IsEmpty() && !SessionOptions.bAutoDetectLanguage)
		{
			MultipartString += FString::Printf(TEXT("--%s\r\n"), *Boundary);
			MultipartString += TEXT("Content-Disposition: form-data; name=\"language\"\r\n\r\n");
			MultipartString += SessionOptions.LanguageCode + TEXT("\r\n");
		}

		// Add audio file header
		MultipartString += FString::Printf(TEXT("--%s\r\n"), *Boundary);
		MultipartString += TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n");
		MultipartString += TEXT("Content-Type: audio/wav\r\n\r\n");

		// Convert string to bytes
		FTCHARToUTF8 Converter(*MultipartString);
		MultipartBody.Append((uint8*)Converter.Get(), Converter.Length());

		// Append WAV data
		MultipartBody.Append(WAVData);

		// Add final boundary
		FString FinalBoundary = FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary);
		FTCHARToUTF8 FinalConverter(*FinalBoundary);
		MultipartBody.Append((uint8*)FinalConverter.Get(), FinalConverter.Length());

		UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Multipart body size: %d bytes"), MultipartBody.Num());

		// Create HTTP request
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL(TEXT("https://api.elevenlabs.io/v1/speech-to-text"));
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("xi-api-key"), ApiKey);
		HttpRequest->SetHeader(TEXT("Content-Type"), ContentType);
		HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
		HttpRequest->SetTimeout(30.0f);
		HttpRequest->SetContent(MultipartBody);
		HttpRequest->OnProcessRequestComplete().BindSP(this, &FElevenLabsSpeechProvider::OnTranscriptionResponse);

		// Send request
		if (HttpRequest->ProcessRequest())
		{
			UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: HTTP request sent successfully"));
		}
		else
		{
			UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Failed to initiate HTTP request"));
			SetStatus(ESpeechToTextStatus::Error);
			OnError.ExecuteIfBound(TEXT("Failed to send audio to API"));
		}
	}
	else
	{
		UE_LOG(LogSpeech, Warning, TEXT("ElevenLabs: No audio recorded"));
		SetStatus(ESpeechToTextStatus::Stopped);
		OnStatusChanged.ExecuteIfBound(ESpeechToTextStatus::Stopped, TEXT(""));
	}
}

bool FElevenLabsSpeechProvider::IsSessionActive() const
{
	FScopeLock Lock(&StatusLock);
	return CurrentStatus == ESpeechToTextStatus::Connecting ||
	       CurrentStatus == ESpeechToTextStatus::Started ||
	       CurrentStatus == ESpeechToTextStatus::Recognizing ||
	       CurrentStatus == ESpeechToTextStatus::Recognized;
}

ESpeechToTextStatus FElevenLabsSpeechProvider::GetStatus() const
{
	FScopeLock Lock(&StatusLock);
	return CurrentStatus;
}

void FElevenLabsSpeechProvider::SetApiKey(const FString& InApiKey)
{
	ApiKey = InApiKey;
}

// Audio processing

void FElevenLabsSpeechProvider::OnAudioDataAvailable(const TArray<float>& AudioData)
{
	// Accumulate audio data while recording
	// We'll send it all at once when StopSession is called
	FScopeLock Lock(&AudioLock);
	AccumulatedAudio.Append(AudioData);
	UE_LOG(LogSpeech, Verbose, TEXT("ElevenLabs: Accumulated %d samples (total: %d)"), AudioData.Num(), AccumulatedAudio.Num());
}

// HTTP response handling

void FElevenLabsSpeechProvider::OnTranscriptionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	if (!bSuccess)
	{
		UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: HTTP request failed - bSuccess=false"));
		
		if (Request.IsValid())
		{
			FString URL = Request->GetURL();
			EHttpRequestStatus::Type Status = Request->GetStatus();
			UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Request URL: %s, Status: %d"), *URL, (int32)Status);
		}
		
		SetStatus(ESpeechToTextStatus::Error);
		OnError.ExecuteIfBound(TEXT("Network request failed - check internet connection"));
		return;
	}
	
	if (!Response.IsValid())
	{
		UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: HTTP response is invalid"));
		SetStatus(ESpeechToTextStatus::Error);
		OnError.ExecuteIfBound(TEXT("Invalid API response"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();

	UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: HTTP Response %d: %s"), ResponseCode, *ResponseBody);

	if (ResponseCode == 200)
	{
		// Parse JSON response
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			FString TranscribedText;
			if (JsonObject->TryGetStringField(TEXT("text"), TranscribedText))
			{
				// Clean up leading/trailing "..." that ElevenLabs uses for noise/uncertainty
				TranscribedText = TranscribedText.TrimStartAndEnd();
				TranscribedText.RemoveFromStart(TEXT("..."));
				TranscribedText = TranscribedText.TrimStartAndEnd();
				TranscribedText.RemoveFromEnd(TEXT("..."));
				TranscribedText = TranscribedText.TrimStartAndEnd();
				
				// Remove trailing punctuation (?, !, ., , :) to prevent sending punctuation to LLM
				// The LLM will infer intent from context better without speech-inferred punctuation
				while (TranscribedText.Len() > 0)
				{
					TCHAR LastChar = TranscribedText[TranscribedText.Len() - 1];
					if (LastChar == TEXT('?') || LastChar == TEXT('!') || LastChar == TEXT('.') || 
						LastChar == TEXT(',') || LastChar == TEXT(':') || LastChar == TEXT(';'))
					{
						TranscribedText = TranscribedText.Left(TranscribedText.Len() - 1);
					}
					else
					{
						break;
					}
				}
				
				UE_LOG(LogSpeech, Log, TEXT("ElevenLabs: Transcription: %s"), *TranscribedText);

				SetStatus(ESpeechToTextStatus::Recognized);
				OnFinalTranscript.ExecuteIfBound(TranscribedText);
				
				SetStatus(ESpeechToTextStatus::Stopped);
				OnStatusChanged.ExecuteIfBound(ESpeechToTextStatus::Stopped, TEXT(""));
			}
			else
			{
				UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Response missing 'text' field"));
				SetStatus(ESpeechToTextStatus::Error);
				OnError.ExecuteIfBound(TEXT("Invalid API response format"));
			}
		}
		else
		{
			UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Failed to parse JSON response"));
			SetStatus(ESpeechToTextStatus::Error);
			OnError.ExecuteIfBound(TEXT("Failed to parse API response"));
		}
	}
	else
	{
		UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: API error %d: %s"), ResponseCode, *ResponseBody);
		SetStatus(ESpeechToTextStatus::Error);
		
		// Try to extract error message from response
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
		FString ErrorMessage = FString::Printf(TEXT("API error %d"), ResponseCode);
		
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			FString DetailMessage;
			if (JsonObject->TryGetStringField(TEXT("detail"), DetailMessage))
			{
				ErrorMessage = DetailMessage;
				UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Error detail: %s"), *DetailMessage);
			}
			else if (JsonObject->TryGetStringField(TEXT("message"), DetailMessage))
			{
				ErrorMessage = DetailMessage;
				UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Error message: %s"), *DetailMessage);
			}
			
			// Log the full JSON for debugging
			FString JsonStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
			FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
			UE_LOG(LogSpeech, Error, TEXT("ElevenLabs: Full error JSON: %s"), *JsonStr);
		}
		
		OnError.ExecuteIfBound(ErrorMessage);
	}
}

// WAV conversion

TArray<uint8> FElevenLabsSpeechProvider::ConvertToWAV(const TArray<float>& FloatData, int32 SampleRate)
{
	// Convert float samples to PCM16
	TArray<int16> PCM16Samples;
	PCM16Samples.Reserve(FloatData.Num());
	
	for (float Sample : FloatData)
	{
		float ClampedSample = FMath::Clamp(Sample, -1.0f, 1.0f);
		int16 PCM16Sample = static_cast<int16>(ClampedSample * 32767.0f);
		PCM16Samples.Add(PCM16Sample);
	}

	// Calculate sizes
	int32 NumChannels = 1;  // Mono
	int32 BitsPerSample = 16;
	int32 BytesPerSample = BitsPerSample / 8;
	int32 DataSize = PCM16Samples.Num() * BytesPerSample;
	int32 FileSize = 36 + DataSize;  // 36 = size of WAV header without data

	TArray<uint8> WAVData;
	WAVData.Reserve(44 + DataSize);  // 44 = full WAV header size

	// Helper lambda to write bytes
	auto WriteBytes = [&WAVData](const void* Data, int32 Size)
	{
		const uint8* ByteData = static_cast<const uint8*>(Data);
		for (int32 i = 0; i < Size; i++)
		{
			WAVData.Add(ByteData[i]);
		}
	};

	auto WriteString = [&WriteBytes](const char* Str)
	{
		WriteBytes(Str, 4);
	};

	auto WriteUInt32 = [&WriteBytes](uint32 Value)
	{
		WriteBytes(&Value, 4);
	};

	auto WriteUInt16 = [&WriteBytes](uint16 Value)
	{
		WriteBytes(&Value, 2);
	};

	// RIFF header
	WriteString("RIFF");
	WriteUInt32(FileSize);
	WriteString("WAVE");

	// fmt chunk
	WriteString("fmt ");
	WriteUInt32(16);  // fmt chunk size
	WriteUInt16(1);   // PCM format
	WriteUInt16(NumChannels);
	WriteUInt32(SampleRate);
	WriteUInt32(SampleRate * NumChannels * BytesPerSample);  // Byte rate
	WriteUInt16(NumChannels * BytesPerSample);  // Block align
	WriteUInt16(BitsPerSample);

	// data chunk
	WriteString("data");
	WriteUInt32(DataSize);

	// Write PCM samples (little-endian)
	for (int16 Sample : PCM16Samples)
	{
		WriteUInt16(static_cast<uint16>(Sample));
	}

	return WAVData;
}

// Utility methods

void FElevenLabsSpeechProvider::SetStatus(ESpeechToTextStatus NewStatus)
{
	FScopeLock Lock(&StatusLock);
	CurrentStatus = NewStatus;
}

// Static config methods

FString FElevenLabsSpeechProvider::GetApiKeyFromConfig()
{
	FString ApiKey;
	GConfig->GetString(TEXT("VibeUE.VoiceInput"), TEXT("ElevenLabsApiKey"), ApiKey, GEditorPerProjectIni);
	return ApiKey;
}

void FElevenLabsSpeechProvider::SaveApiKeyToConfig(const FString& ApiKey)
{
	GConfig->SetString(TEXT("VibeUE.VoiceInput"), TEXT("ElevenLabsApiKey"), *ApiKey, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}
