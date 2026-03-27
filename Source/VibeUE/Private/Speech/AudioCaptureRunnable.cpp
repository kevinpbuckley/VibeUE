// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "AudioCaptureRunnable.h"
#include "Speech/SpeechToTextService.h"

FAudioCaptureRunnable::FAudioCaptureRunnable(int32 InSampleRate)
	: TargetSampleRate(InSampleRate)
	, bIsRunning(false)
	, bIsCapturing(false)
	, Thread(nullptr)
	, ChunkSize(1600) // 100ms at 16kHz
{
	AudioCapture = MakeUnique<Audio::FAudioCaptureSynth>();
}

FAudioCaptureRunnable::~FAudioCaptureRunnable()
{
	Stop();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FAudioCaptureRunnable::Init()
{
	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Initializing"));

	if (!AudioCapture.IsValid())
	{
		UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: AudioCapture is null"));
		return false;
	}

	// Open default audio capture stream
	if (!AudioCapture->OpenDefaultStream())
	{
		UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: Failed to open default stream"));
		return false;
	}

	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Initialized successfully"));
	return true;
}

uint32 FAudioCaptureRunnable::Run()
{
	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Run() started"));

	bIsRunning = true;

	TArray<float> AudioData;
	int32 LastProcessedSamples = 0;  // Track how many samples we've already processed
	
	// FAudioCaptureSynth has an internal buffer limit (~192000 samples at 48kHz = 4 seconds)
	// We need to restart capture periodically to avoid buffer overflow
	const int32 BufferResetThreshold = 144000;  // Reset at 3 seconds (75% of buffer) to avoid overflow

	while (bIsRunning)
	{
		if (bIsCapturing && AudioCapture.IsValid() && AudioCapture->IsStreamOpen())
		{
			// Get audio data from capture synth
			if (AudioCapture->GetAudioData(AudioData))
			{
				// GetAudioData returns cumulative buffer - we only want NEW samples
				int32 TotalSamples = AudioData.Num();
				int32 NewSampleCount = TotalSamples - LastProcessedSamples;
				
				if (NewSampleCount > 0)
				{
					// Extract only the NEW samples
					TArray<float> NewSamples;
					NewSamples.Append(&AudioData[LastProcessedSamples], NewSampleCount);
					LastProcessedSamples = TotalSamples;
					
					// Process audio data
					FScopeLock Lock(&BufferLock);
					CapturedBuffer.Append(NewSamples);

					UE_LOG(LogSpeech, Verbose, TEXT("AudioCaptureRunnable: Got %d new samples (total: %d, buffer: %d)"), 
						NewSampleCount, TotalSamples, CapturedBuffer.Num());

					// If we have enough samples for a chunk, fire delegate
					while (CapturedBuffer.Num() >= ChunkSize)
					{
						// Extract chunk
						TArray<float> Chunk;
						Chunk.Append(CapturedBuffer.GetData(), ChunkSize);

						// Remove extracted samples from buffer
						CapturedBuffer.RemoveAt(0, ChunkSize, EAllowShrinking::No);

						// Fire delegate
						// Note: This is called from background thread, so the callback must be thread-safe!
						OnAudioDataCaptured.ExecuteIfBound(Chunk);

						UE_LOG(LogSpeech, Verbose, TEXT("AudioCaptureRunnable: Captured chunk of %d samples"), ChunkSize);
					}
				}
				
				// Check if internal buffer is getting full - restart capture to reset it
				// This prevents "Attempt to write past end of buffer" warnings
				if (TotalSamples >= BufferResetThreshold)
				{
					UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Resetting capture buffer (had %d samples)"), TotalSamples);
					
					// Stop and restart to clear internal buffer
					AudioCapture->StopCapturing();
					AudioData.Empty();
					LastProcessedSamples = 0;
					
					// Small delay to ensure clean restart
					FPlatformProcess::Sleep(0.001f);
					
					// Restart capturing
					if (!AudioCapture->StartCapturing())
					{
						UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: Failed to restart capturing after buffer reset"));
						bIsCapturing = false;
					}
				}
			}
		}
		else if (!bIsCapturing)
		{
			// Reset sample tracking when not capturing
			LastProcessedSamples = 0;
		}

		// Sleep briefly to avoid busy-waiting (check every 50ms)
		FPlatformProcess::Sleep(0.05f);
	}

	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Run() ended"));
	return 0;
}

void FAudioCaptureRunnable::Stop()
{
	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Stop() called"));

	bIsRunning = false;
	StopCapture();
}

void FAudioCaptureRunnable::Exit()
{
	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Exit() called"));

	if (AudioCapture.IsValid() && AudioCapture->IsStreamOpen())
	{
		AudioCapture->AbortCapturing();
	}
}

bool FAudioCaptureRunnable::StartCapture()
{
	if (bIsCapturing)
	{
		UE_LOG(LogSpeech, Warning, TEXT("AudioCaptureRunnable: Already capturing"));
		return true;
	}

	if (!AudioCapture.IsValid())
	{
		UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: Cannot start capture, AudioCapture is null"));
		return false;
	}

	// Create and start thread FIRST if not already running
	// The thread's Init() method opens the audio stream
	if (!Thread)
	{
		Thread = FRunnableThread::Create(this, TEXT("AudioCaptureThread"), 0, TPri_Normal);
		
		// Wait for thread to initialize the audio stream (with timeout)
		int32 WaitAttempts = 0;
		while (!AudioCapture->IsStreamOpen() && WaitAttempts < 50)
		{
			FPlatformProcess::Sleep(0.01f);
			WaitAttempts++;
		}
		
		if (!AudioCapture->IsStreamOpen())
		{
			UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: Audio stream did not open after waiting"));
			return false;
		}
	}

	// Verify stream is open before starting capture
	if (!AudioCapture->IsStreamOpen())
	{
		UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: Cannot start capturing - stream is not open"));
		return false;
	}

	// Now start audio capture (stream should be open from Init())
	if (!AudioCapture->StartCapturing())
	{
		UE_LOG(LogSpeech, Error, TEXT("AudioCaptureRunnable: Failed to start capturing"));
		return false;
	}

	bIsCapturing = true;

	double CurrentTime = FPlatformTime::Seconds();
	UE_LOG(LogSpeech, Warning, TEXT("[VOICE DEBUG] AudioCaptureRunnable::StartCapture() started at time %.3f"), CurrentTime);
	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Started capturing audio"));
	return true;
}

void FAudioCaptureRunnable::StopCapture()
{
	if (!bIsCapturing)
	{
		UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: StopCapture called but not capturing"));
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	UE_LOG(LogSpeech, Warning, TEXT("[VOICE DEBUG] AudioCaptureRunnable::StopCapture() called at time %.3f"), CurrentTime);
	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: StopCapture called - stopping audio capture"));

	if (AudioCapture.IsValid())
	{
		AudioCapture->StopCapturing();
	}

	bIsCapturing = false;

	// Clear buffer
	{
		FScopeLock Lock(&BufferLock);
		CapturedBuffer.Empty();
	}

	UE_LOG(LogSpeech, Log, TEXT("AudioCaptureRunnable: Stopped capturing audio"));
}

TArray<float> FAudioCaptureRunnable::ConvertToMono(const float* AudioData, int32 NumFrames, int32 NumChannels)
{
	TArray<float> MonoBuffer;

	if (NumChannels == 1)
	{
		// Already mono, just copy
		MonoBuffer.Append(AudioData, NumFrames);
	}
	else if (NumChannels == 2)
	{
		// Convert stereo to mono by averaging channels
		MonoBuffer.Reserve(NumFrames);
		for (int32 i = 0; i < NumFrames; ++i)
		{
			float LeftSample = AudioData[i * 2];
			float RightSample = AudioData[i * 2 + 1];
			float MonoSample = (LeftSample + RightSample) * 0.5f;
			MonoBuffer.Add(MonoSample);
		}
	}
	else
	{
		// Multi-channel audio (rare for microphones), average all channels
		MonoBuffer.Reserve(NumFrames);
		for (int32 i = 0; i < NumFrames; ++i)
		{
			float Sum = 0.0f;
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				Sum += AudioData[i * NumChannels + Channel];
			}
			float MonoSample = Sum / static_cast<float>(NumChannels);
			MonoBuffer.Add(MonoSample);
		}
	}

	return MonoBuffer;
}
